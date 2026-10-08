# Checks that the touch controls turn touches into what the play scene does, on a clock the test
# moves: a thumb in the joystick's zone dragged steers the way it is dragged, and let go stops,
# while a drag within the dead zone steers nowhere; a skill button tapped casts as a tap, while one
# moved past the slop is a drag that casts at the point it aims, as a share of the pad, and one
# dropped on the cancel zone casts nothing; one held for a long press casts on the body itself,
# while one held a little less is a tap; the square buttons, the arts button among them, and the
# pause button name themselves; a touch elsewhere is on the world, where a button past the body's
# skills stands, and a button whose slot is empty casts nothing. While placing, for the arts panel,
# a skill button pressed names its slot to place on, the pause button's place closes, a tap
# elsewhere is the panel's, and a finger moved up or down scrolls it. Every word the controls draw
# from hud.csv fits the room they give it, in each locale.
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
	var buttons_right: bool = named == ["genealogy", "sect", "factions", "arts", "pause"] \
			and ground == {"do": "world", "at": Vector2(240, 60)} and past.do == "world"
	print("touch_test: buttons %s; the ground %s; the fifth button with two skills %s: %s" % [
		named, ground, past, "pass" if buttons_right else "FAIL"])

	# An empty slot's button, tapped, casts nothing. Control: its neighbour holding a skill casts.
	var gapped := _touch(5)
	var slots: Array[Dictionary] = gapped.reach.duplicate()
	slots[3] = {}
	gapped.show_skills(slots)
	var fourth: Vector2 = gapped.SKILLS[3].at
	var empty_press: Dictionary = gapped.take(_touch_event(0, fourth, true))
	var empty_release: Dictionary = gapped.take(_touch_event(0, fourth, false))
	gapped.take(_touch_event(0, gapped.SKILLS[2].at, true))
	var neighbour: Dictionary = gapped.take(_touch_event(0, gapped.SKILLS[2].at, false))
	var empty_right: bool = gapped.skill_count == 5 and empty_press == {"do": "none"} \
			and empty_release == {"do": "none"} and neighbour == {"do": "cast", "slot": 2, "how": "tap"}
	print("touch_test: an empty fourth slot pressed %s, released %s; the third, holding a skill, %s: %s" % [
		empty_press, empty_release, neighbour, "pass" if empty_right else "FAIL"])

	# Placing: the fourth button names its slot, even empty; the pause button's place closes; a tap
	# on the panel is the panel's; a finger moved up scrolls on, down scrolls back; the finger on the
	# arts button that opened the panel, lifted once placing, asks nothing. Control: the same press
	# on the main button while not placing is a skill's, casting on release.
	gapped.take(_touch_event(5, gapped.BUTTONS.arts.get_center(), true))
	gapped.placing = true
	var opener: Dictionary = gapped.take(_touch_event(5, gapped.BUTTONS.arts.get_center(), false))
	var place: Dictionary = gapped.take(_touch_event(1, fourth, true))
	var close: Dictionary = gapped.take(_touch_event(1, gapped.PAUSE.get_center(), true))
	gapped.take(_touch_event(1, Vector2(60, 60), true))
	var on_panel: Dictionary = gapped.take(_touch_event(1, Vector2(60, 62), false))
	gapped.take(_touch_event(1, Vector2(60, 200), true))
	gapped.take(_drag(1, Vector2(60, 200 - gapped.SCROLL - 4)))
	var up: Dictionary = gapped.take(_touch_event(1, Vector2(60, 200 - gapped.SCROLL - 4), false))
	gapped.take(_touch_event(1, Vector2(60, 100), true))
	gapped.take(_drag(1, Vector2(60, 100 + gapped.SCROLL + 4)))
	var down: Dictionary = gapped.take(_touch_event(1, Vector2(60, 100 + gapped.SCROLL + 4), false))
	gapped.placing = false
	gapped.take(_touch_event(1, main, true))
	var not_placing: Dictionary = gapped.take(_touch_event(1, main, false))
	var placing_right: bool = opener == {"do": "none"} and place == {"do": "place", "slot": 3} and close == {"do": "button", "button": "close"} \
			and on_panel == {"do": "panel", "at": Vector2(60, 60)} and up == {"do": "scroll", "way": 1} \
			and down == {"do": "scroll", "way": -1} and not_placing == {"do": "cast", "slot": 0, "how": "tap"}
	print("touch_test: placing, the arts button's finger lifted %s, the fourth button %s, the pause button's place %s, the panel %s, moved up %s, down %s; not placing, the main button %s: %s" % [
		opener, place, close, on_panel, up, down, not_placing, "pass" if placing_right else "FAIL"])
	touch.free()
	two.free()
	gapped.free()
	return stick_right and casts_right and press_right and buttons_right and empty_right and placing_right \
			and _fit_pass()


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
