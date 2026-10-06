## Draws the borders between factions' territories: along every edge between two cells in
## different territories, a dotted line in each owner's accent, one pixel inside its own cell, and
## none inside a cell no faction holds.
class_name Territory
extends Node2D

## The borders shown, so a test can read them.
var borders: Array = []
var _colours: Array[Color] = []


## Shows the borders `shown`, each [from, to, owner on the left or top, owner on the right or
## bottom], an owner -1 for none, in `colours` indexed by faction.
func show_borders(shown: Array, colours: Array[Color]) -> void:
	borders = shown
	_colours = colours
	queue_redraw()


func _draw() -> void:
	for border: Array in borders:
		var from: Vector2 = border[0]
		var to: Vector2 = border[1]
		var along := (to - from).normalized()
		var across := Vector2(along.y, along.x)
		for step in range(0, int(from.distance_to(to)), 2):
			var at := from + along * step
			if border[2] >= 0:
				draw_rect(Rect2(at - across, Vector2.ONE), _colours[border[2] % _colours.size()])
			if border[3] >= 0:
				draw_rect(Rect2(at, Vector2.ONE), _colours[border[3] % _colours.size()])
