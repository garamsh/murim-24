## The shortcut rail on the keyboard layout, drawn as art/mockup/button_bar.gd's rail lays it out
## (evidence/pr-198): a column hanging from its top-right corner, a row for each panel or screen the
## play scene opens by a key or a click, each its icon, its word and its key, the row under the
## cursor marked in gold and the row whose panel or screen is open lit in jade. The play scene gives
## it its rows; tap() turns a click into the action of the row clicked. It holds no game rule.
extends "res://hud/panel.gd"

const ROW_HEIGHT := 16
## Where a row's icon and word start, and the margin round the rows.
const ICON_X := 4
const WORD_X := 17
const INSET := 3
## Each icon a row or a touch button may name, 9 pixels square, a letter a pixel as _pixel_icon
## draws it: a tree of three kin, a banner on its pole, crossed swords and a jian upright, as
## touch.lua and mobile_hud_arts.lua draw them, then a gear and two bars.
const ICONS := {
	"genealogy": ["...###...", "...#.#...", "...###...", "....#....", ".#######.", ".#.....#.",
			"###...###", "#.#...#.#", "###...###"],
	"sect": ["#........", "#######..", "#ppppp#..", "#ppppp##.", "#pp#pp#..", "#######..",
			"#........", "#........", "##......."],
	"factions": ["#.......#", ".#.....#.", "..#...#..", "...#.#...", "....#....", "...#.#...",
			".##...##.", ".#.....#.", "#.......#"],
	"arts": ["...#.....", "...#.....", "...#.....", "...#.....", "...#.....", "...#.....", ".#####...",
			"...l.....", "...l....."],
	"settings": ["....#....", ".#.###.#.", "..#####..", ".##...##.", "###...###", ".##...##.",
			"..#####..", ".#.###.#.", "....#...."],
	"pause": [".........", ".##...##.", ".##...##.", ".##...##.", ".##...##.", ".##...##.",
			".##...##.", ".##...##.", "........."],
}

## The rows, top first, each a Dictionary of its `action`, the HUD's name for the panel or screen
## it opens; `key`, the word on its key, or "" for none; `icon`, a key of ICONS; and `label`, the
## hud.csv key of its word.
var rows: Array[Dictionary] = []
## The action of the row whose panel or screen is open, or "" for none.
var lit := ""
## The row under the cursor, or -1 for none.
var hovered := -1
## Where the rail's top-right corner stands, in the HUD's pixels.
var top_right := Vector2.ZERO


func _notification(what: int) -> void:
	if what == NOTIFICATION_TRANSLATION_CHANGED:
		queue_redraw()


## Returns the rail's rect in the HUD's pixels: as wide as its widest word and key, in the current
## locale, a row high for each row.
func rect() -> Rect2:
	_font = get_theme_default_font() if _font == null else _font
	var widest := 0.0
	for row: Dictionary in rows:
		widest = maxf(widest, _width(tr(row.label)) + _width(row.key))
	var size := Vector2(WORD_X + widest + 8 + 2 * INSET, rows.size() * ROW_HEIGHT + 2 * INSET)
	return Rect2(top_right - Vector2(size.x, 0), size)


## Returns the rect of row `n`, in the HUD's pixels.
func row_rect(n: int) -> Rect2:
	var frame := rect()
	return Rect2(frame.position + Vector2(INSET, INSET + n * ROW_HEIGHT), Vector2(frame.size.x - 2 * INSET, ROW_HEIGHT))


## Marks the row under `at`, a point in the HUD's pixels, or none where `at` is on no row.
func hover(at: Vector2) -> void:
	var under := _row_at(at)
	if under != hovered:
		hovered = under
		queue_redraw()


## Takes a click at `at`, a point in the HUD's pixels. Returns the action of the row clicked, or ""
## for a click on no row.
func tap(at: Vector2) -> String:
	var row := _row_at(at)
	return rows[row].action if row >= 0 else ""


func _row_at(at: Vector2) -> int:
	for n in rows.size():
		if row_rect(n).has_point(at):
			return n
	return -1


func _draw() -> void:
	if rows.is_empty():
		return
	_frame(rect(), Palette.INK_2)
	for n in rows.size():
		var row := rows[n]
		var cell := row_rect(n)
		var on: bool = row.action == lit
		if on or n == hovered:
			draw_rect(cell, Palette.INK_1)
			draw_rect(Rect2(cell.position, Vector2(1, cell.size.y)), Palette.JADE_4 if on else Palette.GOLD_4)
		_pixel_icon(ICONS[row.icon], cell.position + Vector2(ICON_X, 3),
				Palette.JADE_7 if on else (Palette.PAPER_6 if row.icon == "arts" else Palette.GOLD_4))
		_text(cell.position + Vector2(WORD_X, 2), tr(row.label), Palette.JADE_7 if on else Palette.PAPER_6)
		_right(cell.end.x - 3, cell.position.y + 2, row.key, Palette.GOLD_4)
