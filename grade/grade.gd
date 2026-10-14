## Grades the world drawn under it by the time of day and the season (#209), through
## grade.gdshader over the whole screen, under the HUD: each key of grades.json is a grade at a
## share of the day, blended linearly between the keys round the time, then by its season's factors.
## Lights the world shows, lit windows and the steered person's torch, warm the night round them by
## each key's `lamp`, and the fire ramp's brightest steps, emitters, keep their colour and bloom.
## The quality the settings choose sets what it adds: low shows the grade, the hue-shifted shadows
## and the lights only, standard and high the light vignette and the bloom too. It decides nothing.
extends CanvasLayer

const WorldArt := preload("res://world_art/world_art.gd")

const GRADES := "res://grade/grades.json"
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


## Shows the grade of `of_day`, the share of the day gone from midnight, 0 to 1, in `season`, 0 to
## 3 from spring.
func show_time(of_day: float, season: int) -> void:
	visible = true
	_grade = grade_at(of_day, season)
	_apply()


## Shows `lights`, as WorldArt.lights() gives them for `view`, the part of the world on screen in
## its pixels: each one's rect and reach in screen pixels, at most MAX_LIGHTS of them.
func show_lights(lights: PackedFloat32Array, view: Rect2) -> void:
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
## `saturation`, `contrast` and `lamp`, how much the lights show, 0 to 1.
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
	grade.lamp = lerpf(a.lamp, b.lamp, share)
	return grade


## What the pass draws with, as its shader's parameters: the grade's, `hue_shift`, `vignette`,
## `art_pixel`, `bloom`, and the lights shown, `light_count` of `light_rects` and `light_reach`.
func params() -> Dictionary:
	var shown := {}
	for name: String in ["gain", "lift", "shadow", "light", "saturation", "contrast", "lamp", "hue_shift",
			"vignette", "art_pixel", "bloom", "light_count", "light_rects", "light_reach"]:
		shown[name] = ($Pass.material as ShaderMaterial).get_shader_parameter(name)
	return shown


func _apply() -> void:
	if _grade.is_empty():
		return
	var material := $Pass.material as ShaderMaterial
	for field: String in _grade:
		material.set_shader_parameter(field, _grade[field])
	material.set_shader_parameter("hue_shift", HUE_SHIFT)
	material.set_shader_parameter("vignette", 0.0 if quality == Quality.LOW else VIGNETTE)
	material.set_shader_parameter("art_pixel", art_pixel)
	material.set_shader_parameter("bloom", 0.0 if quality == Quality.LOW else BLOOM)
	material.set_shader_parameter("light_count", _light_rects.size())
	material.set_shader_parameter("light_rects", _light_rects)
	material.set_shader_parameter("light_reach", _light_reach)


static func _vector(values: Array) -> Vector3:
	return Vector3(values[0], values[1], values[2])
