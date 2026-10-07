#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "flow_field.hpp"
#include "map.hpp"
#include "spatial_hash.hpp"

namespace murim {

/// Seed a world is created from; the same seed gives the same world.
enum class Seed : std::uint64_t {};

/// Digest of a world's whole state; equal states give equal hashes.
enum class StateHash : std::uint64_t {};

/// Talent for one discipline: how much each practice adds to attainment,
/// and the most attainment it can reach. Hidden from the player.
struct Aptitude {
  std::uint32_t growth = 0;
  std::uint32_t limit = 0;
};

/// A person's sex, drawn at birth.
enum class Sex : std::uint8_t { FEMALE, MALE };

/// A person of the world. A person keeps its identity, its index among the
/// world's people, for as long as the world runs, through every army it
/// serves in and after it dies; a dead person has no health and stands in no
/// army. It may use a skill again from tick `ready_at`, and returns to
/// village `home`, if it has one, when its army disbands. It was born on
/// tick `born`, before the world began for the people it starts with, into
/// `family`, of `parents` if it had any, of sex `sex` with given name `given`
/// among its sex's names, with `talent` and `attainment` per discipline, and
/// lives `lifespan` days, extended by its attainment, if it ages at all. A
/// founder of a village, and the children settling with it, take the new
/// family it heads.
/// `spouses` are the people it married, in order, and `children` the people
/// born to it. It has gone `hunger` days unfed. A person carrying soul `soul`
/// is born again when it dies.
struct Person {
  Position position{};
  std::uint64_t health = 0;
  std::uint64_t ready_at = 0;
  std::optional<std::size_t> home;
  std::int64_t born = 0;
  std::size_t family = 0;
  std::vector<std::size_t> parents;
  Sex sex = Sex::FEMALE;
  std::size_t given = 0;
  std::vector<std::size_t> spouses;
  std::vector<std::size_t> children;
  std::uint32_t hunger = 0;
  std::vector<Aptitude> talent;
  std::vector<std::uint32_t> attainment;
  std::optional<std::uint64_t> lifespan;
  std::optional<std::size_t> soul;
};

/// A uniform draw over [mean - spread, mean + spread].
struct Spread {
  std::uint32_t mean = 0;
  std::uint32_t spread = 0;
};

/// How one discipline's talent is drawn at birth.
struct DisciplineDraw {
  std::string name;
  Spread growth;
  Spread limit;
};

/// The lowest and highest value a draw can give, or a span of ages.
struct Range {
  std::uint32_t low = 0;
  std::uint32_t high = 0;
};

/// A name as it is shown: in Korean, and romanised for English.
struct NameEntry {
  std::string korean;
  std::string romanised;
};

/// The names people bear: a surname for each family, drawn by the seed when
/// the world begins, and given names for each sex, drawn at birth.
struct Names {
  std::vector<NameEntry> surnames;
  std::vector<NameEntry> female;
  std::vector<NameEntry> male;
};

/// A person's full name: its family's surname and its given name.
struct Name {
  NameEntry surname;
  NameEntry given;
};

/// How people marry and have children, as data, checked once a day. A
/// living person of `marrying_age`, in days, with a home and no living
/// spouse marries with a chance of `marriage_chance` per mille a year: a
/// partner of the other sex and another family, as free to marry, whose
/// home is the same village or one whose centre lies within `neighbourhood`
/// cells of its own, drawn by the seed. A wife of `bearing_age` whose husband
/// lives bears a child with a chance of `birth_chance` per mille a year, no
/// sooner than `birth_spacing` days after her last, while the world holds
/// fewer than MAX_PEOPLE living. The child is born by the one birth rule
/// into its father's family, or its mother's unless `fathers_family`, and
/// at that parent's side, home and army. With `balanced_sexes` it takes the
/// sex fewer among the living unmarried whose home is its own and who are not
/// past marrying age, children included; otherwise, or when they are as many,
/// its sex is drawn at even odds.
struct Kinship {
  Range marrying_age;
  std::int32_t neighbourhood = 0;
  std::uint32_t marriage_chance = 0;
  Range bearing_age;
  std::uint32_t birth_chance = 0;
  std::uint32_t birth_spacing = 0;
  bool fathers_family = true;
  bool balanced_sexes = false;
};

/// How a village feeds its people, as data, checked once a day. Each person
/// whose home it is eats `consumption` food a day from its stock; while the
/// stock cannot feed them all, none eats, and a person unfed for
/// `hunger_days` days moves, by the seed, to a village within
/// `migration_reach` cells with room and food, or dies. On the first day of
/// winter its intact fields add their harvest to its stock, which never
/// holds more than its storage.
struct Husbandry {
  std::uint32_t consumption = 0;
  std::uint32_t hunger_days = 0;
  std::int32_t migration_reach = 0;
};

/// The most a chance per mille may be.
constexpr std::uint32_t PER_MILLE = 1'000;

/// What a field shows: sown in spring, growing in summer, ripe in autumn,
/// fallow in winter, and burnt whenever it is below its intact stage.
enum class FieldState : std::uint8_t { SOWN, GROWING, RIPE, FALLOW, BURNT };

/// What a structure gives its village: the people it houses, the food it
/// stores, and the food it yields each harvest, which makes it a field.
struct Holding {
  std::uint32_t housing = 0;
  std::uint32_t storage = 0;
  std::uint32_t harvest = 0;
};

/// What a village pressing against its cap may do: raise a structure, found
/// a village on free land, or raid a hostile neighbour.
enum class Expansion : std::uint8_t { BUILD, FOUND, RAID };

/// A structure growth may lay: the `kind` of building it is, which names its
/// art, its footprint, whether it blocks movement and what it holds.
struct Plan {
  std::string kind;
  Footprint footprint{1, 1};
  bool blocks = true;
  Holding holding;
};

/// Plan `plan` laid at `offset` from a new village's corner.
struct Placement {
  std::size_t plan = 0;
  Cell offset{};
};

/// How villages grow, as data, checked every `interval` days. A village with
/// living residents presses against its cap once the people whose home it is
/// reach `pressure` per mille of it; it then takes the first expansion of its
/// owner's stance, `stances` by faction, that it can:
/// - build: while every structure of its own is intact, it lays the plan of
///   `builds` that most raises its scarcest holding, in people, as rubble
///   on free buildable cells within `vicinity` cells of its centre, nearest
///   it, for its residents to raise by the repair rule
/// - found: with more residents than `settlers` and none of its people still
///   settling, it lays `layout` as rubble on a `settlement` area of free
///   buildable land, drawn by the seed among those whose centre lies within
///   `reach` cells of its own, `spacing` cells clear of any structure and held
///   by no other faction; `settlers` of its residents, a founder's household
///   first, march there as the new village's residents, and the founder heads a
///   new family; the layout makes a village of `settlement_kind`, which names
///   its art
/// - raid: with more residents than `raiders` and no party out, it sends
///   `raiders` of them against the nearest village of a faction it is hostile
///   to, with living residents and its centre within `reach` cells
/// A cell lies in the territory of the owner of the nearest village with
/// living residents whose centre lies within `claim` cells of it.
struct Growth {
  std::uint32_t interval = 1;
  std::uint32_t pressure = PER_MILLE;
  std::int32_t reach = 0;
  std::int32_t vicinity = 0;
  std::int32_t spacing = 0;
  std::int32_t claim = 0;
  std::vector<Plan> plans;
  std::vector<std::size_t> builds;
  Footprint settlement{1, 1};
  std::string settlement_kind;
  std::vector<Placement> layout;
  std::uint32_t settlers = 0;
  std::uint32_t raiders = 0;
  std::vector<std::vector<Expansion>> stances;
};

/// How people are born, age and are born again, as data. Each birth draws a
/// talent per discipline and a lifespan in days. A person ages out when its
/// age passes its lifespan plus `days_per_attainment` days for every point of
/// attainment it holds. A soul's past lives widen its next talent draw: a
/// discipline it practised draws its limit `discipline_widening` further
/// down and up, and every discipline `upper_widening` further up, each
/// divided by the lives it has lived, so the widening shrinks as lives
/// repeat. Without a lifespan no one ages. A soul is born again, and a child
/// born, with `newborn_health`. The people a world starts with are
/// `founding_age` days old, or newborn without one. People marry and have
/// children by `kinship`, if given, and bear `names`. Villages feed their
/// people by `husbandry`, if given, which caps how many a village holds, and
/// grow by `growth`, if given, as they press against their caps.
struct Life {
  std::vector<DisciplineDraw> disciplines;
  std::uint64_t newborn_health = 1;
  std::optional<Spread> lifespan;
  std::uint32_t days_per_attainment = 0;
  std::uint32_t discipline_widening = 0;
  std::uint32_t upper_widening = 0;
  std::optional<Spread> founding_age;
  std::optional<Kinship> kinship;
  Names names;
  std::optional<Husbandry> husbandry;
  std::optional<Growth> growth;
};

/// What a soul carries between bodies: the lives it has lived, and whether
/// each discipline was practised in one of them.
struct Soul {
  std::uint32_t lives = 0;
  std::vector<bool> practised;
};

/// Returns the range a soul with `soul` history draws discipline
/// `discipline`'s talent limit from under `life`; an empty history draws
/// from the discipline's own spread.
[[nodiscard]] Range limit_range(const Life& life, const Soul& soul,
                                std::size_t discipline);

/// The world's calendar: days in a season, seasons in a year. How many ticks
/// make a day is the world's data (Rules).
struct Calendar {
  std::uint64_t days_per_season;
  std::uint64_t seasons_per_year;
};

/// Thirty days to a season, four seasons a year.
constexpr Calendar CALENDAR{30, 4};

/// Days in a year.
constexpr std::uint64_t DAYS_PER_YEAR =
    CALENDAR.days_per_season * CALENDAR.seasons_per_year;

/// A minute of play to a day.
constexpr std::uint64_t TICKS_PER_DAY = 1'800;

/// A day of the calendar, each part counted from 1 but `season`, from 0.
struct Date {
  std::uint64_t year;
  std::uint64_t season;
  std::uint64_t day;
};

/// A person as a world is created with: standing at `cell`, with `health`.
struct Recruit {
  Cell cell;
  std::uint64_t health;
};

/// An ability as data. A body uses it on the nearest living hostile body
/// within `range`, or on its own position when `range` is 0; every hostile
/// body within `radius` of that point takes `body_damage`, and every
/// structure whose footprint that disc reaches loses `structure_damage` of
/// durability. A negative `structure_damage` restores durability instead, up
/// to the intact stage's floor. A village with living residents takes damage
/// only from factions hostile to its owner, and repair only from the others.
/// The user waits `cooldown` ticks between uses.
struct Skill {
  std::int32_t range;
  std::int32_t radius;
  std::uint64_t body_damage;
  std::int32_t structure_damage;
  std::uint64_t cooldown;
};

/// An army as a world is created with: its `members`, each a new person,
/// bound for `objective`, of `faction`, choosing among `skills` by the
/// world's seed. An army is a roster of people, never a count.
struct ArmySpec {
  Cell objective;
  std::vector<Recruit> members;
  std::size_t faction;
  std::vector<Skill> skills;
};

/// What an army does: march toward `objective`, and at each strike use one
/// of `skills`, indices into the army's own skills, drawn by the seed. An
/// army created from an ArmySpec follows its objective with all its skills
/// until it is given another order.
struct Order {
  Cell objective;
  std::vector<std::size_t> skills;
};

/// A breakable object covering `footprint` from `cell`, its top-left cell.
/// Its cells block movement for as long as the world runs, whatever its
/// stage.
struct Structure {
  Cell cell{};
  std::uint32_t durability = 0;
  Footprint footprint{1, 1};
  /// Whether its footprint blocks movement; a field does not.
  bool blocks = true;
};

/// A village as a world is created with: its structures by index, and the
/// army living in it, whose members make it their home. The first two
/// members of each of its `households` start as one: a man and a woman,
/// married to each other, each heading a family of their own.
struct VillageSpec {
  std::vector<std::size_t> structures;
  std::size_t residents;
  /// What each of `structures` holds, in their order; none holds nothing.
  std::vector<Holding> holdings;
  std::uint32_t households = 0;
};

/// Where a village stands in its life cycle, derived from its structures,
/// its residents and the bodies standing in it.
enum class VillageState : std::uint8_t {
  PROSPEROUS,
  RAIDED,
  RUINS,
  REBUILDING
};

/// The land a founded village was laid out on: the growth's settlement `area`
/// from `corner`, making a village of `kind`.
struct Settlement {
  std::string kind;
  Cell corner;
  Footprint area;
};

/// The army whose strike brought a village's last standing structure to
/// rubble, and its faction.
struct Destroyer {
  std::size_t army;
  std::size_t faction;
};

/// The most cells a structure's footprint may span along either side.
constexpr std::int32_t MAX_FOOTPRINT = 256;

/// How broken a structure is, from durability by the floors in Rules.
enum class Stage : std::uint8_t { INTACT, DAMAGED, DESTROYED, RUBBLE };

/// The largest range, radius and separation a world accepts, in sub-cell
/// units; it keeps the products of their squares within 64 bits.
constexpr std::int32_t MAX_REACH = std::int32_t{1} << 26;

/// The most living people a world may hold.
constexpr std::size_t MAX_PEOPLE = 500;

/// The most health one person may have.
constexpr std::uint64_t MAX_HEALTH = std::uint64_t{1} << 43;

/// The most damage a skill may do to a body.
constexpr std::uint64_t MAX_BODY_DAMAGE = std::uint64_t{1} << 19;

/// The longest cooldown a skill may have, in ticks.
constexpr std::uint64_t MAX_COOLDOWN = std::uint64_t{1} << 32;

/// The most a talent's growth or limit may be drawn as, its widenings
/// included.
constexpr std::uint32_t MAX_TALENT = std::uint32_t{1} << 20;

/// The longest lifespan a birth may draw, in days.
constexpr std::uint32_t MAX_LIFESPAN_DAYS = std::uint32_t{1} << 20;

/// How a faction's leadership passes by duel. A person may challenge the
/// leader, if `members_only` holds, only from an army of the faction; the two
/// then fight by the one skill rule, hostile to each other alone, until one
/// has no more than `yield_health` left, the leader keeping the lead when
/// both fall together. A challenger who wins leads the faction, and, if
/// `allies_turn_hostile` holds, every faction at peace with it turns hostile
/// to it.
struct Duel {
  bool members_only = true;
  std::uint64_t yield_health = 0;
  bool allies_turn_hostile = true;
};

/// Why an army's lead may not challenge a faction's leader, the first that
/// holds in this order; `NONE` when it may.
enum class ChallengeRefusal : std::uint8_t {
  NONE,
  /// The faction does not pass its lead by duel.
  NO_SUCCESSION,
  /// The faction has no leader.
  NO_LEADER,
  /// The army has no living member to challenge.
  NO_CHALLENGER,
  /// The duel takes members only, and the army is of another faction.
  NOT_A_MEMBER,
  /// The army's lead is the leader.
  IS_LEADER,
  /// A duel for the faction's lead is being fought.
  DUEL_IN_PROGRESS,
};

/// A duel being fought for a faction's lead: the challenger and the leader,
/// as person indices.
struct Duellists {
  std::size_t challenger;
  std::size_t leader;
};

/// A faction's leadership as a world is created with: the person leading
/// it, if anyone does, and how the lead passes, if it passes at all.
struct Leadership {
  std::optional<std::size_t> leader;
  std::optional<Duel> succession;
};

/// The data a world's rules read.
struct Rules {
  /// Sub-cell units people keep apart; 0 lets them overlap.
  std::int32_t separation;
  /// `hostile[a][b]`: whether bodies of faction `a` attack those of `b`.
  std::vector<std::vector<bool>> hostile;
  /// The lowest durability of the intact, damaged and destroyed stages; a
  /// structure below all three is rubble.
  std::array<std::uint32_t, 3> stage_floors;
  /// Ticks in a day of the calendar.
  std::uint64_t ticks_per_day = TICKS_PER_DAY;
};

/// Sub-cell units a person moves per tick.
constexpr std::int32_t SPEED = 16;

/// The simulated world, advanced one deterministic step at a time.
class World {
 public:
  /// Throws std::invalid_argument when the separation, or a skill's range or
  /// radius, lies outside [0, MAX_REACH], `hostile` is not square, a recruit
  /// has no health or more than MAX_HEALTH, the world would hold more than
  /// MAX_PEOPLE people, an army has no skills or a faction outside
  /// `hostile`, a skill does more than MAX_BODY_DAMAGE to a person or waits
  /// longer than MAX_COOLDOWN, a structure's footprint is not 1 to
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
  /// its centre, or stances for more factions than `hostile`. Within these
  /// limits no damage, health, durability or cooldown arithmetic exceeds its
  /// type.
  World(Seed seed, Map map, std::vector<ArmySpec> armies,
        std::vector<Structure> structures, Rules rules,
        std::vector<VillageSpec> villages = {},
        std::vector<Leadership> leadership = {}, Life life = {});

