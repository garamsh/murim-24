# Checks that a world driven through the adapter reaches the core's own hash
# and people's positions and health for the same scenario and tick count,
# the core's coming from the world_hash reference program; that the adapter
# refuses what the core, or the conversion to the core's types, refuses; that
# a body given orders through the adapter moves and strikes; that a region's
# villages keep their residents' own skills off their structures; and that a
# region's people are read back with their names and kin; that a region's
# villages are read back with the cap, stock and population the core derives;
# and that a region's sects and manuals are read back as the core holds them; and that the player
# applies to a sect, hears rumours and shows attainment through the adapter as the core holds them;
# and that a snapshot reads in one call what the per-army, per-structure and per-strike reads give;
# and that the date reads the tick within the day and the day's length.
extends RefCounted

const SEED := 42
const OTHER_SEED := 43
const TICKS := 2000
const REFERENCE := "res://build/extension/world_hash"
const MAP_ROWS: Array[String] = [
	"........#.......",
	"........#.......",
	"........#.......",
	"........#.......",
	"........#.......",
	"........#.......",
	"........#.......",
	"........#.......",
	"................",
]
const STRIKE := {"range": 512, "radius": 256, "body_damage": 1, "structure_damage": 4, "cooldown": 24}
const BLAST := {"range": 768, "radius": 512, "body_damage": 2, "structure_damage": 9, "cooldown": 64}
# Twelve people of 100 health from (0, 0) and ten of 101 from (12, 6), in rows of four, each army
# bound for the other's corner.
var _armies: Array[Dictionary] = [
	{"objective": Vector2i(14, 7), "members": _members(Vector2i(0, 0), 12, 100), "faction": 0,
		"skills": [STRIKE, BLAST]},
	{"objective": Vector2i(1, 1), "members": _members(Vector2i(12, 6), 10, 101), "faction": 1,
		"skills": [STRIKE]},
]
# Beside the wall's only gap, at (8, 8), and clear of the cells leading into it, since a structure
# blocks its cell.
const STRUCTURES: Array[Dictionary] = [
	{"cell": Vector2i(7, 7), "durability": 100},
	{"cell": Vector2i(9, 7), "durability": 60},
]
const RULES := {"separation": 256, "hostile": [[false, true], [true, false]], "stage_floors": [75, 25, 1]}
# One person at home and ten hostile people holding their camp, who strike nothing back.
const ORDER_TICKS := 800
# The region the play scene opens on, its first Cult village, and how long it is watched.
const REGION_SEED := 2026
const CULT_VILLAGE := 1
# The region's first Unorthodox village.
const UNORTHODOX_VILLAGE := 2
const VILLAGE_TICKS := 600
# Years the region is lived, in five-tick days, before its realms, arts and rumours are read.
const LIVED_YEARS := 10
# Ticks in a day of the region's calendar.
const TICKS_PER_DAY := 1800
const HOME := Vector2i(1, 1)
const CAMP := Vector2i(12, 7)
const BESIDE_CAMP := Vector2i(12, 5)
const HIT := {"range": 768, "radius": 0, "body_damage": 200, "structure_damage": 0, "cooldown": 10}
const IDLE := {"range": 0, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 0}
var _steered_armies: Array[Dictionary] = [
	{"objective": HOME, "members": _members(HOME, 1, 1000), "faction": 0, "skills": [HIT]},
	{"objective": CAMP, "members": _members(Vector2i(11, 6), 10, 100), "faction": 1, "skills": [IDLE]},
]


