# Draws the genealogy and sect panels over seed 2026 at tick 0, opened on the player's army, in a
# 480 x 270 window in each locale, and prints each image's SHA-256, so that two checkouts of
# hud/panel.gd can be compared.
# Run: godot --path . --script <this file>
extends SceneTree

const LOCALES := ["ko", "en"]
var _frame := 0
var _layer := CanvasLayer.new()
var _world: WorldAdapter
var _panels: Array[Control] = []
var _shown := 0


func _initialize() -> void:
	_world = WorldAdapter.create_region(2026, 0)
	root.add_child(_layer)
	var army := _world.army_count() - 1
	var font: Font = load("res://hud/fonts/Galmuri9.ttf")
	var genealogy: Control = load("res://hud/genealogy.gd").new()
	var sect: Control = load("res://hud/sect.gd").new()
	for panel: Control in [genealogy, sect]:
		panel.use_font(font)
		_layer.add_child(panel)
		_panels.append(panel)
	var person: int = _world.roster(army)[0]
	genealogy.show_person(_world, person, person)
	sect.show_sect(_world, army)
	sect.visible = false


func _process(_delta: float) -> bool:
	_frame += 1
	if _frame % 5 != 0:
		return false
	var step := _frame / 5 - 1
	var locale: String = LOCALES[step / 2 % 2]
	var panel := _panels[step % 2]
	var image := root.get_texture().get_image()
	print("compare_panels: %s %s %s" % [["genealogy", "sect"][(step - 1) % 2] if step > 0 else "start",
			TranslationServer.get_locale(), image.get_data().hex_encode().sha256_text().left(16)])
	if step >= 4:
		return true
	TranslationServer.set_locale(locale)
	for each in _panels:
		each.visible = each == panel
	return false