  /// Gives army `army` an order, which it follows from the next tick on.
  /// With `challenge`, the army's lead also challenges that faction's
  /// leader to a duel, the same order for the player's body as for an NPC.
  /// Throws std::invalid_argument when there is no such army, the objective
  /// is not passable or not reachable from where one of its members stands,
  /// or the skills are empty or name one the army lacks; or it challenges a
  /// faction with no duel succession or no leader, from an army with no
  /// members, an army of another faction where the duel takes members only,
  /// with the leader as its lead, or while that faction's duel is fought.
  void order(std::size_t army, Order order,
             std::optional<std::size_t> challenge = std::nullopt);

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

  /// Returns the date the world has reached.
  [[nodiscard]] Date date() const;

  /// Returns person `person`'s age in whole years of the calendar.
  [[nodiscard]] std::int64_t age_in_years(std::size_t person) const;

  /// Returns the faction of the army person `person` serves in, if it
  /// serves in one; the dead serve in none.
  [[nodiscard]] std::optional<std::size_t> faction_of(std::size_t person) const;

  /// Returns the faction army `army` fights for now.
  [[nodiscard]] std::size_t faction(std::size_t army) const;

  /// Returns the skills army `army` has now.
  [[nodiscard]] const std::vector<Skill>& skills(std::size_t army) const;

