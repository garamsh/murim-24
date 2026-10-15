#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "map.hpp"

namespace murim {

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

/// What a field shows: sown in spring, growing in summer, ripe in autumn,
/// fallow in winter, and burnt whenever it is below its intact stage.
enum class FieldState : std::uint8_t { SOWN, GROWING, RIPE, FALLOW, BURNT };

/// What a structure gives its village: the people it houses, the food it
/// stores, the food it yields each harvest, which makes it a field, and the
/// pupils it teaches a sect seated in the village. A workshop arms `arms` and
/// tends `tends` of the people whose home the village is; while it stands
/// and its village's arms cover them, they strike `edge` per mille harder,
/// and while its tends cover them, they recover `healing` health a day. A
/// village's capacities add up over its standing structures; its edge and
/// healing are the greatest among them.
struct Holding {
  std::uint32_t housing = 0;
  std::uint32_t storage = 0;
  std::uint32_t harvest = 0;
  std::uint32_t pupils = 0;
  std::uint32_t arms = 0;
  std::uint32_t tends = 0;
  std::uint32_t edge = 0;
  std::uint32_t healing = 0;
};

/// Where a structure stands in its village (ADR 0076): on the square's edge,
/// among the homes nearest the square, or at the edge beyond the farthest
/// home.
enum class Role : std::uint8_t { SQUARE, HOME, EDGE };

/// A breakable object covering `footprint` from `cell`, its top-left cell.
/// Its cells block movement for as long as the world runs, whatever its
/// stage.
struct Structure {
  Cell cell{};
  std::uint32_t durability = 0;
  Footprint footprint{1, 1};
  /// Whether its footprint blocks movement; a field does not.
  bool blocks = true;
  Role role = Role::HOME;
};

/// Land growth keeps clear as though a structure covered it: `area` from
/// `corner`, its top-left cell.
struct Clearing {
  Cell corner{};
  Footprint area{1, 1};
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
  /// The land growth keeps clear in it, such as a seat's court; cells off the
  /// map are ignored.
  std::vector<Clearing> clearings;
  /// The kind of each of `structures`, in their order, which an occupation
  /// names as its workplace; none names no kind.
  std::vector<std::string> kinds;
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

/// How broken a structure is, from durability by the floors in Rules.
enum class Stage : std::uint8_t { INTACT, DAMAGED, DESTROYED, RUBBLE };

/// The stage each of a world's structures is in, read from its durability by
/// the lowest durability of the intact, damaged and destroyed stages. It reads
/// the structures where they stand, so it outlives neither them nor a change
/// to how many there are.
class Stages {
 public:
  Stages(const std::vector<Structure>& structures,
         std::array<std::uint32_t, 3> floors);

  /// Returns the stage structure `structure` is in: below all three floors it
  /// is rubble. Throws std::out_of_range when there is no such structure.
  [[nodiscard]] Stage of(std::size_t structure) const;

 private:
  const std::vector<Structure>* structures_;
  std::array<std::uint32_t, 3> floors_;
};

/// A village as a world keeps it: as it was created or founded, who last
/// brought it to ruins, the food it holds, its raids, and the land it was
/// founded on.
struct VillageRecord {
  VillageSpec spec;
  std::optional<Destroyer> destroyer;
  std::uint64_t stock = 0;
  /// The army its raids are fought by, once it has raided, and the village
  /// its party is out against.
  std::optional<std::size_t> party;
  std::optional<std::size_t> raiding;
  /// The corner of the land it was founded on, if it was founded.
  std::optional<Cell> founded;
  /// The growth's tier it has reached, kept once reached (ADR 0076).
  std::size_t tier = 0;
  /// Where its residents were bound as it was created or founded: the
  /// centre its square is kept round and its distance from others is
  /// measured from, which stays where it was laid.
  Cell centre{};
};

/// A village and the growth's tier its residents have reached.
struct Reached {
  std::size_t village;
  std::size_t tier;
};

/// What each village's standing workshops do for its people: the edge they
/// strike with and the healing they recover, by village, each 0 where its
/// arms or tends fall short of the people whose home it is.
struct Service {
  std::vector<std::uint32_t> edge;
  std::vector<std::uint32_t> healing;
};

/// A raid a village sends: the army fighting it, its `party`, and the village
/// it is out against, its `target`.
struct Raid {
  std::size_t party;
  std::size_t target;
};

/// What a strike changing one of a village's structures did to the village:
/// nothing, brought it to ruins, or gave it to the striking army as its
/// residents.
enum class VillageChange : std::uint8_t { NONE, RUINED, RETAKEN };

/// The villages of a world, numbered in the order they were added, and the
/// village each structure belongs to: the one place either is kept. It
/// decides what a village holds, how many people it can hold, what its
/// workshops do for them, how its stock fills and feeds them, which village
/// takes in the hungry, and who ruins and who retakes it. Who makes it home,
/// and the armies they serve in, the world keeps.
class Villages {
 public:
  /// No villages and no structures.
  Villages() = default;

  /// No villages, and `structures` structures belonging to none.
  explicit Villages(std::size_t structures);

  /// The villages and the village of each structure, as a save holds them,
  /// unchecked until kept() is asked.
  Villages(std::vector<VillageRecord> records,
           std::vector<std::optional<std::size_t>> village_of);

  /// Returns how many villages there are.
  [[nodiscard]] std::size_t size() const;

  /// Returns whether there are none.
  [[nodiscard]] bool empty() const;

  /// Returns village `village`. Throws std::out_of_range when there is no
  /// such village.
  [[nodiscard]] const VillageRecord& at(std::size_t village) const;

