#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "arts.hpp"
#include "attack.hpp"
#include "calendar.hpp"
#include "chart.hpp"
#include "chronicle.hpp"
#include "diplomacy.hpp"
#include "event.hpp"
#include "flow_field.hpp"
#include "growth.hpp"
#include "kinship.hpp"
#include "leadership.hpp"
#include "life.hpp"
#include "limits.hpp"
#include "map.hpp"
#include "names.hpp"
#include "person.hpp"
#include "progress.hpp"
#include "sect.hpp"
#include "skill.hpp"
#include "spatial_hash.hpp"
#include "talent.hpp"
#include "talk.hpp"
#include "trade.hpp"
#include "village.hpp"

namespace murim {

/// Seed a world is created from; the same seed gives the same world.
enum class Seed : std::uint64_t {};

/// Digest of a world's whole state; equal states give equal hashes.
enum class StateHash : std::uint64_t {};

/// A person as a world is created with: standing at `cell`, with `health`.
struct Recruit {
  Cell cell;
  std::uint64_t health;
};

/// An army as a world is created with: its `members`, each a new person,
/// bound for `objective`, of `faction`, choosing among `skills` by the
/// world's seed, and making village `home` their home, if it names one,
/// without being its residents. An army is a roster of people, never a count.
struct ArmySpec {
  Cell objective;
  std::vector<Recruit> members;
  std::size_t faction;
  std::vector<Skill> skills;
  std::optional<std::size_t> home = std::nullopt;
};

/// What an army does: march toward `objective`, and at each strike use one
/// of `skills`, indices into the army's own skills, drawn by the seed. An
/// army created from an ArmySpec follows its objective with all its skills
/// until it is given another order. With a `target`, an attack intent, its
/// members close on that person and aim every automatic strike at it. The
/// core ends the intent when the target dies, stops being a foe of the army's
/// lead, or leaves every member's sight, and the army then holds where its
/// lead stands, as it does when a driver ends the intent through
/// World::end_intent; an order without a target, a move order, replaces it.
struct Order {
  Cell objective;
  std::vector<std::size_t> skills;
  std::optional<std::size_t> target = std::nullopt;
};

/// The data a world's rules read.
struct Rules {
  /// Sub-cell units people keep apart; 0 lets them overlap.
  std::int32_t separation;
  /// `hostile[a][b]`: whether bodies of faction `a` attack those of `b` by
  /// their stances, where no declaration stands between the two.
  std::vector<std::vector<bool>> hostile;
  /// The lowest durability of the intact, damaged and destroyed stages; a
  /// structure below all three is rubble.
  std::array<std::uint32_t, 3> stage_floors;
  /// Ticks in a day of the calendar.
  std::uint64_t ticks_per_day = TICKS_PER_DAY;
  /// Cells, along each axis, within which an event reaches a person as a
  /// rumour wherever it concerns.
  std::int32_t rumour_reach = 0;
  /// Cells, along each axis, within which one person may talk to another.
  std::int32_t talk_reach = 0;
  /// Cells within which a person no driver steers, with no foe within its
  /// reach, closes on the nearest foe: at home, one inside its village's
  /// claim, and on the march any; 0 for none.
  std::int32_t sight = 0;
  /// The core's capacity for the living, the most people the world holds,
  /// up to PEOPLE_CEILING: a storage and performance limit, not a rule of
  /// play. Births stop at it, and a world or save with more is refused.
  std::size_t people_capacity = MAX_PEOPLE;
  /// Cells within which a person a driver steers, carrying a soul, sees the
  /// land for its soul's chart; 0 charts nothing.
  std::int32_t map_sight = 0;
  /// How many may strike one person at once and how those waiting circle
  /// (ADR 0068); none for no limit.
  std::optional<Tokens> tokens = std::nullopt;
};

/// Sub-cell units a person moves per tick.
constexpr std::int32_t SPEED = 16;

/// Sub-cell units a person moves along each axis per tick on a diagonal step:
/// the whole number nearest SPEED over the square root of two, so it covers
/// within 3% of SPEED either way.
constexpr std::int32_t DIAGONAL_SPEED = 11;

class SaveCodec;

/// The simulated world, advanced one deterministic step at a time.
class World {
 public:
  /// Throws std::invalid_argument when the separation, the rumour reach, the
  /// talk reach, or a
  /// skill's range or radius, lies outside [0, MAX_REACH], `hostile` is not
  /// square, a recruit has no health or more than MAX_HEALTH, the world would
  /// hold more than MAX_PEOPLE people, an army has no skills or a faction
  /// outside `hostile` or a home that is no village,
  /// the rules' attack tokens are not valid(),
  /// a skill does more than MAX_BODY_DAMAGE to a person or
  /// waits longer than MAX_COOLDOWN, a structure's footprint is not 1 to
  /// MAX_FOOTPRINT cells along each side or covers a cell that is not
  /// passable or that another structure covers, a recruit stands on a cell
  /// that is not passable or that a structure covers, or from which its
  /// army's objective cannot be reached, or a village names a structure or an
  /// army that does not exist, a structure that another village names, or an
  /// army that is another village's residents, or more households than its
  /// residents pair into, a faction's leader is no
  /// person the world creates, or `leadership` names more factions than
  /// `hostile`; a faction it leaves out has no leader; a day lasts no tick;
  /// or `life` could draw a talent above MAX_TALENT or a lifespan or founding
  /// age above MAX_LIFESPAN_DAYS, gives a newborn no health or more than
  /// MAX_HEALTH, or gives kinship an age range running downward, a chance
  /// above PER_MILLE or a neighbourhood outside [0, MAX_REACH], or gives
  /// growth no interval, a pressure above PER_MILLE, a reach, vicinity,
  /// spacing or claim outside [0, MAX_REACH], a plan or settlement of a side
  /// outside [1, MAX_FOOTPRINT], a build or placement naming no plan, a
  /// layout with a structure outside its settlement, over another or over
  /// its centre, or stances for more factions than `hostile`; or an
  /// admission gives a chance above PER_MILLE or a reach outside
  /// [0, MAX_REACH] or an elder share above PER_MILLE; or a sect names a
  /// faction outside `hostile`, a village
  /// that does not exist, or a founder who is no person the world creates or
  /// who serves in no army of the sect's faction, or `life`'s manuals name a
  /// discipline it does not draw, more sects than there are, a chance above
  /// PER_MILLE or a reach outside [0, MAX_REACH]; or `life`'s cultivation
  /// could draw a growth or limit above MAX_TALENT, recovers more than
  /// PER_MILLE or every 0 ticks, its realms are not ascending by qi or have a
  /// might above MAX_MIGHT, an art names a discipline it does not draw or has
  /// a skill outside the limits an army's are within or of a rank beyond the
  /// realms, a skill costs more than MAX_TALENT qi, or the learning gives a
  /// chance, an ease or a potency above PER_MILLE or a reading reach outside
  /// [0, MAX_REACH]; or the trade gives a ware a price outside
  /// [1, MAX_TALENT], health above MAX_HEALTH, qi above MAX_TALENT or a
  /// discipline it does not draw, a purse or wage above MAX_TALENT, or a
  /// wounded or drained share above PER_MILLE. Within these limits no
  /// damage, health, durability or cooldown arithmetic exceeds its type.
  World(Seed seed, Map map, std::vector<ArmySpec> armies,
        std::vector<Structure> structures, Rules rules,
        std::vector<VillageSpec> villages = {},
        std::vector<Leadership> leadership = {}, Life life = {},
        std::vector<SectSpec> sects = {});

