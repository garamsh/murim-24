## The prompt over the person the player may talk to, drawn as art/mockup/dialogue.gd draws it
## (evidence/pr-180): on a slip of ink centred over its head, the talk key's cap and the word to
## talk with its name; or, over a person whose faction is hostile, the refusal in crimson and no
## key. The play scene says whom and where. It holds no game rule.
extends "res://hud/panel.gd"

const PersonName := preload("res://hud/person_name.gd")
const CRIMSON := Color("9c1f24")
const CRIMSON_LIGHT := Color("d63a2f")

## Where the person's head stands, in the HUD's pixels.
var head := Vector2.ZERO
## The word on the talk key's cap.
var key := ""
## The person the prompt names, or -1 while it shows none.
var person := -1
## Whether the person refuses to talk for its faction's hostility.
var refused := false


## Shows the prompt over `talked_to` in `world`, its head at `at` in the HUD's pixels, refused or
## not as `hostile` says; or hides it for a person of -1.
func point_at(world: WorldAdapter, talked_to: int, at: Vector2, hostile: bool) -> void:
	_world = world
	visible = talked_to >= 0
	if person == talked_to and head == at and refused == hostile:
		return
	person = talked_to
	head = at
	refused = hostile
	queue_redraw()


## Returns the words the prompt shows, in the current locale.
func words() -> String:
	if refused:
		return tr("DIALOGUE_HOSTILE")
	return tr("DIALOGUE_TALK") + " · " + PersonName.of(_world, person, true)


func _draw() -> void:
	if person < 0:
		return
	_font = get_theme_default_font() if _font == null else _font
	var word := words()
	var cap := 0.0 if refused else keycap_width(key) + 4
	var width := cap + _width(word) + 8
	var at := (head - Vector2(width / 2.0, 18)).floor()
	var edge := CRIMSON if refused else GOLD_DARK
	draw_rect(Rect2(at, Vector2(width, 17)), edge)
	draw_rect(Rect2(at + Vector2.ONE, Vector2(width - 2, 15)), INK)
	# A tick under the slip points at the person.
	draw_rect(Rect2(head.x - 1, at.y + 17, 3, 1), edge)
	draw_rect(Rect2(head.x, at.y + 18, 1, 1), edge)
	if not refused:
		_keycap(at + Vector2(4, 2), key)
	_text(at + Vector2(4 + cap, 2), word, CRIMSON_LIGHT if refused else PAPER)
