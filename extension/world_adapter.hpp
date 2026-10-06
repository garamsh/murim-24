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
#include <godot_cpp/variant/vector2i.hpp>
#include <optional>
#include <vector>

#include "map.hpp"
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
  /// `map_rows`, `armies`, `structures` and `rules`, as the core's
  /// constructor takes them, each struct as a Dictionary keyed by its field
  /// names:
  /// - army: `start` and `objective` (Vector2i cells), `count`, `strength`,
  ///   `faction` (int) and `skills` (Array of skills);
  /// - skill: `range`, `radius`, `body_damage`, `structure_damage`,
  ///   `cooldown` (int);
  /// - structure: `cell` (Vector2i) and `durability` (int);
  /// - rules: `separation` (int), `hostile` (Array of Arrays of bool) and
  ///   `stage_floors` (Array of three ints).
  /// Returns null, after push_error, when the core or the conversion to its
  /// types refuses the input.
  static godot::Ref<WorldAdapter> create(
      std::int64_t seed, const godot::PackedStringArray& map_rows,
      const godot::TypedArray<godot::Dictionary>& armies,
      const godot::TypedArray<godot::Dictionary>& structures,
      const godot::Dictionary& rules);

  /// Returns an adapter over the core's M1 benchmark scenario, focused where
  /// the scenario places its focus.
  static godot::Ref<WorldAdapter> create_m1();

  /// Places the point the tiers are measured from, in sub-cell units.
  void set_focus(const godot::Vector2i& focus);

  /// Advances the world by one step.
  void tick();

  /// Returns the world's state hash, its bits read as a signed int.
  [[nodiscard]] std::int64_t hash() const;

  /// Returns how many armies the world was created with.
  [[nodiscard]] std::int64_t army_count() const;

  /// Returns the positions of army `army`'s bodies, in sub-cell units and
  /// creation order; exact while coordinates stay within 2^24.
  [[nodiscard]] godot::PackedVector2Array body_positions(
      std::int64_t army) const;

  /// Returns the agent count of each of army `army`'s bodies, in the order
  /// of body_positions().
  [[nodiscard]] godot::PackedInt64Array body_counts(std::int64_t army) const;

  /// Returns how many structures the world was created with.
  [[nodiscard]] std::int64_t structure_count() const;

  /// Returns the cell structure `structure` stands on.
  [[nodiscard]] godot::Vector2i structure_cell(std::int64_t structure) const;

  /// Returns the stage structure `structure` is in, one of the STAGE_*
  /// constants.
  [[nodiscard]] std::int64_t structure_stage(std::int64_t structure) const;

 protected:
  static void _bind_methods();

 private:
  // Set by create(), the only way GDScript gets an adapter, since the class is
  // registered abstract.
  std::optional<murim::World> world_;
  std::size_t army_count_ = 0;
  std::size_t structure_count_ = 0;

  // Returns an adapter over a world created from these arguments, as the
  // core's constructor takes them.
  static godot::Ref<WorldAdapter> over(murim::Seed seed, murim::Map map,
                                       std::vector<murim::ArmySpec> armies,
                                       std::vector<murim::Structure> structures,
                                       murim::Rules rules);
  [[nodiscard]] murim::World& world();
  [[nodiscard]] const murim::World& world() const;
  [[nodiscard]] std::size_t army_index(std::int64_t army) const;
  [[nodiscard]] std::size_t structure_index(std::int64_t structure) const;
};

}  // namespace murim_godot
