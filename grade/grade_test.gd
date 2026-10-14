# Checks what grade.tscn gives its shader. By grades.json, the grade at noon in summer is noon's
# key with summer's factors, while at dusk it is dusk's (control: noon's differs), and between two
# keys it is the blend of them. At standard the pass draws the hue shift and the vignette, at low
# the hue shift and no vignette (control: standard's vignette). Until a time is shown the pass is
# hidden and draws nothing; once one is, it shows (control: the same pass after show_time()). The
# lights show by the key's lamp, fully at midnight and not at noon; each light given reaches the
# shader in screen pixels, and of more than MAX_LIGHTS only the nearest the screen's middle (control:
# under the cap every light is kept), at every quality.
extends RefCounted

const SCENE := "res://grade/grade.tscn"
const Grade := preload("res://grade/grade.gd")
const SUMMER := 1


func run() -> bool:
	var grade: Grade = load(SCENE).instantiate()
	var hidden_before := not grade.visible
	var noon := grade.grade_at(0.5, SUMMER)
	var dusk := grade.grade_at(0.76, SUMMER)
	var between := grade.grade_at(0.71, SUMMER)
	var halfway: Vector3 = noon.gain.lerp(dusk.gain, 0.5)
	var grades_pass: bool = noon.gain.is_equal_approx(Vector3(1.02, 1.0, 0.96)) \
			and is_equal_approx(noon.saturation, 1.06) and noon.lift == Vector3.ZERO \
			and dusk.gain.is_equal_approx(Vector3(1.05 * 1.02, 0.9, 0.8 * 0.96)) \
			and not dusk.gain.is_equal_approx(noon.gain) and between.gain.is_equal_approx(halfway)
	grade.show_time(0.5, SUMMER)
	var shown_after: bool = grade.visible
	grade.quality = Grade.Quality.STANDARD
	var standard := grade.params()
	grade.quality = Grade.Quality.LOW
	var low := grade.params()
	var quality_pass: bool = is_equal_approx(standard.vignette, Grade.VIGNETTE) and standard.vignette > 0.0 \
			and is_equal_approx(low.vignette, 0.0) and is_equal_approx(low.hue_shift, Grade.HUE_SHIFT) \
			and is_equal_approx(standard.hue_shift, Grade.HUE_SHIFT) \
			and Vector3(standard.gain).is_equal_approx(noon.gain)
	var winter := grade.grade_at(0.5, 3)
	var spring := grade.grade_at(0.5, 0)
	var midnight := grade.grade_at(0.0, SUMMER)
	var wrapped := grade.grade_at(1.0, SUMMER)
	var before_midnight := grade.grade_at(0.99999, SUMMER)
	var boundaries_pass: bool = winter.gain.is_equal_approx(Vector3(0.96, 0.99, 1.06)) \
			and spring.gain.is_equal_approx(Vector3(1.0, 1.02, 0.98)) \
			and not winter.gain.is_equal_approx(noon.gain) \
			and midnight.gain.is_equal_approx(wrapped.gain) \
			and before_midnight.gain.is_equal_approx(midnight.gain)
	grade.quality = Grade.Quality.HIGH
	var high := grade.params()
	boundaries_pass = boundaries_pass and is_equal_approx(high.vignette, standard.vignette)
	var other: Grade = load(SCENE).instantiate()
	other.show_time(0.76, SUMMER)
	var separate: bool = Vector3(grade.params().gain).is_equal_approx(noon.gain) \
			and Vector3(other.params().gain).is_equal_approx(dusk.gain)
	other.free()
	print("grade_test: spring %s, winter %s, midnight %s wrapped %s before %s; high vignette %.2f: %s" % [
		spring.gain, winter.gain, midnight.gain, wrapped.gain, before_midnight.gain, high.vignette,
		"pass" if boundaries_pass else "FAIL"])
	var passed: bool = grades_pass and quality_pass and boundaries_pass and separate and hidden_before and shown_after
	print("grade_test: noon in summer gain %s saturation %.2f, dusk gain %s, between them %s against %s; standard vignette %.2f hue shift %.2f, low vignette %.2f hue shift %.2f; hidden before a time %s, shown after %s: %s" % [
		noon.gain, noon.saturation, dusk.gain, between.gain, halfway, standard.vignette, standard.hue_shift,
		low.vignette, low.hue_shift, hidden_before, shown_after, "pass" if passed else "FAIL"])
	grade.free()
	return passed and _lights_pass()


func _lights_pass() -> bool:
	var grade: Grade = load(SCENE).instantiate()
	var lamps := [grade.grade_at(0.0, SUMMER).lamp, grade.grade_at(0.5, SUMMER).lamp, grade.grade_at(0.76, SUMMER).lamp]
	var lamp_pass: bool = is_equal_approx(lamps[0], 1.0) and is_equal_approx(lamps[1], 0.0) and is_equal_approx(lamps[2], 0.5)
	grade.art_pixel = 3.0
	grade.show_time(0.0, SUMMER)
	var view := Rect2(100, 50, 640, 360)
	grade.show_lights(PackedFloat32Array([110, 60, 7, 3, 28, 400, 200, 0, 0, 48]), view)
	var two := grade.params()
	var rects: PackedVector4Array = two.light_rects
	var reach: PackedFloat32Array = two.light_reach
	var shown_pass: bool = two.light_count == 2 and rects.size() == 2 \
			and rects[0].is_equal_approx(Vector4(30, 30, 21, 9)) and is_equal_approx(reach[0], 84.0) \
			and rects[1].is_equal_approx(Vector4(900, 450, 0, 0)) and is_equal_approx(reach[1], 144.0) \
			and is_equal_approx(two.lamp, 1.0)
	# Forty lights down a row through the middle, the first at it and each further out.
	var many := PackedFloat32Array()
	var middle := view.get_center()
	for light in Grade.MAX_LIGHTS + 8:
		many.append_array([middle.x + light * 10, middle.y, 0, 0, 20])
	grade.show_lights(many, view)
	var capped := grade.params()
	var kept: PackedVector4Array = capped.light_rects
	var farthest_kept := 0.0
	for rect in kept:
		farthest_kept = maxf(farthest_kept, rect.x)
	var cap_pass: bool = capped.light_count == Grade.MAX_LIGHTS and kept.size() == Grade.MAX_LIGHTS \
			and is_equal_approx(farthest_kept, (middle.x + (Grade.MAX_LIGHTS - 1) * 10 - view.position.x) * 3.0)
	grade.quality = Grade.Quality.LOW
	var low := grade.params()
	var quality_pass: bool = low.light_count == Grade.MAX_LIGHTS
	grade.free()
	var passed := lamp_pass and shown_pass and cap_pass and quality_pass
	print("grade_test: lamp at midnight, noon and dusk %s; two lights shown as %s reaching %s; %d of %d lights kept, the farthest at x %.0f; low keeping %d lights: %s" % [
		lamps, rects, reach, capped.light_count, many.size() / 5, farthest_kept, low.light_count,
		"pass" if passed else "FAIL"])
	return passed
