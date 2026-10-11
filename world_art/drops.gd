## Draws the drops of blood combat.gd keeps, on the ground under everything standing, each a pixel
## of its shade of Combat.BLOOD thinned by combat.gdshader's dither as it fades. It decides nothing.
extends Node2D

const Combat := preload("res://world_art/combat.gd")

# The drops shown, as Combat.shown_drops() gives them.
var _shown: Array[Dictionary] = []


## Shows `drops`, as Combat.shown_drops() gives them, from the next frame on.
func show_drops(drops: Array[Dictionary]) -> void:
	if drops.is_empty() and _shown.is_empty():
		return
	_shown = drops
	queue_redraw()


func _draw() -> void:
	for drop: Dictionary in _shown:
		draw_rect(Rect2(drop.at, Vector2.ONE), Color(Combat.BLOOD[drop.shade], drop.left))
