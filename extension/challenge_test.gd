# Checks that a challenge passes through the adapter as the core takes it: a Cult member's
# challenge starts a duel the adapter reads, and once won the challenger leads and the Cult's ally
# turns hostile to it. Control: an outsider's challenge is refused, for not being a member, and
# a challenger who loses leaves the leader leading.
extends RefCounted

const ORTHODOX := 0
const CULT := 1
const ALLY := 2
const LEADER_ARMY := 0
const MEMBER_ARMY := 1
const OUTSIDER_ARMY := 2
const SEAT := Vector2i(10, 10)
const DUEL_TICKS := 2000
const LEADER_HEALTH := 100
const STRIKE := {"range": 512, "radius": 0, "body_damage": 50, "structure_damage": 0, "cooldown": 10}
const IDLE := {"range": 0, "radius": 0, "body_damage": 0, "structure_damage": 0, "cooldown": 0}
const RULES := {
	"separation": 72,
	"hostile": [[false, true, false], [true, false, false], [false, false, false]],
	"stage_floors": [75, 25, 1],
}
const LEADERSHIP: Array[Dictionary] = [
	{},
	{"leader": 0, "duel": {"members_only": true, "yield_health": 0, "allies_turn_hostile": true}},
	{},
]


func run() -> bool:
	var won := _cult(1000)
	var outsider_refusal := won.challenge_refusal(OUTSIDER_ARMY, CULT)
	var outsider_refused := not won.order(OUTSIDER_ARMY, SEAT, PackedInt64Array([0]), CULT)
	var member_refusal := won.challenge_refusal(MEMBER_ARMY, CULT)
	var no_duel_before := won.duel(CULT).is_empty()
	var ally_peaceful := not won.hostile(ALLY, CULT)
	var challenged := won.order(MEMBER_ARMY, SEAT, PackedInt64Array([0]), CULT)
	var fought := won.duel(CULT)
	var challenger: int = won.roster(MEMBER_ARMY)[0]
	_run(won)
	var won_right := outsider_refusal == WorldAdapter.CHALLENGE_NOT_A_MEMBER and outsider_refused \
			and member_refusal == WorldAdapter.CHALLENGE_NONE and no_duel_before and ally_peaceful \
			and challenged and fought == {"challenger": challenger, "leader": 0} \
			and won.leader(CULT) == challenger and won.duel(CULT).is_empty() \
			and won.person_health(0) == 0 and won.hostile(ALLY, CULT)
	print("challenge_test: outsider refused %s for %d; member may %s; duel %s; after %d ticks the Cult's leader is %d (challenger %d), the old leader's health %d, its ally hostile %s: %s" % [
		outsider_refused, outsider_refusal, member_refusal == WorldAdapter.CHALLENGE_NONE, fought,
		DUEL_TICKS, won.leader(CULT), challenger, won.person_health(0), won.hostile(ALLY, CULT),
		"pass" if won_right else "FAIL"])

	# Control: a challenger who loses dies, and the leader still leads.
	var lost := _cult(1)
	var lost_challenged := lost.order(MEMBER_ARMY, SEAT, PackedInt64Array([0]), CULT)
	_run(lost)
	var lost_right := lost_challenged and lost.leader(CULT) == 0 and lost.roster(MEMBER_ARMY).is_empty() \
			and not lost.hostile(ALLY, CULT)
	print("challenge_test: a losing challenger leaves leader %d, its army %d strong: %s" % [
		lost.leader(CULT), lost.roster(MEMBER_ARMY).size(), "pass" if lost_right else "FAIL"])
	return won_right and lost_right


# A Cult led by person 0 at SEAT, a member of `health` beside it, an Orthodox outsider and the
# Cult's ally far off.
func _cult(health: int) -> WorldAdapter:
	var rows := PackedStringArray()
	for y in 24:
		rows.append(".".repeat(24))
	var armies: Array[Dictionary] = [
		{"objective": SEAT, "members": [{"cell": SEAT, "health": LEADER_HEALTH}], "faction": CULT, "skills": [STRIKE]},
		{"objective": Vector2i(4, 10), "members": [{"cell": Vector2i(4, 10), "health": health}], "faction": CULT, "skills": [STRIKE]},
		{"objective": Vector2i(1, 22), "members": [{"cell": Vector2i(1, 22), "health": 1}], "faction": ORTHODOX, "skills": [IDLE]},
		{"objective": Vector2i(22, 22), "members": [{"cell": Vector2i(22, 22), "health": 1}], "faction": ALLY, "skills": [IDLE]},
	]
	return WorldAdapter.create(9, rows, armies, [], RULES, LEADERSHIP)


func _run(world: WorldAdapter) -> void:
	for i in DUEL_TICKS:
		world.tick()