func run() -> bool:
	var core := _core_run()
	var adapter := _adapter_run(SEED)
	# Control: another seed must differ, or equality could come from a constant hash.
	var other_seed := _adapter_run(OTHER_SEED)
	var hashes_match := core.size() > 0 and adapter[0] == core[0] and other_seed[0] != core[0]
	print("world_adapter_test: %d ticks, %d armies, %d structures; core hash %s, adapter hash %s for seed %d, %s for seed %d: %s" % [
		TICKS, _armies.size(), STRUCTURES.size(), core[0] if core.size() > 0 else "none", adapter[0], SEED,
		other_seed[0], OTHER_SEED, "pass" if hashes_match else "FAIL"])

	# Setup: someone died, so the comparison covers the rosters shrinking. Control: another seed's
	# people differ, so equality is not a constant.
	var bodies := adapter.slice(1)
	var fell := bodies.size() < 22
	var bodies_match := fell and core.slice(1) == bodies and other_seed.slice(1) != core.slice(1)
	print("world_adapter_test: %d people through the adapter, %d from the core, %d for seed %d; positions and health equal: %s" % [
		bodies.size(), core.size() - 1, other_seed.size() - 1, OTHER_SEED, "pass" if bodies_match else "FAIL"])

	var strike_without_cooldown := STRIKE.duplicate()
	strike_without_cooldown.erase("cooldown")
	# Refused by the core, then by the conversion to the core's types.
	var invalid: Array[Array] = [
		[_with_first_army("members", [{"cell": Vector2i(8, 3), "health": 100}]), RULES],
		[_with_first_army("skills", []), RULES],
		[_with_first_army("members", [{"cell": Vector2i(1, 1), "health": 0}]), RULES],
		[_with_first_army("members", [{"cell": Vector2i(1, 1), "health": -1}]), RULES],
		[_with_first_army("members", [{"cell": Vector2i(1, 1)}]), RULES],
		[_with_first_army("skills", [strike_without_cooldown]), RULES],
		[_armies, _with_rules("stage_floors", [75, 25])],
	]
	var refused := 0
	for scenario in invalid:
		if _create(SEED, scenario[0], scenario[1]) == null:
			refused += 1
	# Control: the valid scenario is accepted, so a null above is the refusal under test.
	var accepted := _create(SEED, _armies, RULES) != null
	print("world_adapter_test: valid scenario accepted %s; %d of %d invalid scenarios refused: %s" % [
		accepted, refused, invalid.size(), "pass" if accepted and refused == invalid.size() else "FAIL"])
	# A region's day is as many ticks as it is given: five ticks bring the next day. A day of no
	# ticks, or fewer, is refused; control: the five-tick day is accepted.
	var short := WorldAdapter.create_region(SEED, 0, 5)
	var first_day: int = short.date().day if short != null else -1
	for i in 5 if short != null else 0:
		short.tick()
	var days_pass: bool = short != null and short.date().day == first_day + 1 \
			and WorldAdapter.create_region(SEED, 0, 0) == null and WorldAdapter.create_region(SEED, 0, -1) == null
	print("world_adapter_test: a region of five-tick days accepted %s, on day %d after five ticks from day %d; days of 0 and -1 ticks refused: %s" % [
		short != null, short.date().day if short != null else -1, first_day, "pass" if days_pass else "FAIL"])
	# A village index the region lacks, one past its last or below 0, is refused before anyone is born;
	# control: its last village is accepted.
	var count: int = short.villages().size() if short != null else 0
	var homes_pass: bool = count > 0 and WorldAdapter.create_region(SEED, count) == null \
			and WorldAdapter.create_region(SEED, -1) == null and WorldAdapter.create_region(SEED, count - 1) != null
	print("world_adapter_test: villages %d and -1 of %d refused, village %d accepted: %s" % [
		count, count, count - 1, "pass" if homes_pass else "FAIL"])
	# The date gives the tick within the day and the day's length: a region of five-tick days reads
	# 5, its tick 0 at a day's start, 3 three ticks on and 0 again as the next day starts. Control: a
	# region of the core's default day reads its own length, not 5.
	var timed := WorldAdapter.create_region(SEED, 0, 5)
	var ticks_seen: Array[int] = [timed.date().tick]
	for i in 3:
		timed.tick()
	ticks_seen.append(timed.date().tick)
	for i in 2:
		timed.tick()
	ticks_seen.append(timed.date().tick)
	var default_day: int = WorldAdapter.create_region(SEED, 0).date().ticks_per_day
	var time_pass: bool = timed.date().ticks_per_day == 5 and ticks_seen == [0, 3, 0] and default_day != 5 \
			and default_day > 0
	print("world_adapter_test: five-tick days read %d ticks a day, the tick %s over five ticks; the default day reads %d: %s" % [
		timed.date().ticks_per_day, ticks_seen, default_day, "pass" if time_pass else "FAIL"])
	var orders_pass := _orders_pass()
	var reads_pass := _reads_pass()
	var villages_pass := _villages_pass()
	var kin_pass := _kin_pass()
	var economy_pass := _economy_pass()
	var sects_pass := _sects_pass()
	# Lived once for the three checks that read it, none of which ticks it further.
	var lived := WorldAdapter.create_region(REGION_SEED, 0, 5)
	for i in LIVED_YEARS * 120 * 5:
		lived.tick()
	var play_loop_pass := _play_loop_pass(lived)
	var qi_pass := _qi_pass(lived)
	var lessons_pass := _lessons_pass(lived)
	var event_people_pass := _event_people_pass()
	var snapshot_pass := _snapshot_pass()
	return snapshot_pass and hashes_match and bodies_match and accepted and refused == invalid.size() and orders_pass \
			and reads_pass and villages_pass and kin_pass and economy_pass and days_pass and homes_pass and time_pass and sects_pass \
			and play_loop_pass and qi_pass and lessons_pass and event_people_pass