  /// Gives army `army` an order, which it follows from the next tick on.
  /// With `challenge`, the army's lead also challenges that faction's
  /// leader to a duel, the same order for the player's body as for an NPC.
  /// Throws std::invalid_argument when there is no such army, the objective
  /// is not passable or not reachable from where one of its members stands,
  /// or the skills are empty or name one the army lacks; or it challenges a
  /// faction with no duel succession or no leader, from an army with no
  /// members, an army of another faction where the duel takes members only,
  /// with the leader as its lead, or while that faction's duel is fought; or
  /// it names a target attack_refusal() refuses.
  void order(std::size_t army, Order order,
             std::optional<std::size_t> challenge = std::nullopt);

  /// Returns why army `army` may not take person `person` as its attack
  /// intent's target, which is why order() would refuse that target, or
  /// AttackRefusal::NONE when it may. Throws std::invalid_argument when there
  /// is no such army.
  [[nodiscard]] AttackRefusal attack_refusal(std::size_t army,
                                             std::size_t person) const;

  /// Returns the person army `army`'s attack intent targets, if it holds
  /// one. Throws std::out_of_range when there is no such army.
  [[nodiscard]] std::optional<std::size_t> target(std::size_t army) const;

  /// Returns the cell army `army`'s order marches it toward, which an ended
  /// attack intent sets to where its lead stands. Throws std::out_of_range
  /// when there is no such army.
  [[nodiscard]] Cell objective(std::size_t army) const;

  /// Ends army `army`'s attack intent, if it holds one, as a driver does when
  /// its player clears the target: the army holds where its lead stands, its
  /// objective that cell, as when the core ends the intent. Throws
  /// std::out_of_range when there is no such army.
  void end_intent(std::size_t army);

  /// Returns the living foes within the rules' sight of person `person`,
  /// nearest first, the first in index order among equals: those its army
  /// may take as a target. Empty when the person is dead or serves in no
  /// army. Throws std::invalid_argument when there is no such person.
  [[nodiscard]] std::vector<std::size_t> foes_in_sight(
      std::size_t person) const;

