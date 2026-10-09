## Shows the core's M1 benchmark scenario in the approved art, through world_art.tscn, and sounds
## it through sound.tscn, with a camera the arrow keys move, at the PC's scale ViewScale gives the
## window. It ticks the world at the core's tick rate whatever the frame rate, draws the bodies
## between their last two ticks' positions on every frame, names the people round the lead of the
## army nearest the battle's centre as the play scene names those round the player, at the HUD's
## scale, and holds no game rule.
extends Node2D

const ViewScale := preload("res://view_scale.gd")
const Hud := preload("res://hud/hud.gd")
const Nameplates := preload("res://hud/nameplates.gd")
const CAMERA_PIXELS_PER_SECOND := 200.0
# Ticks one frame may run to catch up; beyond it the simulation falls behind instead of stalling.
const MAX_TICKS_PER_FRAME := 4

var world: WorldAdapter
var ticks := 0
## Bodies the world holds after the last frame, on screen or not.
var bodies := 0
## Bodies drawn in the last frame, the ones on screen.
var bodies_drawn := 0
## People within the names' reach in the last frame, and the names shown over them.
var named_near := 0
var names_shown := 0

var _since_tick := 0.0
var _names := Nameplates.new()
var _hud_scale := 1

@onready var _art: Node2D = $WorldArt
@onready var _camera: Camera2D = $Camera
@onready var _sound: Node2D = $Sound


func _ready() -> void:
	var scale := ViewScale.world_scale(Vector2i(get_viewport_rect().size), false)
	_camera.zoom = Vector2(scale, scale)
	world = WorldAdapter.create_m1()
	_art.show_world(world, -1)
	_camera.position = _art.to_screen(Vector2(WorldAdapter.m1_centre()))
	_art.name_around(_nearest_army(Vector2(WorldAdapter.m1_centre())))
	_sound.show_world(world, _art.to_screen)
	var window := Vector2i(get_viewport_rect().size)
	_hud_scale = ViewScale.hud_scale(window, false)
	var layer := CanvasLayer.new()
	layer.scale = Vector2(_hud_scale, _hud_scale)
	add_child(layer)
	_names.use_font(load(Hud.FONT))
	_names.size = Vector2(window / _hud_scale)
	layer.add_child(_names)


func _process(delta: float) -> void:
	_camera.position += Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down") \
			* CAMERA_PIXELS_PER_SECOND * delta / _camera.zoom.x
	var step := 1.0 / WorldAdapter.TICKS_PER_SECOND
	_since_tick = minf(_since_tick + delta, step * MAX_TICKS_PER_FRAME)
	while _since_tick >= step:
		world.tick()
		_art.note_tick()
		_sound.note_tick()
		ticks += 1
		_since_tick -= step
	var screen := get_viewport_rect().size / _camera.zoom
	var view := Rect2(_camera.position - screen / 2.0, screen)
	_art.refresh(view, _since_tick / step)
	_sound.hearing = view
	bodies_drawn = _art.drawn
	var people: Array[Dictionary] = []
	for person: Dictionary in _art.near:
		people.append({"person": person.person, "relation": person.relation,
			"head": ((person.head - view.position) * _camera.zoom / _hud_scale).floor()})
	_names.show_names(world, people, [])
	named_near = people.size()
	names_shown = _names.plates.size()
	bodies = 0
	for army in world.army_count():
		bodies += world.body_positions(army).size()


# The army whose lead stands nearest `at`, in sub-cell units.
func _nearest_army(at: Vector2) -> int:
	var nearest := -1
	var least := INF
	for army in world.army_count():
		var roster := world.roster(army)
		if roster.is_empty():
			continue
		var apart := world.person_position(roster[0]).distance_to(at)
		if apart < least:
			least = apart
			nearest = army
	return nearest