# On a tick of the M1 battle with uses of skills recorded and someone dead, a snapshot holds every
# army's roster, positions, health and ready ticks, its faction, every structure's stage, and every
# use with whom it struck, as the reads one army, structure or use at a time give them. Setup: the
# tick recorded uses and the rosters shrank. Control: a snapshot of another tick differs.
func _snapshot_pass() -> bool:
	var world := WorldAdapter.create_m1()
	var start := world.snapshot()
	var ticks := 0
	while ticks < TICKS and (world.strikes().is_empty() or world.snapshot().person.size() == start.person.size()):
		world.tick()
		ticks += 1
	var snapshot := world.snapshot()
	var setup: bool = not world.strikes().is_empty() and snapshot.person.size() < start.person.size()
	var first := PackedInt64Array([0])
	var persons := PackedInt64Array()
	var positions := PackedVector2Array()
	var healths := PackedInt64Array()
	var ready := PackedInt64Array()
	var factions := PackedInt64Array()
	for army in world.army_count():
		persons.append_array(world.roster(army))
		positions.append_array(world.body_positions(army))
		healths.append_array(world.body_strengths(army))
		ready.append_array(world.body_ready(army))
		factions.append(world.army_faction(army))
		first.append(persons.size())
	var stages := PackedInt64Array()
	for structure in world.structure_count():
		stages.append(world.structure_stage(structure))
	var strikes_match := true
	var strikes := world.strikes()
	for i in strikes.size():
		var strike: Dictionary = strikes[i]
		strikes_match = strikes_match and snapshot.strike_person[i] == strike.person \
				and snapshot.strike_shape[i] == strike.shape and snapshot.strike_range[i] == strike.range \
				and snapshot.strike_radius[i] == strike.radius and snapshot.strike_body_damage[i] == strike.body_damage \
				and snapshot.strike_origin[i] == strike.origin and snapshot.strike_toward[i] == strike.toward \
				and snapshot.strike_end[i] == strike.end \
				and snapshot.struck.slice(snapshot.strike_first[i], snapshot.strike_first[i + 1]) == strike.struck
	var reads_match: bool = snapshot.first == first and snapshot.person == persons and snapshot.position == positions \
			and snapshot.health == healths and snapshot.ready == ready and snapshot.faction == factions \
			and snapshot.stage == stages and snapshot.strike_person.size() == strikes.size() and strikes_match
	world.tick()
	var later := world.snapshot()
	var passed: bool = setup and reads_match and (later.position != snapshot.position or later.health != snapshot.health)
	print("world_adapter_test: after %d ticks of the M1 battle, %d uses and %d of %d people living, the snapshot reads as the per-army, per-structure and per-use reads %s; a tick later it differs %s: %s" % [
		ticks, strikes.size(), snapshot.person.size(), start.person.size(), reads_match,
		later.position != snapshot.position or later.health != snapshot.health, "pass" if passed else "FAIL"])
	return passed


# `lived`, a region lived ten years of five-tick days, reads its realms, and a person who knows an
# art reads its qi within its store, a realm among them, its nature, and its skills: its army's
# first, priced at no qi, then its art skills, named, with their cost and nature. Control: a world
# from create() has no realms, and its people hold no qi, know no art and read only their army's
# skills.
func _qi_pass(lived: WorldAdapter) -> bool:
	var realms := lived.realms()
	var learned := -1
	for army in lived.army_count():
		for person: int in lived.roster(army):
			if learned == -1 and lived.person_skills(person).size() > lived.skills(army).size():
				learned = person
	var right: bool = realms.size() == 8 and realms[0].korean == "삼류" and learned != -1
	var skills: Array[Dictionary] = []
	if learned != -1:
		skills = lived.person_skills(learned)
		var nature := lived.person_nature(learned)
		right = right and lived.person_qi(learned) <= lived.person_qi_capacity(learned) \
				and lived.person_qi_capacity(learned) > 0 \
				and lived.person_realm(learned) >= 1 and lived.person_realm(learned) <= realms.size() \
				and nature.has("orientation") and nature.has("polarity") \
				and skills[0].art == -1 and skills[0].qi_cost == 0 and skills.back().art >= 0 \
				and skills.back().qi_cost > 0 and not String(skills.back().korean).is_empty() \
				and skills.back().orientation == nature.orientation
	var bare := _create(SEED, _armies, RULES)
	var bare_right: bool = bare.realms().is_empty() and bare.person_qi(0) == 0 and bare.person_realm(0) == 0 \
			and bare.person_nature(0).is_empty() and bare.person_skills(0).size() == bare.skills(0).size() \
			and WorldAdapter.CAST_NO_QI != WorldAdapter.CAST_ON_COOLDOWN
	var passed: bool = right and bare_right
	print("world_adapter_test: %d realms, the first %s; person %d knows %d skills, the last %s, qi %d of %d, realm %d, nature %s; a world from create() reads none %s: %s" % [
		realms.size(), realms[0] if realms.size() > 0 else {}, learned, skills.size(), skills.back() if skills.size() > 0 else {},
		lived.person_qi(learned) if learned != -1 else -1, lived.person_qi_capacity(learned) if learned != -1 else -1,
		lived.person_realm(learned) if learned != -1 else -1, lived.person_nature(learned) if learned != -1 else {},
		bare_right, "pass" if passed else "FAIL"])
	return passed


# A region lived 3,200 one-tick days records births and deaths among its events, each naming its
# person: one born is no older than the days lived, and one who died has no health. Control: the
# region's other events name no person.
func _event_people_pass() -> bool:
	const DAYS := 3200
	var lived := WorldAdapter.create_region(REGION_SEED, 0, 1)
	for i in DAYS:
		lived.tick()
	var born := 0
	var died := 0
	var others := 0
	var right := true
	for event: Dictionary in lived.events(0):
		if event.kind == WorldAdapter.EVENT_BORN:
			born += 1
			right = right and lived.person_age(event.person) <= DAYS
		elif event.kind == WorldAdapter.EVENT_DIED:
			died += 1
			right = right and lived.person_health(event.person) == 0
		else:
			others += 1
			right = right and not event.has("person")
	var passed := right and born > 0 and died > 0 and others > 0
	print("world_adapter_test: of a region's last %d events, %d births and %d deaths name their person and %d others none %s: %s" % [
		born + died + others, born, died, others, right, "pass" if passed else "FAIL"])
	return passed


