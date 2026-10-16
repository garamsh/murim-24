## Checks the pixels world_art/outline.gdshader draws, which need a renderer, as
## world_art/bodies_test.gd checks only what the view gives it. It is a tool, not a test: no test
## runs it. Run it by hand on a machine with a display, as
## `godot --path . --script res://world_art/outline_check_tool.gd`, and report what it prints in
## the pull request. It draws the whole M1 battle at 1x, a tick at a time. On a tick an outlined body
## flashes, struck, it draws the frame once with the outlines hidden, then once for each such body
## and for up to STILL_BODIES outlined bodies that do not flash, with that body's outline alone, so
## no other body's outline is counted as its own. For each it counts the pixels of its outline
## frame its outline drew on screen, and those of them where the body's own pixel shows, in its
## band's colour, flashed or not. Once flashing bodies' outlines have drawn FLASHING_DRAWN pixels
## or more and a still body's some, over what hides them, it reports: no outline may be drawn over
## its body's own pixels shown. It exits 0 when none is, 1 when one is or the battle's ticks, frames
## or MAX_SECONDS run out first, and 2 when it cannot draw.
extends SceneTree

const SCENE := "res://world_art/world_art.tscn"
const WorldArt := preload("res://world_art/world_art.gd")
const Ground := preload("res://world_art/ground.gd")
# The fewest frames before the battle ticks; it waits on until every area of the ground is baked
# and shown.
const SETTLE_FRAMES := 60
const BATTLE_TICKS := 600
# The impact core a flashing pixel is drawn in, as world_art/flash.gdshaderinc's IMPACT.
const IMPACT := Color("fff8dc")
# The 4x4 ordered dither world_art/dither.gdshaderinc flashes a body's pixels by.
const BAYER := [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]
# How near two colours read back are to be the same: within a step of 8 bits, as a palette colour
# drawn reads back exactly, where ground shaded near a body's ink must not pass for it.
const SAME := 0.004
# The fewest outline pixels flashing bodies' outlines must draw on screen before it reports, and
# the most still bodies drawn alone a tick.
const FLASHING_DRAWN := 20
const STILL_BODIES := 4
# Frames waited after the view changes before what it draws is read back, so the image read is of
# the change.
const DRAW_FRAMES := 3
# The most frames and wall-clock seconds it runs before it gives up and fails, so a run that cannot
# finish says so instead of running on.
const MAX_FRAMES := 60000
const MAX_SECONDS := 600

var _viewport: SubViewport
var _art: Node2D
var _world: WorldAdapter
var _view := Vector2i.ZERO
var _frame := 0
var _wait := 0
var _ticks := 0
var _phase := "settle"
# The bodies drawn alone this tick, the next of them, and the frame with no outline.
var _chosen: Array[Dictionary] = []
var _next := 0
var _without: Image
# What the outlines drawn alone have drawn so far.
var _flashing := 0
var _flashing_drawn := 0
var _still := 0
var _over := 0
var _lines: Array[String] = []


func _initialize() -> void:
	if DisplayServer.get_name() == "headless":
		printerr("outline_check: needs a display to draw")
		quit(2)
		return
	_world = WorldAdapter.create_m1()
	_view = Vector2i(_whole().size)
	_viewport = SubViewport.new()
	_viewport.size = _view
	_viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(_viewport)
	_art = load(SCENE).instantiate()
	_viewport.add_child(_art)
	_art.show_world(_world, -1)
	# What combat draws moves with time between the frames compared; it is not under check.
	_art.get_node("Combat").visible = false


func _process(_delta: float) -> bool:
	_frame += 1
	var seconds := Time.get_ticks_msec() / 1000
	if _frame > MAX_FRAMES or seconds > MAX_SECONDS:
		printerr("outline_check: stopped after %d frames and %d s, %d ticks, flashing bodies' outlines drew %d pixels and %d still ones' some" % [
			_frame, seconds, _ticks, _flashing_drawn, _still])
		quit(1)
		return false
	if _wait > 0:
		_wait -= 1
		return false
	match _phase:
		"settle":
			_art.refresh(_whole(), 1.0)
			# The trees and raised strips hide bodies only once their areas are baked and shown, and a
			# loaded machine bakes them over more frames.
			var ground: Node2D = _art.get_node("Ground")
			var areas := ceili(_whole().size.x / (Ground.CHUNK * WorldArt.CELL.x)) \
					* ceili(_whole().size.y / (Ground.CHUNK * WorldArt.CELL.y))
			if _frame >= SETTLE_FRAMES and ground.baked and ground.shown == areas:
				print("outline_check: the ground's %d areas baked and shown after %d frames" % [areas, _frame])
				_phase = "battle"
		"battle":
			_ticks += 1
			if _ticks > BATTLE_TICKS:
				printerr("outline_check: in %d ticks flashing bodies' outlines drew %d pixels and %d still ones' some; %d outline pixels over a body's own pixels shown" % [
					BATTLE_TICKS, _flashing_drawn, _still, _over])
				quit(1)
				return false
			_world.tick()
			_art.note_tick()
			_art.refresh(_whole(), 1.0)
			_chosen = _to_draw(_outlined_bodies())
			if not _chosen.is_empty():
				_art.get_node("Outlines").visible = false
				_wait = DRAW_FRAMES
				_phase = "without"
		"without":
			# Nothing is refreshed until the next tick, so every frame read until then is of this one.
			_without = _viewport.get_texture().get_image()
			_next = 0
			_show_alone()
			_phase = "alone"
		"alone":
			_tally(_chosen[_next], _viewport.get_texture().get_image())
			_next += 1
			if _next < _chosen.size():
				_show_alone()
			else:
				_judge()
	return false