  /// Has person `person` use skill `skill`, by index among its skills, its
  /// army's followed by those it knows, at `target` on the next tick, in place
  /// of its automatic strike, the same cast for the player's body as for an
  /// NPC. A point beyond the skill's range is brought within it along the line
  /// from the caster, and the caster itself is the centre of a skill cast at
  /// itself. A later cast before that tick replaces this one, and a cast that
  /// cast_refusal() refuses on that tick lapses. Throws std::invalid_argument
  /// when cast_refusal() names a reason, or there is no such person.
  void cast(std::size_t person, std::size_t skill, const Target& target);

  /// Moves living person `person` from the army it serves in to the end of
  /// army `army`'s roster. Throws std::invalid_argument when there is no
  /// such army, the person is dead or does not exist, or `army`'s objective
  /// cannot be reached from where the person stands.
  void conscript(std::size_t person, std::size_t army);

  /// Returns every member of army `army` who has a home to the residents of
  /// that home, in roster order; members with none stay. Throws
  /// std::out_of_range when there is no such army.
  void disband(std::size_t army);

  /// Advances the world by one step.
  void tick();

  /// Returns the hash of the current state.
  [[nodiscard]] StateHash hash() const;

  /// Returns how many people the world has held, living or dead.
  [[nodiscard]] std::size_t population() const;

  /// Returns person `person`.
  [[nodiscard]] const Person& person(std::size_t person) const;

  /// Returns each person's attack slot and token against the foe it engages,
  /// with room for every person the world has held (ADR 0068).
  [[nodiscard]] const Attacks& attacks() const;

  /// Returns how many people live.
  [[nodiscard]] std::size_t living() const;

  /// Returns person `person`'s name, or nothing when the world's life names
  /// no surnames or no given names for its sex.
  [[nodiscard]] std::optional<Name> name(std::size_t person) const;

  /// Returns the living members of army `army`, as person indices, in the
  /// order they joined it; the first is its lead.
  [[nodiscard]] const std::vector<std::size_t>& roster(std::size_t army) const;

  /// Returns structure `structure`, numbered in creation order.
  [[nodiscard]] const Structure& structure(std::size_t structure) const;

  /// Returns the stage structure `structure` is in.
  [[nodiscard]] Stage stage(std::size_t structure) const;

  /// Returns the state of village `village`: ruins while every structure is
  /// rubble; otherwise raided while a living body of a faction hostile to its
  /// owner could damage one of its structures from where it stands, a
  /// skill's range and radius away; otherwise prosperous while every
  /// structure is intact, and rebuilding while one is not.
  [[nodiscard]] VillageState village_state(std::size_t village) const;

  /// Returns the faction owning village `village`: its residents' faction.
  /// The first army to restore one of its structures while its residents
  /// have no living member becomes its residents, and its home.
  [[nodiscard]] std::size_t owner(std::size_t village) const;

  /// Returns how many people village `village` can hold: the least of the
  /// housing of its structures still standing, the people its intact fields'
  /// harvests feed for a year, and the people a year's food its standing
  /// structures store feeds. Without husbandry it is MAX_PEOPLE.
  [[nodiscard]] std::size_t cap(std::size_t village) const;

  /// Returns the food village `village` holds.
  [[nodiscard]] std::uint64_t stock(std::size_t village) const;

  /// Returns how many living people make village `village` their home.
  [[nodiscard]] std::size_t residents_of(std::size_t village) const;

  /// Returns what structure `structure` shows as a field, or nothing when it
  /// yields no harvest.
  [[nodiscard]] std::optional<FieldState> field_state(
      std::size_t structure) const;

  /// Returns how many armies the world holds, those growth formed included.
  [[nodiscard]] std::size_t army_count() const;

  /// Returns how many structures the world holds, those growth laid
  /// included, numbered in the order they were laid.
  [[nodiscard]] std::size_t structure_count() const;

  /// Returns how many villages the world holds, those founded included.
  [[nodiscard]] std::size_t village_count() const;

  /// Returns the index, among the growth's plans, of the plan structure
  /// `structure` was laid from, or nothing for one the world was created
  /// with.
  [[nodiscard]] std::optional<std::size_t> plan_of(std::size_t structure) const;

  /// Returns the faction whose territory each cell of the map lies in, row by
  /// row: the owner of the nearest village with living residents whose
  /// centre lies within the growth's claim, the lowest village among equals,
  /// or nothing beyond every such village and without growth.
  [[nodiscard]] std::vector<std::optional<std::size_t>> territory() const;

  /// Gives living person `person` a new soul with no history, so that it is
  /// born again when it dies. Returns the soul. Throws std::invalid_argument
  /// when the person is dead or does not exist.
  std::size_t ensoul(std::size_t person);

  /// Returns soul `soul`'s history.
  [[nodiscard]] const Soul& soul(std::size_t soul) const;

  /// Returns what soul `soul`'s bodies have seen: the cells within the
  /// rules' map sight of each, at the end of every tick it was steered, and
  /// each village with a structure in view as it was then. A soul keeps its
  /// chart when it is born again, and no rule reads it.
  [[nodiscard]] const Chart& chart(std::size_t soul) const;

