## The player's silver and wares, drawn as art/mockup/trade.gd lays them out (evidence/pr-181): its
## silver on a strip under the leader's, and on the keyboard layout a belt right of the technique
## bar, one cell for each of WorldAdapter.wares, its key, its icon and how many the player holds,
## greyed at none. What it shows is read from the world through the adapter. It holds no game rule.
extends "res://hud/panel.gd"

## The silver strip's size, and where it stands in the native 480x270: under the leader's on the
## keyboard layout, and under the touch layout's square buttons, which stand where it would.
const STRIP_SIZE := Vector2(100, 18)
const STRIP_AT := {false: Vector2(4, 92), true: Vector2(4, 138)}
const BELT_STEP := 32

## Each ware's key, by its index in WorldAdapter.wares.
var keys: Array[String] = []
## Where the belt stands, its top-left, in the HUD's pixels.
var belt_at := Vector2.ZERO
## Whether the belt shows, and where the strip stands: the keyboard layout's, or not, on touch.
var belt := true
## The silver and the count of each ware the player holds, as last read.
var silver := 0
var held := PackedInt64Array()
var _wares: Array = []


## Reads the silver and wares person `person` of `world` holds, and redraws when they changed.
func read(world: WorldAdapter, person: int) -> void:
	var now_silver := world.person_silver(person)
	var now_held := world.person_held(person)
	if _wares.is_empty():
		_wares = world.wares()
	if now_silver != silver or now_held != held:
		silver = now_silver
		held = now_held
		queue_redraw()


## Returns the strip's rect in the HUD's pixels, for a layout check.
func strip_rect() -> Rect2:
	return Rect2(STRIP_AT[not belt], STRIP_SIZE)


## Returns the belt's rect in the HUD's pixels, for a layout check.
func belt_rect() -> Rect2:
	return Rect2(belt_at, Vector2(_wares.size() * BELT_STEP + 6, 26))


func _draw() -> void:
	_font = get_theme_default_font() if _font == null else _font
	var strip := strip_rect()
	_frame(strip, INK)
	_ware_icon("silver", strip.position + Vector2(4, 1))
	var text := tr("SILVER") % silver
	_text(Vector2(strip.end.x - 6 - _width(text), strip.position.y + 3), fit(text, strip.size.x - 28), PAPER)
	if not belt or _wares.is_empty():
		return
	var rect := belt_rect()
	_frame(rect, INK)
	for n in mini(_wares.size(), held.size()):
		var cell := Rect2(rect.position + Vector2(4 + n * BELT_STEP, 3), Vector2(30, 20))
		draw_rect(cell, INK)
		draw_rect(cell.grow(-1), INK_DARK)
		var none := held[n] == 0
		_ware_icon(_wares[n].name, cell.position + Vector2(12, 2), none)
		if n < keys.size():
			_text(cell.position + Vector2(2, 3), keys[n], STONE_DARK if none else GOLD)
		var count := str(held[n])
		_text(cell.end - Vector2(_width(count) + 1, 12), count, STONE_DARK if none else PAPER, true)
