## The new-game panel over the title screen, drawn as art/mockup/new_game_screen.png lays it out:
## the seed typed into its field or drawn at random, a line saying the same seed opens the same
## world, and the button that starts it. Every word comes from hud.csv through Godot's
## translations. It holds no game rule.
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
	if digit >= 0 and typed.length() < MAX_DIGITS:
		typed += str(digit)
	elif event.keycode == KEY_BACKSPACE:
		typed = typed.left(-1)
	elif event.keycode == KEY_R:
		typed = str(randi())
	elif event.keycode == KEY_ENTER or event.keycode == KEY_KP_ENTER:
		return "start"
	elif event.keycode == KEY_ESCAPE:
		close()
		return "back"
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
	_text(Vector2(DIALOG.position.x + 12, y + 2), tr("NEW_GAME_SEED"), GOLD)
	draw_rect(FIELD, JADE)
	draw_rect(FIELD.grow(-1), INK_DARK)
	_text(FIELD.position + Vector2(5, 2), typed, PAPER)
	# The caret, after what was typed.
	draw_rect(Rect2(FIELD.position.x + 6 + _width(typed), y + 3, 1, 10), PAPER)
	_slip(Vector2(RANDOM_X, y + 1), DIALOG.end.x - 12 - RANDOM_X, tr("NEW_GAME_RANDOM"), false)
	_text(Vector2(DIALOG.position.x + 12, y + 26), tr("NEW_GAME_SAME"), STONE)
	_text(Vector2(DIALOG.position.x + 12, y + 40), tr("NEW_GAME_EMPTY"), STONE_DARK)
	draw_rect(Rect2(DIALOG.position.x + 6, y + 58, DIALOG.size.x - 12, 1), INK_LIGHT)
	_slip(Vector2(DIALOG.position.x + floorf((DIALOG.size.x - START_WIDTH) / 2.0), y + 66), START_WIDTH,
			tr("NEW_GAME_START"), true)