  /// Returns the faction each cell's territory lay in as soul `soul` last
  /// saw it, row by row: as territory() derives it, from the villages its
  /// chart remembers with living residents, and nothing for a cell it has
  /// not seen.
  [[nodiscard]] std::vector<std::optional<std::size_t>> remembered_territory(
      std::size_t soul) const;

  /// Returns the date the world has reached.
  [[nodiscard]] Date date() const;

  /// Returns how many ticks a day of the world's calendar takes.
  [[nodiscard]] std::uint64_t ticks_per_day() const {
    return rules_.ticks_per_day;
  }

  /// Returns the date of tick `tick` on the world's calendar.
  [[nodiscard]] Date date_at(std::uint64_t tick) const;

  /// Returns how many ticks the world has run, the tick a person's
  /// `ready_at` is counted against.
  [[nodiscard]] std::uint64_t tick_count() const { return tick_count_; }

  /// Returns every use of a skill on the last tick, in the order people
  /// struck; none before the first tick. Neither hashed nor saved.
  [[nodiscard]] const std::vector<Strike>& strikes() const { return strikes_; }

  /// Returns person `person`'s age in whole years of the calendar.
  [[nodiscard]] std::int64_t age_in_years(std::size_t person) const;

  /// Returns the faction of the army person `person` serves in, if it
  /// serves in one; the dead serve in none.
  [[nodiscard]] std::optional<std::size_t> faction_of(std::size_t person) const;

  /// Returns the faction army `army` fights for now.
  [[nodiscard]] std::size_t faction(std::size_t army) const;

  /// Returns the skills army `army` has now.
  [[nodiscard]] const std::vector<Skill>& skills(std::size_t army) const;

  /// Returns how many skills person `person` may cast: its army's, followed
  /// by the art skills it knows; none once it is dead. Throws
  /// std::out_of_range when there is no such person.
  [[nodiscard]] std::size_t skill_count(std::size_t person) const;

  /// Returns person `person`'s skill `skill`, by index among its army's
  /// followed by those it knows. Throws std::out_of_range when it has no
  /// such skill.
  [[nodiscard]] const Skill& skill_of(std::size_t person,
                                      std::size_t skill) const;

  /// Returns the tick from which person `person` may use its skill `skill`
  /// again: its army's skills share one, and each art skill it knows has its
  /// own. Throws std::out_of_range when it has no such skill.
  [[nodiscard]] std::uint64_t ready_at(std::size_t person,
                                       std::size_t skill) const;

  /// Returns person `person`'s realm rank: how many of the realms its qi
  /// capacity reaches, 0 for none. Throws std::out_of_range when there is no
  /// such person.
  [[nodiscard]] std::uint32_t rank(std::size_t person) const;

  /// Returns person `person`'s nature, its heart method's, the art it
  /// learned first, or nothing when it knows no art. Throws std::out_of_range
  /// when there is no such person.
  [[nodiscard]] std::optional<Nature> nature(std::size_t person) const;

  /// Returns the realms people cultivate into, ascending.
  [[nodiscard]] const std::vector<Realm>& realms() const;

  /// Returns the arts people learn.
  [[nodiscard]] const std::vector<Art>& arts() const;

  /// Returns the person leading faction `faction`, if it has a leader.
  [[nodiscard]] std::optional<std::size_t> leader(std::size_t faction) const;

  /// Returns whether faction `faction` elects its leader.
  [[nodiscard]] bool elects(std::size_t faction) const;

  /// Returns why army `army`'s lead may not challenge faction `faction`'s
  /// leader, which is why order() would refuse that challenge, or
  /// ChallengeRefusal::NONE when it may. Throws std::invalid_argument when
  /// there is no such army.
  [[nodiscard]] ChallengeRefusal challenge_refusal(std::size_t army,
                                                   std::size_t faction) const;

  /// Returns why person `person` may not cast skill `skill` at `target`,
  /// which is why cast() would refuse it, or CastRefusal::NONE when it may.
  /// Throws std::invalid_argument when there is no such person.
  [[nodiscard]] CastRefusal cast_refusal(std::size_t person, std::size_t skill,
                                         const Target& target) const;

  /// Returns the living person hostile to person `person` that its skill
  /// `skill` would aim at: the nearest within the skill's range ahead of
  /// `facing`, within a quarter turn either side, or the nearest behind when
  /// none is ahead, the first in roster order among equals. A facing of
  /// (0, 0) has everyone ahead, as the automatic strike aims. Returns
  /// nothing when no one is in range, or the person is dead or its army has
  /// no such skill. Throws std::invalid_argument when there is no such
  /// person or the facing is longer than MAX_REACH along an axis.
  [[nodiscard]] std::optional<std::size_t> aim(std::size_t person,
                                               std::size_t skill,
                                               Step facing) const;

  /// Returns the duel being fought for faction `faction`'s lead, if one is.
  [[nodiscard]] std::optional<Duellists> duel(std::size_t faction) const;

