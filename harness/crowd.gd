## The player swinging inside the M1 battle's crowd: the army nearest the screen's middle is steered,
## the camera on its lead; from the first frame its lead's own use is shown, six consecutive frames are
## kept, then saved whole and as a crop round the lead. TAG names the tree. Korean locale.
extends SceneTree
var view: Node
var frame := 0
var army := -1
var held: Array[Image] = []
var waited := 0


func _initialize() -> void:
	TranslationServer.set_locale("ko")
	root.size = Vector2i(1920, 1080)
	DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_ALWAYS_ON_TOP, true)
	view = load("res://world_view/world_view.tscn").instantiate()
	root.add_child(view)


func _process(_delta: float) -> bool:
	frame += 1
	var art: Node2D = view.get_node("WorldArt")
	var camera: Camera2D = view.get_node("Camera")
	if frame == 120:
		army = view._nearest_army(Vector2(WorldAdapter.m1_centre()))
		art.steered = army
		print("crowd: steering army %d" % army)
	if army >= 0 and not view.world.roster(army).is_empty():
		camera.position = art.body_at(army, 0, 1.0).round()
	if army >= 0 and held.size() < 6:
		var mine: Array = art.combat.shown_strikes(Time.get_ticks_msec()).filter(
				func(strike: Dictionary) -> bool: return strike.steered)
		if not mine.is_empty() or not held.is_empty():
			held.append(root.get_texture().get_image())
	if held.size() == 6:
		for n in 6:
			held[n].save_png("res://build/issue355/%s-crowd%d-ko.png" % [OS.get_environment("TAG"), n])
			var crop := held[n].get_region(Rect2i(960 - 240, 540 - 150, 480, 270))
			crop.resize(960, 540, Image.INTERPOLATE_NEAREST)
			crop.save_png("res://build/issue355/%s-crowd-crop%d-ko.png" % [OS.get_environment("TAG"), n])
		print("crowd: saved six frames from frame %d" % frame)
		quit()
	if frame > 3000:
		print("crowd: the steered lead never struck")
		quit(2)
	return false
