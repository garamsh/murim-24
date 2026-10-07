# Checks that sound.tscn maps each kind of event the core reports to its sound: in a world of two
# hostile bands, a torch-bearer beside a structure and a mender beside a damaged one, strikes, hits,
# deaths, a structure breaking and one mended each start a voice with their kind's stream; in a
# region, the player marching on a hostile village rings its bell. Controls: the same world at
# peace, with no skill reaching a structure, reports none of them; the region with the player left
# at home rings no bell; an event outside what is heard starts no voice. In the M1 battle of 500
# people the voices sounding never pass the cap, reach it, and drop what finds no voice; control:
# below the cap every event heard starts a voice, and once the sounds end the voices free up.
extends RefCounted

const Sound := preload("res://sound/sound.gd")
const SCENE := "res://sound/sound.tscn"
const SEED := 42
const TICKS := 600
const STEP := 1.0 / WorldAdapter.TICKS_PER_SECOND
const EVERYWHERE := Rect2(-1e6, -1e6, 2e6, 2e6)
const MAP_ROWS: Array[String] = [
	"................",
	"................",
	"................",
	"................",
	"................",
	"................",
	"................",
	"................",
	"................",
	"................",
]
const BLOW := {"range": 768, "radius": 256, "body_damage": 3, "structure_damage": 0, "cooldown": 10}
const TORCH := {"range": 0, "radius": 256, "body_damage": 0, "structure_damage": 5, "cooldown": 10}
const MEND := {"range": 0, "radius": 256, "body_damage": 0, "structure_damage": -5, "cooldown": 10}
# The torch-bearer's structure intact, and the mender's damaged, by the floors below.
const STRUCTURES: Array[Dictionary] = [
	{"cell": Vector2i(3, 8), "durability": 100},
	{"cell": Vector2i(11, 8), "durability": 30},
]
const FLOORS := [75, 25, 1]
# The region the play scene opens on, and its first Cult village, the player's home.
const REGION_SEED := 2026
const CULT_VILLAGE := 1
const MARCH_TICKS := 3000
const CAP_TICKS := 300


func run() -> bool:
	return _mapping_pass() and _bell_pass() and _cap_pass()


func _mapping_pass() -> bool:
	var war := _listen(_world(true), TICKS)
	var peace := _listen(_world(false), TICKS)
	var mapped := true
	for kind: int in [Sound.Heard.STRIKE, Sound.Heard.HIT, Sound.Heard.FALL, Sound.Heard.FIRE, Sound.Heard.MEND]:
		mapped = mapped and war.played[kind] > 0 and war.streams.has(Sound.STREAMS[kind])
	var silent: bool = peace.heard == PackedInt64Array([0, 0, 0, 0, 0, 0]) and peace.streams.is_empty()
	# An event outside what is heard is counted and starts no voice.
	var sound: Node2D = load(SCENE).instantiate()
	sound.hearing = Rect2(0, 0, 100, 100)
	var outside: bool = not sound.hear(Sound.Heard.HIT, Vector2(500, 500)) and sound.heard[Sound.Heard.HIT] == 1 \
			and sound.sounding() == 0
	var inside: bool = sound.hear(Sound.Heard.HIT, Vector2(50, 50)) and sound.sounding() == 1
	sound.free()
	var passed := mapped and silent and outside and inside
	print("sound_test: at war over %d ticks, heard %s and played %s by kind %s, each kind's stream sounded %s; at peace heard %s, silent %s; an event outside the hearing unplayed %s, inside played %s: %s" % [
		TICKS, war.heard, war.played, Sound.Heard.keys(), mapped, peace.heard, silent, outside, inside,
		"pass" if passed else "FAIL"])
	return passed


# Marches the player on the nearest village of a faction hostile to its own, its torch in hand,
# until the bell rings there.
func _bell_pass() -> bool:
	var marched := WorldAdapter.create_region(REGION_SEED, CULT_VILLAGE)
	var player := marched.army_count() - 1
	var home: Vector2 = marched.body_positions(player)[0] / WorldAdapter.CELL_SIZE
	var target := Vector2i(-1, -1)
	for village: Dictionary in marched.villages():
		var centre: Vector2i = village.corner + village.area / 2
		if marched.hostile(marched.army_faction(player), village.faction) \
				and (target.x < 0 or home.distance_to(centre) < home.distance_to(target)):
			target = centre
	var ordered := target.x >= 0 and marched.order(player, target, PackedInt64Array([1]))
	var rung := _listen(marched, MARCH_TICKS, true)
	var stayed := _listen(WorldAdapter.create_region(REGION_SEED, CULT_VILLAGE), MARCH_TICKS)
	var passed: bool = ordered and rung.played[Sound.Heard.BELL] > 0 \
			and rung.streams.has(Sound.STREAMS[Sound.Heard.BELL]) and stayed.heard[Sound.Heard.BELL] == 0
	print("sound_test: the player ordered on the hostile village at %s %s, the bell heard %d times after %d ticks; left at home, heard %d times in %d: %s" % [
		target, ordered, rung.heard[Sound.Heard.BELL], rung.ticks, stayed.heard[Sound.Heard.BELL], MARCH_TICKS,
		"pass" if passed else "FAIL"])
	return passed