  /// Returns whether people of faction `from` attack those of `target`: at
  /// a war declared between them, and not at a peace declared; otherwise as
  /// the rules' hostility says.
  [[nodiscard]] bool hostile(std::size_t from, std::size_t target) const;

  /// Returns how faction `faction` stands toward faction `other`. Throws
  /// std::out_of_range when there is no such faction.
  [[nodiscard]] Relation relation(std::size_t faction, std::size_t other) const;

  /// Returns the declaration standing between factions `faction` and
  /// `other`, if one does. Throws std::out_of_range when there is no such
  /// faction.
  [[nodiscard]] std::optional<Declared> declaration(std::size_t faction,
                                                    std::size_t other) const;

  /// Returns why person `person` may not declare `what` toward faction
  /// `faction`, which is why declare() would refuse it, or
  /// DeclarationRefusal::NONE when it may. Throws std::invalid_argument when
  /// there is no such person or faction.
  [[nodiscard]] DeclarationRefusal declaration_refusal(std::size_t person,
                                                       std::size_t faction,
                                                       Declaration what) const;

  /// Has person `person`, its faction's leader, declare `what` toward
  /// faction `faction` at once, for both factions, the same order for the
  /// player's body as for an NPC. Throws std::invalid_argument when
  /// declaration_refusal() names a reason, or there is no such person or
  /// faction.
  void declare(std::size_t person, std::size_t faction, Declaration what);

  /// Marks army `army` as steered by a driver, or as its own, when `steered`
  /// is false. The rules that decide for people on their own, such as
  /// diplomacy, decide nothing for a steered army's members. Throws
  /// std::out_of_range when there is no such army.
  void steer(std::size_t army, bool steered);

  /// Returns whether a driver steers army `army`. Throws std::out_of_range
  /// when there is no such army.
  [[nodiscard]] bool steered(std::size_t army) const;

  /// Returns who last brought village `village` to ruins, if anyone has; it
  /// is kept when the village is rebuilt.
  [[nodiscard]] std::optional<Destroyer> destroyer(std::size_t village) const;

  /// Returns the land village `village` was founded on, or nothing for a
  /// village the world was created with.
  [[nodiscard]] std::optional<Settlement> settlement(std::size_t village) const;

  /// Returns how many sects the world holds, standing or fallen.
  [[nodiscard]] std::size_t sect_count() const;

  /// Returns the sect person `person` belongs to, if any.
  [[nodiscard]] std::optional<std::size_t> sect_of(std::size_t person) const;

  /// Returns sect `sect`'s name.
  [[nodiscard]] const std::string& sect_name(std::size_t sect) const;

  /// Returns the faction sect `sect` belongs to.
  [[nodiscard]] std::size_t sect_faction(std::size_t sect) const;

  /// Returns the village that is sect `sect`'s seat.
  [[nodiscard]] std::size_t sect_seat(std::size_t sect) const;

  /// Returns whether sect `sect`'s seat is held: its village is not in
  /// ruins and is owned by the sect's faction.
  [[nodiscard]] bool seat_held(std::size_t sect) const;

  /// Returns sect `sect`'s living members, by person index, ascending.
  [[nodiscard]] std::vector<std::size_t> sect_members(std::size_t sect) const;

  /// Returns sect `sect`'s elders, ascending: the elder share of its living
  /// members of the highest best attainment, the lower index among equals,
  /// at least one while it stands; none for a faction with no admission.
  [[nodiscard]] std::vector<std::size_t> sect_elders(std::size_t sect) const;

  /// Returns sect `sect`'s master: its elder of the highest best attainment,
  /// the lower index among equals, if it has an elder.
  [[nodiscard]] std::optional<std::size_t> sect_master(std::size_t sect) const;

  /// Returns how many members sect `sect` may hold: the pupils its seat's
  /// standing structures teach while the seat is held, and 0 otherwise.
  [[nodiscard]] std::size_t sect_cap(std::size_t sect) const;

  /// Returns whether sect `sect` stands: whether it has a living member.
  [[nodiscard]] bool sect_stands(std::size_t sect) const;

  /// Returns every manual and where it is now.
  [[nodiscard]] const std::vector<Manual>& manuals() const;

  /// Returns why person `person` may not join sect `sect`, which is why
  /// apply() would refuse it, or ApplicationRefusal::NONE when it may: the
  /// thresholds of the daily admission, without its chance or its draw among
  /// sects. Throws std::invalid_argument when there is no such person or sect.
  [[nodiscard]] ApplicationRefusal application_refusal(std::size_t person,
                                                       std::size_t sect) const;

  /// Makes person `person` a member of sect `sect` at once, the same
  /// application for the player's body as for an NPC. Throws
  /// std::invalid_argument when application_refusal() names a reason, or
  /// there is no such person or sect.
  void apply(std::size_t person, std::size_t sect);