  /// Returns the person leading faction `faction`, if it has a leader.
  [[nodiscard]] std::optional<std::size_t> leader(std::size_t faction) const;

  /// Returns why army `army`'s lead may not challenge faction `faction`'s
  /// leader, which is why order() would refuse that challenge, or
  /// ChallengeRefusal::NONE when it may. Throws std::invalid_argument when
  /// there is no such army.
  [[nodiscard]] ChallengeRefusal challenge_refusal(std::size_t army,
                                                   std::size_t faction) const;

  /// Returns the duel being fought for faction `faction`'s lead, if one is.
  [[nodiscard]] std::optional<Duellists> duel(std::size_t faction) const;

  /// Returns whether people of faction `from` attack those of `target`.
  [[nodiscard]] bool hostile(std::size_t from, std::size_t target) const;

  /// Returns who last brought village `village` to ruins, if anyone has; it
  /// is kept when the village is rebuilt.
  [[nodiscard]] std::optional<Destroyer> destroyer(std::size_t village) const;

  /// Returns the land village `village` was founded on, or nothing for a
  /// village the world was created with.
  [[nodiscard]] std::optional<Settlement> settlement(std::size_t village) const;

 private:
  /// A duel fought for faction `faction`'s lead, under its rule.
  struct Fight {
    std::size_t faction = 0;
    std::size_t challenger = 0;
    std::size_t leader = 0;
    Duel rule;
  };