func _cap_pass() -> bool:
	var sound: Node2D = load(SCENE).instantiate()
	sound.hearing = EVERYWHERE
	var world: WorldAdapter = WorldAdapter.create_m1()
	sound.show_world(world)
	var most := 0
	for i in CAP_TICKS:
		world.tick()
		sound.note_tick()
		sound.advance(STEP)
		most = maxi(most, sound.sounding())
	var dropped: int = sound.dropped
	var capped: bool = most == Sound.VOICES and dropped > 0
	# Control: once the longest sound has ended every voice is free, and below the cap each kind
	# heard on a tick that changed nothing starts one.
	var longest := 0.0
	for stream in Sound.STREAMS:
		longest = maxf(longest, stream.get_length())
	sound.advance(longest)
	sound.note_tick()
	var freed: bool = sound.sounding() == 0
	var started := 0
	for kind in Sound.STREAMS.size():
		if sound.hear(kind, Vector2.ZERO):
			started += 1
	var below: bool = freed and started == Sound.STREAMS.size() and sound.sounding() == started
	sound.free()
	var passed := capped and below
	print("sound_test: the M1 battle over %d ticks sounded at most %d voices of %d, %d events dropped; freed after %.1f s %s, then %d of %d kinds heard started: %s" % [
		CAP_TICKS, most, Sound.VOICES, dropped, longest, freed, started,
		Sound.STREAMS.size(), "pass" if passed else "FAIL"])
	return passed


# A world of two hostile bands of four, a torch-bearer beside an intact structure and a mender
# beside a damaged one; at peace, no faction is hostile and all four carry the bands' blow, which
# finds no one to aim at, since a skill of range 0 strikes where its user stands.
func _world(at_war: bool) -> WorldAdapter:
	var armies: Array[Dictionary] = [
		{"objective": Vector2i(6, 2), "members": _members(Vector2i(1, 1), 4), "faction": 0, "skills": [BLOW]},
		{"objective": Vector2i(2, 2), "members": _members(Vector2i(7, 1), 4), "faction": 1, "skills": [BLOW]},
		{"objective": Vector2i(2, 8), "members": _members(Vector2i(2, 8), 1), "faction": 2,
			"skills": [TORCH if at_war else BLOW]},
		{"objective": Vector2i(10, 8), "members": _members(Vector2i(10, 8), 1), "faction": 2,
			"skills": [MEND if at_war else BLOW]},
	]
	var hostile := [[false, at_war, false], [at_war, false, false], [false, false, false]]
	return WorldAdapter.create(SEED, PackedStringArray(MAP_ROWS), armies, STRUCTURES,
			{"separation": 256, "hostile": hostile, "stage_floors": FLOORS})


# Listens to `world` for up to `ticks` ticks, everywhere, the clock moving a tick's length each;
# stops at the first bell when `until_bell` is set. Returns what was heard and played by kind, and
# every stream a voice started with.
func _listen(world: WorldAdapter, ticks: int, until_bell := false) -> Dictionary:
	var sound: Node2D = load(SCENE).instantiate()
	sound.hearing = EVERYWHERE
	sound.show_world(world)
	var streams := {}
	var ran := 0
	for i in ticks:
		world.tick()
		var before: PackedInt64Array = sound.played.duplicate()
		sound.note_tick()
		sound.advance(STEP)
		ran += 1
		for voice in Sound.VOICES:
			if sound.voice_stream(voice) != null:
				streams[sound.voice_stream(voice)] = true
		if until_bell and sound.played[Sound.Heard.BELL] > before[Sound.Heard.BELL]:
			break
	var heard: Dictionary = {"heard": sound.heard.duplicate(), "played": sound.played.duplicate(),
		"streams": streams, "ticks": ran}
	sound.free()
	return heard


# `count` members of 30 health, in a row from `corner`.
func _members(corner: Vector2i, count: int) -> Array:
	var members := []
	for i in count:
		members.append({"cell": corner + Vector2i(i, 0), "health": 30})
	return members
