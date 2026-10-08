## A person's martial progress as the rebirth screen shows it, from
## WorldAdapter.person_attainment: the discipline of their highest attainment and that attainment.
## Talent stays hidden.
extends RefCounted

## What `of` shows for a person who has attained nothing.
const NONE := "—"


## Returns `person`'s discipline of highest attainment, the first among equals, and that
## attainment, in the current locale, or NONE while every attainment is 0.
static func of(world: WorldAdapter, person: int) -> String:
	var attained := world.person_attainment(person)
	var names := world.discipline_names()
	var best := -1
	for discipline in mini(attained.size(), names.size()):
		if attained[discipline] > 0 and (best < 0 or attained[discipline] > attained[best]):
			best = discipline
	if best < 0:
		return NONE
	return shown(names[best], attained[best])


## Returns discipline `name`, as the core names it, and `points` as `of` shows them.
static func shown(name: String, points: int) -> String:
	var key := "DISCIPLINE_" + name.to_upper().replace(" ", "_")
	return "%s %d" % [TranslationServer.translate(key), points]