# `lived`, a region lived ten years of five-tick days, reads its arts, each named with its skills,
# and a person's art skill names its index among its art's skills, the skill arts() names alike. Its
# progress gives each skill learned with its person, art, skill and how it was learned, and each
# realm gained with its person and a rank the person has reached, while none of its events names an
# art. A person's heart art is the art of its first art skill, which agrees with it fully, some art
# it does not know agreeing less, and a skill lies within its realm where the skill's rank does not
# pass the person's. Control: a person knowing no art has no heart art. Control: a world from
# create() has no arts.
func _lessons_pass(lived: WorldAdapter) -> bool:
	var arts := lived.arts()
	var right: bool = arts.size() == 3 and arts[0].korean == "매화검법" and arts[0].romanised == "Plum Blossom Sword" \
			and arts[0].skills.size() == 3 and arts[0].skills[1].korean == "이십사수매화검법" and arts[0].skills[1].rank == 3 \
			and arts[0].skills[1].qi_cost == 400 and arts[0].skills[1].cooldown == 300
	var named := 0
	for army in lived.army_count():
		for person: int in lived.roster(army):
			for skill: Dictionary in lived.person_skills(person):
				if skill.art >= 0:
					right = right and arts[skill.art].skills[skill.skill].korean == skill.korean
					named += 1
	var learned := 0
	var gained := 0
	for event: Dictionary in lived.progress(0):
		if event.kind == WorldAdapter.PROGRESS_LEARNED:
			learned += 1
			right = right and lived.person_qi_capacity(event.person) > 0 and event.art < arts.size() \
					and event.skill < arts[event.art].skills.size() \
					and [WorldAdapter.LEARNED_BY_COMPREHENSION, WorldAdapter.LEARNED_BY_MANUAL,
						WorldAdapter.LEARNED_BY_TEACHING].has(event.by)
		elif event.kind == WorldAdapter.PROGRESS_REALM_GAINED:
			gained += 1
			right = right and lived.person_realm(event.person) >= event.rank and event.rank >= 1
	right = right and named > 0 and learned > 0 and gained > 0
	var hearts := 0
	var eased := 0
	var artless := 0
	for army in lived.army_count():
		for person: int in lived.roster(army):
			var first := -1
			for skill: Dictionary in lived.person_skills(person):
				if skill.art >= 0 and first < 0:
					first = skill.art
			right = right and lived.heart_art(person) == first
			if first < 0:
				artless += 1
				continue
			hearts += 1
			right = right and lived.art_agreement(person, first) == WorldAdapter.PER_MILLE
			for art in arts.size():
				eased += 1 if lived.art_agreement(person, art) < WorldAdapter.PER_MILLE else 0
				for skill in arts[art].skills.size():
					right = right and lived.within_realm(person, art, skill) \
							== (int(arts[art].skills[skill].rank) <= lived.person_realm(person))
	right = right and hearts > 0 and eased > 0 and artless > 0
	var bare_right := _create(SEED, _armies, RULES).arts().is_empty() \
			and lived.events(0).all(func(event: Dictionary) -> bool: return not event.has("art"))
	var passed := right and bare_right
	print("world_adapter_test: %d arts, %d art skills known named alike, %d skills learned and %d realms gained in its progress, none among its events; %d with a heart art, %d arts eased, %d knowing none; a world from create() has no arts %s: %s" % [
		arts.size(), named, learned, gained, hearts, eased, artless, bare_right, "pass" if passed else "FAIL"])
	return passed


# Each village of a region starts with people, under its cap, and food in store; a day later its stock has fed every one of them a measure. Control: the stock read
# changes by exactly the population, so it is not a constant.
func _economy_pass() -> bool:
	var region := WorldAdapter.create_region(REGION_SEED)
	var villages := region.villages()
	var starts: Array = []
	var right := villages.size() > 0
	for village in villages.size():
		var population := region.village_population(village)
		right = right and population > 0 and population < region.village_cap(village) \
				and region.village_stock(village) > 0
		starts.append([region.village_stock(village), population])
	for i in TICKS_PER_DAY:
		region.tick()
	var fed := true
	for village in villages.size():
		fed = fed and region.village_stock(village) == starts[village][0] - starts[village][1]
	var passed: bool = right and fed
	print("world_adapter_test: %d villages, first with %d people under a cap of %d and %d food, %d a day later: %s" % [
		villages.size(), region.village_population(0), region.village_cap(0), starts[0][0],
		region.village_stock(0), "pass" if passed else "FAIL"])
	return passed


