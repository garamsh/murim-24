## Captures the M1 battle from world_view.tscn at fixed ticks, with the camera on the battle's centre
## as the scene places it, and for each shot a 3x crop round the foe most ringed by hostile people.
## Run with --fixed-fps 30, so each frame advances the world by exactly one tick and both trees
## shoot the same ticks. TAG names the tree; OUT the directory the PNGs go to.
##
## The ring is measured from the snapshot: for each living body, the hostile bodies within 3.2 cells
## (820 sub-cell units) of it, split at 576 units into those near enough to strike (the strike ring
## sits at 480) and those standing back (the waiting ring sits at 672), less four times its allies within
## the same 3.2 cells, so a foe cut off from its own side scores highest.
extends SceneTree

const SHOTS := [90, 180, 270, 360, 450, 600, 750, 900, 1050, 1200, 1350, 1500, 1650, 1800]
# Ticks at which only the most ringed foe is measured and printed, to choose the ring's shot.
const PROBE_EVERY := 30
const LAST := 2400
const NEAR := 820.0
const STRIKE_BAND := 576.0
const CROP := Vector2i(640, 360)
const ZOOM := 3

var view: Node
var frame := 0
var tag := OS.get_environment("TAG")
var out := OS.get_environment("OUT")


func _initialize() -> void:
	TranslationServer.set_locale("ko")
	root.size = Vector2i(1920, 1080)
	DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_ALWAYS_ON_TOP, true)
	view = load("res://world_view/world_view.tscn").instantiate()
	root.add_child(view)
	_quiet()


func _process(_delta: float) -> bool:
	frame += 1
	var ticks: int = view.ticks
	if _shot_pending(ticks):
		if ticks in SHOTS or ticks in _extra:
			_shoot(ticks)
		elif ticks % PROBE_EVERY == 0:
			_done[ticks] = true
			var ring := _most_ringed()
			if not ring.is_empty():
				print("probe: tick %d person %d at %s hostile %d (striking band %d, standing back %d), allies %d" % [
					ticks, ring.person, ring.position, ring.hostile, ring.striking, ring.back, ring.allies])
	if ticks > LAST or frame > LAST + 300:
		quit(0 if ticks > LAST else 2)
	return false


var _done := {}
# More shot ticks, from SHOTS in the environment, comma separated.
var _extra := Array(OS.get_environment("SHOTS").split(",", false)).map(func(t): return int(t))


func _shot_pending(ticks: int) -> bool:
	return not _done.has(ticks)


func _shoot(ticks: int) -> void:
	_done[ticks] = true
	var image := root.get_texture().get_image()
	var name := "%s-tick%04d" % [tag, ticks]
	image.save_png("%s/m1/%s.png" % [out, name])
	var ring := _most_ringed()
	var line := "capture: %s hash %s bodies %d" % [name, view.world.hash(), view.bodies]
	if ring.is_empty():
		print(line + " no foe ringed")
		return
	var camera: Camera2D = view.get_node("Camera")
	var art: Node2D = view.get_node("WorldArt")
	var at: Vector2 = (art.to_screen(ring.position) - camera.position) * camera.zoom + Vector2(root.size) / 2.0
	var corner := (at - Vector2(CROP) / 2.0).round()
	corner = corner.clamp(Vector2.ZERO, Vector2(root.size - CROP))
	var crop := image.get_region(Rect2i(Vector2i(corner), CROP))
	crop.resize(CROP.x * ZOOM, CROP.y * ZOOM, Image.INTERPOLATE_NEAREST)
	crop.save_png("%s/ring/%s.png" % [out, name])
	print(line + " ringed person %d at %s screen %s: hostile within 3.2 cells %d (striking band %d, standing back %d), allies %d" % [
		ring.person, ring.position, at.round(), ring.hostile, ring.striking, ring.back, ring.allies])


# The living body with the most hostile bodies round it, less four times its allies, and the counts.
func _most_ringed() -> Dictionary:
	var world: WorldAdapter = view.world
	var snap: Dictionary = world.snapshot()
	var first: PackedInt64Array = snap.first
	var faction: PackedInt64Array = snap.faction
	var position: PackedVector2Array = snap.position
	var person: PackedInt64Array = snap.person
	var side := PackedInt64Array()
	side.resize(position.size())
	for army in first.size() - 1:
		for i in range(first[army], first[army + 1]):
			side[i] = faction[army]
	var best := {}
	var best_score := -INF
	for i in position.size():
		var hostile := 0
		var striking := 0
		var allies := 0
		for j in position.size():
			if j == i:
				continue
			var apart := position[i].distance_to(position[j])
			if apart > NEAR:
				continue
			if side[j] == side[i]:
				allies += 1
			elif world.hostile(side[j], side[i]):
				hostile += 1
				if apart <= STRIKE_BAND:
					striking += 1
		var score := hostile - 4 * allies
		if hostile >= 6 and score > best_score:
			best_score = score
			best = {"person": person[i], "position": position[i], "hostile": hostile,
					"striking": striking, "back": hostile - striking, "allies": allies}
	return best


# The settings rows that cut what a capture shows besides the bodies, as play.gd applies them:
# damage numbers off, blood off, hit stop (and its screen shake) off, rain off. Screen quality
# stays at its default, standard.
func _quiet() -> void:
	var combat: Node = view.get_node("WorldArt/Combat")
	combat.damage_numbers = "off"
	combat.blood = false
	combat.hit_stop = false
	view.get_node("Grade").raining = false
