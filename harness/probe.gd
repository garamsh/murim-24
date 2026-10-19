extends SceneTree
# Bakes a map with built cells on the plain across a forest edge, on the hills' west edge, and below
# a mountain's south face, warped and unwarped, and saves 4x crops of each block, with the count of
# pixels differing between the two over each block's screen rect grown by one pixel.
const Ground := preload("res://world_art/ground.gd")
const SIZE := Vector2i(64, 64)
const BLOCKS := {"flat": Rect2i(21, 10, 6, 4), "slope": Rect2i(28, 42, 4, 3), "drop": Rect2i(4, 36, 5, 2)}
var grounds := []
var frame := 0
var out: String
var relief: Relief

func _initialize() -> void:
	out = OS.get_cmdline_user_args()[0]
	relief = Relief.new(SIZE)
	for y in SIZE.y:
		for x in range(24, SIZE.x):
			relief.cover(Vector2i(x, y), "forest")
	for y in range(28, 36):
		for x in range(2, 12):
			relief.lay(Vector2i(x, y), "rock", 4, "rock_face", false)
	for y in range(38, 50):
		for x in range(30, 40):
			relief.lay(Vector2i(x, y), "hills", 1 if x < 33 or y < 40 or y > 47 else 2, "bank_face", true)
	for name in BLOCKS:
		var r: Rect2i = BLOCKS[name]
		for y in range(r.position.y, r.end.y):
			for x in range(r.position.x, r.end.x):
				relief.build(Vector2i(x, y))
	for options in [{}, {"shader": {"warp": 0.0}}]:
		var g := Ground.new()
		g.standing = Node2D.new()
		root.add_child(g)
		root.add_child(g.standing)
		g.prepare(relief, options)
		grounds.append(g)

func _process(_d: float) -> bool:
	frame += 1
	var view := Rect2(Vector2.ZERO, Vector2(SIZE) * Relief.CELL)
	if not (grounds[0].stream(view) and grounds[1].stream(view)):
		return frame > 900 and _quit()
	for name in BLOCKS:
		var r: Rect2i = BLOCKS[name]
		var shots := []
		for g in grounds:
			shots.append(_composite(g, Rect2i(Vector2i(r.position.x * 16 - 24, r.position.y * 12 - 64), Vector2i(r.size.x * 16 + 48, r.size.y * 12 + 88))))
		var diff := 0
		var a: Image = shots[0]
		var b: Image = shots[1]
		for y in a.get_height():
			for x in a.get_width():
				if a.get_pixel(x, y) != b.get_pixel(x, y):
					diff += 1
		print("probe: %s %s differs on %d of %d pixels round it" % [name, r, diff, a.get_width() * a.get_height()])
		var origin := Vector2i(r.position.x * 16 - 24, r.position.y * 12 - 64)
		var on := 0
		var on_diff := 0
		var where := {}
		for gy in range(r.position.y * 12 - 1, r.end.y * 12 + 1):
			for gx in range(r.position.x * 16 - 1, r.end.x * 16 + 1):
				var sy := gy - int(round(relief.lift_at(Vector2(gx + 0.5, gy + 0.5))))
				var at := Vector2i(gx, sy) - origin
				on += 1
				if a.get_pixelv(at) != b.get_pixelv(at):
					on_diff += 1
					var d := mini(mini(gx - (r.position.x * 16 - 1), r.end.x * 16 - gx), mini(gy - (r.position.y * 12 - 1), r.end.y * 12 - gy))
					where[d] = where.get(d, 0) + 1
		print("probe: %s footprint and seam: %d of %d pixels differ, by depth in from the seam %s" % [name, on_diff, on, where])
		for i in 2:
			var s: Image = shots[i]
			s.resize(s.get_width() * 4, s.get_height() * 4, Image.INTERPOLATE_NEAREST)
			s.save_png("%s_%s_%s.png" % [out, name, ["warped", "unwarped"][i]])
	return _quit()

func _quit() -> bool:
	quit()
	return true

# `ground`'s pixels over `rect`, in pixels on the unraised ground's image space: each chunk's ground
# with its strips over it.
func _composite(ground: Node2D, rect: Rect2i) -> Image:
	var shot := Image.create_empty(rect.size.x, rect.size.y, false, Image.FORMAT_RGBA8)
	for key: Vector2i in [Vector2i(0, 0), Vector2i(1, 0), Vector2i(0, 1), Vector2i(1, 1)]:
		var images: Dictionary = ground.images_of(key)
		if images.is_empty():
			continue
		var at := Vector2i(images.ground_at)
		shot.blend_rect(images.ground, Rect2i(Vector2i.ZERO, images.ground.get_size()), at - rect.position)
		for strip: Vector2i in images.strips:
			var piece: Dictionary = images.strips[strip]
			shot.blend_rect(piece.image, Rect2i(Vector2i.ZERO, piece.image.get_size()), Vector2i(piece.at) - rect.position)
	return shot