  /// Returns the events the world keeps, at most EVENT_CAPACITY, oldest
  /// first.
  [[nodiscard]] const std::vector<Event>& events() const;

  /// Returns the steps of people's progress the world keeps, each skill
  /// learned and realm reached, at most PROGRESS_CAPACITY, oldest first.
  [[nodiscard]] const std::vector<Progress>& progress() const;

  /// Returns the events kept that reach person `person` as rumours, newest
  /// first: those within the rumour reach of where it stands,
  /// and wherever they happened, the births and deaths of its kin (its
  /// family, spouses, parents and children) and what befell its faction: a
  /// village of it founded, raided or ruined, or by it raided or ruined, a
  /// sect of it fallen, or its lead changing hands. Throws std::out_of_range
  /// when there is no such person.
  [[nodiscard]] std::vector<Event> rumours(std::size_t person) const;

  /// Returns why person `person` may not talk to person `other`, or
  /// TalkRefusal::NONE when it may. Throws std::invalid_argument when there
  /// is no such person.
  [[nodiscard]] TalkRefusal talk_refusal(std::size_t person,
                                         std::size_t other) const;

  /// Returns what person `person` offers one who talks to it, in the order
  /// they are offered: nothing once it is dead, and otherwise its news, then
  /// its wares while it sells any. Throws std::out_of_range when there is no
  /// such person.
  [[nodiscard]] std::vector<Offer> offers(std::size_t person) const;

  /// Returns the trade's wares, in order, or none without a trade.
  [[nodiscard]] const std::vector<Ware>& wares() const;

  /// Returns whether person `person` sells ware `ware`: while it lives and
  /// its best discipline, its highest talent limit, is the ware's. This is
  /// the one rule of who sells what. Throws std::out_of_range when there is
  /// no such person or ware.
  [[nodiscard]] bool sells(std::size_t person, std::size_t ware) const;

  /// Returns why person `buyer` may not buy one of ware `ware` from person
  /// `seller`, or BuyRefusal::NONE when it may. Throws std::invalid_argument
  /// when there is no such person or ware.
  [[nodiscard]] BuyRefusal buy_refusal(std::size_t buyer, std::size_t seller,
                                       std::size_t ware) const;

  /// Person `buyer` buys one of ware `ware` from person `seller` at once,
  /// its price in silver going to the seller, the same for the player's
  /// body as for an NPC. Throws std::invalid_argument when buy_refusal()
  /// names a reason.
  void buy(std::size_t buyer, std::size_t seller, std::size_t ware);

  /// Returns why person `person` may not consume ware `ware`, or
  /// ConsumeRefusal::NONE when it may. Throws std::invalid_argument when
  /// there is no such person or ware.
  [[nodiscard]] ConsumeRefusal consume_refusal(std::size_t person,
                                               std::size_t ware) const;

  /// Person `person` consumes one of ware `ware` at once, its health rising
  /// by the ware's up to newborn_health and its qi by the ware's up to its
  /// store, the same for the player's body as for an NPC. Throws
  /// std::invalid_argument when consume_refusal() names a reason.
  void consume(std::size_t person, std::size_t ware);

 private:
  friend class SaveCodec;

  /// A world holding `map`, `rules` and `life` and nothing else yet, for a
  /// save to fill in before restore().
  World(Map map, Rules rules, Life life);

  /// Rebuilds what a world derives from the rest of its state. Throws
  /// std::invalid_argument when that state breaks an invariant the
  /// constructor and tick() keep: a reference to a record that does not
  /// exist, a living person on no roster or on two, or data outside the
  /// constructor's limits.
  void restore();
  [[nodiscard]] bool rostered() const;

  struct Army {
    std::size_t faction;
    std::vector<Skill> skills;
    Order order;
    FlowField field;
    std::vector<std::size_t> roster;
    bool steered = false;
  };

  /// A crowd slot no person holds.
  static constexpr std::size_t ABSENT = std::numeric_limits<std::size_t>::max();

  /// Every living person as the tick began, indexed together, with the
  /// person and the army of each.
  struct Crowd {
    SpatialHash hash;
    std::vector<std::size_t> people;
    std::vector<std::size_t> armies;
    /// Each person's index in the crowd, or ABSENT for one not in it.
    std::vector<std::size_t> slot;
  };