# Each sect of a region is read back as the core holds it: a name, its faction, a seat that is a
# village of the seat template held by that faction, its founders as members with an elder among
# them as master, a cap from its seat's pupils, standing, and its manual held. The player's body,
# born into an ordinary village, belongs to no sect. Only the Orthodox elect their leader, and only
# the Demonic pass it by duel. Each sect's gate is a seat gate standing in its seat. Control: a
# world made by create() has no sects and no manuals, and its factions elect no one and duel for
# no lead.
func _sects_pass() -> bool:
	var region := WorldAdapter.create_region(REGION_SEED)
	var villages := region.villages()
	var right := region.sect_count() == region.faction_names().size()
	for sect in region.sect_count():
		var seat := region.sect_seat(sect)
		var members := region.sect_members(sect)
		var master := region.sect_master(sect)
		right = right and not region.sect_name(sect).is_empty() and region.sect_faction(sect) == sect \
				and villages[seat].kind == "seat" and region.sect_seat_held(sect) \
				and members.size() == 2 and region.sect_elders(sect).has(master) \
				and region.person_sect(master) == sect and region.sect_cap(sect) > members.size() \
				and region.sect_stands(sect)
		var gate := region.sect_gate(sect)
		right = right and gate >= 0 and region.structure_kind(gate) == "seat_gate" \
				and Rect2i(villages[seat].corner, villages[seat].area).has_point(region.structure_cell(gate))
	var manuals := region.manuals()
	var held := manuals.size() == region.sect_count()
	for manual: Dictionary in manuals:
		held = held and manual.place == "sect"
	var player: int = region.roster(region.army_count() - 1)[0]
	var plain := WorldAdapter.create(SEED, PackedStringArray(MAP_ROWS), _armies, STRUCTURES, RULES)
	var electing: Array[String] = []
	for faction in region.faction_names().size():
		if region.elects(faction):
			electing.append(region.faction_names()[faction])
	var dueling: Array[String] = []
	for faction in region.faction_names().size():
		if region.passes_by_duel(faction):
			dueling.append(region.faction_names()[faction])
	var plain_electing := 0
	for faction in plain.faction_count():
		plain_electing += 1 if plain.elects(faction) or plain.passes_by_duel(faction) else 0
	var none: bool = plain.sect_count() == 0 and plain.manuals().is_empty() and plain_electing == 0 \
			and plain.faction_count() > 0
	var passed: bool = right and held and region.person_sect(player) == -1 and none \
			and electing == ["Orthodox"] and dueling == ["Demonic"]
	print("world_adapter_test: %d sects, first %s of faction %d seated in village %d with gate %d, members %s, elders %s, master %d, cap %d; manuals %s; player's sect %d; electing factions %s, dueling %s; a world from create has %d sects and %d of %d factions electing or dueling: %s" % [
		region.sect_count(), region.sect_name(0), region.sect_faction(0), region.sect_seat(0),
		region.sect_gate(0), region.sect_members(0), region.sect_elders(0), region.sect_master(0), region.sect_cap(0),
		manuals, region.person_sect(player), electing, dueling, plain.sect_count(), plain_electing,
		plain.faction_count(), "pass" if passed else "FAIL"])
	return passed


# The player's body in a region has a surname and a given name in Korean and romanised, the family
# of its own army, and, as the world begins, no parents, spouse or children. Every person in the
# region bears a surname and a given name from names(). Control: a world made by create() names no
# one, and its names() are empty.
func _kin_pass() -> bool:
	var region := WorldAdapter.create_region(REGION_SEED)
	var army := region.army_count() - 1
	var body: int = region.roster(army)[0]
	var name := region.person_name(body)
	var named := true
	for key in ["surname", "given", "surname_en", "given_en"]:
		named = named and name.has(key) and not String(name[key]).is_empty()
	var kin := region.person_family(body) == army and region.person_spouse(body) == -1 \
			and region.person_parents(body).is_empty() and region.person_children(body).is_empty()
	var created := _create(SEED, _armies, RULES)
	var unnamed := created.person_name(0).is_empty()
	var table := region.names()
	var surnames := {}
	var givens := {}
	for entry: Dictionary in table.surnames:
		surnames[[entry.korean, entry.romanised]] = true
	for entry: Dictionary in table.female + table.male:
		givens[[entry.korean, entry.romanised]] = true
	# Setup: the region has people, and the table has names.
	var people := 0
	var from_table := not surnames.is_empty() and not givens.is_empty()
	for army_index in region.army_count():
		for person in region.roster(army_index):
			var person_name := region.person_name(person)
			people += 1
			from_table = from_table and surnames.has([person_name.surname, person_name.surname_en]) \
					and givens.has([person_name.given, person_name.given_en])
	var empty_table: Dictionary = created.names()
	var no_names: bool = empty_table.surnames.is_empty() and empty_table.female.is_empty() \
			and empty_table.male.is_empty()
	var passed: bool = named and kin and unnamed and people > 0 and from_table and no_names
	print("world_adapter_test: the player's body is %s %s (%s %s) of family %d, spouse %d, %d parents, %d children; names() holds %d surnames and %d given names, every one of %d people's among them %s; a created world names no one %s, its names() empty %s: %s" % [
		name.get("surname", ""), name.get("given", ""), name.get("surname_en", ""), name.get("given_en", ""),
		region.person_family(body), region.person_spouse(body), region.person_parents(body).size(),
		region.person_children(body).size(), surnames.size(), givens.size(), people, from_table, unnamed, no_names,
		"pass" if passed else "FAIL"])
	return passed


