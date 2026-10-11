## Draws the blood combat.gd keeps in one MultiMesh, a pixel an instance, through blood.gdshader:
## the drops on the ground (`sprays` false) under everything standing, or the pixels of the sprays
## flying (`sprays` true) over it. Its buffer is rebuilt once a tick from combat's lists, and a
## frame only tells the shader the time, so a frame does no work a pixel. It decides nothing.
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


## Rebuilds the pixels from `combat` as its last tick left them, the sprays' where they are at `now`.
func rebuild(combat: Combat, now: int) -> void:
	_set_up()
	var data := PackedFloat32Array()
	if sprays:
		data.resize(combat.sprays.size() * Combat.SPRAY_PIXELS * FLOATS)
		var n := 0
		for spray: Dictionary in combat.sprays:
			for k in Combat.SPRAY_PIXELS:
				var p: Vector2 = Combat.spray_pixel(spray, k, now)
				_put(data, n, p, Combat.BLOOD[k % Combat.BLOOD.size()], spray.ms)
				n += 1
	else:
		data.resize(combat.drop_at.size() * FLOATS)
		for i in combat.drop_at.size():
			_put(data, i, combat.drop_at[i], Combat.BLOOD[combat.drop_shade[i]], combat.drop_ms[i])
	var count := data.size() / FLOATS
	if multimesh.instance_count < count:
		multimesh.instance_count = count + 256
	if count > 0:
		data.resize(multimesh.instance_count * FLOATS)
		multimesh.buffer = data
	multimesh.visible_instance_count = count
	visible = count > 0


## Tells the shader the time drawn, `now` in milliseconds.
func show_at(now: int) -> void:
	_set_up()
	_material.set_shader_parameter("now", now / 1000.0)


# Writes instance `n`: the pixel at `p`, in `colour`, fallen at `ms`, with its dither threshold.
func _put(data: PackedFloat32Array, n: int, p: Vector2, colour: Color, ms: int) -> void:
	var at := n * FLOATS
	var pixel := Vector2i(p.floor())
	data[at] = 1.0
	data[at + 3] = pixel.x + 0.5
	data[at + 5] = 1.0
	data[at + 7] = pixel.y + 0.5
	data[at + 8] = colour.r
	data[at + 9] = colour.g
	data[at + 10] = colour.b
	data[at + 11] = 1.0
	data[at + 12] = ms / 1000.0
	data[at + 13] = (BAYER[posmod(pixel.y, 4) * 4 + posmod(pixel.x, 4)] + 0.5) / 16.0


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
	_material = ShaderMaterial.new()
	_material.shader = preload("res://world_art/blood.gdshader")
	_material.set_shader_parameter("lasts", (Combat.SPRAY_MS if sprays else Combat.DROP_MS) / 1000.0)
	material = _material
