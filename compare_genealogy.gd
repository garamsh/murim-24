# Renders the genealogy panel as main draws it and as this branch draws it, over the same world
# and person in each locale, and prints whether each pair of images is identical.
extends SceneTree

var _frame := 0
var _panels: Array[Control] = []
var _viewports: Array[SubViewport] = []
var _locale := 0


func _initialize() -> void:
	TranslationServer.set_locale("ko")
	var world := WorldAdapter.create_region(2026, 0)
	for i in 70 * 1800:
		world.tick()
	var person := -1
	for army in world.army_count():
		for p in world.roster(army):
			if person < 0 and world.person_spouse(p) >= 0 and world.person_children(p).size() > 0:
				person = p
	for path in ["res://hud/zz_genealogy_main.gd", "res://hud/genealogy.gd"]:
		var viewport := SubViewport.new()
		viewport.size = Vector2i(480, 270)
		viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		root.add_child(viewport)
		var panel: Control = load(path).new()
		panel.use_font(load("res://hud/fonts/Galmuri9.ttf"))
		viewport.add_child(panel)
		panel.show_person(world, person, person)
		_panels.append(panel)
		_viewports.append(viewport)


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame % 6 != 0:
		return false
	var images := _viewports.map(func(v: SubViewport) -> Image: return v.get_texture().get_image())
	print("compare_genealogy: %s main %s branch %s identical %s" % [TranslationServer.get_locale(),
		images[0].get_data().hex_encode().sha256_text().left(16), images[1].get_data().hex_encode().sha256_text().left(16),
		images[0].get_data() == images[1].get_data()])
	_locale += 1
	if _locale == 2:
		return true
	TranslationServer.set_locale("en")
	return false
