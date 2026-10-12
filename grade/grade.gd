## Grades the world drawn under it by the time of day and the season (#209), through
## grade.gdshader over the whole screen, under the HUD: each key of grades.json is a grade at a
## share of the day, blended linearly between the keys round the time, then by its season's factors.
## The quality the settings choose sets what it adds: low shows the grade and the hue-shifted
## shadows only, standard and high the light vignette too. It decides nothing.
extends CanvasLayer

const GRADES := "res://grade/grades.json"
## The qualities, as the settings' screen effects row holds them.
enum Quality { LOW, STANDARD, HIGH }
## How far the darkest shadows lean blue-violet and the lights warm, and how much the corners
## darken, at standard and high.
const HUE_SHIFT := 1.0
const VIGNETTE := 0.18

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


## Shows the grade of `of_day`, the share of the day gone from midnight, 0 to 1, in `season`, 0 to
## 3 from spring.
func show_time(of_day: float, season: int) -> void:
	visible = true
	_grade = grade_at(of_day, season)
	_apply()


## The grade of `of_day` in `season`: `gain`, `lift`, `shadow` and `light` as Vector3s, and
## `saturation` and `contrast`.
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
	return grade


## What the pass draws with, as its shader's parameters: the grade's, `hue_shift`, `vignette` and
## `art_pixel`.
func params() -> Dictionary:
	var shown := {}
	for name: String in ["gain", "lift", "shadow", "light", "saturation", "contrast", "hue_shift", "vignette", "art_pixel"]:
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


static func _vector(values: Array) -> Vector3:
	return Vector3(values[0], values[1], values[2])
