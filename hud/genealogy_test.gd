# Checks that the genealogy panel shows what the adapter reads: after the region has lived long
# enough for families to form, the panel on a parent shows that person's name, spouse and
# children, their family name, age and sect, and talent and lifespan as unknown; moving down and
# pressing Enter centres it on a child, whose parents then include that parent, and whose
# generation in the header is one more than its parents'. Control: a person the world began with
# has no parents, the panel shows none, and its header reads the first generation.
extends RefCounted

const SEED := 2026
# Days for marriages and births to come: the first births come within forty.
const DAYS := 50
const TICKS_PER_DAY := 1800


func run() -> bool:
	TranslationServer.set_locale("ko")
	var world := WorldAdapter.create_region(SEED, 0)
	for i in DAYS * TICKS_PER_DAY:
		world.tick()
	var parent := -1
	for army in world.army_count():
		for person in world.roster(army):
			if parent < 0 and world.person_spouse(person) >= 0 and world.person_children(person).size() > 0:
				parent = person
	# Setup: someone has married and had a child, so the panel has kin to show.
	if parent < 0:
		print("genealogy_test: no one has a child after %d days: FAIL" % DAYS)
		return false
	var panel: Control = preload("res://hud/genealogy.gd").new()
	panel.show_person(world, parent, parent)
	var name := world.person_name(parent)
	var spouse := world.person_spouse(parent)
	var children := world.person_children(parent)
	var shown := {}
	for card: Dictionary in panel.cards:
		if not shown.has(card.role):
			shown[card.role] = []
		shown[card.role].append(card.person)
	var centre_card: Dictionary = panel.cards.filter(func(card: Dictionary) -> bool: return card.role == "centre")[0]
	var kin_right: bool = centre_card.name == name.surname + name.given \
			and shown.get("spouse", []) == [spouse] \
			and shown.get("child", []) == Array(children.slice(0, 3)) \
			and shown.get("parent", []) == Array(world.person_parents(parent).slice(0, 2))
	var details_right: bool = panel.details.family == "%s씨 가문" % name.surname \
			and panel.details.age == str(world.person_age(parent)) \
			and panel.details.talent == "알 수 없음" and panel.details.lifespan == "알 수 없음" \
			and panel.details.children == "%d명" % children.size() \
			and panel.details.sect == tr("FACTION_" + world.faction_names()[world.person_faction(parent)].to_upper())

	var moved: bool = panel.move(Vector2i.DOWN)
	var child: int = panel.selected
	panel.centre_selected()
	var child_generation: String = panel.details.generation
	var recentred: bool = moved and children.has(child) and panel.centre == child \
			and panel.cards.any(func(card: Dictionary) -> bool: return card.role == "parent" and card.person == parent)

	# Control: the person the world began with, steered, has no parents to show.
	var founder: int = world.roster(world.army_count() - 1)[0]
	panel.show_person(world, founder, founder)
	var founder_parents: int = panel.cards.filter(func(card: Dictionary) -> bool: return card.role == "parent").size()
	var control := world.person_parents(founder).is_empty() and founder_parents == 0
	# The header's generation: the founder's first, the child's one more than its parents'.
	var founder_generation: String = panel.details.generation
	# Setup: fifty days in, the child's parents are people the world began with, so it is the
	# second generation.
	var parents_founders := world.person_parents(child).size() > 0
	for p in world.person_parents(child):
		parents_founders = parents_founders and world.person_parents(p).is_empty()
	var generations_right: bool = founder_generation == "1대" and parents_founders \
			and child_generation == "2대"
	var passed := kin_right and details_right and recentred and control and generations_right
	print("genealogy_test: after %d days person %d %s%s, spouse %d, children %s; cards %s; details %s; down and Enter centre on child %d %s; the founder %d shows %d parents; generations: founder %s, child %s: %s" % [
		DAYS, parent, name.surname, name.given, spouse, children, shown, panel.details if not details_right else "match",
		child, recentred, founder, founder_parents, founder_generation, child_generation,
		"pass" if passed else "FAIL"])
	panel.free()
	return passed
