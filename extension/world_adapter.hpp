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
class WorldAdapter : public godot::RefCounted {
  GDCLASS(WorldAdapter, godot::RefCounted)

 public:
  /// Returns an adapter over a world created from `seed`, a map of
  /// `map_rows`, `armies` and `separation`, as the core's constructor takes
  /// them. An army is a Dictionary with `start` and `objective` (Vector2i
  /// cells), `count` and `strength` (int). Returns null, after push_error,
  /// when the core or the conversion to its types refuses the input.
  static godot::Ref<WorldAdapter> create(
      std::int64_t seed, const godot::PackedStringArray& map_rows,
      const godot::TypedArray<godot::Dictionary>& armies,
      std::int64_t separation);

  /// Advances the world by one step.
  void tick();

  /// Returns the world's state hash, its bits read as a signed int.
  [[nodiscard]] std::int64_t hash() const;

 protected:
  static void _bind_methods();

 private:
  // Empty only between construction and the end of create(), since the class
  // is registered abstract and GDScript cannot construct it.
  std::optional<murim::World> world_;
};

}  // namespace murim_godot
