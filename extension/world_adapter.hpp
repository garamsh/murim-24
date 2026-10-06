#pragma once

#include <cstddef>
#include <cstdint>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <optional>
#include <string>
#include <vector>

#include "map.hpp"
#include "region.hpp"
#include "world.hpp"

namespace murim_godot {

/// A core world exposed to Godot; GDScript gets one from
/// `WorldAdapter.create()` or `WorldAdapter.create_m1()`, the only ways to
/// make one.
// GDCLASS, godot-cpp's code, declares a copy assignment and expands into code
// these checks reject.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class WorldAdapter : public godot::RefCounted {
  // NOLINTNEXTLINE(misc-const-correctness,modernize-use-auto)
  GDCLASS(WorldAdapter, godot::RefCounted)

 public:
  /// Returns an adapter over a world created from `seed`, a map of
  /// `map_rows`, `armies`, `structures`, `rules` and `leadership`, as the
  /// core's constructor takes them, each struct as a Dictionary keyed by its
  /// field names:
  /// - army: `objective` (Vector2i cell), `members` (Array of members),
  ///   `faction` (int) and `skills` (Array of skills);
  /// - member: `cell` (Vector2i) and `health` (int), a new person;
  /// - skill: `range`, `radius`, `body_damage`, `structure_damage`,
  ///   `cooldown` (int);
  /// - structure: `cell` (Vector2i) and `durability` (int);
  /// - rules: `separation` (int), `hostile` (Array of Arrays of bool) and
  ///   `stage_floors` (Array of three ints);
  /// - leadership, one per faction, may be left out: `leader` (int, a
  ///   person) and `duel` (`members_only` and `allies_turn_hostile`, bool,
  ///   and `yield_health`, int), each left out when the faction has none.
  /// Returns null, after push_error, when the core or the conversion to its
  /// types refuses the input.
  static godot::Ref<WorldAdapter> create(
      std::int64_t seed, const godot::PackedStringArray& map_rows,
      const godot::TypedArray<godot::Dictionary>& armies,
      const godot::TypedArray<godot::Dictionary>& structures,
      const godot::Dictionary& rules,
      const godot::TypedArray<godot::Dictionary>& leadership = {});

  /// Returns an adapter over the core's M1 benchmark scenario.
  static godot::Ref<WorldAdapter> create_m1();

  /// Returns an adapter over the region the core generates from `seed`, with
  /// each faction's leadership, and one person born into village `village`
  /// as the last army. Returns null, after push_error, when the region has no
  /// such village.
  static godot::Ref<WorldAdapter> create_region(std::int64_t seed,
                                                std::int64_t village = 0);

  /// Returns the point the M1 scenario's battle is fought around, in
  /// sub-cell units.
  static godot::Vector2i m1_centre();

  /// Gives army `army` the order to march toward `objective` and strike with
  /// one of `skills`, indices into its skills; with `challenge` 0 or more, its
  /// lead also challenges that faction's leader to a duel. Returns false,
  /// after push_error, when the core or the conversion to its types refuses
  /// it.
  bool order(std::int64_t army, const godot::Vector2i& objective,
             const godot::PackedInt64Array& skills,
             std::int64_t challenge = -1);

  /// Returns how many factions the world's rules name.
  [[nodiscard]] std::int64_t faction_count() const;

  /// Returns the person leading faction `faction`, or -1 when none does.
  [[nodiscard]] std::int64_t leader(std::int64_t faction) const;

  /// Returns why army `army`'s lead may not challenge faction `faction`'s
  /// leader, one of the CHALLENGE_* constants; CHALLENGE_NONE when it may.
  [[nodiscard]] std::int64_t challenge_refusal(std::int64_t army,
                                               std::int64_t faction) const;

  /// Returns the duel fought for faction `faction`'s lead as a Dictionary of
  /// `challenger` and `leader`, person indices; empty when none is fought.
  [[nodiscard]] godot::Dictionary duel(std::int64_t faction) const;

  /// Returns whether people of faction `from` attack those of `target`.
  [[nodiscard]] bool hostile(std::int64_t from, std::int64_t target) const;

  /// Returns the living members of army `army`, as person indices, its lead
  /// first.
  [[nodiscard]] godot::PackedInt64Array roster(std::int64_t army) const;

  /// Returns person `person`'s position, in sub-cell units.
  [[nodiscard]] godot::Vector2 person_position(std::int64_t person) const;

  /// Returns person `person`'s health; 0 once dead.
  [[nodiscard]] std::int64_t person_health(std::int64_t person) const;

  /// Advances the world by one step.
  void tick();

  /// Returns the world's state hash, its bits read as a signed int.
  [[nodiscard]] std::int64_t hash() const;

  /// Returns the map's rows, `#` for a blocked cell and `.` for a passable
  /// one.
  [[nodiscard]] godot::PackedStringArray map_rows() const;

  /// Returns how many armies the world was created with.
  [[nodiscard]] std::int64_t army_count() const;

