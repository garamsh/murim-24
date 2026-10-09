## The title screen, drawn as art/mockup/title_screen.png lays it out, at the least whole multiple
## that covers the screen: the backdrop of mountains
## and the working title from art/title/backdrop.png, a line under it, and a menu to start a new
## game, continue the saved one, which cannot be taken while there is none, open the settings or
## quit, with its keys along the foot of the screen; under the menu, a notice of why a save was not
## loaded. An entry is taken by a tap as well as by the keys. Every word comes from hud.csv through Godot's translations. It holds no game rule.
extends "res://hud/panel.gd"

const BACKDROP := "res://art/title/backdrop.png"
# The mockup's layout, in the native 480x270.
const LINE := Vector2(40, 110)
const MENU := Rect2(40, 150, 136, 86)
const MENU_INSET := Vector2(6, 10)
const FOOT := 16
# The notice's line, between the menu and the foot.
const NOTICE := Vector2(40, 239)
## The menu's entries, by what choosing each does.
const ENTRIES := ["new", "continue", "settings", "quit"]
const ENTRY_KEYS := {"new": "TITLE_NEW", "continue": "TITLE_CONTINUE", "settings": "TITLE_SETTINGS",
		"quit": "TITLE_QUIT"}
## The hud.csv key of the notice for each reason a save is not loaded, by WorldAdapter.LOAD_*
## constant.
const LOAD_KEYS := {
	WorldAdapter.LOAD_MISSING: "TITLE_LOAD_MISSING",
	WorldAdapter.LOAD_NOT_A_SAVE: "TITLE_LOAD_NOT_A_SAVE",
	WorldAdapter.LOAD_UNSUPPORTED_VERSION: "TITLE_LOAD_UNSUPPORTED_VERSION",
	WorldAdapter.LOAD_TRUNCATED: "TITLE_LOAD_TRUNCATED",
	WorldAdapter.LOAD_CORRUPTED: "TITLE_LOAD_CORRUPTED",
}

## The entry the menu has chosen, an index into ENTRIES.
var chosen := 0
## The entries that cannot be taken: continue while there is no save.
var unavailable: Array[String] = []
## The hud.csv key of the notice under the menu, or "" while there is none.
var notice := ""

var _backdrop: Texture2D


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED:
		queue_redraw()


## Shows the title screen with its first entry chosen, continue taken or not as `saved` says
## whether there is a save to continue.
func open(saved: bool) -> void:
	chosen = 0
	unavailable.assign([] if saved else ["continue"])
	notice = ""
	visible = true
	queue_redraw()


## Shows under the menu why the save was not loaded, `refusal` one of the WorldAdapter.LOAD_*
## constants.
func refuse(refusal: int) -> void:
	notice = LOAD_KEYS.get(refusal, "TITLE_LOAD_CORRUPTED")
	queue_redraw()


## Works the menu with `key`: up and down choose among the entries that can be taken, and Enter
## takes the chosen one. Returns what taking it does, one of ENTRIES, or "" when the key took none.
func key(event: InputEventKey) -> String:
	if event.keycode == KEY_UP or event.keycode == KEY_DOWN:
		chosen = _next(chosen, 1 if event.keycode == KEY_DOWN else -1)
		queue_redraw()
	elif ENTER_KEYS.has(event.keycode):
		return ENTRIES[chosen]
	return ""


## Works the menu with a tap at `at`: an entry that can be taken is chosen and taken. Returns
## what taking it does, one of ENTRIES, or "" when the tap took none.
func tap(at: Vector2) -> String:
	var row := _menu_row(MENU.position + MENU_INSET, MENU.size.x - 2 * MENU_INSET.x, ENTRIES.size(),
			_stage().affine_inverse() * at)
	if row < 0 or unavailable.has(ENTRIES[row]):
		return ""
	chosen = row
	queue_redraw()
	return ENTRIES[row]


# The next entry from `from` in `step`'s direction that can be taken, staying put at either end.
func _next(from: int, step: int) -> int:
	var at := from + step
	while at >= 0 and at < ENTRIES.size():
		if not unavailable.has(ENTRIES[at]):
			return at
		at += step
	return from


# Where the screen is drawn: the native layout at the least whole multiple at which the backdrop
# covers the room, centred across it and standing on its foot, so what the room cuts off is sky and
# the keys along the foot stay in view.
func _stage() -> Transform2D:
	if _backdrop == null:
		_backdrop = load(BACKDROP)
	var backdrop := _backdrop.get_size()
	var room := _room()
	var times := ceilf(maxf(room.size.x / backdrop.x, room.size.y / backdrop.y))
	var corner := Vector2(room.get_center().x - backdrop.x * times / 2.0, room.end.y - backdrop.y * times)
	return Transform2D(0.0, Vector2(times, times), 0.0, corner.floor())


func _draw() -> void:
	_font = get_theme_default_font() if _font == null else _font
	draw_set_transform_matrix(_stage())
	draw_texture(_backdrop, Vector2.ZERO)
	_text(LINE, tr("TITLE_LINE"), Palette.PAPER_2, true)
	_frame(MENU, Palette.INK_1)
	var items: Array[Dictionary] = []
	for entry: String in ENTRIES:
		var item := {"text": tr(ENTRY_KEYS[entry])}
		if unavailable.has(entry):
			item.note = tr("TITLE_NO_SAVE")
		items.append(item)
	_menu(MENU.position + MENU_INSET, MENU.size.x - 2 * MENU_INSET.x, items, chosen)
	if not notice.is_empty():
		_text(NOTICE, tr(notice), Palette.PAPER_2, true)
	draw_rect(Rect2(0, size.y - FOOT, size.x, FOOT), Palette.INK_1)
	draw_rect(Rect2(0, size.y - FOOT - 1, size.x, 1), Palette.GOLD_2)
	var keys := tr("TITLE_KEYS")
	_text(Vector2(floorf((size.x - _width(keys)) / 2.0), size.y - FOOT + 1), keys, Palette.STONE_4)