  [[nodiscard]] std::uint64_t next_random();
  [[nodiscard]] bool within_reach(const Army& army, const VillageSpec& spec,
                                  std::int32_t reach) const;
  bool shift(std::size_t structure, Destroyer striker, std::int64_t effect);
  [[nodiscard]] std::optional<std::size_t> aim_of(std::size_t user,
                                                  const Skill& skill,
                                                  const Crowd& crowd) const;
  void strike(std::size_t user, const Crowd& crowd,
              const std::vector<std::uint32_t>& edges,
              std::vector<std::uint64_t>& damage);
  void use(std::size_t user, const Crowd& crowd, std::size_t index,
           Position toward, std::optional<std::size_t> locked,
           const std::vector<std::uint32_t>& edges,
           std::vector<std::uint64_t>& damage);
  bool cover(std::size_t user, const Crowd& crowd, const Skill& skill,
             Position toward, std::optional<std::size_t> locked,
             std::uint64_t body_damage, std::vector<std::uint64_t>& damage);
  [[nodiscard]] const Skill& usable(const Person& person, const Army& army,
                                    std::size_t skill) const;
  [[nodiscard]] std::uint32_t rank_of(const Person& person) const;
  [[nodiscard]] std::size_t army_with(std::size_t person,
                                      std::size_t skill) const;
  [[nodiscard]] static bool ready(const Person& person, std::size_t skill,
                                  const Army& army, std::uint64_t due);
  /// How far an art's nature agrees with person `person`'s: the product of
  /// the learning's ease, or its potency, for each axis that differs, per
  /// mille.
  [[nodiscard]] std::uint64_t agreement(const Person& person, std::size_t art,
                                        bool potency) const;
  /// How a skill is learned: from a teacher or a manual, or by
  /// comprehension, which only a comprehensible skill allows.
  enum class Lesson : std::uint8_t { TAUGHT, COMPREHENDED };
  [[nodiscard]] std::optional<std::size_t> lesson(const Person& person,
                                                  std::size_t art, Lesson how,
                                                  std::uint32_t rank) const;
  void train();
  void cultivate();
  void comprehend(std::size_t learner);
  void recover(const Cultivation& cultivation);
  void read_manuals();
  void teach();
  [[nodiscard]] bool foes(std::size_t person, std::size_t army,
                          std::size_t other, std::size_t other_army) const;
  [[nodiscard]] std::uint64_t foe_kinds(std::size_t person,
                                        const Army& army) const;
  [[nodiscard]] std::int32_t reach(const Person& person,
                                   const Army& army) const;
  [[nodiscard]] bool claims(std::size_t village, Cell cell) const;
  [[nodiscard]] std::optional<Step> closing(std::size_t index, const Army& army,
                                            const Crowd& crowd) const;
  [[nodiscard]] bool sighted(const std::vector<std::size_t>& roster,
                             Position point) const;
  void end_intents(const Crowd& crowd);
  void hold(Army& chasing);
  [[nodiscard]] bool held(std::size_t index, Step pull,
                          const std::vector<bool>& standing,
                          const Crowd& crowd) const;
  [[nodiscard]] Position landing(const Person& caster, const Skill& skill,
                                 const Target& target) const;
  [[nodiscard]] CastRefusal assess_cast(std::size_t person,
                                        std::optional<std::size_t> army,
                                        const Cast& cast,
                                        std::uint64_t due) const;
  /// The army person `person` serves in, if it serves in one.
  [[nodiscard]] std::optional<std::size_t> army_of(std::size_t person) const;
  void start_duel(std::size_t army, std::size_t faction);
  [[nodiscard]] Challenging challenging(std::size_t army) const;
  void enlist(ArmySpec& army, std::size_t pairs, std::size_t& families);
  void add_village(VillageSpec spec);
  void add_sects(std::vector<SectSpec> sects);
  [[nodiscard]] std::uint32_t best_attainment(std::size_t person) const;
  void keep_sects();
  void elect(std::size_t faction);
  void hold_elections(std::uint64_t day);
  /// The living, in index order, and the army each living person serves in,
  /// by person index.
  struct Living {
    std::vector<std::size_t> people;
    std::vector<std::size_t> army_of;
  };
  [[nodiscard]] Living living_people() const;
  /// What admission reads of a sect, counted once for many applicants.
  struct Opening {
    bool held = false;
    bool elder = false;
    std::size_t members = 0;
    std::size_t cap = 0;
  };
  [[nodiscard]] Opening opening(std::size_t sect) const;
  [[nodiscard]] ApplicationRefusal assess(std::size_t person,
                                          std::optional<std::size_t> faction,
                                          std::size_t sect,
                                          const Opening& open) const;
  void admit(const Living& living);
  void record(Cell cell, Happening what);
  void note(Advance what);
  void note_falls(std::vector<std::size_t> sects);
  [[nodiscard]] std::vector<std::size_t> record_deaths();
  [[nodiscard]] bool events_kept() const;
  [[nodiscard]] bool progress_kept() const;
  void settle_duels();
  void negotiate(const Diplomacy& diplomacy);
  void pay(const Trade& trade);
  void shop(const Trade& trade);
  void dose(const Trade& trade);
  void bear(Person& person, const Soul& soul,
            std::optional<Sex> sex = std::nullopt);
  [[nodiscard]] std::uint32_t draw(Range range);
  [[nodiscard]] std::int64_t age_in_days(const Person& person) const;
  void age();
  void found(Person& person, std::optional<Sex> sex);
  void kin(const Kinship& kinship);
  void husband(const Husbandry& husbandry);
  [[nodiscard]] Stages stages() const;
  [[nodiscard]] Service served() const;
  void tend();
  [[nodiscard]] std::vector<std::size_t> homes() const;
  /// A person unfed too long, and the army it serves in.
  struct Unfed {
    std::size_t person;
    std::size_t army;
  };
  void migrate_or_starve(Unfed unfed, std::int32_t reach,
                         std::vector<std::size_t>& population);
  [[nodiscard]] bool may_bear(std::size_t mother, const Kinship& kinship) const;
  [[nodiscard]] std::optional<Sex> scarcer_sex(std::size_t village,
                                               const Kinship& kinship) const;
  [[nodiscard]] bool free_to_marry(std::size_t person,
                                   const Kinship& kinship) const;
  [[nodiscard]] Cell seat(std::size_t village) const;
  void chart_steered();
  [[nodiscard]] bool in_view(std::size_t village, Cell from) const;
  [[nodiscard]] VillageMemory memory_of(std::size_t village) const;
  void marry(std::size_t person, const std::vector<std::size_t>& living,
             const Kinship& kinship);
  void bear_child(std::size_t mother, std::vector<std::size_t>& army_of,
                  const Kinship& kinship);
  /// A person who died carrying a soul, and the army it served in.
  struct Passing {
    std::size_t dead;
    std::size_t army;
  };
  void rebirth(Passing passing);
  void grow(const Growth& growth);
  void recall(std::size_t village);
  void settle();
  [[nodiscard]] bool expand(std::size_t village, Expansion expansion,
                            const Growth& growth);
  [[nodiscard]] bool lay_building(std::size_t village, const Growth& growth);
  [[nodiscard]] bool found_village(std::size_t village, const Growth& growth);
  [[nodiscard]] bool send_raid(std::size_t village, const Growth& growth);
  [[nodiscard]] bool settling(std::size_t village) const;
  [[nodiscard]] std::vector<std::size_t> levy(std::size_t village) const;
  [[nodiscard]] std::vector<std::size_t> households(std::size_t village) const;
  /// A plan laid from a corner cell.
  struct Laid {
    std::size_t plan;
    Cell cell;
  };
  [[nodiscard]] std::optional<std::vector<std::size_t>> lay(
      const std::vector<Laid>& laid, const Growth& growth,
      const std::vector<std::size_t>& movers, Cell destination);
  void attach(std::size_t village, const std::vector<std::size_t>& structures,
              const Growth& growth);
  std::size_t form(std::size_t village, const std::vector<std::size_t>& members,
                   Cell objective, std::optional<std::size_t> into);
  /// Cells of the map, row by row: those a structure or a village's clearing
  /// covers, those that are not buildable, and those an objective or a
  /// living person takes.
  struct Ground {
    std::vector<bool> covered;
    std::vector<bool> rough;
    std::vector<bool> taken;
  };
  [[nodiscard]] Ground ground() const;
  [[nodiscard]] std::vector<Passing> passings() const;
  void steer(const Crowd& crowd, const std::vector<std::size_t>& slot,
             std::vector<Step>& pulls, std::vector<bool>& standing) const;
  void march(std::size_t army, const Crowd& crowd,
             const std::vector<std::size_t>& slot,
             const std::vector<Step>& pulls, const std::vector<bool>& standing);
  void reachable_from(const FlowField& field,
                      const std::vector<std::size_t>& members) const;

