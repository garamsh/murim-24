## Captures one place in the M1 battle from world_view.tscn at tick SHOT, the camera held there
## instead of on the battle's centre: the whole frame and a 3x crop of its middle. Run with
## --fixed-fps 30, so each frame advances the world by exactly one tick and both trees shoot the
## same tick. TAG names the tree, LABEL the shot, OUT the directory the PNGs go to.
##
## AT "x,y" (sub-cell units) holds the camera on that point from the first frame, the same point in
## both trees. AT "crowded" instead moves it, one tick before the shot, onto the body with the most
## hostile bodies within 3.2 cells in this tree, so each tree shows its own most crowded foe.
##
## For the shot it prints, for the body nearest the camera's point, the hostile bodies within 3.2
## cells (820 sub-cell units) of it, split at 576 units into those near enough to strike (the strike
## ring sits at 480) and those standing back (the waiting ring sits at 672), and its allies within
## the same 3.2 cells.
extends SceneTree

const NEAR := 820.0
const STRIKE_BAND := 576.0
const CROP := Vector2i(640, 360)
const ZOOM := 3

var view: Node
var frame := 0
var tag := OS.get_environment("TAG")
var label := OS.get_environment("LABEL")
var out := OS.get_environment("OUT")
var shot := int(OS.get_environment("SHOT"))
var crowded := OS.get_environment("AT") == "crowded"
var at := Vector2.ZERO


func _initialize() -> void:
	TranslationServer.set_locale("ko")
	root.size = Vector2i(1920, 1080)
	DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_ALWAYS_ON_TOP, true)
	view = load("res://world_view/world_view.tscn").instantiate()
	root.add_child(view)
	_quiet()
	if crowded:
		at = Vector2(WorldAdapter.m1_centre())
	else:
		var given := OS.get_environment("AT")
		at = Vector2(float(given.get_slice(",", 0)), float(given.get_slice(",", 1)))


func _process(_delta: float) -> bool:
	frame += 1
	var ticks: int = view.ticks
	if crowded and ticks == shot - 1:
		at = _bodies().position[_most_crowded()]
	var art: Node2D = view.get_node("WorldArt")
	var camera: Camera2D = view.get_node("Camera")
	camera.position = art.to_screen(at)
	if ticks == shot:
		_shoot()
		quit(0)
	elif frame > shot + 300:
		quit(2)
	return false


func _shoot() -> void:
	var image := root.get_texture().get_image()
	var name := "%s-%s-tick%04d" % [tag, label, shot]
	image.save_png("%s/ring/%s-frame.png" % [out, name])
	var corner := (root.size - CROP) / 2
	var crop := image.get_region(Rect2i(corner, CROP))
	crop.resize(CROP.x * ZOOM, CROP.y * ZOOM, Image.INTERPOLATE_NEAREST)
	crop.save_png("%s/ring/%s.png" % [out, name])
	var b := _bodies()
	var nearest := 0
	for i in b.position.size():
		if b.position[i].distance_to(at) < b.position[nearest].distance_to(at):
			nearest = i
	var c := _counts(b, nearest)
	print("ring: %s hash %s camera %s: person %d (faction %d) at %s, hostile within 3.2 cells %d (striking band %d, standing back %d), allies %d" % [
		name, view.world.hash(), at, b.person[nearest], b.side[nearest], b.position[nearest],
		c.hostile, c.striking, c.hostile - c.striking, c.allies])


# Every living body's person, position and faction, from one snapshot.
func _bodies() -> Dictionary:
	var snap: Dictionary = view.world.snapshot()
	var first: PackedInt64Array = snap.first
	var faction: PackedInt64Array = snap.faction
	var side := PackedInt64Array()
	side.resize(snap.position.size())
	for army in first.size() - 1:
		for i in range(first[army], first[army + 1]):
			side[i] = faction[army]
	return {"person": snap.person, "position": snap.position, "side": side}


# The hostile bodies within 3.2 cells of body `i`, those of them in the striking band, and its
# allies within 3.2 cells.
func _counts(b: Dictionary, i: int) -> Dictionary:
	var world: WorldAdapter = view.world
	var position: PackedVector2Array = b.position
	var side: PackedInt64Array = b.side
	var counts := {"hostile": 0, "striking": 0, "allies": 0}
	for j in position.size():
		var apart := position[i].distance_to(position[j])
		if j == i or apart > NEAR:
			continue
		if side[j] == side[i]:
			counts.allies += 1
		elif world.hostile(side[j], side[i]):
			counts.hostile += 1
			if apart <= STRIKE_BAND:
				counts.striking += 1
	return counts


# The body with the most hostile bodies within 3.2 cells, the first in snapshot order on a tie.
func _most_crowded() -> int:
	var b := _bodies()
	var best := 0
	var most := -1
	for i in b.position.size():
		var hostile: int = _counts(b, i).hostile
		if hostile > most:
			most = hostile
			best = i
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
