#include "world_adapter.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "map.hpp"
#include "world.hpp"

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

// Returns `value`, or throws when it is not of `type`.
godot::Variant typed(const godot::Variant& value, godot::Variant::Type type,
                     const char* name) {
  if (value.get_type() != type) {
    throw std::invalid_argument(std::string(name) +
                                " is missing or of the wrong type");
  }
  return value;
}

godot::Variant field(const godot::Dictionary& fields, const char* key,
                     godot::Variant::Type type) {
  return typed(fields.get(key, godot::Variant()), type, key);
}

template <typename Integer>
Integer integer(const godot::Dictionary& fields, const char* key) {
  return narrow<Integer>(field(fields, key, godot::Variant::INT), key);
}

murim::Cell cell(const godot::Dictionary& fields, const char* key) {
  const godot::Variant value = field(fields, key, godot::Variant::VECTOR2I);
  return murim::Cell{narrow<std::int32_t>(value.get("x"), "x"),
                     narrow<std::int32_t>(value.get("y"), "y")};
}

// Returns `convert` applied to each item of `items`, in order.
template <typename Convert>
auto each(const godot::Array& items, Convert convert) {
  std::vector<decltype(convert(godot::Variant()))> converted;
  converted.reserve(static_cast<std::size_t>(items.size()));
  for (const godot::Variant& item : items) {
    converted.push_back(convert(item));
  }
  return converted;
}

godot::Array array(const godot::Dictionary& fields, const char* key) {
  return field(fields, key, godot::Variant::ARRAY);
}

murim::Skill skill(const godot::Dictionary& fields) {
  return murim::Skill{integer<std::int32_t>(fields, "range"),
                      integer<std::int32_t>(fields, "radius"),
                      integer<std::uint64_t>(fields, "body_damage"),
                      integer<std::uint32_t>(fields, "structure_damage"),
                      integer<std::uint64_t>(fields, "cooldown")};
}

murim::ArmySpec army_spec(const godot::Dictionary& fields) {
  return murim::ArmySpec{cell(fields, "start"),
                         cell(fields, "objective"),
                         integer<std::uint32_t>(fields, "count"),
                         integer<std::uint64_t>(fields, "strength"),
                         integer<std::size_t>(fields, "faction"),
                         each(array(fields, "skills"), skill)};
}

murim::Structure structure(const godot::Dictionary& fields) {
  return murim::Structure{cell(fields, "cell"),
                          integer<std::uint32_t>(fields, "durability")};
}

std::vector<bool> hostile_row(const godot::Array& row) {
  return each(row, [](const godot::Variant& hostile) -> bool {
    return typed(hostile, godot::Variant::BOOL, "hostile");
  });
}

std::array<std::uint32_t, 3> stage_floors(const godot::Array& floors) {
  const std::vector<std::uint32_t> values =
      each(floors, [](const godot::Variant& floor) {
        return narrow<std::uint32_t>(
            typed(floor, godot::Variant::INT, "stage_floors"), "stage_floors");
      });
  std::array<std::uint32_t, 3> fixed{};
  if (values.size() != fixed.size()) {
    throw std::invalid_argument("stage_floors must hold three ints");
  }
  std::copy(values.begin(), values.end(), fixed.begin());
  return fixed;
}

murim::Rules rules(const godot::Dictionary& fields) {
  return murim::Rules{
      integer<std::int32_t>(fields, "separation"),
      each(array(fields, "hostile"),
           [](const godot::Variant& row) { return hostile_row(row); }),
      stage_floors(array(fields, "stage_floors"))};
}

}  // namespace

godot::Ref<WorldAdapter> WorldAdapter::create(
    std::int64_t seed, const godot::PackedStringArray& map_rows,
    const godot::TypedArray<godot::Dictionary>& armies,
    const godot::TypedArray<godot::Dictionary>& structures,
    const godot::Dictionary& rules) {
  godot::Ref<WorldAdapter> adapter;
  // The one handler for the core's refusal: it must not cross into Godot.
  try {
    std::vector<std::string> rows;
    for (const godot::String& row : map_rows) {
      rows.emplace_back(row.utf8().get_data());
    }
    adapter.instantiate();
    adapter->world_.emplace(murim::Seed{static_cast<std::uint64_t>(seed)},
                            murim::Map{rows}, each(armies, army_spec),
                            each(structures, structure),
                            murim_godot::rules(rules));
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.create: ", error.what());
    return {};
  }
  return adapter;
}

void WorldAdapter::tick() {
  CRASH_COND_MSG(!world_.has_value(), "WorldAdapter was not made by create()");
  world_->tick();
}

std::int64_t WorldAdapter::hash() const {
  CRASH_COND_MSG(!world_.has_value(), "WorldAdapter was not made by create()");
  return static_cast<std::int64_t>(static_cast<std::uint64_t>(world_->hash()));
}

void WorldAdapter::_bind_methods() {
  godot::ClassDB::bind_static_method(
      WorldAdapter::get_class_static(),
      godot::D_METHOD("create", "seed", "map_rows", "armies", "structures",
                      "rules"),
      &WorldAdapter::create);
  godot::ClassDB::bind_method(godot::D_METHOD("tick"), &WorldAdapter::tick);
  godot::ClassDB::bind_method(godot::D_METHOD("hash"), &WorldAdapter::hash);
}

}  // namespace murim_godot
