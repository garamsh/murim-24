## The title screen, drawn as art/mockup/title_screen.png lays it out: the backdrop of mountains
## and the working title from art/title/backdrop.png, a line under it, and a menu to start a new
## game, continue, which waits for saving and cannot be taken yet, open the settings or quit, with
## its keys along the foot of the screen. Every word comes from hud.csv through Godot's
## translations. It holds no game rule.
extends "res://hud/panel.gd"

const BACKDROP := "res://art/title/backdrop.png"
# The mockup's layout, in the native 480x270.
const LINE := Vector2(40, 110)
const MENU := Rect2(40, 150, 136, 86)
const MENU_INSET := Vector2(6, 10)
const FOOT := 16
## The menu's entries, by what choosing each does; an entry in WAITING cannot be taken yet.
const ENTRIES := ["new", "continue", "settings", "quit"]
const WAITING := ["continue"]
const ENTRY_KEYS := {"new": "TITLE_NEW", "continue": "TITLE_CONTINUE", "settings": "TITLE_SETTINGS",
		"quit": "TITLE_QUIT"}

## The entry the menu has chosen, an index into ENTRIES.
var chosen := 0

var _backdrop: Texture2D


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED:
		queue_redraw()


## Shows the title screen with its first entry chosen.
func open() -> void:
	chosen = 0
	visible = true
	queue_redraw()


## Works the menu with `key`: up and down choose among the entries that can be taken, and Enter
## takes the chosen one. Returns what taking it does, one of ENTRIES, or "" when the key took none.
func key(event: InputEventKey) -> String:
	if event.keycode == KEY_UP or event.keycode == KEY_DOWN:
		chosen = _next(chosen, 1 if event.keycode == KEY_DOWN else -1)
		queue_redraw()
	elif event.keycode == KEY_ENTER or event.keycode == KEY_KP_ENTER:
		return ENTRIES[chosen]
	return ""


# The next entry from `from` in `step`'s direction that can be taken, staying put at either end.
func _next(from: int, step: int) -> int:
	var at := from + step
	while at >= 0 and at < ENTRIES.size():
		if not WAITING.has(ENTRIES[at]):
			return at
		at += step
	return from


func _draw() -> void:
	_font = get_theme_default_font() if _font == null else _font
	if _backdrop == null:
		_backdrop = load(BACKDROP)
	draw_texture(_backdrop, Vector2.ZERO)
	_text(LINE, tr("TITLE_LINE"), PAPER_DARK, true)
	_frame(MENU, INK_DARK)
	var items: Array[Dictionary] = []
	for entry: String in ENTRIES:
		var item := {"text": tr(ENTRY_KEYS[entry])}
		if WAITING.has(entry):
			item.note = tr("TITLE_LATER")
		items.append(item)
	_menu(MENU.position + MENU_INSET, MENU.size.x - 2 * MENU_INSET.x, items, chosen)
	draw_rect(Rect2(0, size.y - FOOT, size.x, FOOT), INK_DARK)
	draw_rect(Rect2(0, size.y - FOOT - 1, size.x, 1), GOLD_DARK)
	var keys := tr("TITLE_KEYS")
	_text(Vector2(floorf((size.x - _width(keys)) / 2.0), size.y - FOOT + 1), keys, STONE)
