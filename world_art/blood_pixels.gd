## Draws the blood combat.gd keeps in one MultiMesh, a pixel an instance, through blood.gdshader:
## the drops on the ground (`sprays` false) under everything standing, each a cluster of
## DROP_PIXELS pixels, a dark core and a lighter edge; or the sprays' pixels (`sprays` true) over
## it, each flying from the struck body's middle as far as combat gives it. Each tick writes into
## its buffer only the pixels combat's ring took since the last, and a frame only tells the shader
## the time, so neither does work for every pixel. It decides nothing.
extends MultiMeshInstance2D

const Combat := preload("res://world_art/combat.gd")
const BAYER := [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]
## The instances a drop takes: its core and two pixels of its edge.
const DROP_PIXELS := 3
## Where a drop's edge lies from its core, by its shape: two pixels, or one where both are the same.
const DROP_EDGES: Array[Array] = [
	[Vector2i(1, 0), Vector2i(0, 1)],
	[Vector2i(-1, 0), Vector2i(1, 0)],
	[Vector2i(1, 0), Vector2i(1, 0)],
]
## How far a spray's pixel falls over its flight, in pixels, after it is thrown.
const SPRAY_FALL := 6.0
# Floats an instance takes in the buffer: its 2D transform, its colour and its custom data.
const FLOATS := 16
# Half the width of the bounds the pixels are drawn in, in pixels: past any map's.
const BOUNDS := 1 << 20

## Whether this draws the sprays' pixels; otherwise the drops.
@export var sprays := false

var _material: ShaderMaterial
var _data := PackedFloat32Array()
# How many of the ring's entries have been written so far.
var _written := 0


## Writes the pixels `combat`'s ring took since the last call.
func rebuild(combat: Combat) -> void:
	_set_up()
	var fallen := combat.spray_pixels_thrown if sprays else combat.drops_fallen
	# A ring begun again, as a world loaded begins it, shows none of what it held.
	if fallen < _written:
		_written = 0
		multimesh.visible_instance_count = 0
	if fallen == _written:
		return
	var most := _most()
	for n in range(maxi(_written, fallen - most), fallen):
		var i := n % most
		if sprays:
			var colour: Color = Combat.BLOOD[combat.spray_pixel_shade[i]]
			_put(i, combat.spray_pixel_at[i], colour, combat.spray_pixel_ms[i], combat.spray_pixel_reach[i])
		else:
			var core: Vector2 = combat.drop_at[i]
			var edges: Array = DROP_EDGES[combat.drop_shape[i]]
			_put(i * DROP_PIXELS, core, Combat.BLOOD[1], combat.drop_ms[i])
			for k in edges.size():
				_put(i * DROP_PIXELS + 1 + k, core + Vector2(edges[k]), Combat.BLOOD[0], combat.drop_ms[i])
	_written = fallen
	multimesh.buffer = _data
	multimesh.visible_instance_count = mini(fallen, most) * _pixels()


## Tells the shader the time drawn, `now` in milliseconds.
func show_at(now: int) -> void:
	_set_up()
	_material.set_shader_parameter("now", now / 1000.0)


# The ring's entries kept, and the instances each takes.
func _most() -> int:
	return Combat.MOST_SPRAY_PIXELS if sprays else Combat.MOST_DROPS


func _pixels() -> int:
	return 1 if sprays else DROP_PIXELS


# Writes instance `n`: the pixel at `p`, in `colour`, fallen at `ms` and flying `reach` from there,
# with its dither threshold: the Bayer matrix's at the pixel for one that stays, and for one that
# flies, whose spray's pixels all start on one pixel, the matrix's by its place in the ring, so a
# spray thins out a pixel at a time rather than all at once.
func _put(n: int, p: Vector2, colour: Color, ms: int, reach := Vector2.ZERO) -> void:
	var at := n * FLOATS
	var pixel := Vector2i(p.floor())
	_data[at] = 1.0
	_data[at + 3] = pixel.x + 0.5
	_data[at + 5] = 1.0
	_data[at + 7] = pixel.y + 0.5
	_data[at + 8] = colour.r
	_data[at + 9] = colour.g
	_data[at + 10] = colour.b
	_data[at + 11] = 1.0
	_data[at + 12] = ms / 1000.0
	var cell := n % 16 if sprays else posmod(pixel.y, 4) * 4 + posmod(pixel.x, 4)
	_data[at + 13] = (BAYER[cell] + 0.5) / 16.0
	_data[at + 14] = reach.x
	_data[at + 15] = reach.y


func _set_up() -> void:
	if multimesh != null:
		return
	multimesh = MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_2D
	multimesh.use_colors = true
	multimesh.use_custom_data = true
	var quad := QuadMesh.new()
	quad.size = Vector2.ONE
	multimesh.mesh = quad
	# The renderer keeps a MultiMesh's bounds from its first draw and culls by them, so the bounds
	# are set over any map, wherever blood will fall.
	multimesh.custom_aabb = AABB(Vector3(-BOUNDS, -BOUNDS, 0), Vector3(2 * BOUNDS, 2 * BOUNDS, 0))
	multimesh.instance_count = _most() * _pixels()
	multimesh.visible_instance_count = 0
	_data.resize(_most() * _pixels() * FLOATS)
	_material = ShaderMaterial.new()
	_material.shader = preload("res://world_art/blood.gdshader")
	_material.set_shader_parameter("lasts", (Combat.SPRAY_MS if sprays else Combat.DROP_MS) / 1000.0)
	_material.set_shader_parameter("fall", SPRAY_FALL if sprays else 0.0)
	material = _material
