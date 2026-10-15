## Grades the world drawn under it by the time of day and the season (#209), through
## grade.gdshader over the whole screen, under the HUD: each key of grades.json is a grade at a
## share of the day, blended linearly between the keys round the time, then by its season's factors.
## Lights the world shows, lit windows and the steered person's torch, warm the night round them by
## each key's `lamp`, and the fire ramp's brightest steps, emitters, keep their colour; they bloom
## while the lamp shows and a hearth, whose fire is drawn in them, is on screen, and the pass skips
## the bloom otherwise. The lights are drawn once a frame into the light field, a viewport of one
## pixel an art pixel, which the pass reads; the field is left undrawn while no light shows. By each
## key's `fog`, a mist thickens toward the screen's top, clear of high ground; and by its `clouds`,
## the shadows of clouds drift over the ground, drawn by `clouds`, the material the caller gives
## world_art's ground, so nothing standing on it is shaded. Both drift with the wind as the time
## shown passes. While it rains, until the world holds weather a switch the settings set, rain
## streaks fall over the world, the grade dims and greys by grades.json's `rain`, and the overcast
## casts no clouds' shadows. The quality the settings choose sets what it adds: low shows the grade,
## the hue-shifted shadows and the lights only, standard and high the light vignette, the bloom, the
## fog, the clouds' shadows and the rain too. It decides nothing.
extends CanvasLayer

const WorldArt := preload("res://world_art/world_art.gd")

const GRADES := "res://grade/grades.json"
const CLOUDS := preload("res://grade/clouds.gdshader")
## The qualities, as the settings' screen effects row holds them.
enum Quality { LOW, STANDARD, HIGH }
## How far the darkest shadows lean blue-violet and the lights warm, and how much the corners
## darken, at standard and high.
const HUE_SHIFT := 1.0
const VIGNETTE := 0.18
## How strongly emitters bloom, at standard and high.
const BLOOM := 0.6
## The most lights drawn at once, those nearest the screen's middle.
const MAX_LIGHTS := 32

## The material the ground is drawn with, for the clouds' shadows to fall on it.
var clouds := ShaderMaterial.new()
## The quality shown, of Quality.
var quality := Quality.STANDARD:
	set(value):
		quality = value
		_apply()
## Whether it rains, until the world holds weather; low draws no rain whatever it says.
var raining := false:
	set(value):
		raining = value
		_apply()
## The screen pixels an art pixel spans, the view's whole-pixel scale.
var art_pixel := 1.0:
	set(value):
		art_pixel = value
		_apply()

var _table := {}
var _grade := {}
var _light_rects := PackedVector4Array()
var _light_reach := PackedFloat32Array()
var _hearths := 0
var _view_origin := Vector2.ZERO
var _grid_offset := Vector2.ZERO
# The relief the fog reads, the view last shown, and the cells of the relief in it and the rect of
# them, the texture built anew only when that rect moves (#336).
var _relief: Relief
var _view := Rect2()
var _cells: ImageTexture
var _cells_window := Rect2i()
var _drift := 0.0


func _init() -> void:
	clouds.shader = CLOUDS


## Shows the grade of `of_day`, the share of the day gone from midnight, 0 to 1, in `season`, 0 to
## 3 from spring.
func show_time(of_day: float, season: int) -> void:
	visible = true
	_grade = grade_at(of_day, season)
	_drift = Time.get_ticks_msec() / 1000.0
	_apply()


## Shows the fog over `relief`, the world's, so its high ground rises above it.
func show_relief(relief: Relief) -> void:
	_relief = relief
	_cells = null
	_show_cells(_view)
	_apply()


