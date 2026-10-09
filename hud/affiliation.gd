## A faction and a sect as the HUD shows them, from the names the core gives: their names in the
## current locale, and a faction's sprite set, by which its bodies, its accent and the text of its
## name are drawn. It holds no game rule.
extends RefCounted

## The factions' sprite sets under art/bodies/, in the order of Palette's accents.
const SPRITES := ["orthodox", "demonic", "unorthodox"]


## Returns faction `faction` of `world` named in the current locale.
static func faction_name(world: WorldAdapter, faction: int) -> String:
	return TranslationServer.translate("FACTION_" + world.faction_names()[faction].to_upper())


## Returns sect `sect` of `world` named in the current locale.
static func sect_name(world: WorldAdapter, sect: int) -> String:
	return TranslationServer.translate("SECT_" + world.sect_name(sect).to_upper().replace(" ", "_"))


## Returns the index in SPRITES of faction `faction` of `world`'s sprite set: the first for a
## faction out of range or with none of its own.
static func sprite(world: WorldAdapter, faction: int) -> int:
	var names := world.faction_names()
	return maxi(SPRITES.find(names[faction].to_lower()), 0) if faction >= 0 and faction < names.size() else 0
