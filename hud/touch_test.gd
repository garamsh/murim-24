# Checks that the touch controls turn touches into what the play scene does, on a clock the test
# moves: a thumb in the joystick's zone dragged steers the way it is dragged, and let go stops,
# while a drag within the dead zone steers nowhere; a skill button tapped casts as a tap, while one
# moved past the slop is a drag that casts at the point it aims, as a share of the pad, and one
# dropped on the cancel zone casts nothing; one held for a long press casts on the body itself,
# while one held a little less is a tap; the square buttons and the pause button name themselves;
# a touch elsewhere is on the world, where a button past the body's skills stands. Every word the
# controls draw from hud.csv fits the room they give it, in each locale.
extends RefCounted

const Touch := preload("res://hud/touch.gd")
const Hud := preload("res://hud/hud.gd")
const WordFit := preload("res://hud/word_fit.gd")


func run() -> bool:
	var touch := _touch(5)
	var now := [0]
	touch.clock = func() -> int: return now[0]

	# The joystick: pushed right of where the thumb lands, then let go. Control: within the dead zone.
	var landed := Vector2(60, 200)
	var pressed_stick: Dictionary = touch.take(_touch_event(0, landed, true))
	var pushed: Dictionary = touch.take(_drag(0, landed + Vector2(40, 0)))
	var let_go: Dictionary = touch.take(_touch_event(0, landed + Vector2(40, 0), false))
	touch.take(_touch_event(0, landed, true))
	var nudged: Dictionary = touch.take(_drag(0, landed + Vector2(touch.STICK_DEAD - 1, 0)))
	touch.take(_touch_event(0, landed, false))
	var stick_right: bool = pressed_stick.do == "none" and pushed.do == "steer" \
			and pushed.way.is_equal_approx(Vector2(1, 0)) and let_go.do == "steer" and let_go.way == Vector2.ZERO \
			and nudged.do == "steer" and nudged.way == Vector2.ZERO
	print("touch_test: stick pressed %s, pushed 40 px right %s, let go %s; nudged within its dead zone %s: %s" % [
		pressed_stick, pushed, let_go, nudged, "pass" if stick_right else "FAIL"])

	# A tap; a drag past the slop to the pad's edge and beyond; the same drag onto the cancel zone.
	# Control: a touch moved less than the slop is still a tap.
	var main: Vector2 = touch.SKILLS[0].at
	touch.take(_touch_event(1, main, true))
	var tapped: Dictionary = touch.take(_touch_event(1, main, false))
	touch.take(_touch_event(1, main, true))
	touch.take(_drag(1, main + Vector2(touch.SLOP - 2, 0)))
	var still_tap: Dictionary = touch.take(_touch_event(1, main + Vector2(touch.SLOP - 2, 0), false))
	var second: Vector2 = touch.SKILLS[1].at
	touch.take(_touch_event(2, second, true))
	touch.take(_drag(2, second + Vector2(0, -touch.PAD * 2)))
	var aiming: Dictionary = touch.aiming()
	var dragged: Dictionary = touch.take(_touch_event(2, second + Vector2(0, -touch.PAD * 2), false))
	touch.take(_touch_event(2, second, true))
	touch.take(_drag(2, second + Vector2(0, -touch.PAD)))
	touch.take(_drag(2, touch.CANCEL.at))
	var over_cancel: bool = touch.aiming().get("cancel", false)
	var cancelled: Dictionary = touch.take(_touch_event(2, touch.CANCEL.at, false))
	var casts_right: bool = tapped == {"do": "cast", "slot": 0, "how": "tap"} \
			and still_tap == {"do": "cast", "slot": 0, "how": "tap"} and aiming.get("slot", -1) == 1 \
			and dragged.do == "cast" and dragged.how == "point" and dragged.aim.is_equal_approx(Vector2(0, -1)) \
			and over_cancel and cancelled == {"do": "none"} and touch.aiming().is_empty()
	print("touch_test: tapped %s; moved under the slop %s; dragged past the pad %s; onto the cancel zone %s, released %s: %s" % [
		tapped, still_tap, dragged, over_cancel, cancelled, "pass" if casts_right else "FAIL"])

	# A long press, and a press a little shorter.
	var fifth: Vector2 = touch.SKILLS[4].at
	touch.take(_touch_event(3, fifth, true))
	now[0] += touch.LONG_PRESS_MS
	var held_self: bool = touch.holding_self()
	var held: Dictionary = touch.take(_touch_event(3, fifth, false))
	touch.take(_touch_event(3, fifth, true))
	now[0] += touch.LONG_PRESS_MS - 100
	var short_self: bool = touch.holding_self()
	var short: Dictionary = touch.take(_touch_event(3, fifth, false))
	var press_right: bool = held_self and held == {"do": "cast", "slot": 4, "how": "self"} and not short_self \
			and short == {"do": "cast", "slot": 4, "how": "tap"}
	print("touch_test: held %d ms %s; held %d ms %s: %s" % [touch.LONG_PRESS_MS, held,
		touch.LONG_PRESS_MS - 100, short, "pass" if press_right else "FAIL"])

	# The square buttons, the pause button and the world; control: with two skills, the fifth
	# button's place is world.
	var named: Array[String] = []
	for button: String in touch.BUTTONS:
		named.append(touch.take(_touch_event(4, touch.BUTTONS[button].get_center(), true)).get("button", ""))
		touch.take(_touch_event(4, Vector2.ZERO, false))
	named.append(touch.take(_touch_event(4, touch.PAUSE.get_center(), true)).get("button", ""))
	touch.take(_touch_event(4, Vector2.ZERO, false))
	var ground: Dictionary = touch.take(_touch_event(4, Vector2(240, 60), true))
	touch.take(_touch_event(4, Vector2.ZERO, false))
	var two := _touch(2)
	var past: Dictionary = two.take(_touch_event(0, two.SKILLS[4].at, true))
	var buttons_right: bool = named == ["genealogy", "sect", "challenge", "pause"] \
			and ground == {"do": "world", "at": Vector2(240, 60)} and past.do == "world"
	print("touch_test: buttons %s; the ground %s; the fifth button with two skills %s: %s" % [
		named, ground, past, "pass" if buttons_right else "FAIL"])
	touch.free()
	two.free()
	return stick_right and casts_right and press_right and buttons_right and _fit_pass()


