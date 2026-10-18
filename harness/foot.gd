extends SceneTree
func _init() -> void:
	var t = load("res://play/play_test.gd").new()
	print("result ", t._foot_pass())
	quit()
