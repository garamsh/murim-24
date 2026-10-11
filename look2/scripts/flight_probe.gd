extends SceneTree
# flight_probe.gd: draws two pixels of blood.gdshader at (40, 40) told to fly (20, 0) and (0, 20), at
# their flight's end, and prints where non-clear pixels land. Quits on frame 10.
const BloodPixels := preload("res://world_art/blood_pixels.gd")
var _vp: SubViewport
var _node: MultiMeshInstance2D
var _f := 0
func _initialize() -> void:
	_vp = SubViewport.new(); _vp.size = Vector2i(100, 100); _vp.transparent_bg = true
	_vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	root.add_child(_vp)
	_node = BloodPixels.new(); _node.sprays = true
	_vp.add_child(_node)
	_node._set_up()
	_node._put(0, Vector2(40, 40), Color.RED, 0, Vector2(20, 0))
	_node._put(1, Vector2(40, 40), Color.BLUE, 0, Vector2(0, 20))
	_node.multimesh.buffer = _node._data
	_node.multimesh.visible_instance_count = 2
	_node._material.set_shader_parameter("now", 0.2)
func _process(_d: float) -> bool:
	_f += 1
	if _f == 8:
		var img := _vp.get_texture().get_image()
		for y in 100:
			for x in 100:
				var c := img.get_pixel(x, y)
				if c.a > 0.5: print("pixel ", x, ",", y, " ", c.to_html(false))
	return _f >= 10
