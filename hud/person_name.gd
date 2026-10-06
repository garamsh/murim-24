## A person's name as the HUD and the genealogy panel show it, from WorldAdapter.person_name.
extends RefCounted


## Returns `person`'s name in the locale's order: surname first in Korean, given name first in
## English. `short` gives the English given name alone, which fits a card; Korean names fit whole.
static func of(world: WorldAdapter, person: int, short := false) -> String:
	var name := world.person_name(person)
	if name.is_empty():
		return TranslationServer.translate("GENEALOGY_UNNAMED")
	if TranslationServer.get_locale().begins_with("ko"):
		return name.surname + name.given
	if short:
		return name.given_en
	return "%s %s" % [name.given_en, name.surname_en]