# The region create_region() makes, the player born into the first Cult village, keeps every
# structure intact while nobody raids it: the Cult's torch, used where its residents stand, spares
# their own village. Control: the same torch used beside a structure that no village holds breaks
# it, so the region's structures stand because their villages reach the core.
func _villages_pass() -> bool:
	var region := WorldAdapter.create_region(REGION_SEED, CULT_VILLAGE)
	var torch: Dictionary = region.skills(region.army_count() - 1)[1]
	for i in VILLAGE_TICKS:
		region.tick()
	var intact := 0
	for structure in region.structure_count():
		if region.structure_stage(structure) == WorldAdapter.STAGE_INTACT:
			intact += 1
	var bare := WorldAdapter.create(SEED, PackedStringArray(MAP_ROWS), [
		{"objective": Vector2i(6, 7), "members": _members(Vector2i(6, 7), 1, 100), "faction": 0,
			"skills": [torch]}], [{"cell": Vector2i(7, 7), "durability": 10000}],
		{"separation": 256, "hostile": [[false]], "stage_floors": [10000, 5000, 1]})
	for i in VILLAGE_TICKS:
		bare.tick()
	var broken := bare.structure_stage(0) != WorldAdapter.STAGE_INTACT
	var faction: int = region.villages()[CULT_VILLAGE].faction
	var passed: bool = region.structure_count() > 0 and intact == region.structure_count() \
			and torch.structure_damage > 0 and broken
	print("world_adapter_test: born into village %d of faction %d, after %d ticks %d of %d region structures intact; the torch beside a structure no village holds breaks it %s: %s" % [
		CULT_VILLAGE, faction, VILLAGE_TICKS, intact, region.structure_count(), broken,
		"pass" if passed else "FAIL"])
	return passed


# What the view reads: each army's faction and health, its people's health, each structure's
# footprint, and for a region its terrain, territory and villages, which agree with the map's
# blocked cells, the person born into the first village and the structures inside the villages. The
# territory is the core's: each village's centre lies in its owner's, and a cell no village claims
# lies in none. A world made by create() has no region to read, and its structures are one cell.
func _reads_pass() -> bool:
	var world := _create(SEED, _armies, RULES)
	var strength := 0
	for body_strength in world.body_strengths(0):
		strength += body_strength
	var armies_read := world.army_faction(0) == 0 and world.army_faction(1) == 1 \
			and world.army_strength(0) == 1200 and world.army_strength(1) == 1010 and strength == 1200
	var no_region := world.terrain().is_empty() and world.territory().is_empty() \
			and world.villages().is_empty() and world.terrain_names().is_empty() \
			and world.faction_names().is_empty()
	var region := WorldAdapter.create_region(SEED)
	var rows := region.map_rows()
	var names := region.terrain_names()
	var terrain := region.terrain()
	var territory := region.territory()
	var cells := rows.size() * rows[0].length()
	# Blocked cells are exactly the cells of the kinds bodies cannot pass.
	var agrees := terrain.size() == cells and territory.size() == cells
	for i in terrain.size() if agrees else 0:
		var blocked := rows[i / rows[0].length()][i % rows[0].length()] == "#"
		agrees = agrees and blocked == (names[terrain[i]] in ["water", "mountain"]) \
				and territory[i] >= -1 and territory[i] < region.faction_names().size()
	var villages := region.villages()
	var claimed := territory.has(-1)
	for village: Dictionary in villages:
		var centre: Vector2i = village.corner + village.area / 2
		claimed = claimed and territory[centre.y * rows[0].length() + centre.x] == village.faction
	var born_faction := region.army_faction(region.army_count() - 1)
	# Six to eight villages, and a seat for each sect besides.
	var farms := villages.filter(func(village: Dictionary) -> bool: return village.kind != "seat").size()
	var villages_read := farms >= 6 and farms <= 8 and villages.size() == farms + region.sect_count() \
			and born_faction == int(villages[0].faction)
	# Every region structure lies inside a village's area, and some span several cells.
	var footprints := {}
	var inside := 0
	for structure in region.structure_count():
		var rect := Rect2i(region.structure_cell(structure), region.structure_footprint(structure))
		footprints[rect.size] = true
		for village: Dictionary in villages:
			if Rect2i(village.corner, village.area).encloses(rect):
				inside += 1
				break
	var footprints_read := world.structure_footprint(0) == Vector2i.ONE \
			and inside == region.structure_count() and footprints.has(Vector2i(3, 3))
	# Kinds: none from create(), the template's for a region, barrels for M1's barricade.
	var kinds := {}
	for structure in region.structure_count():
		kinds[region.structure_kind(structure)] = true
	var m1 := WorldAdapter.create_m1()
	var kinds_read: bool = world.structure_kind(0) == "" and m1.structure_kind(0) == "barrels" \
			and kinds.has("house") and kinds.has("barrels") and not kinds.has("") \
			and villages[0].kind in ["hamlet", "village"]
	# Skills come back as create() took them, a shape left out as an area; control: the second
	# army's differ. A shape given comes back, and one naming no shape is refused.
	var arc_armies := _armies.duplicate(true)
	arc_armies[0].skills[0].shape = WorldAdapter.SHAPE_ARC
	var arc := _create(SEED, arc_armies, RULES)
	var unshaped := _armies.duplicate(true)
	unshaped[0].skills[0].shape = WorldAdapter.SHAPE_TARGETED + 1
	var skills_read: bool = world.skills(0) == _shaped(_armies[0].skills) \
			and world.skills(1) == _shaped(_armies[1].skills) and world.skills(0) != world.skills(1) \
			and arc != null and arc.skills(0) == _shaped(arc_armies[0].skills) \
			and arc.skills(0)[0].shape == WorldAdapter.SHAPE_ARC and _create(SEED, unshaped, RULES) == null
	var passed: bool = armies_read and no_region and agrees and claimed and villages_read \
			and footprints_read and kinds_read and skills_read
	print("world_adapter_test: factions %d, %d; strengths %d, %d, bodies %d; no region from create %s; region of %d cells: terrain %s agrees with blocked cells %s, each village's centre in its owner's territory and some cell in none %s, %d villages, person's faction %d, first village's %d; created footprint %s, region footprints %s, %d of %d structures inside a village: %s" % [
		world.army_faction(0), world.army_faction(1), world.army_strength(0), world.army_strength(1),
		strength, no_region, cells, names, agrees, claimed, villages.size(), born_faction,
		int(villages[0].faction) if villages.size() > 0 else -1, world.structure_footprint(0),
		footprints.keys(), inside, region.structure_count(), "pass" if passed else "FAIL"])
	print("world_adapter_test: structure kinds: created %s, M1 %s, region %s, first village %s; skills read back %s: %s" % [
		"\"%s\"" % world.structure_kind(0), m1.structure_kind(0), kinds.keys(),
		villages[0].kind if villages.size() > 0 else "none", skills_read,
		"pass" if kinds_read and skills_read else "FAIL"])
	return passed