  struct Village {
    VillageSpec spec;
    std::optional<Destroyer> destroyer;
    std::uint64_t stock = 0;
    /// The army its raids are fought by, once it has raided, and the village
    /// its party is out against.
    std::optional<std::size_t> party;
    std::optional<std::size_t> raiding;
    /// The corner of the land it was founded on, if it was founded.
    std::optional<Cell> founded;
  };

  struct Army {
    std::size_t faction;
    std::vector<Skill> skills;
    Order order;
    FlowField field;
    std::vector<std::size_t> roster;
  };

  /// Every living person as the tick began, indexed together, with the
  /// person and the army of each.
  struct Crowd {
    SpatialHash hash;
    std::vector<std::size_t> people;
    std::vector<std::size_t> armies;
  };

  [[nodiscard]] std::uint64_t next_random();
  [[nodiscard]] bool within_reach(const Army& army, const VillageSpec& spec,
                                  std::int32_t reach) const;
  bool shift(std::size_t structure, Destroyer striker, std::int64_t effect);
  void strike(std::size_t user, const Crowd& crowd,
              std::vector<std::uint64_t>& damage);
  [[nodiscard]] bool duelling(std::size_t one, std::size_t other) const;
  void start_duel(std::size_t army, std::size_t faction);
  [[nodiscard]] ChallengeRefusal refusal(const Army& challenging,
                                         std::size_t faction) const;
  void enlist(ArmySpec& army, std::size_t pairs, std::size_t& families);
  void add_village(VillageSpec spec);
  void settle_duels();
  void bear(Person& person, const Soul& soul,
            std::optional<Sex> sex = std::nullopt);
  [[nodiscard]] std::uint32_t draw(Range range);
  [[nodiscard]] std::int64_t age_in_days(const Person& person) const;
  void age();
  void found(Person& person, std::optional<Sex> sex);
  void kin(const Kinship& kinship);
  void husband(const Husbandry& husbandry);
  [[nodiscard]] Holding held(std::size_t village, Stage worst) const;
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
  /// Cells of the map, row by row: those a structure covers, those that are
  /// not buildable, and those an objective or a living person takes.
  struct Ground {
    std::vector<bool> covered;
    std::vector<bool> rough;
    std::vector<bool> taken;
  };
  [[nodiscard]] Ground ground() const;
  [[nodiscard]] std::vector<Passing> passings() const;
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
  std::vector<Village> villages_;
  std::vector<Leadership> leadership_;
  std::vector<Fight> duels_;
  Life life_;
  std::vector<Soul> souls_;
  /// Each family's surname, by index among the life's surnames.
  std::vector<std::size_t> surnames_;
  // The village each structure belongs to, if any.
  std::vector<std::optional<std::size_t>> village_of_;
  // The growth plan each structure was laid from, if any.
  std::vector<std::optional<std::size_t>> plans_;
};

}  // namespace murim