  /// Returns the world's date: `year` and `day` counted from 1, `season`
  /// from 0, as the core's calendar gives them.
  [[nodiscard]] godot::Dictionary date() const;

  /// Returns how many skills army `army` has now.
  [[nodiscard]] std::int64_t skill_count(std::int64_t army) const;

  /// Returns the positions of army `army`'s living members, each one person,
  /// in sub-cell units and roster order; exact while coordinates stay within
  /// 2^24. Its size is the roster's.
  [[nodiscard]] godot::PackedVector2Array body_positions(
      std::int64_t army) const;

  /// Returns how many structures the world was created with.
  [[nodiscard]] std::int64_t structure_count() const;

  /// Returns the cell structure `structure` stands on.
  [[nodiscard]] godot::Vector2i structure_cell(std::int64_t structure) const;

  /// Returns the footprint of structure `structure`, its width and height in
  /// cells, from structure_cell() as its top-left cell.
  [[nodiscard]] godot::Vector2i structure_footprint(
      std::int64_t structure) const;

  /// Returns the kind of building structure `structure` is, such as `house`;
  /// empty for a world made by create(), whose structures carry no kind.
  [[nodiscard]] godot::String structure_kind(std::int64_t structure) const;

  /// Returns army `army`'s skills, each a Dictionary keyed as create() takes
  /// it: `range`, `radius`, `body_damage`, `structure_damage` and `cooldown`.
  [[nodiscard]] godot::TypedArray<godot::Dictionary> skills(
      std::int64_t army) const;

  /// Returns the stage structure `structure` is in, one of the STAGE_*
  /// constants.
  [[nodiscard]] std::int64_t structure_stage(std::int64_t structure) const;

  /// Returns the faction army `army` fights for now; a soul born again
  /// takes its new family's.
  [[nodiscard]] std::int64_t army_faction(std::int64_t army) const;

  /// Returns the health army `army`'s members were created with, together.
  [[nodiscard]] std::int64_t army_strength(std::int64_t army) const;

  /// Returns the health of each of army `army`'s living members, in the
  /// order of body_positions().
  [[nodiscard]] godot::PackedInt64Array body_strengths(std::int64_t army) const;

  /// Returns the names of the terrain kinds, by index; empty unless the
  /// world was made by create_region().
  [[nodiscard]] godot::PackedStringArray terrain_names() const;

  /// Returns the terrain kind of each cell, row by row; empty unless the
  /// world was made by create_region().
  [[nodiscard]] godot::PackedInt64Array terrain() const;

  /// Returns the names of the factions, by index; empty unless the world was
  /// made by create_region().
  [[nodiscard]] godot::PackedStringArray faction_names() const;

  /// Returns the faction owning each cell, row by row; empty unless the
  /// world was made by create_region().
  [[nodiscard]] godot::PackedInt64Array territory() const;

  /// Returns each village as a Dictionary of `name`, `kind` (its template's
  /// name), `faction` (int),
  /// `corner` (Vector2i) and `area` (Vector2i, its width and height in
  /// cells); empty unless the world was made
  /// by create_region().
  [[nodiscard]] godot::TypedArray<godot::Dictionary> villages() const;

 protected:
  static void _bind_methods();

 private:
  // Set by create(), the only way GDScript gets an adapter, since the class is
  // registered abstract.
  std::optional<murim::World> world_;
  std::vector<std::string> map_rows_;
  std::size_t army_count_ = 0;
  std::vector<std::string> structure_kinds_;
  std::vector<std::uint64_t> strengths_;
  std::size_t structure_count_ = 0;
  std::size_t faction_count_ = 0;
  // What create_region() keeps of the region for display; empty otherwise.
  murim::RegionData region_data_;
  std::vector<std::size_t> terrain_;
  std::vector<std::size_t> territory_;
  std::vector<murim::Village> villages_;

  // Returns an adapter over a world created from these arguments, as the
  // core's constructor takes them.
  static godot::Ref<WorldAdapter> over(
      murim::Seed seed, std::vector<std::string> map_rows,
      std::vector<murim::ArmySpec> armies,
      std::vector<murim::Structure> structures, murim::Rules rules,
      std::vector<murim::VillageSpec> villages = {},
      std::vector<murim::Leadership> leadership = {}, murim::Life life = {});
  // Returns an adapter over the region generated from `seed`, with one person
  // born into village `village` as the last army; throws std::out_of_range
  // when there is no such village.
  static godot::Ref<WorldAdapter> born_into(murim::Seed seed,
                                            std::size_t village);
  [[nodiscard]] murim::World& world();
  [[nodiscard]] const murim::World& world() const;
  [[nodiscard]] std::size_t army_index(std::int64_t army) const;
  [[nodiscard]] std::size_t structure_index(std::int64_t structure) const;
  [[nodiscard]] std::size_t faction_index(std::int64_t faction) const;
  [[nodiscard]] std::size_t person_index(std::int64_t person) const;
};

}  // namespace murim_godot
