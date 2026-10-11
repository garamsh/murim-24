## Draws the blood combat.gd keeps in one MultiMesh, a pixel an instance, through blood.gdshader:
## the drops on the ground (`sprays` false) under everything standing, or the sprays' pixels
## (`sprays` true) over it. Each tick writes into its buffer only the pixels combat's ring took
## since the last, and a frame only tells the shader the time, so neither does work for every
## pixel. It decides nothing.
extends MultiMeshInstance2D

const Combat := preload("res://world_art/combat.gd")
const BAYER := [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]
# Floats an instance takes in the buffer: its 2D transform, its colour and its custom data.
const FLOATS := 16
# Half the width of the bounds the pixels are drawn in, in pixels: past any map's.
const BOUNDS := 1 << 20

## Whether this draws the sprays' pixels; otherwise the drops.
@export var sprays := false

var _material: ShaderMaterial
var _data := PackedFloat32Array()
# How many of the ring's pixels have been written so far.
var _written := 0


## Writes the pixels `combat`'s ring took since the last call.
func rebuild(combat: Combat) -> void:
	_set_up()
	var fallen := combat.spray_pixels_thrown if sprays else combat.drops_fallen
	if fallen == _written:
		return
	var most := _most()
	var at := combat.spray_pixel_at if sprays else combat.drop_at
	var ms := combat.spray_pixel_ms if sprays else combat.drop_ms
	var shade := combat.spray_pixel_shade if sprays else combat.drop_shade
	for n in range(maxi(_written, fallen - most), fallen):
		var i := n % most
		_put(i, at[i], Combat.BLOOD[shade[i]], ms[i])
	_written = fallen
	multimesh.buffer = _data
	multimesh.visible_instance_count = mini(fallen, most)


## Tells the shader the time drawn, `now` in milliseconds.
func show_at(now: int) -> void:
	_set_up()
	_material.set_shader_parameter("now", now / 1000.0)


func _most() -> int:
	return Combat.MOST_SPRAY_PIXELS if sprays else Combat.MOST_DROPS


# Writes instance `n`: the pixel at `p`, in `colour`, fallen at `ms`, with its dither threshold.
func _put(n: int, p: Vector2, colour: Color, ms: int) -> void:
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
	_data[at + 13] = (BAYER[posmod(pixel.y, 4) * 4 + posmod(pixel.x, 4)] + 0.5) / 16.0


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
	multimesh.instance_count = _most()
	multimesh.visible_instance_count = 0
	_data.resize(_most() * FLOATS)
	_material = ShaderMaterial.new()
	_material.shader = preload("res://world_art/blood.gdshader")
	_material.set_shader_parameter("lasts", (Combat.SPRAY_MS if sprays else Combat.DROP_MS) / 1000.0)
	material = _material
