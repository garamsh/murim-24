#pragma once

#include <cstdint>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>

#include "world.hpp"

namespace murim_godot {

/// A core world exposed to Godot; GDScript gets one from
/// `WorldAdapter.create(seed)`.
class WorldAdapter : public godot::RefCounted {
  GDCLASS(WorldAdapter, godot::RefCounted)

 public:
  /// Returns an adapter over a world created from `seed`, its bits read as the
  /// core's seed.
  static godot::Ref<WorldAdapter> create(std::int64_t seed);

  /// Advances the world by one step.
  void tick();

  /// Returns the world's state hash, its bits read as a signed int.
  [[nodiscard]] std::int64_t hash() const;

 protected:
  static void _bind_methods();

 private:
  murim::World world_{murim::Seed{}};
};

}  // namespace murim_godot
