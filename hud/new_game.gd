## The new-game panel over the title screen, drawn as art/mockup/new_game_screen.png lays it out:
## the seed typed into its field or drawn at random, a line saying the same seed opens the same
## world, and the button that starts it. A tap on the field brings up the screen's keyboard for
## digits, one on the random slip draws a seed, and one on the start button starts. Every word comes
## from hud.csv through Godot's translations. It holds no game rule.
extends "res://hud/panel.gd"

# The mockup's layout, in the native 480x270.
const DIALOG := Rect2(100, 66, 280, 138)
const FIELD := Rect2(144, 98, 138, 16)
const RANDOM_X := 290
const START_WIDTH := 96
# As many digits as a seed drawn at random has, so the pause panel shows any seed whole.
const MAX_DIGITS := 10

## The digits typed so far; empty draws the seed at random when the world starts.
var typed := ""


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED:
		queue_redraw()


## Opens the panel with its field empty.
func open() -> void:
	typed = ""
	visible = true
	queue_redraw()


## Works the panel with `key`: a digit is typed into the field, Backspace takes the last back, R
## draws a seed at random into it, Enter starts the world and Escape goes back. Returns "start"
## or "back" when the key does either, and "" otherwise.
func key(event: InputEventKey) -> String:
	var digit := _digit(event.keycode)
	# The screen's keyboard may send a digit as its character alone.
	if digit < 0 and event.unicode >= 0x30 and event.unicode <= 0x39:
		digit = event.unicode - 0x30
	if digit >= 0 and typed.length() < MAX_DIGITS:
		typed += str(digit)
	elif event.keycode == KEY_BACKSPACE:
		typed = typed.left(-1)
	elif event.keycode == KEY_R:
		typed = str(randi())
	elif ENTER_KEYS.has(event.keycode):
		DisplayServer.virtual_keyboard_hide()
		return "start"
	elif event.keycode == KEY_ESCAPE:
		close()
		return "back"
	queue_redraw()
	return ""


## Works the panel with a tap at `at`: on the field it brings up the screen's keyboard for
## digits, on the random slip it draws a seed into the field, on the start button it starts the
## world, and on the close word, or anywhere outside the panel, it goes back. Returns "start" or
## "back" when the tap does either, and "" otherwise.
func tap(at: Vector2) -> String:
	if not DIALOG.has_point(at) or _close_hit(DIALOG, tr("NEW_GAME_BACK"), at):
		DisplayServer.virtual_keyboard_hide()
		close()
		return "back"
	if FIELD.has_point(at):
		if DisplayServer.has_feature(DisplayServer.FEATURE_VIRTUAL_KEYBOARD):
			DisplayServer.virtual_keyboard_show(typed, FIELD, DisplayServer.KEYBOARD_TYPE_NUMBER, MAX_DIGITS)
	elif _random_slip().has_point(at):
		typed = str(randi())
	elif _start_slip().has_point(at):
		DisplayServer.virtual_keyboard_hide()
		return "start"
	queue_redraw()
	return ""


## Returns the seed the world starts from: the one typed, or one drawn at random when the field
## is empty.
func world_seed() -> int:
	return int(typed) if not typed.is_empty() else randi()


func _digit(keycode: Key) -> int:
	if keycode >= KEY_0 and keycode <= KEY_9:
		return keycode - KEY_0
	if keycode >= KEY_KP_0 and keycode <= KEY_KP_9:
		return keycode - KEY_KP_0
	return -1


func _draw() -> void:
	_dim()
	_dialog(DIALOG, tr("NEW_GAME_TITLE"), tr("NEW_GAME_BACK"), tr("NEW_GAME_KEYS"))
	var y := FIELD.position.y
	_text(Vector2(DIALOG.position.x + 12, y + 2), tr("NEW_GAME_SEED"), Palette.GOLD_4)
	draw_rect(FIELD, Palette.JADE_4)
	draw_rect(FIELD.grow(-1), Palette.INK_1)
	_text(FIELD.position + Vector2(5, 2), typed, Palette.PAPER_6)
	# The caret, after what was typed.
	draw_rect(Rect2(FIELD.position.x + 6 + _width(typed), y + 3, 1, 10), Palette.PAPER_6)
	var random := _random_slip()
	_slip(random.position, random.size.x, tr("NEW_GAME_RANDOM"), false)
	_text(Vector2(DIALOG.position.x + 12, y + 26), tr("NEW_GAME_SAME"), Palette.STONE_4)
	_text(Vector2(DIALOG.position.x + 12, y + 40), tr("NEW_GAME_EMPTY"), Palette.STONE_3)
	draw_rect(Rect2(DIALOG.position.x + 6, y + 58, DIALOG.size.x - 12, 1), Palette.INK_4)
	var start := _start_slip()
	_slip(start.position, start.size.x, tr("NEW_GAME_START"), true)


func _random_slip() -> Rect2:
	return Rect2(RANDOM_X, FIELD.position.y + 1, DIALOG.end.x - 12 - RANDOM_X, SLIP_HEIGHT)


func _start_slip() -> Rect2:
	return Rect2(DIALOG.position.x + floorf((DIALOG.size.x - START_WIDTH) / 2.0), FIELD.position.y + 66,
			START_WIDTH, SLIP_HEIGHT)
