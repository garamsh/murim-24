#include "world_adapter.hpp"

#include <cstdint>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "map.hpp"

namespace murim_godot {
namespace {

// Returns `value` as `Integer`, or throws when it does not survive the trip.
template <typename Integer>
Integer narrow(std::int64_t value, const char* name) {
  const auto converted = static_cast<Integer>(value);
  if (static_cast<std::int64_t>(converted) != value ||
      (value < 0 && !std::is_signed_v<Integer>)) {
    throw std::invalid_argument(std::string(name) + " is out of range");
  }
  return converted;
}

godot::Variant field(const godot::Dictionary& army, const char* key,
                     godot::Variant::Type type) {
  const godot::Variant value = army.get(key, godot::Variant());
  if (value.get_type() != type) {
    throw std::invalid_argument(std::string("an army's ") + key +
                                " is missing or of the wrong type");
  }
  return value;
}

murim::Cell cell(const godot::Dictionary& army, const char* key) {
  const godot::Vector2i value = field(army, key, godot::Variant::VECTOR2I);
  return murim::Cell{value.x, value.y};
}

murim::ArmySpec army_spec(const godot::Dictionary& army) {
  return murim::ArmySpec{
      cell(army, "start"), cell(army, "objective"),
      narrow<std::uint32_t>(field(army, "count", godot::Variant::INT), "count"),
      narrow<std::uint64_t>(field(army, "strength", godot::Variant::INT),
                            "strength")};
}

}  // namespace

godot::Ref<WorldAdapter> WorldAdapter::create(
    std::int64_t seed, const godot::PackedStringArray& map_rows,
    const godot::TypedArray<godot::Dictionary>& armies,
    std::int64_t separation) {
  godot::Ref<WorldAdapter> adapter;
  // The one handler for the core's refusal: it must not cross into Godot.
  try {
    std::vector<std::string> rows;
    for (const godot::String& row : map_rows) {
      rows.emplace_back(row.utf8().get_data());
    }
    std::vector<murim::ArmySpec> specs;
    for (std::int64_t i = 0; i < armies.size(); ++i) {
      specs.push_back(army_spec(armies[i]));
    }
    adapter.instantiate();
    adapter->world_.emplace(murim::Seed{static_cast<std::uint64_t>(seed)},
                            murim::Map{rows}, specs,
                            narrow<std::int32_t>(separation, "separation"));
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.create: ", error.what());
    return {};
  }
  return adapter;
}

void WorldAdapter::tick() { world_.value().tick(); }

std::int64_t WorldAdapter::hash() const {
  return static_cast<std::int64_t>(
      static_cast<std::uint64_t>(world_.value().hash()));
}

void WorldAdapter::_bind_methods() {
  godot::ClassDB::bind_static_method(
      WorldAdapter::get_class_static(),
      godot::D_METHOD("create", "seed", "map_rows", "armies", "separation"),
      &WorldAdapter::create);
  godot::ClassDB::bind_method(godot::D_METHOD("tick"), &WorldAdapter::tick);
  godot::ClassDB::bind_method(godot::D_METHOD("hash"), &WorldAdapter::hash);
}

}  // namespace murim_godot
