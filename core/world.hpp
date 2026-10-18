#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "area_fields.hpp"
#include "areas.hpp"
#include "arts.hpp"
#include "attack.hpp"
#include "calendar.hpp"
#include "chart.hpp"
#include "chronicle.hpp"
#include "coarse_graph.hpp"
#include "diplomacy.hpp"
#include "event.hpp"
#include "flow_field.hpp"
#include "growth.hpp"
#include "kinship.hpp"
#include "layout.hpp"
#include "leadership.hpp"
#include "life.hpp"
#include "limits.hpp"
#include "map.hpp"
#include "names.hpp"
#include "occupation.hpp"
#include "person.hpp"
#include "progress.hpp"
#include "schedule.hpp"
#include "sect.hpp"
#include "skill.hpp"
#include "spatial_hash.hpp"
#include "talent.hpp"
#include "talk.hpp"
#include "trade.hpp"
#include "village.hpp"
#include "village_fields.hpp"

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

/// Where a far army stands (ADR 0077): the area and cell its lead stood on
/// when it went far, and how far along its coarse path it has come since,
/// which no rule advances until the far tier's rules are built.
struct FarAnchor {
  std::size_t area;
  Cell cell;
  std::uint32_t progress;
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
  /// residents pair into, or a window over its centre, square and structures
  /// wider or taller than VillageFields::MAX_SIDE, a faction's leader is no
  /// person the world creates, or `leadership` names more factions than
  /// `hostile`; a faction it leaves out has no leader; a day lasts no tick;
  /// or `life` could draw a talent above MAX_TALENT or a lifespan or founding
  /// age above MAX_LIFESPAN_DAYS, gives a newborn no health or more than
  /// MAX_HEALTH, or gives kinship an age range running downward, a chance
  /// above PER_MILLE or a neighbourhood outside [0, MAX_REACH], or gives
  /// growth no interval, a pressure above PER_MILLE, a reach, vicinity,
  /// spacing or claim outside [0, MAX_REACH], tiers keeping more than a
  /// village's window holds (VillageFields::holds), a plan or settlement of a
  /// side outside [1, MAX_FOOTPRINT], a build or placement naming no plan, a
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

  /// Returns the food village `village`'s intact fields add to its stock at
  /// each harvest, before its storage bounds the stock.
  [[nodiscard]] std::uint64_t harvest(std::size_t village) const;

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

  /// Returns the kind of the plan structure `structure` was laid from, or
  /// nothing for one the world was created with. Throws std::out_of_range
  /// when there is no such structure.
  [[nodiscard]] std::optional<std::string> laid_kind(
      std::size_t structure) const;

  /// Returns the faction whose territory each cell of the map lies in, row by
  /// row: the owner of the nearest village with living residents whose
  /// centre lies within the growth's claim, the lowest village among equals,
  /// or nothing beyond every such village and without growth.
  [[nodiscard]] std::vector<std::optional<std::size_t>> territory() const;

  /// Gives living person `person` a new soul with no history, so that it is
  /// born again when it dies. Returns the soul. Throws std::invalid_argument
  /// when the person is dead or does not exist.
  std::size_t ensoul(std::size_t person);

  /// Returns the army the latest body carrying soul `soul` serves in, or
  /// nothing when it serves in none, as once it has died.
  [[nodiscard]] std::optional<std::size_t> soul_army(std::size_t soul) const;

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

  /// Returns the army whose roster holds person `person`, if one does; the
  /// dead serve in none.
  [[nodiscard]] std::optional<std::size_t> army_of(std::size_t person) const;

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

  /// Returns the art person `person` learned first, its heart method, or
  /// nothing while it knows no art. Throws std::out_of_range when there is no
  /// such person.
  [[nodiscard]] std::optional<std::size_t> heart_art(std::size_t person) const;

  /// Returns how far art `art`'s nature agrees with person `person`'s, per
  /// mille, as the learning eases it: PER_MILLE for an art of its own nature
  /// or while it knows none, less for one of another. Throws
  /// std::out_of_range when there is no such person or art.
  [[nodiscard]] std::uint64_t art_agreement(std::size_t person,
                                            std::size_t art) const;

  /// Returns whether skill `skill` of art `art` lies within person
  /// `person`'s realm, so that its rank does not bar the person from it.
  /// Throws std::out_of_range when there is no such person, art or skill.
  [[nodiscard]] bool within_realm(std::size_t person, std::size_t art,
                                  std::size_t skill) const;

  /// Returns the realms people cultivate into, ascending.
  [[nodiscard]] const std::vector<Realm>& realms() const;

