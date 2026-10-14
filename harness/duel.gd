## A staged duel for #184's evidence: an Orthodox swordsman sweeping a Demonic foe beside it, drawn by
## world_art.tscn at 3x, captured every third frame from the tick the sweep lands, Korean locale.
## Also a dart thrower (Unorthodox, a line) and an area user, one each, in runs of their own.
extends SceneTree
const MAP_ROWS: Array[String] = ["............", "............", "............", "............", "............",
	"............", "............", "............", "............"]
const STEP := {"range": 1, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 1,
	"shape": WorldAdapter.SHAPE_TARGETED}
const KINDS := {
	"sweep": {"range": 384, "radius": 0, "body_damage": 10, "structure_damage": 0, "cooldown": 30, "shape": WorldAdapter.SHAPE_ARC},
	"dart": {"range": 2048, "radius": 128, "body_damage": 10, "structure_damage": 0, "cooldown": 30, "shape": WorldAdapter.SHAPE_LINE},
	"area": {"range": 384, "radius": 512, "body_damage": 10, "structure_damage": 0, "cooldown": 30, "shape": WorldAdapter.SHAPE_AREA},
}
const FACTION := {"sweep": 0, "dart": 2, "area": 1}
const RULES := {"separation": 256, "hostile": [[false, true, true], [true, false, true], [true, true, false]],
	"stage_floors": [75, 25, 1]}
var art: Node2D
var world: WorldAdapter
var camera: Camera2D
var focus := Vector2.ZERO
var hit_ms := 0
var held: Array[Image] = []
var times := PackedInt64Array()
var kinds := ["sweep", "dart", "area"]
var kind := 0
var frame := 0
var shot := -1


func _initialize() -> void:
	TranslationServer.set_locale("ko")
	root.size = Vector2i(1920, 1080)
	DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_ALWAYS_ON_TOP, true)
	_start()


func _start() -> void:
	if art != null:
		art.queue_free()
	var name: String = kinds[kind]
	var gap := 1 if name != "dart" else 6
	var armies: Array[Dictionary] = [
		{"objective": Vector2i(3, 4), "faction": FACTION[name], "skills": [KINDS[name]], "members": [{"cell": Vector2i(3, 4), "health": 100}]},
		{"objective": Vector2i(3 + gap, 4), "faction": 1 if name != "area" else 0, "skills": [STEP],
			"members": [{"cell": Vector2i(3 + gap, 4), "health": 100}]},
	]
	world = WorldAdapter.create(1, PackedStringArray(MAP_ROWS), armies, [], RULES)
	art = load("res://world_art/world_art.tscn").instantiate()
	root.add_child(art)
	art.show_world(world, -1)
	art.pixel_scale = 3
	camera = Camera2D.new()
	focus = art.to_screen(Vector2(3 + gap / 2.0, 4.5) * WorldAdapter.CELL_SIZE).round()
	# Drawn at 3x by scaling the art itself, the focus in the window's middle.
	art.scale = Vector2(3, 3)
	art.position = (Vector2(root.size) / 2.0 - focus * 3.0).round()
	frame = 0
	shot = -1


func _process(_delta: float) -> bool:
	frame += 1
	var screen := Vector2(root.size) / 3.0
	var view := Rect2(focus - screen / 2.0, screen)
	if shot < 0 and frame > 30 and frame % 4 == 0:
		world.tick()
		art.note_tick()
		if not world.strikes().filter(func(s: Dictionary) -> bool: return s.body_damage > 0).is_empty():
			shot = 0
			hit_ms = Time.get_ticks_msec()
	art.refresh(view, 1.0)
	if shot >= 0:
		held.append(root.get_texture().get_image())
		times.append(Time.get_ticks_msec() - hit_ms)
		shot += 1
		if shot >= 16:
			for n in held.size():
				held[n].save_png("res://build/issue184/seq/%s%d-ko.png" % [kinds[kind], n])
			print("duel: %s shots at %s ms after the hit" % [kinds[kind], times])
			held.clear()
			times = PackedInt64Array()
			kind += 1
			if kind >= kinds.size():
				quit()
				return false
			_start()
	if frame > 2000:
		quit(2)
	return false
