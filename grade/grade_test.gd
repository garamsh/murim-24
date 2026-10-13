# Checks what grade.tscn gives its shader. By grades.json, the grade at noon in summer is noon's
# key with summer's factors, while at dusk it is dusk's (control: noon's differs), and between two
# keys it is the blend of them. At standard the pass draws the hue shift and the vignette, at low
# the hue shift and no vignette (control: standard's vignette). Until a time is shown the pass is
# hidden and draws nothing; once one is, it shows (control: the same pass after show_time()).
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
	return passed