## Shows `lights`, as WorldArt.lights() gives them for `view`, the part of the world on screen in
## its pixels: each one's rect and reach in screen pixels, at most MAX_LIGHTS of them; and how many
## `hearths` are among them, which the bloom waits for. The fog drifts over the world from `view`'s
## corner, which may lie between world pixels as the camera moves a screen pixel at a time: the art
## pixels the pass steps in begin where the world's whole pixels fall on the screen. The fog reads
## the relief's cells in view, and as far below as raised ground lifts.
func show_lights(lights: PackedFloat32Array, view: Rect2, hearths := 0) -> void:
	_hearths = hearths
	_view_origin = view.position.floor()
	_grid_offset = (_view_origin - view.position) * art_pixel
	var count := lights.size() / WorldArt.LIGHT_FLOATS
	var order := range(count)
	if count > MAX_LIGHTS:
		var middle := view.get_center()
		var apart := func(light: int) -> float:
			var at := light * WorldArt.LIGHT_FLOATS
			return Rect2(lights[at], lights[at + 1], lights[at + 2], lights[at + 3]).get_center().distance_squared_to(middle)
		order.sort_custom(func(a: int, b: int) -> bool: return apart.call(a) < apart.call(b))
		order.resize(MAX_LIGHTS)
	# New arrays, as the material keeps the ones it was last given.
	_light_rects = PackedVector4Array()
	_light_reach = PackedFloat32Array()
	for light: int in order:
		var at := light * WorldArt.LIGHT_FLOATS
		var corner := (Vector2(lights[at], lights[at + 1]) - view.position) * art_pixel
		_light_rects.append(Vector4(corner.x, corner.y, lights[at + 2] * art_pixel, lights[at + 3] * art_pixel))
		_light_reach.append(lights[at + 4] * art_pixel)
	_view = view
	if _relief != null:
		_show_cells(view)
	_apply()


# Gives the fog the relief's cells within `view` and a cell round it, and as many rows below as the
# highest level lifts ground into view, built anew only when that rect of cells moves.
func _show_cells(view: Rect2) -> void:
	var first := Vector2i((view.position / Relief.CELL).floor()) - Vector2i.ONE
	var last := Vector2i(((view.end + Vector2(0, Relief.TOP * Relief.LEVEL_PX)) / Relief.CELL).floor()) + Vector2i.ONE
	var window := Rect2i(first, last - first + Vector2i.ONE).intersection(Rect2i(Vector2i.ZERO, _relief.size))
	if window == _cells_window and _cells != null:
		return
	_cells_window = window
	_cells = ImageTexture.create_from_image(_relief.cell_image_of(window)) if window.has_area() else null


## The grade of `of_day` in `season`: `gain`, `lift`, `shadow` and `light` as Vector3s, and
## `saturation`, `contrast`, and `lamp`, `fog` and `clouds`, how much the lights, the fog and the
## clouds' shadows show, 0 to 1.
func grade_at(of_day: float, season: int) -> Dictionary:
	if _table.is_empty():
		_table = JSON.parse_string(FileAccess.get_file_as_string(GRADES))
	var keys: Array = _table.keys
	var t := fposmod(of_day, 1.0)
	var k := 0
	while k < keys.size() - 2 and float(keys[k + 1].at) <= t:
		k += 1
	var a: Dictionary = keys[k]
	var b: Dictionary = keys[k + 1]
	var span := maxf(float(b.at) - float(a.at), 0.0001)
	var share := clampf((t - float(a.at)) / span, 0.0, 1.0)
	var of_season: Dictionary = _table.seasons[posmod(season, _table.seasons.size())]
	var grade := {}
	for field: String in ["gain", "lift", "shadow", "light"]:
		grade[field] = _vector(a[field]).lerp(_vector(b[field]), share)
	grade.gain *= _vector(of_season.gain)
	grade.saturation = lerpf(a.saturation, b.saturation, share) * float(of_season.saturation)
	grade.contrast = lerpf(a.contrast, b.contrast, share)
	for field: String in ["lamp", "fog", "clouds"]:
		grade[field] = lerpf(a[field], b[field], share)
	return grade