  /// Returns the arts people learn.
  [[nodiscard]] const std::vector<Art>& arts() const;

  /// Returns the person leading faction `faction`, if it has a leader.
  [[nodiscard]] std::optional<std::size_t> leader(std::size_t faction) const;

  /// Returns whether faction `faction` elects its leader.
  [[nodiscard]] bool elects(std::size_t faction) const;

  /// Returns whether faction `faction`'s lead passes by duel. Throws
  /// std::out_of_range when there is no such faction.
  [[nodiscard]] bool passes_by_duel(std::size_t faction) const;

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

  /// Gives the world the tier values of a quality profile, or none, so that
  /// it runs live everywhere (ADR 0048); a world is created with none. The
  /// profile is neither saved nor hashed, so a loaded world is given one
  /// again. Takes effect when the tiers are next recomputed.
  void set_profile(std::optional<Profile> profile);

  /// Recomputes every area's tier from where the steered armies' living
  /// members stand (ADR 0042), as tick() does on each tick a second divides,
  /// and hands each army whose tier changes over: an army acts at the nearest
  /// tier among the areas its living members stand in. An army going far
  /// keeps every member's record and takes its lead's area and cell as its
  /// anchor; one coming back stands its lead on the open cell nearest the
  /// anchor and the rest, in roster order, on the open cells nearest it, with
  /// every record otherwise as it was. With no profile, or no steered army,
  /// every area is live.
  void retier();

  /// Returns every area's tier.
  [[nodiscard]] const Areas& areas() const;

  /// Returns the fields people step by, for a measure to read.
  [[nodiscard]] const AreaFields& fields() const;

  /// Returns where army `army` stands while it is far; nothing while it is
  /// live or stepped. Throws std::out_of_range when there is no such army.
  [[nodiscard]] std::optional<FarAnchor> far(std::size_t army) const;

  /// Returns who last brought village `village` to ruins, if anyone has; it
  /// is kept when the village is rebuilt.
  [[nodiscard]] std::optional<Destroyer> destroyer(std::size_t village) const;

  /// Returns the index of the growth's tier village `village` has reached,
  /// kept once reached (ADR 0076); 0 for a world that does not grow. Throws
  /// std::out_of_range when there is no such village.
  [[nodiscard]] std::size_t tier(std::size_t village) const;

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

  /// Returns whom person `person` would talk to: the nearest person it may
  /// talk to, or would but for hostility, the first in army and roster order
  /// among equals; nothing when there is none. Throws std::out_of_range when
  /// there is no such person.
  [[nodiscard]] std::optional<std::size_t> talk_target(
      std::size_t person) const;

  /// Returns what person `person` offers one who talks to it, in the order
  /// they are offered: nothing once it is dead, and otherwise its news, then
  /// its wares while it sells any. Throws std::out_of_range when there is no
  /// such person.
  [[nodiscard]] std::vector<Offer> offers(std::size_t person) const;

  /// Returns the trade's wares, in order, or none without a trade.
  [[nodiscard]] const std::vector<Ware>& wares() const;

  /// Returns the occupations, in order, or none without them.
  [[nodiscard]] const std::vector<Occupation>& occupations() const;

  /// Returns whether person `person` sells ware `ware`: while it lives and
  /// its occupation sells the ware (ADR 0081). This is the one rule of who
  /// sells what. Throws std::out_of_range when there is no such person or
  /// ware.
  [[nodiscard]] bool sells(std::size_t person, std::size_t ware) const;

  /// Returns the occupation, by index among the occupations, person `person`
  /// holds, if any (ADR 0081's trade). Once a day, and as the world is
  /// created, each village fills its occupations' openings from the living
  /// people whose home it is, by assign(); a living person with no home
  /// holds none, and the dead keep what they held. Throws std::out_of_range
  /// when there is no such person.
  [[nodiscard]] std::optional<std::size_t> occupation_of(
      std::size_t person) const;

  /// Returns what person `person` is doing by its schedule (ADR 0081): one
  /// of a village's residents follows its occupation's schedule, or the idle
  /// one without an occupation, while its army is not steered and is bound
  /// for the village's centre with no attack intent, going in each band of
  /// hours to its household's house, its workplace or the village's square.
  /// It is walking while more than a cell from its spot there, and otherwise
  /// working at its workplace in a work band, at home at its house in a home
  /// band, or idle; anyone following no schedule is idle. Throws
  /// std::out_of_range when there is no such person.
  [[nodiscard]] Activity activity(std::size_t person) const;

