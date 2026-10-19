# Checks the names place_labels.gd gives the places near the player (#185): in each locale, every
# village of seed 2026's region, anchored apart across a 1920×1080 room, is named inside it, a sect's
# seat on a plaque by its sect's name in its faction's text colour over the faction's name, any
# other village outlined by its kind's word and its number, none over another. Control: a place
# anchored past the room's edge is left out. Setup: the region holds seats and other villages.
extends RefCounted

const Palette := preload("res://hud/palette.gd")
const Affiliation := preload("res://hud/affiliation.gd")
const PlaceLabels := preload("res://hud/place_labels.gd")
const SEED := 2026
const ROOM := Vector2(1920, 1080)
# The anchors' spacing across the room, wide and tall enough for the longest label.
const STEP := Vector2(320, 120)


func run() -> bool:
	var world := WorldAdapter.create_region(SEED)
	var seats := {}
	for sect in world.sect_count():
		seats[world.sect_seat(sect)] = sect
	var count := world.villages().size()
	var labels := PlaceLabels.new()
	labels.size = ROOM
	var passed := not seats.is_empty() and count > seats.size()
	for locale: String in ["ko", "en"]:
		TranslationServer.set_locale(locale)
		var places: Array[Dictionary] = []
		for village in count:
			var column := village % 5
			var row := village / 5
			places.append({"village": village, "anchor": Vector2(160 + column * STEP.x, 160 + row * STEP.y)})
		# The control: anchored past the room's right edge.
		places.append({"village": 0, "anchor": Vector2(ROOM.x + 40, 160)})
		labels.show_places(world, places, [])
		var room := Rect2(Vector2.ZERO, ROOM)
		var named := labels.labels.map(func(label: Dictionary) -> int: return label.village)
		var styled := 0
		var overlaps := 0
		for i in labels.labels.size():
			var label: Dictionary = labels.labels[i]
			var village: int = label.village
			var right: bool
			if seats.has(village):
				var sect: int = seats[village]
				var faction := world.sect_faction(sect)
				right = label.seat and label.lines.size() == 2 and label.lines[0][0] == Affiliation.sect_name(world, sect) \
						and label.lines[0][1] == Palette.FACTION_TEXT[Affiliation.sprite(world, faction)] \
						and label.lines[1][0] == Affiliation.faction_name(world, faction)
			else:
				var kind: String = world.villages()[village].kind
				right = not label.seat and label.lines.size() == 1 \
						and label.lines[0][0] == tr("PLACE_" + kind.to_upper()) % (village + 1)
			styled += 1 if right and room.encloses(label.rect) else 0
			for other: Dictionary in labels.labels.slice(i + 1):
				overlaps += 1 if (other.rect as Rect2).intersects(label.rect) else 0
		var all_named := named.size() == count and range(count).all(func(v: int) -> bool: return named.has(v))
		var locale_passed: bool = all_named and styled == count and overlaps == 0
		print("place_labels_test: in %s %d of %d places named, %d of them in their style inside the room, %d overlapping; the one past the edge left out %s, as %s: %s" % [
			locale, named.size(), count, styled, overlaps, named.count(0) == 1,
			labels.labels.map(func(label: Dictionary) -> Array: return label.lines.map(func(line: Array) -> String: return line[0])),
			"pass" if locale_passed else "FAIL"])
		passed = passed and locale_passed
	TranslationServer.set_locale("ko")
	labels.free()
	return passed
