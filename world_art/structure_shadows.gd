## The shadows standing structures cast on the ground, all drawn by this one node from one atlas,
## so the renderer draws every structure's shadow in a single batch however many stand. A shadow
## lies past its structure's right and bottom edges: a band SHADOW_REACH[stage] pixels wide of ink
## on every other pixel, shorter as the structure falls and none from rubble. Each footprint and
## stage's image is drawn into the atlas once; the node draws again only when a shadow is set or
## changes, since structures do not move. It decides nothing.
extends Node2D

## How wide a standing structure's shadow is, in pixels, by stage.
const SHADOW_REACH := [6, 6, 3, 0]
const SHADOW_INK := Color("14101a")
## A cell's width and depth on the ground, in pixels.
const CELL := Relief.CELL

var _atlas: ImageTexture
var _atlas_image: Image
# Where each footprint and stage's image lies in the atlas, by Vector3i(width, depth, stage).
var _regions := {}
# Each shadow drawn, by the caller's id: {at, footprint, stage}.
var _shown := {}


## Sets shadow `id` to lie from `at`, its structure's footprint's top-left on the ground as drawn,
## for a structure of `footprint` cells at `stage`.
func set_shadow(id: int, at: Vector2, footprint: Vector2i, stage: int) -> void:
	var shadow := {"at": at, "footprint": footprint, "stage": stage}
	if _shown.get(id) == shadow:
		return
	_region(footprint, stage)
	_shown[id] = shadow
	queue_redraw()


## Shadow `id` as set, {at, footprint, stage}, or empty for one never set.
func shadow_of(id: int) -> Dictionary:
	return _shown.get(id, {})


## How many textures the shadows are drawn from: one, once any is set.
func textures() -> int:
	return 0 if _atlas == null else 1


func _draw() -> void:
	for shadow: Dictionary in _shown.values():
		var region: Rect2 = _regions[Vector3i(shadow.footprint.x, shadow.footprint.y, shadow.stage)]
		draw_texture_rect_region(_atlas, Rect2(shadow.at, region.size), region)


# The atlas region of a footprint and stage's image, drawn into the atlas the first time it is
# asked for: the atlas grows to the right by the image's width.
func _region(footprint: Vector2i, stage: int) -> Rect2:
	var key := Vector3i(footprint.x, footprint.y, stage)
	if _regions.has(key):
		return _regions[key]
	var image := _image(footprint, stage)
	var from := 0 if _atlas_image == null else _atlas_image.get_width()
	var grown := Image.create_empty(from + image.get_width(),
			maxi(image.get_height(), 0 if _atlas_image == null else _atlas_image.get_height()), false, Image.FORMAT_RGBA8)
	if _atlas_image != null:
		grown.blit_rect(_atlas_image, Rect2i(Vector2i.ZERO, _atlas_image.get_size()), Vector2i.ZERO)
	grown.blit_rect(image, Rect2i(Vector2i.ZERO, image.get_size()), Vector2i(from, 0))
	_atlas_image = grown
	_atlas = ImageTexture.create_from_image(_atlas_image)
	_regions[key] = Rect2(Vector2(from, 0), image.get_size())
	return _regions[key]


# A footprint and stage's shadow from the footprint's top-left: the band past its right edge and
# the one past its bottom edge.
func _image(footprint: Vector2i, stage: int) -> Image:
	var reach: int = SHADOW_REACH[stage]
	var right := int(footprint.x * CELL.x)
	var bottom := int(footprint.y * CELL.y)
	var image := Image.create_empty(right + SHADOW_REACH[0], bottom + SHADOW_REACH[0], false, Image.FORMAT_RGBA8)
	for band: Rect2i in [Rect2i(right, reach, reach, bottom - 2), Rect2i(reach, bottom, right, reach / 2)]:
		for x in range(band.position.x, band.end.x):
			for y in range(band.position.y, band.end.y):
				if (x + y) % 2 == 0:
					image.set_pixel(x, y, SHADOW_INK)
	return image