## What the pass draws with, as its shader's parameters: the grade's but `clouds`, `gain` and
## `saturation` dimmed while it rains, `hue_shift`, `vignette`, `art_pixel`, `grid_offset`, where
## the world's art pixels begin on the screen, `bloom`, `light_count`,
## `rain`, 1 while it rains, its `drift`, and `fog`; what the light field draws with, the lights
## shown, `light_rects` and `light_reach`, the fog's and the clouds' `view_origin`, `drift`, the
## relief's `cells` in view, `cells_size` of them from cell `cells_origin`, and whether it draws,
## `field_drawn`; and `clouds`, how dark the clouds' material draws their shadows, and
## `clouds_read_field`, whether it reads them from the light field.
func params() -> Dictionary:
	var shown := {}
	for name: String in ["gain", "lift", "shadow", "light", "saturation", "contrast", "lamp", "hue_shift",
			"vignette", "art_pixel", "grid_offset", "bloom", "light_count", "rain", "fog"]:
		shown[name] = ($Pass.material as ShaderMaterial).get_shader_parameter(name)
	for name: String in ["light_rects", "light_reach", "view_origin", "drift", "cells", "cells_origin", "cells_size"]:
		shown[name] = ($LightField/Field.material as ShaderMaterial).get_shader_parameter(name)
	shown.field_drawn = $LightField.render_target_update_mode == SubViewport.UPDATE_ALWAYS
	shown.clouds = clouds.get_shader_parameter("clouds")
	shown.clouds_read_field = clouds.get_shader_parameter("field") == $LightField.get_texture()
	return shown


func _apply() -> void:
	if _grade.is_empty():
		return
	var material := $Pass.material as ShaderMaterial
	var low := quality == Quality.LOW
	var rains := raining and not low
	for field: String in _grade:
		if not ["fog", "clouds"].has(field):
			material.set_shader_parameter(field, _grade[field])
	if rains:
		material.set_shader_parameter("gain", _grade.gain * _vector(_table.rain.gain))
		material.set_shader_parameter("saturation", _grade.saturation * float(_table.rain.saturation))
	material.set_shader_parameter("rain", 1.0 if rains else 0.0)
	material.set_shader_parameter("drift", _drift)
	var fog: float = 0.0 if low else _grade.fog
	var shade: float = 0.0 if low or rains else _grade.clouds
	material.set_shader_parameter("fog", fog)
	clouds.set_shader_parameter("clouds", shade)
	clouds.set_shader_parameter("field", $LightField.get_texture())
	clouds.set_shader_parameter("art_pixel", art_pixel)
	clouds.set_shader_parameter("grid_offset", _grid_offset)
	material.set_shader_parameter("grid_offset", _grid_offset)
	material.set_shader_parameter("hue_shift", HUE_SHIFT)
	material.set_shader_parameter("vignette", 0.0 if low else VIGNETTE)
	material.set_shader_parameter("art_pixel", art_pixel)
	var blooming: bool = not low and _hearths > 0 and _grade.lamp > 0.0
	material.set_shader_parameter("bloom", BLOOM if blooming else 0.0)
	material.set_shader_parameter("light_count", _light_rects.size())
	material.set_shader_parameter("light_field", $LightField.get_texture())
	var field := $LightField/Field.material as ShaderMaterial
	field.set_shader_parameter("art_pixel", art_pixel)
	field.set_shader_parameter("light_count", _light_rects.size())
	field.set_shader_parameter("light_rects", _light_rects)
	field.set_shader_parameter("light_reach", _light_reach)
	field.set_shader_parameter("fog", fog)
	field.set_shader_parameter("clouds", shade)
	field.set_shader_parameter("view_origin", _view_origin)
	field.set_shader_parameter("grid_offset", _grid_offset)
	field.set_shader_parameter("drift", _drift)
	field.set_shader_parameter("cells", _cells)
	field.set_shader_parameter("cells_origin", _cells_window.position)
	field.set_shader_parameter("cells_size", _cells_window.size if _cells != null else Vector2i.ZERO)
	var drawn: bool = (_light_rects.size() > 0 and _grade.lamp > 0.0) or fog > 0.0 or shade > 0.0
	$LightField.render_target_update_mode = SubViewport.UPDATE_ALWAYS if drawn else SubViewport.UPDATE_DISABLED
	if is_inside_tree():
		# One pixel an art pixel, the part pixels at both of the screen's edges included.
		var cells := Vector2i((get_viewport().get_visible_rect().size / art_pixel).ceil()) + Vector2i.ONE
		$LightField.size = cells
		$LightField/Field.size = Vector2(cells)


static func _vector(values: Array) -> Vector3:
	return Vector3(values[0], values[1], values[2])