  std::uint64_t rng_state_;
  std::uint64_t tick_count_ = 0;
  Map map_;
  Rules rules_;
  std::vector<Person> people_;
  std::vector<Army> armies_;
  std::vector<Structure> structures_;
  SpatialHash structure_index_;
  // The widest footprint's side, in sub-cell units: a structure's centre lies
  // within it of every point of its footprint.
  std::int32_t structure_extent_ = 0;
  Villages villages_;
  Leaders leaders_;
  /// A sect: its name, its faction and its seat village.
  struct Sect {
    std::string name;
    std::size_t faction = 0;
    std::size_t seat = 0;
  };
  std::vector<Sect> sects_;
  std::vector<Manual> manuals_;
  /// The declaration standing between each two factions.
  Declarations declarations_;
  /// The latest events, oldest first, at most EVENT_CAPACITY.
  Chronicle<Event, EVENT_CAPACITY> events_;
  std::vector<Strike> strikes_;
  /// The latest steps of progress, oldest first, at most PROGRESS_CAPACITY.
  Chronicle<Progress, PROGRESS_CAPACITY> progress_;
  Life life_;
  std::vector<Soul> souls_;
  /// Each soul's chart, by the soul's index.
  std::vector<Chart> charts_;
  /// Each family's surname, by index among the life's surnames.
  std::vector<std::size_t> surnames_;
  // The growth plan each structure was laid from, if any.
  std::vector<std::optional<std::size_t>> plans_;
  // Each person's attack slot and token, and each foe's slots taken.
  Attacks attacks_;
};

}  // namespace murim
