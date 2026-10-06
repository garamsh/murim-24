# Records play.tscn's genealogy panel for the PR's evidence. Before recording, the region lives
# DAYS days so that families form; the panel then opens on the first person with a spouse and a
# child, as G would on them, at frame OPEN; the selection moves down to a child and Enter centres
# the tree on it at RECENTRE; the locale switches to English at ENGLISH.
# Run: godot --path . --write-movie <dir>/genealogy.png --quit-after 120 --script <this file>
extends SceneTree

const DAYS := 50
const TICKS_PER_DAY := 1800
const OPEN := 10
const RECENTRE := 40
const ENGLISH := 70
var _frame := 0
var _play: Node


func _initialize() -> void:
	_play = load("res://play/play.tscn").instantiate()
	root.add_child(_play)


func _process(_delta: float) -> bool:
	_frame += 1
	var world: WorldAdapter = _play.world
	var panel: Control = _play.get_node("Hud").genealogy
	if _frame == 1:
		for i in DAYS * TICKS_PER_DAY:
			_play.step()
	if _frame == OPEN:
		var own: int = world.roster(_play.steered)[0]
		for army in world.army_count():
			for person in world.roster(army):
				if not panel.visible and world.person_spouse(person) >= 0 and world.person_children(person).size() > 0:
					panel.show_person(world, person, own)
	if _frame == RECENTRE:
		panel.move(Vector2i.DOWN)
		panel.centre_selected()
	if _frame == ENGLISH:
		_play.get_node("Hud").switch_locale()
	return false