  /// The path fields the villages' schedules hold (ADR 0081), the bytes
  /// their cells take, and how many fields have been built, lodgings found
  /// and errands found since the world was created or loaded.
  struct FieldUse {
    std::size_t fields = 0;
    std::size_t bytes = 0;
    std::uint64_t builds = 0;
    std::uint64_t lodgings = 0;
    std::uint64_t errands = 0;
  };

  /// Returns the path fields the villages' schedules hold.
  [[nodiscard]] FieldUse schedule_fields() const;

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

  // An army's course: bound for its objective while it is live or stepped,
  // each member stepping by its area's field (ADR 0043), or where it stands
  // while it is far, never both.
  struct Bound {};
  using Course = std::variant<Bound, FarAnchor>;

  struct Army {
    std::size_t faction;
    std::vector<Skill> skills;
    Order order;
    Course course;
    std::vector<std::size_t> roster;
    bool steered = false;
  };

  // Whether `army` is far.
  [[nodiscard]] static bool far_off(const Army& army);
  [[nodiscard]] static std::int32_t structure_reach(const Army& army);
  [[nodiscard]] std::optional<std::size_t> bearing_on(std::size_t army) const;
  [[nodiscard]] bool standing_within(std::size_t village, Position from,
                                     std::int32_t reach) const;
  [[nodiscard]] std::optional<Cell> next_objective(const Army& army,
                                                   std::size_t village) const;
  [[nodiscard]] std::optional<std::size_t> nearest_standing(
      std::size_t village, Position from) const;
  [[nodiscard]] std::optional<Cell> nearest_in_ring(const Structure& structure,
                                                    std::int32_t ring,
                                                    Position from,
                                                    Cell objective,
                                                    std::size_t village) const;

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
  [[nodiscard]] std::size_t army_with(std::size_t person,
                                      std::size_t skill) const;
  [[nodiscard]] static bool ready(const Person& person, std::size_t skill,
                                  const Army& army, std::uint64_t due);
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
  // The foe a person engages, as its index in the crowd, and how it closes
  // on it.
  struct Closing {
    std::optional<std::size_t> foe;
    Approach approach = OnFoe{};
  };
  [[nodiscard]] std::vector<bool> granting(const Crowd& crowd) const;
  [[nodiscard]] std::optional<std::size_t> engaged_foe(
      std::size_t index, const Army& army, const Crowd& crowd,
      const std::vector<bool>& granting) const;
  [[nodiscard]] std::vector<Closing> close_in(const Crowd& crowd);
  void bear_on_villages(const Crowd& crowd,
                        const std::vector<Closing>& closings);
  [[nodiscard]] std::optional<Step> closing(std::size_t index, const Army& army,
                                            const Crowd& crowd,
                                            const Closing& close) const;
  [[nodiscard]] bool sighted(const std::vector<std::size_t>& roster,
                             Position point) const;
  void end_intents(const Crowd& crowd);
  void hold(Army& chasing);
  [[nodiscard]] bool held(std::size_t index, Step pull,
                          const std::vector<bool>& standing,
                          const Crowd& crowd) const;
  [[nodiscard]] bool may_aim(std::size_t self, const Army& own,
                             std::size_t target) const;
  [[nodiscard]] Position landing(const Person& caster, const Skill& skill,
                                 const Target& target) const;
  [[nodiscard]] CastRefusal assess_cast(std::size_t person,
                                        std::optional<std::size_t> army,
                                        const Cast& cast,
                                        std::uint64_t due) const;
  [[nodiscard]] std::size_t army_serving(std::size_t person) const;
  // The only writers of the rosters, each keeping serving_ with them.
  void enrol(std::size_t person, std::size_t army);
  void strike_off(std::size_t person);
  void strike_off_dead();
  void refit(std::size_t army, Army refitted);
  void index_rosters();
  void start_duel(std::size_t army, std::size_t faction);
  [[nodiscard]] Challenging challenging(std::size_t army) const;
  void enlist(ArmySpec& army, std::size_t pairs, std::size_t& families);
  void add_village(VillageSpec spec);
  void add_sects(std::vector<SectSpec> sects);
  [[nodiscard]] std::uint32_t best_attainment(std::size_t person) const;
  void keep_sects();
  void elect(std::size_t faction);
  void hold_elections(std::uint64_t day);
  /// The living, in index order.
  struct Living {
    std::vector<std::size_t> people;
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
  [[nodiscard]] std::optional<std::size_t> first_seller(std::size_t buyer,
                                                        std::size_t ware) const;
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
  void assign_occupations();
  // A village's grounds as its schedule reads them (ADR 0081): its square's
  // free cells, each of its structures' doors, by index in the village, and
  // the path fields of all its places over its window, built when the
  // grounds are found; derived and kept until a structure is laid on its
  // window, which its layout epoch marks, it is given a structure, or it
  // reaches a tier, neither saved nor hashed.
  struct Grounds {
    std::uint64_t version = 0;
    std::size_t tier = 0;
    std::size_t places = 0;
    std::uint64_t layout = 0;
    std::vector<Cell> square;
    std::vector<std::optional<Cell>> doors;
    VillageFields fields;
  };
  // The places, as numbered for a village's fields, where each of its
  // residents lives and works, by roster rank: 0 its square, 1 + i its i-th
  // structure, none where it has no house or workplace. Derived and kept
  // until a roster writer, a marriage, a death that leaves a spouse, a
  // changed occupation or a structure's fall changes them, neither saved nor
  // hashed.
  struct Lodging {
    std::uint64_t version = 0;
    std::size_t army = 0;
    std::uint64_t army_epoch = 0;
    std::uint64_t village_epoch = 0;
    std::vector<std::optional<std::size_t>> homes;
    std::vector<std::optional<std::size_t>> works;
  };
  // Where a person following its village's schedule goes: its key, the
  // village's grounds, the band's kind of place, the place of that kind its
  // lodging gives it, as numbered for the fields, and the wander period; and
  // the spot it stands on. `followed` is the tick it last followed its
  // schedule on. One never found holds grounds 0, which no grounds are, so
  // it shares no key.
  struct Errand {
    std::size_t village = 0;
    std::uint64_t grounds = 0;
    std::uint64_t period = 0;
    Where where = Where::HOME;
    std::size_t place = 0;
    Cell spot{};
    std::uint64_t followed = 0;
  };
  // One of a village's residents as its schedule reads it: the village, its
  // rank in the residents' roster, and the person.
  struct Follower {
    std::size_t village = 0;
    std::size_t rank = 0;
    std::size_t person = 0;
  };
  // A person's pull toward where it goes, and whether it counts as standing.
  struct Pull {
    Step step{0, 0};
    bool standing = true;
  };
  [[nodiscard]] bool follows(std::size_t village) const;
  [[nodiscard]] std::uint64_t army_epoch(std::size_t army) const;
  void bump_army(std::size_t army);
  void bump_village(std::size_t village);
  void bump_serving(std::size_t person);
  [[nodiscard]] std::uint64_t layout_epoch(std::size_t village) const;
  void bump_layout(std::size_t village);
  [[nodiscard]] bool current(const Grounds& grounds, std::size_t village) const;
  [[nodiscard]] bool current(const Lodging& lodging, std::size_t village) const;
  [[nodiscard]] Grounds grounds_of(std::size_t village) const;
  [[nodiscard]] Lodging lodging_of(std::size_t village) const;
  [[nodiscard]] std::vector<std::optional<std::size_t>> homes_of(
      std::size_t village, const std::vector<bool>& standing) const;
  [[nodiscard]] std::vector<std::optional<std::size_t>> works_of(
      std::size_t village, const std::vector<bool>& standing) const;
  [[nodiscard]] Errand errand_of(const Follower& follower,
                                 const Grounds& grounds,
                                 const Lodging& lodging) const;
  [[nodiscard]] Errand key_of(const Follower& follower, const Grounds& grounds,
                              const Lodging& lodging) const;
  [[nodiscard]] static bool same_key(const Errand& one, const Errand& other);
  [[nodiscard]] std::vector<Cell> seeds_of(std::size_t village,
                                           const Grounds& grounds,
                                           std::size_t place) const;
  void find_grounds(std::size_t village);
  void prepare_grounds();
  void follow_schedules();
  void build_field(std::size_t village, Grounds& grounds,
                   std::size_t place) const;
  [[nodiscard]] std::optional<Step> errand_pull(const Errand& errand,
                                                Cell cell) const;
  [[nodiscard]] Pull pull_of(const FlowField& field, const Army& army,
                             std::size_t rank, Cell cell) const;
  [[nodiscard]] std::vector<std::size_t> homes() const;
  void migrate_or_starve(std::size_t person,
                         std::vector<std::size_t>& population,
                         std::int32_t reach);
  [[nodiscard]] bool may_bear(std::size_t mother, const Kinship& kinship) const;
  [[nodiscard]] std::optional<Sex> scarcer_sex(std::size_t village,
                                               const Kinship& kinship) const;
  [[nodiscard]] bool free_to_marry(std::size_t person,
                                   const Kinship& kinship) const;
  [[nodiscard]] Cell seat(std::size_t village) const;
  void chart_steered();
  [[nodiscard]] VillageMemory memory_of(std::size_t village) const;
  void marry(std::size_t person, const std::vector<std::size_t>& living,
             const Kinship& kinship);
  void bear_child(std::size_t mother, const Kinship& kinship);
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
  [[nodiscard]] bool lay_square_kind(std::size_t village, const Growth& growth);
  [[nodiscard]] bool lay_building(std::size_t village, const Growth& growth);
  [[nodiscard]] bool intact(std::size_t village) const;
  [[nodiscard]] bool lay_plan(std::size_t village, std::size_t plan,
                              const Growth& growth);
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
  [[nodiscard]] std::optional<std::vector<Laid>> core_layout(
      Cell centre, const Growth& growth, const Lot& ground) const;
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
  [[nodiscard]] Lot lot() const;
  [[nodiscard]] bool unplaced(const VillageRecord& village) const;
  [[nodiscard]] std::vector<Cell> open_sites(std::size_t village,
                                             const Growth& growth) const;
  [[nodiscard]] Townsite site(std::size_t village, const Growth& growth) const;
  [[nodiscard]] Clearing square(std::size_t village) const;
  [[nodiscard]] std::vector<Structure> homes(std::size_t village) const;
  [[nodiscard]] std::vector<Passing> passings() const;
  [[nodiscard]] const std::vector<std::size_t>& fields_of(const Crowd& crowd);
  void steer(const Crowd& crowd, const std::vector<std::size_t>& fields,
             const std::vector<Closing>& closings, std::vector<Step>& pulls,
             std::vector<bool>& standing,
             std::vector<std::size_t>& seeking) const;
  void march(std::size_t army, const Crowd& crowd,
             const std::vector<std::size_t>& fields,
             const std::vector<Step>& pulls, const std::vector<bool>& standing,
             const std::vector<std::size_t>& seeking);
  void reachable_from(Cell objective,
                      const std::vector<std::size_t>& members) const;
  void go_far(Army& army);
  void come_back(Army& army, FarAnchor far);

