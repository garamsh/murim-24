#include "world_adapter.hpp"

#include <cstdint>
#include <godot_cpp/core/class_db.hpp>

namespace murim_godot {

godot::Ref<WorldAdapter> WorldAdapter::create(std::int64_t seed) {
  godot::Ref<WorldAdapter> adapter;
  adapter.instantiate();
  adapter->world_ = murim::World{murim::Seed{static_cast<std::uint64_t>(seed)}};
  return adapter;
}

void WorldAdapter::tick() { world_.tick(); }

std::int64_t WorldAdapter::hash() const {
  return static_cast<std::int64_t>(static_cast<std::uint64_t>(world_.hash()));
}

void WorldAdapter::_bind_methods() {
  godot::ClassDB::bind_static_method(WorldAdapter::get_class_static(),
                                     godot::D_METHOD("create", "seed"),
                                     &WorldAdapter::create);
  godot::ClassDB::bind_method(godot::D_METHOD("tick"), &WorldAdapter::tick);
  godot::ClassDB::bind_method(godot::D_METHOD("hash"), &WorldAdapter::hash);
}

}  // namespace murim_godot
