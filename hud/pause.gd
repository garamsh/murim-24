## The pause panel over the game, drawn as art/mockup/pause_screen.png lays it out: a menu to
## resume, save, open the settings, leave for the title or quit, under it whether the save was
## written, over the seed the world was opened from; beside it the controls guide, each control's keys on their caps and what it does, as the
## play scene binds them. An entry is taken by a tap as well as by the keys. Every word comes from hud.csv through Godot's translations. It holds no
## game rule.
extends "res://hud/panel.gd"

# The mockup's layout, in the native 480x270, grown by a row of the guide for the keys 1 to 9 and 0,
# another for K, and another for the wares' keys, half up and half down.
const DIALOG := Rect2(80, 10, 320, 250)
const MENU_WIDTH := 120
const GUIDE_X := 230
const GUIDE_STEP := 16
# Where what a control does starts, right of its keys' caps.
const GUIDE_TEXT_X := 64
## The menu's entries, by what choosing each does.
const ENTRIES := ["resume", "save", "settings", "title", "quit"]
const ENTRY_KEYS := {"resume": "PAUSE_RESUME", "save": "PAUSE_SAVE", "settings": "PAUSE_SETTINGS",
		"title": "PAUSE_TO_TITLE", "quit": "PAUSE_QUIT"}

## The entry the menu has chosen, an index into ENTRIES.
var chosen := 0
## The controls the guide shows, each a Dictionary of its `keys`, the words on its caps, and
## `does`, the hud.csv key of what it does.
var controls: Array[Dictionary] = []
## The seed the world was opened from.
var world_seed := 0
## The hud.csv key of the notice under the menu, or "" while there is none.
var notice := ""


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED:
		queue_redraw()


## Opens the panel on the world opened from `opened_from`, its guide showing `shown`, as
## `controls` holds them, with its first entry chosen.
func open(shown: Array[Dictionary], opened_from: int) -> void:
	controls = shown
	world_seed = opened_from
	chosen = 0
	notice = ""
	visible = true
	queue_redraw()


## Shows under the menu whether the save was written, as `written` says.
func note_save(written: bool) -> void:
	notice = "PAUSE_SAVED" if written else "PAUSE_NOT_SAVED"
	queue_redraw()


## Works the menu with `key`: up and down choose an entry, Enter takes it and Escape resumes.
## Returns what taking it does, one of ENTRIES, or "" when the key took none.
func key(event: InputEventKey) -> String:
	if event.keycode == KEY_UP or event.keycode == KEY_DOWN:
		chosen = clampi(chosen + (1 if event.keycode == KEY_DOWN else -1), 0, ENTRIES.size() - 1)
		queue_redraw()
	elif ENTER_KEYS.has(event.keycode):
		return ENTRIES[chosen]
	elif event.keycode == KEY_ESCAPE:
		return "resume"
	return ""


## Works the menu with a tap at `at`: an entry tapped is chosen and taken, and the close word, or
## anywhere outside the panel, resumes. Returns what taking it does, one of ENTRIES, or "" when the
## tap took none.
func tap(at: Vector2) -> String:
	if not DIALOG.has_point(at) or _close_hit(DIALOG, tr("PAUSE_CLOSE"), at):
		return "resume"
	var row := _menu_row(Vector2(DIALOG.position.x + 10, DIALOG.position.y + 32), MENU_WIDTH, ENTRIES.size(), at)
	if row < 0:
		return ""
	chosen = row
	queue_redraw()
	return ENTRIES[row]


func _draw() -> void:
	_dim()
	_dialog(DIALOG, tr("PAUSE_TITLE"), tr("PAUSE_CLOSE"), tr("PAUSE_KEYS"))
	var left := DIALOG.position.x + 10
	var items: Array[Dictionary] = []
	for entry: String in ENTRIES:
		items.append({"text": tr(ENTRY_KEYS[entry])})
	_menu(Vector2(left, DIALOG.position.y + 32), MENU_WIDTH, items, chosen)
	if not notice.is_empty():
		# A row under the menu's last entry.
		var notice_y := DIALOG.position.y + 32 + ENTRIES.size() * MENU_STEP + 4
		_text(Vector2(left + 2, notice_y), tr(notice), Palette.JADE_4 if notice == "PAUSE_SAVED" else Palette.LACQUER_5)
	var seed_y := DIALOG.end.y - 40
	draw_rect(Rect2(left, seed_y - 5, MENU_WIDTH, 1), Palette.INK_4)
	_text(Vector2(left + 2, seed_y), tr("PAUSE_SEED"), Palette.GOLD_4)
	var value := str(world_seed)
	_text(Vector2(left + MENU_WIDTH - 4 - _width(value), seed_y), value, Palette.STONE_4)
	draw_rect(Rect2(GUIDE_X - 10, DIALOG.position.y + 26, 1, DIALOG.size.y - 52), Palette.INK_4)
	_text(Vector2(GUIDE_X, DIALOG.position.y + 26), tr("CONTROLS_TITLE"), Palette.GOLD_4)
	draw_rect(Rect2(GUIDE_X, DIALOG.position.y + 39, DIALOG.end.x - 10 - GUIDE_X, 1), Palette.INK_4)
	for n in controls.size():
		var y := DIALOG.position.y + 44 + n * GUIDE_STEP
		var x := GUIDE_X
		for cap: String in controls[n].keys:
			x += _keycap(Vector2(x, y), tr(cap)) + 2
		_text(Vector2(GUIDE_X + GUIDE_TEXT_X, y), tr(controls[n].does), Palette.PAPER_6)
