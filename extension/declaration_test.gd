# Checks that war and peace pass through the adapter as the core takes them: a leader's war on a
# faction at peace with its own is declared, read back as relation() and declaration() give it, its
# date the core's; a member's is refused, for not leading, and declare() refuses it. A region made
# by create_region() marks the player's army steered and no other, and steer() marks any.
extends RefCounted

const OWN := 0
const OTHER := 1
const IDLE := {"range": 0, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 1}
const RULES := {
	"separation": 72,
	"hostile": [[false, false], [false, false]],
	"stage_floors": [75, 25, 1],
}
const LEADERSHIP: Array[Dictionary] = [{"leader": 0}, {}]


func run() -> bool:
	var world := _world()
	var allied := world.relation(OWN, OTHER) == WorldAdapter.RELATION_ALLY and world.declaration(OWN, OTHER).is_empty()
	var member_refusal := world.declaration_refusal(1, OTHER, WorldAdapter.DECLARATION_WAR)
	var member_refused := not world.declare(1, OTHER, WorldAdapter.DECLARATION_WAR)
	var leader_refusal := world.declaration_refusal(0, OTHER, WorldAdapter.DECLARATION_WAR)
	var declared := world.declare(0, OTHER, WorldAdapter.DECLARATION_WAR)
	var made := world.declaration(OTHER, OWN)
	var date := world.date()
	var read_right: bool = world.relation(OTHER, OWN) == WorldAdapter.RELATION_AT_WAR and world.hostile(OTHER, OWN) \
			and made == {"what": WorldAdapter.DECLARATION_WAR, "by": OWN, "tick": 0, "year": date.year,
					"season": date.season}
	var passed := allied and member_refusal == WorldAdapter.DECLARE_NOT_THE_LEADER and member_refused \
			and leader_refusal == WorldAdapter.DECLARE_NONE and declared and read_right
	print("declaration_test: allied at first %s; a member refused %d, declare() refusing %s; the leader may %s and declares %s, read back %s: %s" % [
		allied, member_refusal, member_refused, leader_refusal == WorldAdapter.DECLARE_NONE, declared, made,
		"pass" if passed else "FAIL"])

	var region := WorldAdapter.create_region(2026, 0)
	var player := region.player_army()
	var others_free := true
	for army in region.army_count():
		others_free = others_free and (army == player or not region.steered(army))
	var marked := region.steered(player) and others_free
	region.steer(0, true)
	var steered_any := region.steered(0)
	print("declaration_test: the region's player army %d steered %s, no other %s; steer() marks army 0 %s: %s" % [
		player, region.steered(player), others_free, steered_any, "pass" if marked and steered_any else "FAIL"])
	return passed and marked and steered_any


# Faction 0 led by person 0, with a member, person 1, in an army of its own, and faction 1 at peace
# with it.
func _world() -> WorldAdapter:
	var rows := PackedStringArray([".".repeat(8), ".".repeat(8)])
	var armies: Array[Dictionary] = [
		{"objective": Vector2i(0, 0), "members": [{"cell": Vector2i(0, 0), "health": 1}], "faction": OWN, "skills": [IDLE]},
		{"objective": Vector2i(3, 0), "members": [{"cell": Vector2i(3, 0), "health": 1}], "faction": OWN, "skills": [IDLE]},
		{"objective": Vector2i(7, 1), "members": [{"cell": Vector2i(7, 1), "health": 1}], "faction": OTHER, "skills": [IDLE]},
	]
	return WorldAdapter.create(1, rows, armies, [], RULES, LEADERSHIP)
