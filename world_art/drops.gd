## Draws the drops of blood combat.gd keeps, on the ground under everything standing, each a pixel
## of its shade of Combat.BLOOD thinned by combat.gdshader's dither as it fades, read from combat's
## ring as it is. It decides nothing.
extends Node2D

const Combat := preload("res://world_art/combat.gd")

var _combat: Combat
var _now := 0
var _view := Rect2()
var _shown := false


## Shows `combat`'s drops at `now` within `view` from the next frame on.
func show_drops(combat: Combat, now: int, view: Rect2) -> void:
	_combat = combat
	_now = now
	_view = view
	# With no drop fresher than Combat.DROP_MS since the last draw, there is nothing to draw again.
	var fresh := combat.drops_fallen > 0 and now - combat.drop_ms[(combat.drops_fallen - 1) % Combat.MOST_DROPS] < Combat.DROP_MS
	if fresh or _shown:
		_shown = fresh
		queue_redraw()


func _draw() -> void:
	if _combat == null:
		return
	var at := _combat.drop_at
	var ms := _combat.drop_ms
	var shade := _combat.drop_shade
	for i in at.size():
		var age := _now - ms[i]
		if age < Combat.DROP_MS and _view.has_point(at[i]):
			draw_rect(Rect2(at[i], Vector2.ONE), Color(Combat.BLOOD[shade[i]], 1.0 - float(age) / Combat.DROP_MS))
