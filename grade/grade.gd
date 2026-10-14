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
## shown passes. The quality the settings choose sets what it adds: low shows the grade, the
## hue-shifted shadows and the lights only, standard and high the light vignette, the bloom, the fog
## and the clouds' shadows too. It decides nothing.
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
var _cells: ImageTexture
var _map_size := Vector2i.ZERO
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
	_cells = ImageTexture.create_from_image(relief.cell_image())
	_map_size = relief.size
	_apply()


## Shows `lights`, as WorldArt.lights() gives them for `view`, the part of the world on screen in
## its pixels: each one's rect and reach in screen pixels, at most MAX_LIGHTS of them; and how many
## `hearths` are among them, which the bloom waits for. The fog drifts over the world from `view`'s
## corner.
func show_lights(lights: PackedFloat32Array, view: Rect2, hearths := 0) -> void:
	_hearths = hearths
	_view_origin = view.position
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
	_apply()


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


## What the pass draws with, as its shader's parameters: the grade's but `clouds`, `hue_shift`,
## `vignette`, `art_pixel`, `bloom`, `light_count` and `fog`; what the light field draws with, the
## lights shown, `light_rects` and `light_reach`, the fog's and the clouds' `view_origin`, `drift`,
## `cells` and `map_size`, and whether it draws, `field_drawn`; and `clouds`, how dark the clouds'
## material draws their shadows, and `clouds_read_field`, whether it reads them from the light field.
func params() -> Dictionary:
	var shown := {}
	for name: String in ["gain", "lift", "shadow", "light", "saturation", "contrast", "lamp", "hue_shift",
			"vignette", "art_pixel", "bloom", "light_count", "fog"]:
		shown[name] = ($Pass.material as ShaderMaterial).get_shader_parameter(name)
	for name: String in ["light_rects", "light_reach", "view_origin", "drift", "cells", "map_size"]:
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
	for field: String in _grade:
		if not ["fog", "clouds"].has(field):
			material.set_shader_parameter(field, _grade[field])
	var fog: float = 0.0 if low else _grade.fog
	var shade: float = 0.0 if low else _grade.clouds
	material.set_shader_parameter("fog", fog)
	clouds.set_shader_parameter("clouds", shade)
	clouds.set_shader_parameter("field", $LightField.get_texture())
	clouds.set_shader_parameter("art_pixel", art_pixel)
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
	field.set_shader_parameter("drift", _drift)
	field.set_shader_parameter("cells", _cells)
	field.set_shader_parameter("map_size", _map_size)
	var drawn: bool = (_light_rects.size() > 0 and _grade.lamp > 0.0) or fog > 0.0 or shade > 0.0
	$LightField.render_target_update_mode = SubViewport.UPDATE_ALWAYS if drawn else SubViewport.UPDATE_DISABLED
	if is_inside_tree():
		# One pixel an art pixel, the screen's last part pixel included.
		var cells := Vector2i((get_viewport().get_visible_rect().size / art_pixel).ceil())
		$LightField.size = cells
		$LightField/Field.size = Vector2(cells)


static func _vector(values: Array) -> Vector3:
	return Vector3(values[0], values[1], values[2])
