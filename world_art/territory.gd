## Draws the borders between factions' territories: along every edge between two cells owned by
## different factions, a dotted line in each owner's accent, one pixel inside its own cell.
class_name Territory
extends Node2D

var _borders: Array = []
var _colours: Array[Color] = []


## Shows `borders`, each [from, to, owner on the left or top, owner on the right or bottom], in
## `colours` indexed by faction.
func show_borders(borders: Array, colours: Array[Color]) -> void:
	_borders = borders
	_colours = colours
	queue_redraw()


func _draw() -> void:
	for border: Array in _borders:
		var from: Vector2 = border[0]
		var to: Vector2 = border[1]
		var along := (to - from).normalized()
		var across := Vector2(along.y, along.x)
		for step in range(0, int(from.distance_to(to)), 2):
			var at := from + along * step
			draw_rect(Rect2(at - across, Vector2.ONE), _colours[border[2] % _colours.size()])
			draw_rect(Rect2(at, Vector2.ONE), _colours[border[3] % _colours.size()])
