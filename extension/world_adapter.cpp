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
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "m1_scenario.hpp"
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
  // The one handler for the core's refusal: it must not cross into Godot.
  try {
    std::vector<std::string> rows;
    for (const godot::String& row : map_rows) {
      rows.emplace_back(row.utf8().get_data());
    }
    return over(murim::Seed{static_cast<std::uint64_t>(seed)}, murim::Map{rows},
                each(armies, army_spec), each(structures, structure),
                murim_godot::rules(rules));
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.create: ", error.what());
    return {};
  }
}

godot::Ref<WorldAdapter> WorldAdapter::create_m1() {
  murim::M1Scenario scenario = murim::m1_scenario();
  godot::Ref<WorldAdapter> adapter = over(
      scenario.seed, murim::Map{scenario.map_rows}, std::move(scenario.armies),
      std::move(scenario.structures), std::move(scenario.rules));
  adapter->world().set_focus(murim::centre_of(scenario.focus));
  return adapter;
}

godot::Ref<WorldAdapter> WorldAdapter::over(
    murim::Seed seed, murim::Map map, std::vector<murim::ArmySpec> armies,
    std::vector<murim::Structure> structures, murim::Rules rules) {
  godot::Ref<WorldAdapter> adapter;
  adapter.instantiate();
  adapter->army_count_ = armies.size();
  adapter->structure_count_ = structures.size();
  adapter->world_.emplace(seed, std::move(map), std::move(armies),
                          std::move(structures), std::move(rules));
  return adapter;
}

murim::World& WorldAdapter::world() {
  CRASH_COND_MSG(!world_.has_value(), "WorldAdapter was not made by create()");
  return *world_;
}

const murim::World& WorldAdapter::world() const {
  CRASH_COND_MSG(!world_.has_value(), "WorldAdapter was not made by create()");
  return *world_;
}

std::size_t WorldAdapter::army_index(std::int64_t army) const {
  CRASH_COND_MSG(army < 0 || static_cast<std::uint64_t>(army) >= army_count_,
                 "army index out of range");
  return static_cast<std::size_t>(army);
}

std::size_t WorldAdapter::structure_index(std::int64_t structure) const {
  CRASH_COND_MSG(structure < 0 ||
                     static_cast<std::uint64_t>(structure) >= structure_count_,
                 "structure index out of range");
  return static_cast<std::size_t>(structure);
}

void WorldAdapter::set_focus(const godot::Vector2i& focus) {
  const godot::Variant value = focus;
  world().set_focus(murim::Position{narrow<std::int32_t>(value.get("x"), "x"),
                                    narrow<std::int32_t>(value.get("y"), "y")});
}

void WorldAdapter::tick() { world().tick(); }

std::int64_t WorldAdapter::hash() const {
  return static_cast<std::int64_t>(static_cast<std::uint64_t>(world().hash()));
}

std::int64_t WorldAdapter::army_count() const {
  return static_cast<std::int64_t>(army_count_);
}

godot::PackedVector2Array WorldAdapter::body_positions(
    std::int64_t army) const {
  godot::PackedVector2Array positions;
  for (const murim::Body& body : world().bodies(army_index(army))) {
    positions.push_back(godot::Vector2(static_cast<float>(body.position.x),
                                       static_cast<float>(body.position.y)));
  }
  return positions;
}

godot::PackedInt64Array WorldAdapter::body_counts(std::int64_t army) const {
  godot::PackedInt64Array counts;
  for (const murim::Body& body : world().bodies(army_index(army))) {
    counts.push_back(body.count);
  }
  return counts;
}

std::int64_t WorldAdapter::structure_count() const {
  return static_cast<std::int64_t>(structure_count_);
}

godot::Vector2i WorldAdapter::structure_cell(std::int64_t structure) const {
  const murim::Cell cell = world().structure(structure_index(structure)).cell;
  return godot::Vector2i{cell.x, cell.y};
}

std::int64_t WorldAdapter::structure_stage(std::int64_t structure) const {
  return static_cast<std::int64_t>(world().stage(structure_index(structure)));
}

void WorldAdapter::_bind_methods() {
  const godot::StringName& name = WorldAdapter::get_class_static();
  godot::ClassDB::bind_static_method(
      name,
      godot::D_METHOD("create", "seed", "map_rows", "armies", "structures",
                      "rules"),
      &WorldAdapter::create);
  godot::ClassDB::bind_static_method(name, godot::D_METHOD("create_m1"),
                                     &WorldAdapter::create_m1);
  godot::ClassDB::bind_method(godot::D_METHOD("set_focus", "focus"),
                              &WorldAdapter::set_focus);
  godot::ClassDB::bind_method(godot::D_METHOD("tick"), &WorldAdapter::tick);
  godot::ClassDB::bind_method(godot::D_METHOD("hash"), &WorldAdapter::hash);
  godot::ClassDB::bind_method(godot::D_METHOD("army_count"),
                              &WorldAdapter::army_count);
  godot::ClassDB::bind_method(godot::D_METHOD("body_positions", "army"),
                              &WorldAdapter::body_positions);
  godot::ClassDB::bind_method(godot::D_METHOD("body_counts", "army"),
                              &WorldAdapter::body_counts);
  godot::ClassDB::bind_method(godot::D_METHOD("structure_count"),
                              &WorldAdapter::structure_count);
  godot::ClassDB::bind_method(godot::D_METHOD("structure_cell", "structure"),
                              &WorldAdapter::structure_cell);
  godot::ClassDB::bind_method(godot::D_METHOD("structure_stage", "structure"),
                              &WorldAdapter::structure_stage);
  godot::ClassDB::bind_integer_constant(name, "", "TICKS_PER_SECOND",
                                        murim::TICKS_PER_SECOND);
  godot::ClassDB::bind_integer_constant(name, "", "CELL_SIZE",
                                        murim::CELL_SIZE);
  godot::ClassDB::bind_integer_constant(name, "", "STAGE_INTACT",
                                        static_cast<int>(murim::Stage::INTACT));
  godot::ClassDB::bind_integer_constant(
      name, "", "STAGE_DAMAGED", static_cast<int>(murim::Stage::DAMAGED));
  godot::ClassDB::bind_integer_constant(
      name, "", "STAGE_DESTROYED", static_cast<int>(murim::Stage::DESTROYED));
  godot::ClassDB::bind_integer_constant(name, "", "STAGE_RUBBLE",
                                        static_cast<int>(murim::Stage::RUBBLE));
}

}  // namespace murim_godot
