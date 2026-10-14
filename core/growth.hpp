#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "flow_field.hpp"
#include "limits.hpp"
#include "map.hpp"
#include "village.hpp"

namespace murim {

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
///   it, a cell clear of any structure or village's clearing, for its
///   residents to raise by the repair rule
/// - found: with more residents than `settlers` and none of its people still
///   settling, it lays `layout` as rubble on a `settlement` area of free
///   buildable land, drawn by the seed among those whose centre lies within
///   `reach` cells of its own, `spacing` cells clear of any structure or
///   village's clearing and held by no other faction; `settlers` of its
///   residents, a founder's household first, march there as the new village's
///   residents, and the founder heads a new family; the layout makes a village
///   of `settlement_kind`, which names its art
/// - raid: with more residents than `raiders` and no party out, it sends
///   `raiders` of them against the nearest village of a faction it is hostile
///   to, with living residents and its centre within `reach` cells
/// A village arms and tends `self_reliance` people with no workshop, beside
/// what its standing workshops arm and tend; building counts both among its
/// holdings, after housing, food and storage. By default it arms and tends
/// any number, so it never builds for them.
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
  std::uint32_t self_reliance = std::numeric_limits<std::uint32_t>::max();
};

/// Throws std::invalid_argument when `growth` checks at no interval, presses
/// above PER_MILLE, reaches outside [0, MAX_REACH], or lays a footprint or
/// placement it cannot.
void check_growth(const Growth& growth);

/// Cells of a map, row by row: those a structure or a village's clearing
/// covers, those that are not buildable, and those an objective or a living
/// person takes.
struct Ground {
  std::vector<bool> covered;
  std::vector<bool> rough;
  std::vector<bool> taken;
};

/// Plan `plan` laid from `cell`, its top-left cell.
struct Laid {
  std::size_t plan;
  Cell cell;
};

/// The plan each structure was laid from, the one place it is kept: none for
/// a structure the world was created with.
class Plans {
 public:
  /// No structures.
  Plans() = default;

  /// `structures` structures, laid from no plan.
  explicit Plans(std::size_t structures);

  /// The plan of each structure as a save holds them, unchecked until kept()
  /// is asked.
  explicit Plans(std::vector<std::optional<std::size_t>> laid_from);

  /// Returns the plan each structure was laid from, by structure.
  [[nodiscard]] const std::vector<std::optional<std::size_t>>& laid_from()
      const;

  /// Returns the plan structure `structure` was laid from, if any. Throws
  /// std::out_of_range when there is no such structure.
  [[nodiscard]] const std::optional<std::size_t>& plan_of(
      std::size_t structure) const;

  /// Takes one more structure, laid from `plan` if it names one.
  void add(std::optional<std::size_t> plan);

  /// Returns what structure `structure` holds by the plan it was laid from in
  /// `growth`, the first plan's for one laid from none.
  [[nodiscard]] Holding holding_of(std::size_t structure,
                                   const Growth& growth) const;

  /// Whether the saved-state invariant holds: a plan or none for each of
  /// `structures` structures, each naming one of `growth`'s plans, and none
  /// without it.
  [[nodiscard]] bool kept(std::size_t structures,
                          const std::optional<Growth>& growth) const;

 private:
  std::vector<std::optional<std::size_t>> laid_from_;
};

/// Returns the plan of the growth's builds that most raises the scarcest of
/// what `held` holds, in people: its housing, the people its harvest and its
/// storage feed a year by `husbandry` (one a day without it), and the people
/// it arms and tends beyond the growth's self-reliance; the first among
/// equals, or nothing when none raises it.
[[nodiscard]] std::optional<std::size_t> build_plan(
    const Holding& held, const Growth& growth,
    const std::optional<Husbandry>& husbandry);

/// Returns the corner from which `footprint` lies within the growth's
/// vicinity of `centre`, on cells of a map of `size` that `ground` leaves
/// free: no cell covered or rough within one cell of it and none taken under
/// it; the nearest `centre`, the first row by row among equals, or nothing
/// when there is none.
[[nodiscard]] std::optional<Cell> build_site(Footprint footprint, Cell centre,
                                             const Growth& growth,
                                             const Ground& ground,
                                             Footprint size);

/// Returns, row by row, the corners of the growth's settlement whose centre
/// lies within its reach of `home` along each axis and `paths` reaches, on
/// cells of a map of `size` that `ground` leaves free, no cell covered within
/// its spacing, and none of whose cells `territory` gives to a faction other
/// than `faction`.
[[nodiscard]] std::vector<Cell> settlement_sites(
    Cell home, const Growth& growth, const Ground& ground, Footprint size,
    const FlowField& paths,
    const std::vector<std::optional<std::size_t>>& territory,
    std::size_t faction);

/// Returns the growth's layout laid from a settlement's `corner`.
[[nodiscard]] std::vector<Laid> layout_at(Cell corner, const Growth& growth);

}  // namespace murim
