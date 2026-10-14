extends SceneTree
var play: Node
var frame := 0
func _initialize() -> void:
	play = load("res://play/play.tscn").instantiate()
	root.add_child(play)
func _process(_d: float) -> bool:
	frame += 1
	if frame == 3:
		play.start(2026)
	if frame == 10:
		var r = play.get_node("WorldArt").relief
		var counts := {}
		for y in r.size.y:
			for x in r.size.x:
				var l: int = r.level_at(Vector2i(x, y))
				var inland: bool = x >= 20 and y >= 15 and x < r.size.x - 20 and y < r.size.y - 15
				var k := "%d%s" % [l, " inland" if inland else ""]
				counts[k] = counts.get(k, 0) + 1
		print("levels: size %s player %s counts %s" % [r.size, play.get_node("Camera").position / Relief.CELL, counts])
		quit()
	return false
