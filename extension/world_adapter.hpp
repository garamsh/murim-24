#pragma once

#include <cstdint>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <optional>

#include "world.hpp"

namespace murim_godot {

/// A core world exposed to Godot; GDScript gets one from
/// `WorldAdapter.create()`, the only way to make one.
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

  /// Advances the world by one step.
  void tick();

  /// Returns the world's state hash, its bits read as a signed int.
  [[nodiscard]] std::int64_t hash() const;

 protected:
  static void _bind_methods();

 private:
  // Set by create(), the only way GDScript gets an adapter, since the class is
  // registered abstract.
  std::optional<murim::World> world_;
};

}  // namespace murim_godot