# In each locale, every word the controls draw fits the room they give it. Setup: every TOUCH_ key
# is measured.
func _fit_pass() -> bool:
	var fit := WordFit.new()
	for locale: String in Hud.LOCALES:
		TranslationServer.set_locale(locale)
		for key: String in Touch.ROOM:
			var word := fit.say(key)
			fit.check(word, fit.width(word), Touch.ROOM[key])
	TranslationServer.set_locale(Hud.LOCALES[0])
	var unmeasured := fit.unmeasured(["TOUCH_"])
	var passed := unmeasured.is_empty() and fit.untranslated.is_empty() and fit.overruns.is_empty()
	print("touch_test: %d keys measured in %d checks across %s; unmeasured %s; untranslated %s; overruns %s: %s" % [
		fit.measured.size(), fit.checks, Hud.LOCALES, unmeasured, fit.untranslated, fit.overruns,
		"pass" if passed else "FAIL"])
	return passed


# Touch controls showing `count` skills.
func _touch(count: int) -> Control:
	var touch: Control = Touch.new()
	var skills: Array[Dictionary] = []
	for n in count:
		skills.append({"icon": "strike", "range": 48.0, "radius": 0.0})
	touch.show_skills(skills)
	return touch


func _touch_event(index: int, at: Vector2, pressed: bool) -> InputEventScreenTouch:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = pressed
	return event


func _drag(index: int, at: Vector2) -> InputEventScreenDrag:
	var event := InputEventScreenDrag.new()
	event.index = index
	event.position = at
	return event
