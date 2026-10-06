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
#include "region.hpp"
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
                      integer<std::int32_t>(fields, "structure_damage"),
                      integer<std::uint64_t>(fields, "cooldown")};
}

murim::Recruit recruit(const godot::Dictionary& fields) {
  return murim::Recruit{cell(fields, "cell"),
                        integer<std::uint64_t>(fields, "health")};
}

murim::ArmySpec army_spec(const godot::Dictionary& fields) {
  return murim::ArmySpec{cell(fields, "objective"),
                         each(array(fields, "members"), recruit),
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
    return over(murim::Seed{static_cast<std::uint64_t>(seed)}, std::move(rows),
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
      scenario.seed, std::move(scenario.map_rows), std::move(scenario.armies),
      std::move(scenario.structures), std::move(scenario.rules));
  adapter->structure_kinds_ = std::move(scenario.structure_kinds);
  return adapter;
}

godot::Ref<WorldAdapter> WorldAdapter::create_region(std::int64_t seed) {
  murim::Region region = murim::generate_region(
      murim::Seed{static_cast<std::uint64_t>(seed)}, murim::region_data());
  region.armies.push_back(murim::born_in(region, 0));
  std::vector<std::string> kinds = std::move(region.structure_kinds);
  godot::Ref<WorldAdapter> adapter =
      over(region.seed, std::move(region.map_rows), std::move(region.armies),
           std::move(region.structures), std::move(region.rules));
  adapter->structure_kinds_ = std::move(kinds);
  adapter->region_data_ = murim::region_data();
  adapter->terrain_ = std::move(region.terrain);
  adapter->territory_ = std::move(region.territory);
  adapter->villages_ = std::move(region.villages);
  return adapter;
}

godot::Vector2i WorldAdapter::m1_centre() {
  const murim::Position centre = murim::centre_of(murim::m1_scenario().centre);
  return godot::Vector2i{centre.x, centre.y};
}

godot::Ref<WorldAdapter> WorldAdapter::over(
    murim::Seed seed, std::vector<std::string> map_rows,
    std::vector<murim::ArmySpec> armies,
    std::vector<murim::Structure> structures, murim::Rules rules) {
  godot::Ref<WorldAdapter> adapter;
  adapter.instantiate();
  for (const murim::ArmySpec& army : armies) {
    adapter->skill_counts_.push_back(army.skills.size());
    adapter->skills_.push_back(army.skills);
    adapter->factions_.push_back(army.faction);
    std::uint64_t health = 0;
    for (const murim::Recruit& member : army.members) {
      health += member.health;
    }
    adapter->strengths_.push_back(health);
  }
  adapter->structure_count_ = structures.size();
  adapter->world_.emplace(seed, murim::Map{map_rows}, std::move(armies),
                          std::move(structures), std::move(rules));
  adapter->map_rows_ = std::move(map_rows);
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
  CRASH_COND_MSG(
      army < 0 || static_cast<std::uint64_t>(army) >= skill_counts_.size(),
      "army index out of range");
  return static_cast<std::size_t>(army);
}

std::size_t WorldAdapter::structure_index(std::int64_t structure) const {
  CRASH_COND_MSG(structure < 0 ||
                     static_cast<std::uint64_t>(structure) >= structure_count_,
                 "structure index out of range");
  return static_cast<std::size_t>(structure);
}

bool WorldAdapter::order(std::int64_t army, const godot::Vector2i& objective,
                         const godot::PackedInt64Array& skills) {
  // The one handler for the core's refusal: it must not cross into Godot.
  try {
    const godot::Variant cell = objective;
    std::vector<std::size_t> chosen;
    for (const std::int64_t skill : skills) {
      chosen.push_back(narrow<std::size_t>(skill, "skill"));
    }
    world().order(
        narrow<std::size_t>(army, "army"),
        murim::Order{murim::Cell{narrow<std::int32_t>(cell.get("x"), "x"),
                                 narrow<std::int32_t>(cell.get("y"), "y")},
                     std::move(chosen)});
    return true;
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.order: ", error.what());
    return false;
  }
}

void WorldAdapter::tick() { world().tick(); }

std::int64_t WorldAdapter::hash() const {
  return static_cast<std::int64_t>(static_cast<std::uint64_t>(world().hash()));
}

godot::PackedStringArray WorldAdapter::map_rows() const {
  godot::PackedStringArray rows;
  for (const std::string& row : map_rows_) {
    rows.push_back(godot::String(row.c_str()));
  }
  return rows;
}

std::int64_t WorldAdapter::army_count() const {
  return static_cast<std::int64_t>(skill_counts_.size());
}

std::int64_t WorldAdapter::skill_count(std::int64_t army) const {
  return static_cast<std::int64_t>(skill_counts_.at(army_index(army)));
}

godot::PackedVector2Array WorldAdapter::body_positions(
    std::int64_t army) const {
  godot::PackedVector2Array positions;
  for (const std::size_t member : world().roster(army_index(army))) {
    const murim::Position position = world().person(member).position;
    positions.push_back(godot::Vector2(static_cast<float>(position.x),
                                       static_cast<float>(position.y)));
  }
  return positions;
}

std::int64_t WorldAdapter::structure_count() const {
  return static_cast<std::int64_t>(structure_count_);
}

godot::Vector2i WorldAdapter::structure_cell(std::int64_t structure) const {
  const murim::Cell cell = world().structure(structure_index(structure)).cell;
  return godot::Vector2i{cell.x, cell.y};
}

godot::Vector2i WorldAdapter::structure_footprint(
    std::int64_t structure) const {
  const murim::Footprint footprint =
      world().structure(structure_index(structure)).footprint;
  return godot::Vector2i{footprint.width, footprint.height};
}

godot::String WorldAdapter::structure_kind(std::int64_t structure) const {
  const std::size_t index = structure_index(structure);
  return index < structure_kinds_.size()
             ? godot::String(structure_kinds_.at(index).c_str())
             : godot::String();
}

godot::TypedArray<godot::Dictionary> WorldAdapter::skills(
    std::int64_t army) const {
  godot::TypedArray<godot::Dictionary> skills;
  for (const murim::Skill& skill : skills_.at(army_index(army))) {
    godot::Dictionary fields;
    fields.set("range", skill.range);
    fields.set("radius", skill.radius);
    fields.set("body_damage", static_cast<std::int64_t>(skill.body_damage));
    fields.set("structure_damage", skill.structure_damage);
    fields.set("cooldown", static_cast<std::int64_t>(skill.cooldown));
    skills.push_back(fields);
  }
  return skills;
}

std::int64_t WorldAdapter::structure_stage(std::int64_t structure) const {
  return static_cast<std::int64_t>(world().stage(structure_index(structure)));
}

std::int64_t WorldAdapter::army_faction(std::int64_t army) const {
  return static_cast<std::int64_t>(factions_.at(army_index(army)));
}

std::int64_t WorldAdapter::army_strength(std::int64_t army) const {
  return static_cast<std::int64_t>(strengths_.at(army_index(army)));
}

godot::PackedInt64Array WorldAdapter::body_strengths(std::int64_t army) const {
  godot::PackedInt64Array strengths;
  for (const std::size_t member : world().roster(army_index(army))) {
    strengths.push_back(
        static_cast<std::int64_t>(world().person(member).health));
  }
  return strengths;
}

godot::PackedStringArray WorldAdapter::terrain_names() const {
  godot::PackedStringArray names;
  if (!terrain_.empty()) {
    for (const murim::TerrainKind& kind : region_data_.terrain) {
      names.push_back(godot::String(kind.name.c_str()));
    }
  }
  return names;
}

godot::PackedInt64Array WorldAdapter::terrain() const {
  godot::PackedInt64Array kinds;
  for (const std::size_t kind : terrain_) {
    kinds.push_back(static_cast<std::int64_t>(kind));
  }
  return kinds;
}

godot::PackedStringArray WorldAdapter::faction_names() const {
  godot::PackedStringArray names;
  if (!terrain_.empty()) {
    for (const murim::Faction& faction : region_data_.factions) {
      names.push_back(godot::String(faction.name.c_str()));
    }
  }
  return names;
}

godot::PackedInt64Array WorldAdapter::territory() const {
  godot::PackedInt64Array owners;
  for (const std::size_t owner : territory_) {
    owners.push_back(static_cast<std::int64_t>(owner));
  }
  return owners;
}

godot::TypedArray<godot::Dictionary> WorldAdapter::villages() const {
  godot::TypedArray<godot::Dictionary> villages;
  for (const murim::Village& village : villages_) {
    godot::Dictionary fields;
    fields.set("name", godot::String(village.name.c_str()));
    fields.set("kind", godot::String(village.kind.c_str()));
    fields.set("faction", static_cast<std::int64_t>(village.faction));
    fields.set("corner", godot::Vector2i{village.corner.x, village.corner.y});
    fields.set("area",
               godot::Vector2i{village.area.width, village.area.height});
    villages.push_back(fields);
  }
  return villages;
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
  godot::ClassDB::bind_static_method(name,
                                     godot::D_METHOD("create_region", "seed"),
                                     &WorldAdapter::create_region);
  godot::ClassDB::bind_static_method(name, godot::D_METHOD("m1_centre"),
                                     &WorldAdapter::m1_centre);
  godot::ClassDB::bind_method(
      godot::D_METHOD("order", "army", "objective", "skills"),
      &WorldAdapter::order);
  godot::ClassDB::bind_method(godot::D_METHOD("tick"), &WorldAdapter::tick);
  godot::ClassDB::bind_method(godot::D_METHOD("map_rows"),
                              &WorldAdapter::map_rows);
  godot::ClassDB::bind_method(godot::D_METHOD("skill_count", "army"),
                              &WorldAdapter::skill_count);
  godot::ClassDB::bind_method(godot::D_METHOD("hash"), &WorldAdapter::hash);
  godot::ClassDB::bind_method(godot::D_METHOD("army_count"),
                              &WorldAdapter::army_count);
  godot::ClassDB::bind_method(godot::D_METHOD("body_positions", "army"),
                              &WorldAdapter::body_positions);
  godot::ClassDB::bind_method(godot::D_METHOD("structure_count"),
                              &WorldAdapter::structure_count);
  godot::ClassDB::bind_method(godot::D_METHOD("structure_cell", "structure"),
                              &WorldAdapter::structure_cell);
  godot::ClassDB::bind_method(
      godot::D_METHOD("structure_footprint", "structure"),
      &WorldAdapter::structure_footprint);
  godot::ClassDB::bind_method(godot::D_METHOD("structure_kind", "structure"),
                              &WorldAdapter::structure_kind);
  godot::ClassDB::bind_method(godot::D_METHOD("skills", "army"),
                              &WorldAdapter::skills);
  godot::ClassDB::bind_method(godot::D_METHOD("structure_stage", "structure"),
                              &WorldAdapter::structure_stage);
  godot::ClassDB::bind_method(godot::D_METHOD("army_faction", "army"),
                              &WorldAdapter::army_faction);
  godot::ClassDB::bind_method(godot::D_METHOD("army_strength", "army"),
                              &WorldAdapter::army_strength);
  godot::ClassDB::bind_method(godot::D_METHOD("body_strengths", "army"),
                              &WorldAdapter::body_strengths);
  godot::ClassDB::bind_method(godot::D_METHOD("terrain_names"),
                              &WorldAdapter::terrain_names);
  godot::ClassDB::bind_method(godot::D_METHOD("terrain"),
                              &WorldAdapter::terrain);
  godot::ClassDB::bind_method(godot::D_METHOD("faction_names"),
                              &WorldAdapter::faction_names);
  godot::ClassDB::bind_method(godot::D_METHOD("territory"),
                              &WorldAdapter::territory);
  godot::ClassDB::bind_method(godot::D_METHOD("villages"),
                              &WorldAdapter::villages);
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