  /// Returns every village, in order.
  [[nodiscard]] const std::vector<VillageRecord>& records() const;

  /// Returns the village each structure belongs to, if any, by structure.
  [[nodiscard]] const std::vector<std::optional<std::size_t>>& membership()
      const;

  /// Returns the village structure `structure` belongs to, if any. Throws
  /// std::out_of_range when there is no such structure.
  [[nodiscard]] const std::optional<std::size_t>& village_of(
      std::size_t structure) const;

  /// Returns whether army `army` is a village's residents.
  [[nodiscard]] bool houses(std::size_t army) const;

  /// Adds the village `spec` describes, among `armies` armies, its residents'
  /// roster `roster` people long, with its storage full. Returns its index.
  /// Throws std::invalid_argument when it names a structure that does not
  /// exist or another village names, an army that does not exist or another
  /// village houses, holdings or kinds that are not one for each of its
  /// structures, or more households than its residents pair into.
  std::size_t add(VillageSpec spec, std::size_t armies, std::size_t roster,
                  const Stages& stages);

  /// Adds a village founded on the land from `corner`, with army `residents`
  /// as its residents and no structure or food yet. Returns its index.
  std::size_t found(std::size_t residents, Cell corner);

  /// Gives village `village` its centre, `centre`.
  void place(std::size_t village, Cell centre);

  /// Has `reached`'s village reach its tier, keeping a higher one it reached
  /// before.
  void reach(Reached reached);

  /// Takes one more structure, belonging to no village.
  void add_structure();

  /// Makes structure `structure` village `village`'s, of kind `kind` and
  /// holding `holding`.
  void attach(std::size_t village, std::size_t structure, Holding holding,
              std::string kind);

  /// Records `raid` as village `village`'s: its party out against its target.
  void send(std::size_t village, Raid raid);

  /// Ends village `village`'s raid; its party stays its own.
  void end_raid(std::size_t village);

  /// Ends village `village`'s raid and lets its party go.
  void release(std::size_t village);

  /// Returns what village `village`'s structures no worse than `worst` hold
  /// together, the greatest edge and healing among them, and the harvest of
  /// its intact fields.
  [[nodiscard]] Holding held(std::size_t village, Stage worst,
                             const Stages& stages) const;

  /// Returns whether every structure of village `village` is in `stage`.
  [[nodiscard]] bool all_in(std::size_t village, Stage stage,
                            const Stages& stages) const;

  /// Returns how many people village `village` can hold: the least of the
  /// housing of its standing structures, the people its intact fields'
  /// harvests feed for a year, and the people a year's food its standing
  /// structures store feeds; `capacity` without `husbandry`.
  [[nodiscard]] std::size_t cap(std::size_t village, const Stages& stages,
                                const std::optional<Husbandry>& husbandry,
                                std::size_t capacity) const;

  /// Returns each village's edge and healing from its standing structures,
  /// each kept only while `reliance` and what they arm, or tend, cover the
  /// `population` people whose home it is.
  [[nodiscard]] Service served(const std::vector<std::size_t>& population,
                               std::uint64_t reliance,
                               const Stages& stages) const;

  /// Fills each village's stock with its intact fields' harvest when
  /// `harvest`, up to its standing storage, then feeds the `population`
  /// people whose home it is `consumption` each from it, or none of them
  /// while it cannot feed them all. Returns whether each village fed them.
  std::vector<bool> feed(const std::vector<std::size_t>& population,
                         std::uint32_t consumption, bool harvest,
                         const Stages& stages);

  /// Returns whether village `village`, home to `people`, takes in a person
  /// unfed too long: while it has room under its cap and food in store.
  [[nodiscard]] bool shelters(std::size_t village, std::size_t people,
                              const Stages& stages,
                              const std::optional<Husbandry>& husbandry,
                              std::size_t capacity) const;

  /// Returns what structure `structure` shows as a field in season `season`,
  /// or nothing when it yields no harvest. Throws std::out_of_range when
  /// there is no such structure.
  [[nodiscard]] std::optional<FieldState> field_state(
      std::size_t structure, const Stages& stages, std::uint64_t season) const;

  /// Takes the strike of `striker` that changed one of village `village`'s
  /// structures, which stood above rubble before it if `stood` and gained
  /// durability if `raised`, while its residents had a living member if
  /// `peopled`. Leaving every structure rubble after one stood records the
  /// striker as the village's destroyer; raising one while no resident lives
  /// makes the striker's army its residents.
  VillageChange shifted(std::size_t village, bool stood, bool raised,
                        bool peopled, Destroyer striker, const Stages& stages);

  /// Whether the saved-state invariant holds among `structures`, `armies`
  /// armies and `factions` factions: a village for each structure or
  /// none, and every structure, army and village a village names one that
  /// exists, with what each of its structures holds, if anything.
  [[nodiscard]] bool kept(const std::vector<Structure>& structures,
                          std::size_t armies, std::size_t factions) const;

 private:
  std::vector<VillageRecord> records_;
  std::vector<std::optional<std::size_t>> village_of_;
};

/// Returns the holder of each cell of a `size` map, row by row: the owner of
/// the nearest of `centres`, each a village's centre and owner, within
/// `claim` cells, the earliest among equals, or nothing beyond every one.
[[nodiscard]] std::vector<std::optional<std::size_t>> claimed(
    Footprint size, std::int32_t claim,
    const std::vector<std::pair<Cell, std::size_t>>& centres);

}  // namespace murim