# A person ordered beside the camp leaves home and strikes the camp's people down. Control: left
# without an order it stays home and the camp keeps everyone. An order naming a skill the person
# lacks is refused, while the valid one was accepted.
func _orders_pass() -> bool:
	var ordered := _steered_run(true)
	var left_alone := _steered_run(false)
	var refused := not _steered_world().order(0, BESIDE_CAMP, PackedInt64Array([1]))
	var passed: bool = ordered.accepted and ordered.moved and ordered.camp < 10 \
			and not left_alone.moved and left_alone.camp == 10 and refused
	print("world_adapter_test: ordered person accepted %s, moved %s, camp left %d of 10; unordered moved %s, camp left %d; order for a missing skill refused %s: %s" % [
		ordered.accepted, ordered.moved, ordered.camp, left_alone.moved, left_alone.camp, refused,
		"pass" if passed else "FAIL"])
	return passed


func _steered_world() -> WorldAdapter:
	return WorldAdapter.create(SEED, PackedStringArray(MAP_ROWS), _steered_armies, [], RULES)


# Runs ORDER_TICKS ticks, after ordering the person beside the camp when `give_order` is set.
func _steered_run(give_order: bool) -> Dictionary:
	var world := _steered_world()
	var accepted := give_order and world.order(0, BESIDE_CAMP, PackedInt64Array([0]))
	var home := world.body_positions(0)[0]
	for i in ORDER_TICKS:
		world.tick()
	var camp := world.body_positions(1).size()
	return {"accepted": accepted, "moved": world.body_positions(0)[0] != home, "camp": camp}


# `skills` as skills() gives them back: each with its shape, an area where it was left out.
func _shaped(skills: Array) -> Array:
	var shaped := []
	for skill: Dictionary in skills:
		var full := skill.duplicate()
		if not full.has("shape"):
			full.shape = WorldAdapter.SHAPE_AREA
		shaped.append(full)
	return shaped


func _create(seed: int, armies: Array[Dictionary], rules: Dictionary) -> WorldAdapter:
	return WorldAdapter.create(seed, PackedStringArray(MAP_ROWS), armies, STRUCTURES, rules)


func _with_first_army(key: String, value: Variant) -> Array[Dictionary]:
	var armies := _armies.duplicate(true)
	armies[0][key] = value
	return armies


func _with_rules(key: String, value: Variant) -> Dictionary:
	var rules := RULES.duplicate(true)
	rules[key] = value
	return rules


# `count` members of `health` each, in rows of four cells from `corner`.
func _members(corner: Vector2i, count: int, health: int) -> Array:
	var members := []
	for i in count:
		members.append({"cell": corner + Vector2i(i % 4, i / 4), "health": health})
	return members


# The run's hash, then one line `army x y health` per living person, as world_hash prints them.
func _adapter_run(seed: int) -> PackedStringArray:
	var world := _create(seed, _armies, RULES)
	for i in TICKS:
		world.tick()
	var lines := PackedStringArray([str(world.hash())])
	for army in world.army_count():
		var healths := world.body_strengths(army)
		var positions := world.body_positions(army)
		for body in healths.size():
			lines.append("%d %d %d %d" % [army, int(positions[body].x), int(positions[body].y), healths[body]])
	return lines