func _whole() -> Rect2:
	var rows := _world.map_rows()
	return Rect2(Vector2.ZERO, Vector2(rows[0].length(), rows.size()) * WorldArt.CELL)


# The outlined bodies of the last refresh, each as {centre, flip, outline, body, white, instance}:
# its frames in atlas pixels, the share of white its band draws it with, and its outline's floats
# in the outlines' buffer. Each outline's body is found in its band by its centre and frame.
func _outlined_bodies() -> Array[Dictionary]:
	var size: Vector2 = _art.get_node("Outlines").texture.get_size()
	var whites := {}
	for child in _art.get_node("Standing").get_children():
		if child is MultiMeshInstance2D:
			var band: MultiMeshInstance2D = child
			var data := band.multimesh.buffer
			for i in band.multimesh.visible_instance_count:
				var base := i * WorldArt.BODY_FLOATS
				whites[[Vector2(data[base + 3], data[base + 7]) + band.position,
						Vector2(data[base + 12], data[base + 13])]] = data[base + 14]
	var outlines: MultiMeshInstance2D = _art.get_node("Outlines")
	var data := outlines.multimesh.buffer
	var found: Array[Dictionary] = []
	for i in outlines.multimesh.visible_instance_count:
		var base := i * WorldArt.BODY_FLOATS
		var key := [Vector2(data[base + 3], data[base + 7]), Vector2(data[base + 14], data[base + 15])]
		if whites.has(key):
			found.append({"centre": key[0], "flip": data[base] < 0.0,
				"outline": Vector2(data[base + 12], data[base + 13]) * size, "body": key[1] * size,
				"white": whites[key], "instance": data.slice(base, base + WorldArt.BODY_FLOATS)})
	return found


# The flashing bodies of `outlined` and up to STILL_BODIES still ones; none when none flashes.
func _to_draw(outlined: Array[Dictionary]) -> Array[Dictionary]:
	var chosen: Array[Dictionary] = []
	var still: Array[Dictionary] = []
	for body in outlined:
		if body.white > 0.5:
			chosen.append(body)
		elif body.white == 0.0 and still.size() < STILL_BODIES:
			still.append(body)
	if not chosen.is_empty():
		chosen.append_array(still)
	return chosen


# Draws the outlines with the next chosen body's alone.
func _show_alone() -> void:
	var outlines: MultiMeshInstance2D = _art.get_node("Outlines")
	var data := outlines.multimesh.buffer
	var instance: PackedFloat32Array = _chosen[_next].instance
	for i in WorldArt.BODY_FLOATS:
		data[i] = instance[i]
	outlines.multimesh.buffer = data
	outlines.multimesh.visible_instance_count = 1
	outlines.visible = true
	_wait = DRAW_FRAMES


func _tally(body: Dictionary, alone: Image) -> void:
	var counts := _count(body, _art.get_node("Outlines").texture.get_image(), alone)
	_over += counts.over_body
	if counts.drawn > 0 and counts.body_shown > 0:
		if body.white > 0.5:
			_flashing += 1
			_flashing_drawn += counts.drawn
		else:
			_still += 1
	if counts.over_body > 0 or (counts.drawn > 0 and body.white > 0.5):
		_lines.append("outline_check: on tick %d a body drawn %.2f of the way to white at %s: %d of its pixels shown, %d outline pixels drawn on screen, %d of them over its own pixels" % [
			_ticks, body.white, body.centre, counts.body_shown, counts.drawn, counts.over_body])


# Reports once flashing bodies' outlines drew FLASHING_DRAWN pixels and a still one's some, or goes
# on to the next tick.
func _judge() -> void:
	_phase = "battle"
	if _flashing_drawn < FLASHING_DRAWN or _still == 0:
		return
	for line in _lines:
		print(line)
	var passed := _over == 0
	print("outline_check: after %d ticks, the outlines of %d flashing bodies drew %d pixels on screen and of %d still ones some; %d outline pixels over a body's own pixels shown: %s" % [
		_ticks, _flashing, _flashing_drawn, _still, _over, "pass" if passed else "FAIL"])
	quit(0 if passed else 1)


# Over `body`'s frame on screen: its pixels shown in the colour its band draws them, the pixels its
# outline drew, and those of them that are its pixels shown.
func _count(body: Dictionary, atlas: Image, alone: Image) -> Dictionary:
	var counts := {"body_shown": 0, "drawn": 0, "over_body": 0}
	var frame := WorldArt.BODY_FRAME
	var top_left := Vector2i((body.centre - Vector2(frame) / 2.0).round())
	for v in frame.y:
		for u in frame.x:
			var source := Vector2i(frame.x - 1 - u if body.flip else u, v)
			var screen := top_left + Vector2i(u, v)
			if not Rect2i(Vector2i.ZERO, _view).has_point(screen):
				continue
			var at := Vector2i(body.body) + source
			var own := atlas.get_pixelv(at)
			# The colour its band draws the pixel in, as world_art/flash.gdshaderinc decides it.
			var flashed: bool = body.white > (BAYER[(at.y % 4) * 4 + at.x % 4] + 0.5) / 16.0
			var shown := _without.get_pixelv(screen)
			var is_shown: bool = own.a > 0.5 and _same(shown, IMPACT if flashed else own)
			if is_shown:
				counts.body_shown += 1
			# A pixel the outline drew shows its colour, where the frame without it did not: whatever
			# else moves between the frames read is not counted.
			var outline := atlas.get_pixelv(Vector2i(body.outline) + source)
			if outline.a > 0.5 and _same(alone.get_pixelv(screen), outline) and not _same(shown, outline):
				counts.drawn += 1
				if is_shown:
					counts.over_body += 1
	return counts


func _same(a: Color, b: Color) -> bool:
	return Vector3(a.r - b.r, a.g - b.g, a.b - b.b).length() < SAME
