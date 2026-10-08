## The five skills the player has put on Q W E R T and on the touch arc's buttons, as
## art/mockup/arts_panel.png shows them. A slot holds a skill by what it is, not by where it stands
## among the person's: an army's skill by its index among its army's skills, an art skill by its art
## and its index in the art, so a slot keeps its skill as the person learns more, and holds none
## once the person no longer has it. It follows one person: another person, as after a rebirth,
## starts from the first SLOTS of its skills. It is the player's choice and holds no game rule; the
## core casts by the index skill() resolves it to. write() and read() keep it in a file of its own,
## which the play scene keeps beside the save.
extends RefCounted

const SLOTS := 5
const SECTION := "loadout"

## The person the slots are for, or -1 before any.
var person := -1
## Each slot's skill, as a Dictionary of its `art`, -1 for an army's skill, and its index `skill`
## among the art's skills or the army's, or an empty Dictionary for an empty slot.
var slots: Array[Dictionary] = []


func _init() -> void:
	slots.resize(SLOTS)
	slots.fill({})


## Returns what skill `index` of `skills`, as WorldAdapter.person_skills gives them, is: the form a
## slot holds it in.
static func identity(skills: Array, index: int) -> Dictionary:
	var skill: Dictionary = skills[index]
	if int(skill.art) < 0:
		return {"art": -1, "skill": index}
	return {"art": int(skill.art), "skill": int(skill.skill)}


## Follows `follower`, a person of `world`: keeps the slots while it is the person they are for,
## and otherwise fills them with its first SLOTS skills.
func follow(world: WorldAdapter, follower: int) -> void:
	if follower == person:
		return
	person = follower
	var skills := world.person_skills(follower)
	for slot in SLOTS:
		slots[slot] = identity(skills, slot) if slot < skills.size() else {}


## Returns the index among the person's skills, as cast() takes it, of the skill in `slot`, or -1
## for an empty slot or a skill the person no longer has.
func skill(world: WorldAdapter, slot: int) -> int:
	if person < 0 or slots[slot].is_empty():
		return -1
	var skills := world.person_skills(person)
	for index in skills.size():
		if identity(skills, index) == slots[slot]:
			return index
	return -1


## Returns the slot holding `held`, a skill as identity() gives it, or -1 for none.
func slot_of(held: Dictionary) -> int:
	return slots.find(held)


## Puts `held`, a skill as identity() gives it, in `slot`, taking it out of any other slot.
func assign(slot: int, held: Dictionary) -> void:
	var before := slot_of(held)
	if before >= 0:
		slots[before] = {}
	slots[slot] = held


## Empties the slot holding `held`, if one does.
func clear(held: Dictionary) -> void:
	var at := slot_of(held)
	if at >= 0:
		slots[at] = {}


## Writes the person and the slots to `path`. Returns whether it was written.
func write(path: String) -> bool:
	var file := ConfigFile.new()
	file.set_value(SECTION, "person", person)
	file.set_value(SECTION, "slots", slots)
	return file.save(path) == OK


## Reads the person and the slots from `path`, as write() wrote them, keeping the slots as they are
## where it holds none. Returns whether it was read.
func read(path: String) -> bool:
	var file := ConfigFile.new()
	if file.load(path) != OK:
		return false
	var kept: Variant = file.get_value(SECTION, "slots", null)
	if not kept is Array or (kept as Array).size() != SLOTS:
		return false
	for slot in SLOTS:
		var held: Variant = kept[slot]
		slots[slot] = held if held is Dictionary else {}
	person = int(file.get_value(SECTION, "person", -1))
	return true
