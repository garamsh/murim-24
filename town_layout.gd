# town_layout.gd -- a hand-laid walled town of #200's scale for the capture tools: 44 x 34 cells
# outside its wall, 40 x 30 inside, its wall 2 cells thick with corner towers, a gate tower over
# the south and the north road, steps up to the walkway, streets and lanes paved, a hall at the
# head of the main street, a temple, courtyard houses in four wards and a market square with stalls.
extends RefCounted

const SIZE := Vector2i(44, 34)
const SOUTH_GATE := Vector2i(22, 33)

# [art name, cell offset from the town's top-left, footprint]
static func pieces() -> Array:
	var p := []
	for c in [Vector2i(0, 0), Vector2i(41, 0), Vector2i(0, 31), Vector2i(41, 31)]:
		p.append(["town_corner_tower", c, Vector2i(3, 3)])
	p.append(["town_gate_tower", Vector2i(20, 32), Vector2i(5, 2)])
	p.append(["town_gate_tower", Vector2i(20, 0), Vector2i(5, 2)])
	for x in range(3, 41):
		if x < 20 or x > 24:
			p.append(["town_wall_south", Vector2i(x, 32), Vector2i(1, 2)])
			p.append(["town_wall_north", Vector2i(x, 0), Vector2i(1, 2)])
	for y in range(3, 31):
		p.append(["town_wall_west", Vector2i(0, y), Vector2i(2, 1)])
		p.append(["town_wall_east", Vector2i(42, y), Vector2i(2, 1)])
	for x in [6, 37]:
		p.append(["town_steps", Vector2i(x, 2), Vector2i(1, 2)])
	p.append(["hall", Vector2i(20, 3), Vector2i(5, 3)])
	p.append(["town_temple", Vector2i(27, 3), Vector2i(4, 3)])
	for c in [Vector2i(3, 4), Vector2i(9, 4), Vector2i(15, 3), Vector2i(3, 10), Vector2i(9, 10), Vector2i(15, 10),
			Vector2i(33, 3), Vector2i(27, 10), Vector2i(33, 10), Vector2i(26, 20), Vector2i(32, 20), Vector2i(26, 26),
			Vector2i(32, 26)]:
		p.append(["town_courtyard_house", c, Vector2i(5, 5)])
	for c in [Vector2i(9, 21), Vector2i(12, 21), Vector2i(15, 21), Vector2i(9, 26), Vector2i(12, 26), Vector2i(15, 26)]:
		p.append(["town_market_stall", c, Vector2i(2, 1)])
	p.append(["well", Vector2i(12, 23), Vector2i(2, 2)])
	for c in [Vector2i(3, 20), Vector2i(3, 24), Vector2i(3, 28)]:
		p.append(["house", c, Vector2i(3, 3)])
	p.append(["storehouse", Vector2i(38, 20), Vector2i(3, 2)])
	p.append(["well", Vector2i(39, 13), Vector2i(2, 2)])
	return p


# The paved cells: the main street from the south gate to the hall and on to the north gate, the
# cross street, the market square and the lanes between the wards.
static func paved() -> Array[Vector2i]:
	var cells: Array[Vector2i] = []
	var add := func(r: Rect2i) -> void:
		for y in range(r.position.y, r.end.y):
			for x in range(r.position.x, r.end.x):
				cells.append(Vector2i(x, y))
	add.call(Rect2i(21, 6, 3, 26))
	add.call(Rect2i(21, 2, 3, 1))
	add.call(Rect2i(2, 17, 40, 2))
	add.call(Rect2i(8, 19, 11, 10))
	add.call(Rect2i(2, 9, 19, 1))
	add.call(Rect2i(25, 8, 16, 2))
	add.call(Rect2i(25, 19, 1, 12))
	add.call(Rect2i(31, 19, 1, 12))
	add.call(Rect2i(22, 32, 1, 2))
	add.call(Rect2i(22, 0, 1, 2))
	return cells


# Lays the town at `corner` in `art`, a world_art.tscn shown: marks its footprints and paving built,
# paves its streets, and stands each piece as world_art.gd stands a structure, with its shadow. It
# returns the sprites laid.
static func lay(art: Node2D, corner: Vector2i) -> Array:
	var cell := Vector2(art.CELL)
	# As world_art.gd marks a world's: every piece's footprint and every paved cell, built.
	for piece: Array in pieces():
		for y in piece[2].y:
			for x in piece[2].x:
				art.relief.build(corner + piece[1] + Vector2i(x, y))
	for c in paved():
		art.relief.build(corner + c)
	var ground_sheet: Dictionary = art._sheet("decorations/ground")
	for c in paved():
		var tagged: Array = ground_sheet.tags["paving"]
		art.built.set_cell(corner + c, 0, Vector2i(tagged[0] + (c.x * 7 + c.y * 3) % (tagged[1] - tagged[0] + 1), 0))
	var sheets := {}
	var laid := []
	for piece: Array in pieces():
		var name: String = piece[0]
		var path: String = ("structures/" + name)
		if not sheets.has(name):
			sheets[name] = art._sheet(path)
		var frame: AtlasTexture = art._region(sheets[name], "intact", 0)
		var at: Vector2i = corner + piece[1]
		var footprint: Vector2i = piece[2]
		var sprite := Sprite2D.new()
		sprite.centered = false
		sprite.texture = frame
		sprite.position = Vector2(at.x, at.y + footprint.y) * cell
		sprite.offset = -Vector2(8, frame.get_size().y - 8)
		art.get_node("Standing").add_child(sprite)
		laid.append(sprite)
		if not name.begins_with("town_wall") and name != "town_steps":
			var shadow := Sprite2D.new()
			shadow.centered = false
			shadow.position = Vector2(at) * cell
			shadow.texture = art._shadow(footprint, 0)
			art.get_node("Laid").add_child(shadow)
	return laid


# Prepares the ground again from the relief with the town's cells built, its first bake freed, and
# no tree on the town.
static func rebake(art: Node2D, corner: Vector2i) -> void:
	var ground: Node2D = art.get_node("Ground")
	for child in ground.get_children():
		if child is SubViewport:
			child.free()
	var town := Rect2i(corner, SIZE).grow(1)
	ground.prepare(art.relief, {"open_at": func(c: Vector2i) -> bool: return not town.has_point(c)})