  std::uint64_t rng_state_;
  /// The seed the world was created from, which keys its counter-based draws.
  std::uint64_t seed_;
  std::uint64_t tick_count_ = 0;
  Map map_;
  // The map's coarse graph, derived from it, and the fields people step by,
  // derived from both; neither is saved.
  CoarseGraph graph_;
  AreaFields fields_;
  // Each person's field in the tick's crowd, by crowd index: scratch, reused
  // across ticks, not saved.
  std::vector<std::size_t> crowd_fields_;
  Rules rules_;
  std::vector<Person> people_;
  std::vector<Army> armies_;
  Areas areas_;
  std::optional<Profile> profile_;
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
  // The people engaging on the tick being run, kept so a tick allocates
  // none; neither saved nor hashed.
  std::vector<Engaging> engaging_;
  /// A seller of the ware being shopped for, and the cell it stands in.
  struct Seller {
    Cell cell{};
    std::size_t person = 0;
  };
  // The army whose roster holds each person, by person index, or none: set
  // by the rosters' writers and rebuilt from the rosters on restore; neither
  // saved nor hashed.
  std::vector<std::optional<std::size_t>> serving_;
  // The sellers of the ware being shopped for, by row, column and index,
  // kept so a day allocates none; neither saved nor hashed.
  std::vector<Seller> sellers_;
  // Each village's grounds and lodging, and each person's errand, as last
  // found (ADR 0081); none is saved or hashed, and each is found again
  // whenever what it is derived from has changed.
  std::vector<Grounds> grounds_;
  std::vector<Lodging> lodgings_;
  std::vector<Errand> errands_;
  // Bumped by the writers that change a roster, and by a marriage, a death
  // that leaves a spouse or a changed occupation of one serving in it, by
  // army; by a structure of the village standing or falling, by village; and
  // by a structure laid on a village's window, by village. Neither saved nor
  // hashed.
  std::vector<std::uint64_t> army_epochs_;
  std::vector<std::uint64_t> village_epochs_;
  std::vector<std::uint64_t> layout_epochs_;
  // The last version given to a village's grounds or lodging, and the fields
  // built, lodgings found and errands found since the world was created or
  // loaded.
  std::uint64_t versions_ = 0;
  std::uint64_t field_builds_ = 0;
  std::uint64_t lodging_finds_ = 0;
  std::uint64_t errand_finds_ = 0;
};

}  // namespace murim