# world_hash's output for the scenario, its fields in the core's order, each list after its length.
func _core_run() -> PackedStringArray:
	var fields: Array = [SEED, TICKS, MAP_ROWS.size()] + MAP_ROWS + [_armies.size()]
	for army in _armies:
		fields += [army.objective.x, army.objective.y, army.members.size()]
		for member in army.members:
			fields += [member.cell.x, member.cell.y, member.health]
		fields += [army.faction, army.skills.size()]
		for skill in army.skills:
			fields += [skill.range, skill.radius, skill.body_damage, skill.structure_damage, skill.cooldown]
	fields.append(STRUCTURES.size())
	for structure in STRUCTURES:
		fields += [structure.cell.x, structure.cell.y, structure.durability]
	fields += [RULES.separation, RULES.hostile.size()]
	for row in RULES.hostile:
		fields.append(row.size())
		for hostile in row:
			fields.append(1 if hostile else 0)
	fields += RULES.stage_floors
	var output := []
	if OS.execute(ProjectSettings.globalize_path(REFERENCE), [" ".join(fields.map(str))], output) != 0:
		return PackedStringArray()
	return String(output[0]).strip_edges().split("\n")


# Born into the region's Cult village 1, the player may join its sect: application_refusal reads
# APPLICATION_NONE, apply makes it a member and applying again reads APPLICATION_IN_A_SECT and is
# refused. Control: born into Unorthodox village 2, the player is refused as APPLICATION_UNPROVEN
# and stays out. In `lived`, the region lived ten years of five-tick days, its rumours reach the
# player, newest first, each of a kind the adapter names, at a cell of the map, and naming records
# the world holds: a birth one no older than the years lived, a death one with no health. Attainment
# reads one value per discipline the region names, the five of its rules of life, and a sect's
# founding master 120 in one; a world from create() names no discipline.
func _play_loop_pass(lived: WorldAdapter) -> bool:
	var world := WorldAdapter.create_region(REGION_SEED, CULT_VILLAGE)
	var player := world.roster(world.army_count() - 1)[0]
	var sect := -1
	for index in world.sect_count():
		if world.sect_faction(index) == world.army_faction(world.army_count() - 1):
			sect = index
	var open := world.application_refusal(player, sect) == WorldAdapter.APPLICATION_NONE
	var applied := world.apply(player, sect) and world.person_sect(player) == sect
	var again := world.application_refusal(player, sect) == WorldAdapter.APPLICATION_IN_A_SECT \
			and not world.apply(player, sect)
	var other := WorldAdapter.create_region(REGION_SEED, UNORTHODOX_VILLAGE)
	var outsider := other.roster(other.army_count() - 1)[0]
	var outsider_faction := other.army_faction(other.army_count() - 1)
	var other_sect := -1
	for index in other.sect_count():
		if other.sect_faction(index) == outsider_faction:
			other_sect = index
	# Setup: the player was born Demonic in CULT_VILLAGE and Unorthodox in UNORTHODOX_VILLAGE, and
	# each faction has a sect.
	var set_up: bool = sect >= 0 and other_sect >= 0 \
			and world.faction_names()[world.army_faction(world.army_count() - 1)] == "Demonic" \
			and other.faction_names()[outsider_faction] == "Unorthodox"
	var refused := set_up \
			and other.application_refusal(outsider, other_sect) == WorldAdapter.APPLICATION_UNPROVEN \
			and not other.apply(outsider, other_sect) and other.person_sect(outsider) == -1
	var heard := lived.rumours(lived.roster(lived.army_count() - 1)[0])
	var kinds := ["founded", "raided", "ruined", "sect_fell", "leader_changed", "born", "died"]
	var rows := lived.map_rows()
	var map := Rect2i(0, 0, rows[0].length(), rows.size())
	var rumours_right := heard.size() > 0
	# Births and deaths heard: one born is no older than the years lived, and one who died has no
	# health. Setup: some were heard.
	var of_people := 0
	for n in heard.size():
		var event: Dictionary = heard[n]
		rumours_right = rumours_right and kinds.has(event.kind) and map.has_point(event.cell) \
				and (n == 0 or event.sequence < heard[n - 1].sequence) \
				and (not event.has("village") or event.village < lived.villages().size())
		if event.kind == "born":
			of_people += 1
			rumours_right = rumours_right and lived.person_age(event.person) <= LIVED_YEARS
		elif event.kind == "died":
			of_people += 1
			rumours_right = rumours_right and lived.person_health(event.person) == 0
	rumours_right = rumours_right and of_people > 0
	var names := world.discipline_names()
	var master := world.sect_master(0)
	var attained := world.person_attainment(master)
	var attainment_right: bool = names == PackedStringArray(["sword", "saber", "fist", "hidden weapons", "medicine"]) \
			and attained.size() == names.size() and Array(attained).max() == 120 \
			and world.person_attainment(player).size() == names.size()
	var bare := _create(SEED, _armies, RULES)
	var bare_right := bare.discipline_names().is_empty() and bare.person_attainment(0).is_empty()
	var passed := set_up and open and applied and again and refused and rumours_right and attainment_right and bare_right
	print("world_adapter_test: the Cult player may apply to sect %d %s, joins %s, is refused again %s; the Unorthodox player refused by sect %d as unproven %s (setup %s); %d rumours after ten years, newest first, within the map and %d of them naming the born or the dead rightly %s, the first %s; disciplines %s, master %d attained %s %s; a world from create() names none %s: %s" % [
		sect, open, applied, again, other_sect, refused, set_up, heard.size(), of_people, rumours_right, heard[0] if heard.size() > 0 else {},
		names, master, attained, attainment_right, bare_right, "pass" if passed else "FAIL"])
	return passed

