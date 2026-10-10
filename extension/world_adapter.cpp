#include "world_adapter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "file_saves.hpp"
#include "flow_field.hpp"
#include "m1_scenario.hpp"
#include "map.hpp"
#include "region.hpp"
#include "save.hpp"
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

// Returns a Vector2 `vector` as sub-cell units, or throws when an axis is not
// finite or does not fit 32 bits.
murim::Position units(const godot::Variant& vector, const char* name) {
  const auto axis = [name](double value) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument(std::string(name) + " is not finite");
    }
    constexpr double BOUND = 2'147'483'648.0;
    if (value < -BOUND || value >= BOUND) {
      throw std::invalid_argument(std::string(name) + " is out of range");
    }
    return static_cast<std::int32_t>(value);
  };
  return murim::Position{axis(vector.get("x")), axis(vector.get("y"))};
}

// Returns a cast's target as cast() takes it: a Vector2 point, an int person
// or null for the caster itself; throws for any other.
murim::Target target_of(const godot::Variant& target) {
  switch (target.get_type()) {
    case godot::Variant::VECTOR2:
      return murim::Point{units(target, "target")};
    case godot::Variant::INT:
      return murim::AtPerson{narrow<std::size_t>(target, "target")};
    case godot::Variant::NIL:
      return murim::Itself{};
    default:
      throw std::invalid_argument(
          "a target must be a Vector2 point, an int person or null");
  }
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

// Returns a skill's `shape`, one of the SHAPE_* constants, or AREA where it
// is left out; throws for a value that names no shape.
murim::Shape shape(const godot::Dictionary& fields) {
  if (!fields.has("shape")) {
    return murim::Shape::AREA;
  }
  const auto value = integer<std::int64_t>(fields, "shape");
  if (value < 0 || value > static_cast<std::int64_t>(murim::Shape::TARGETED)) {
    throw std::invalid_argument("shape names no shape");
  }
  return static_cast<murim::Shape>(value);
}

murim::Skill skill(const godot::Dictionary& fields) {
  murim::Skill skill{integer<std::int32_t>(fields, "range"),
                     integer<std::int32_t>(fields, "radius"),
                     integer<std::uint64_t>(fields, "body_damage"),
                     integer<std::int32_t>(fields, "structure_damage"),
                     integer<std::uint64_t>(fields, "cooldown")};
  skill.shape = shape(fields);
  return skill;
}

// Returns `position`, in sub-cell units, as a Vector2; exact while it lies
// within 2^24.
godot::Vector2 vector(murim::Position position) {
  return {static_cast<float>(position.x), static_cast<float>(position.y)};
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

bool boolean(const godot::Dictionary& fields, const char* key) {
  return field(fields, key, godot::Variant::BOOL);
}

// A faction's leadership: `leader` (int) and `duel` (Dictionary of
// `members_only`, `yield_health` and `allies_turn_hostile`), each left out
// when the faction has none.
murim::Leadership leadership(const godot::Dictionary& fields) {
  murim::Leadership led;
  if (fields.has("leader")) {
    led.leader = integer<std::size_t>(fields, "leader");
  }
  if (fields.has("duel")) {
    const godot::Dictionary duel =
        field(fields, "duel", godot::Variant::DICTIONARY);
    led.succession = murim::Duel{boolean(duel, "members_only"),
                                 integer<std::uint64_t>(duel, "yield_health"),
                                 boolean(duel, "allies_turn_hostile")};
  }
  return led;
}

// Person indices as GDScript reads them.
godot::PackedInt64Array indices(const std::vector<std::size_t>& people) {
  godot::PackedInt64Array result;
  for (const std::size_t person : people) {
    result.push_back(static_cast<std::int64_t>(person));
  }
  return result;
}

// Returns the region the core generates from `seed`, its days `ticks_per_day`
// ticks long.
murim::Region region_of(murim::Seed seed, std::uint64_t ticks_per_day) {
  murim::Region region = murim::generate_region(seed, murim::region_data());
  region.rules.ticks_per_day = ticks_per_day;
  return region;
}

// The soul the player's body carries: the first, and only, one a world from
// create_region() gives.
constexpr std::size_t PLAYER_SOUL = 0;

// The names of the disciplines `life` draws, by index.
std::vector<std::string> discipline_names_of(const murim::Life& life) {
  std::vector<std::string> names;
  names.reserve(life.disciplines.size());
  for (const murim::DisciplineDraw& discipline : life.disciplines) {
    names.push_back(discipline.name);
  }
  return names;
}

// load_refusal()'s verdict on a slot the core reads, beside the LoadError
// values.
constexpr std::int64_t LOAD_NONE = -1;

// The health each army's members were created with, together, by army.
std::vector<std::uint64_t> strengths_of(
    const std::vector<murim::ArmySpec>& armies) {
  std::vector<std::uint64_t> strengths;
  for (const murim::ArmySpec& army : armies) {
    std::uint64_t health = 0;
    for (const murim::Recruit& member : army.members) {
      health += member.health;
    }
    strengths.push_back(health);
  }
  return strengths;
}

// Why the core refused a save, as push_error reports it.
const char* refusal_of(murim::LoadError error) {
  switch (error) {
    case murim::LoadError::MISSING:
      return "the slot holds no save";
    case murim::LoadError::NOT_A_SAVE:
      return "the slot holds no save of this game";
    case murim::LoadError::UNSUPPORTED_VERSION:
      return "the save was written by another version of the game";
    case murim::LoadError::TRUNCATED:
      return "the save is cut short";
    case murim::LoadError::CORRUPTED:
      return "the save is corrupted";
  }
  return "the save is corrupted";
}

// The person indices `people` as GDScript reads them.
godot::PackedInt64Array packed(const std::vector<std::size_t>& people) {
  godot::PackedInt64Array indices;
  for (const std::size_t person : people) {
    indices.push_back(static_cast<std::int64_t>(person));
  }
  return indices;
}

}  // namespace

godot::Ref<WorldAdapter> WorldAdapter::create(
    std::int64_t seed, const godot::PackedStringArray& map_rows,
    const godot::TypedArray<godot::Dictionary>& armies,
    const godot::TypedArray<godot::Dictionary>& structures,
    const godot::Dictionary& rules,
    const godot::TypedArray<godot::Dictionary>& leaderships) {
  // The one handler for the core's refusal: it must not cross into Godot.
  try {
    std::vector<std::string> rows;
    for (const godot::String& row : map_rows) {
      rows.emplace_back(row.utf8().get_data());
    }
    return over(murim::Seed{static_cast<std::uint64_t>(seed)}, std::move(rows),
                each(armies, army_spec), each(structures, structure),
                murim_godot::rules(rules), {}, each(leaderships, leadership));
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

godot::Ref<WorldAdapter> WorldAdapter::create_region(
    std::int64_t seed, std::int64_t village, std::int64_t ticks_per_day) {
  // The one handler for a village the region lacks or a day it refuses: it
  // must not cross into Godot.
  try {
    return born_into(
        region_of(murim::Seed{static_cast<std::uint64_t>(seed)},
                  narrow<std::uint64_t>(ticks_per_day, "ticks_per_day")),
        narrow<std::size_t>(village, "village"));
  } catch (const std::logic_error& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.create_region: ",
                                        error.what());
    return {};
  }
}

godot::Ref<WorldAdapter> WorldAdapter::born_into(murim::Region region,
                                                 std::size_t village) {
  region.armies.push_back(murim::born_in(region, village));
  std::vector<murim::VillageSpec> villages = murim::village_specs(region);
  godot::Ref<WorldAdapter> adapter =
      over(region.seed, region.map_rows, region.armies, region.structures,
           region.rules, std::move(villages), region.leadership, region.life,
           region.sects);
  // The person born last is the player's body, which carries a soul, in the
  // army the player steers.
  adapter->world().ensoul(adapter->world().population() - 1);
  adapter->world().steer(adapter->world().army_count() - 1, true);
  adapter->show(region, murim::Origin{region.seed, village});
  return adapter;
}

void WorldAdapter::show(murim::Region& region, murim::Origin origin) {
  structure_kinds_ = std::move(region.structure_kinds);
  region_data_ = murim::region_data();
  terrain_ = std::move(region.terrain);
  villages_ = std::move(region.villages);
  origin_ = origin;
}

godot::Ref<WorldAdapter> WorldAdapter::load(std::int64_t slot,
                                            const godot::String& directory) {
  // The one handler for a slot out of range or a region the save names that
  // no longer has its village: it must not cross into Godot.
  try {
    std::variant<murim::Saved, murim::LoadError> loaded = murim::load(
        FileSaves{directory}, murim::Slot{narrow<std::uint32_t>(slot, "slot")});
    if (const auto* error = std::get_if<murim::LoadError>(&loaded)) {
      godot::UtilityFunctions::push_error("WorldAdapter.load: ",
                                          refusal_of(*error));
      return {};
    }
    auto& saved = std::get<murim::Saved>(loaded);
    if (!saved.origin) {
      godot::UtilityFunctions::push_error(
          "WorldAdapter.load: the save holds no generated region");
      return {};
    }
    murim::Region region =
        region_of(saved.origin->region, murim::TICKS_PER_DAY);
    region.armies.push_back(murim::born_in(region, saved.origin->village));
    godot::Ref<WorldAdapter> adapter;
    adapter.instantiate();
    adapter->strengths_ = strengths_of(region.armies);
    adapter->faction_count_ = region.rules.hostile.size();
    adapter->names_ = region.life.names;
    adapter->disciplines_ = discipline_names_of(region.life);
    adapter->map_rows_ = region.map_rows;
    adapter->world_.emplace(std::move(saved.world));
    adapter->show(region, *saved.origin);
    return adapter;
  } catch (const std::logic_error& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.load: ", error.what());
    return {};
  }
}

std::int64_t WorldAdapter::load_refusal(std::int64_t slot,
                                        const godot::String& directory) {
  // The one handler for a slot out of range: it must not cross into Godot.
  try {
    const std::variant<murim::Saved, murim::LoadError> loaded = murim::load(
        FileSaves{directory}, murim::Slot{narrow<std::uint32_t>(slot, "slot")});
    if (const auto* error = std::get_if<murim::LoadError>(&loaded)) {
      return static_cast<std::int64_t>(*error);
    }
    return std::get<murim::Saved>(loaded).origin
               ? LOAD_NONE
               : static_cast<std::int64_t>(murim::LoadError::NOT_A_SAVE);
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.load_refusal: ",
                                        error.what());
    return static_cast<std::int64_t>(murim::LoadError::MISSING);
  }
}

godot::String WorldAdapter::saves_directory() {
  return FileSaves::DEFAULT_DIRECTORY;
}

bool WorldAdapter::save(std::int64_t slot,
                        const godot::String& directory) const {
  if (!origin_) {
    godot::UtilityFunctions::push_error(
        "WorldAdapter.save: only a world made by create_region() is saved");
    return false;
  }
  // The one handler for a slot out of range: it must not cross into Godot.
  try {
    FileSaves files{directory};
    if (!murim::save(murim::Saved{origin_, world()}, files,
                     murim::Slot{narrow<std::uint32_t>(slot, "slot")})) {
      godot::UtilityFunctions::push_error(
          "WorldAdapter.save: the save could not be written");
      return false;
    }
    return true;
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.save: ", error.what());
    return false;
  }
}

std::int64_t WorldAdapter::player_army() const {
  const murim::World& held = world();
  // The soul's latest body is the last person carrying it.
  for (std::size_t person = held.population(); person-- > 0;) {
    if (held.person(person).soul != PLAYER_SOUL) {
      continue;
    }
    for (std::size_t army = 0; army < held.army_count(); ++army) {
      const std::vector<std::size_t>& roster = held.roster(army);
      if (std::find(roster.begin(), roster.end(), person) != roster.end()) {
        return static_cast<std::int64_t>(army);
      }
    }
    return -1;
  }
  return -1;
}

std::int64_t WorldAdapter::region_seed() const {
  return origin_ ? static_cast<std::int64_t>(origin_->region) : -1;
}

godot::Vector2i WorldAdapter::m1_centre() {
  const murim::Position centre = murim::centre_of(murim::m1_scenario().centre);
  return godot::Vector2i{centre.x, centre.y};
}

godot::Ref<WorldAdapter> WorldAdapter::over(
    murim::Seed seed, std::vector<std::string> map_rows,
    std::vector<murim::ArmySpec> armies,
    std::vector<murim::Structure> structures, murim::Rules rules,
    std::vector<murim::VillageSpec> villages,
    std::vector<murim::Leadership> leadership, murim::Life life,
    std::vector<murim::SectSpec> sects) {
  godot::Ref<WorldAdapter> adapter;
  adapter.instantiate();
  adapter->strengths_ = strengths_of(armies);
  adapter->faction_count_ = rules.hostile.size();
  adapter->names_ = life.names;
  adapter->disciplines_ = discipline_names_of(life);
  adapter->world_.emplace(seed, murim::Map{map_rows}, std::move(armies),
                          std::move(structures), std::move(rules),
                          std::move(villages), std::move(leadership),
                          std::move(life), std::move(sects));
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
      army < 0 || static_cast<std::uint64_t>(army) >= world().army_count(),
      "army index out of range");
  return static_cast<std::size_t>(army);
}

std::size_t WorldAdapter::faction_index(std::int64_t faction) const {
  CRASH_COND_MSG(
      faction < 0 || static_cast<std::uint64_t>(faction) >= faction_count_,
      "faction index out of range");
  return static_cast<std::size_t>(faction);
}

std::size_t WorldAdapter::person_index(std::int64_t person) const {
  CRASH_COND_MSG(
      person < 0 || static_cast<std::uint64_t>(person) >= world().population(),
      "person index out of range");
  return static_cast<std::size_t>(person);
}

std::size_t WorldAdapter::ware_index(std::int64_t ware) const {
  CRASH_COND_MSG(
      ware < 0 || static_cast<std::uint64_t>(ware) >= world().wares().size(),
      "ware index out of range");
  return static_cast<std::size_t>(ware);
}

std::size_t WorldAdapter::structure_index(std::int64_t structure) const {
  CRASH_COND_MSG(structure < 0 || static_cast<std::uint64_t>(structure) >=
                                      world().structure_count(),
                 "structure index out of range");
  return static_cast<std::size_t>(structure);
}

bool WorldAdapter::order(std::int64_t army, const godot::Vector2i& objective,
                         const godot::PackedInt64Array& skills,
                         std::int64_t challenge, std::int64_t target) {
  // The one handler for the core's refusal: it must not cross into Godot.
  try {
    const godot::Variant cell = objective;
    std::vector<std::size_t> chosen;
    for (const std::int64_t skill : skills) {
      chosen.push_back(narrow<std::size_t>(skill, "skill"));
    }
    std::optional<std::size_t> challenged;
    if (challenge >= 0) {
      challenged = narrow<std::size_t>(challenge, "challenge");
    }
    std::optional<std::size_t> attacked;
    if (target >= 0) {
      attacked = narrow<std::size_t>(target, "target");
    }
    world().order(
        narrow<std::size_t>(army, "army"),
        murim::Order{murim::Cell{narrow<std::int32_t>(cell.get("x"), "x"),
                                 narrow<std::int32_t>(cell.get("y"), "y")},
                     std::move(chosen), attacked},
        challenged);
    return true;
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.order: ", error.what());
    return false;
  }
}

std::int64_t WorldAdapter::attack_refusal(std::int64_t army,
                                          std::int64_t person) const {
  if (person < 0) {
    return static_cast<std::int64_t>(murim::AttackRefusal::NO_TARGET);
  }
  return static_cast<std::int64_t>(world().attack_refusal(
      army_index(army), static_cast<std::size_t>(person)));
}

std::int64_t WorldAdapter::target(std::int64_t army) const {
  const std::optional<std::size_t> aimed = world().target(army_index(army));
  return aimed ? static_cast<std::int64_t>(*aimed) : -1;
}

godot::PackedInt64Array WorldAdapter::foes_in_sight(std::int64_t person) const {
  godot::PackedInt64Array seen;
  for (const std::size_t foe : world().foes_in_sight(person_index(person))) {
    seen.push_back(static_cast<std::int64_t>(foe));
  }
  return seen;
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
  return static_cast<std::int64_t>(world().army_count());
}

std::int64_t WorldAdapter::skill_count(std::int64_t army) const {
  return static_cast<std::int64_t>(world().skills(army_index(army)).size());
}

godot::Dictionary WorldAdapter::date() const {
  const murim::Date today = world().date();
  godot::Dictionary fields;
  fields.set("year", static_cast<std::int64_t>(today.year));
  fields.set("season", static_cast<std::int64_t>(today.season));
  fields.set("day", static_cast<std::int64_t>(today.day));
  return fields;
}

std::int64_t WorldAdapter::tick_count() const {
  return static_cast<std::int64_t>(world().tick_count());
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
  return static_cast<std::int64_t>(world().structure_count());
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
  if (index < structure_kinds_.size()) {
    return {structure_kinds_.at(index).c_str()};
  }
  // A structure growth laid takes its plan's kind.
  const std::optional<std::size_t> plan = world().plan_of(index);
  const std::optional<murim::Growth>& growth = region_data_.life.growth;
  return plan && growth ? godot::String(growth->plans.at(*plan).kind.c_str())
                        : godot::String();
}

godot::TypedArray<godot::Dictionary> WorldAdapter::skills(
    std::int64_t army) const {
  godot::TypedArray<godot::Dictionary> skills;
  for (const murim::Skill& skill : world().skills(army_index(army))) {
    godot::Dictionary fields;
    fields.set("range", skill.range);
    fields.set("radius", skill.radius);
    fields.set("body_damage", static_cast<std::int64_t>(skill.body_damage));
    fields.set("structure_damage", skill.structure_damage);
    fields.set("cooldown", static_cast<std::int64_t>(skill.cooldown));
    fields.set("shape", static_cast<std::int64_t>(skill.shape));
    skills.push_back(fields);
  }
  return skills;
}

godot::TypedArray<godot::Dictionary> WorldAdapter::strikes() const {
  godot::TypedArray<godot::Dictionary> strikes;
  for (const murim::Strike& strike : world().strikes()) {
    godot::Dictionary fields;
    fields.set("person", static_cast<std::int64_t>(strike.person));
    fields.set("skill", static_cast<std::int64_t>(strike.skill));
    fields.set("shape", static_cast<std::int64_t>(strike.shape));
    fields.set("range", strike.range);
    fields.set("radius", strike.radius);
    fields.set("body_damage", static_cast<std::int64_t>(strike.body_damage));
    fields.set("origin", vector(strike.origin));
    fields.set("toward", vector(strike.toward));
    fields.set("end", vector(strike.end));
    godot::PackedInt64Array struck;
    for (const std::size_t person : strike.struck) {
      struck.push_back(static_cast<std::int64_t>(person));
    }
    fields.set("struck", struck);
    strikes.push_back(fields);
  }
  return strikes;
}

std::int64_t WorldAdapter::structure_field_state(std::int64_t structure) const {
  const std::optional<murim::FieldState> state =
      world().field_state(structure_index(structure));
  return state ? static_cast<std::int64_t>(*state) : -1;
}

std::size_t WorldAdapter::village_index(std::int64_t village) const {
  CRASH_COND_MSG(village < 0 || static_cast<std::uint64_t>(village) >=
                                    world().village_count(),
                 "village index out of range");
  return static_cast<std::size_t>(village);
}

std::int64_t WorldAdapter::village_cap(std::int64_t village) const {
  return static_cast<std::int64_t>(world().cap(village_index(village)));
}

std::int64_t WorldAdapter::village_stock(std::int64_t village) const {
  return static_cast<std::int64_t>(world().stock(village_index(village)));
}

std::int64_t WorldAdapter::village_population(std::int64_t village) const {
  return static_cast<std::int64_t>(
      world().residents_of(village_index(village)));
}

std::int64_t WorldAdapter::structure_stage(std::int64_t structure) const {
  return static_cast<std::int64_t>(world().stage(structure_index(structure)));
}

std::int64_t WorldAdapter::army_faction(std::int64_t army) const {
  return static_cast<std::int64_t>(world().faction(army_index(army)));
}

std::int64_t WorldAdapter::army_strength(std::int64_t army) const {
  const std::size_t index = army_index(army);
  return index < strengths_.size()
             ? static_cast<std::int64_t>(strengths_.at(index))
             : 0;
}

godot::PackedInt64Array WorldAdapter::body_strengths(std::int64_t army) const {
  godot::PackedInt64Array strengths;
  for (const std::size_t member : world().roster(army_index(army))) {
    strengths.push_back(
        static_cast<std::int64_t>(world().person(member).health));
  }
  return strengths;
}

godot::PackedInt64Array WorldAdapter::body_ready(std::int64_t army) const {
  godot::PackedInt64Array ready;
  for (const std::size_t member : world().roster(army_index(army))) {
    ready.push_back(static_cast<std::int64_t>(world().person(member).ready_at));
  }
  return ready;
}

godot::TypedArray<godot::Dictionary> WorldAdapter::events(
    std::int64_t from) const {
  godot::TypedArray<godot::Dictionary> kept;
  for (const murim::Event& event : world().events()) {
    if (static_cast<std::int64_t>(event.sequence) < from) {
      continue;
    }
    godot::Dictionary entry;
    entry.set("sequence", static_cast<std::int64_t>(event.sequence));
    entry.set("tick", static_cast<std::int64_t>(event.tick));
    entry.set("cell", godot::Vector2i{event.cell.x, event.cell.y});
    entry.set("kind", static_cast<std::int64_t>(event.what.index()));
    if (const auto* born = std::get_if<murim::Born>(&event.what)) {
      entry.set("person", static_cast<std::int64_t>(born->person));
    } else if (const auto* died = std::get_if<murim::Died>(&event.what)) {
      entry.set("person", static_cast<std::int64_t>(died->person));
    }
    kept.push_back(entry);
  }
  return kept;
}

godot::TypedArray<godot::Dictionary> WorldAdapter::progress(
    std::int64_t from) const {
  godot::TypedArray<godot::Dictionary> kept;
  for (const murim::Progress& step : world().progress()) {
    if (static_cast<std::int64_t>(step.sequence) < from) {
      continue;
    }
    godot::Dictionary entry;
    entry.set("sequence", static_cast<std::int64_t>(step.sequence));
    entry.set("tick", static_cast<std::int64_t>(step.tick));
    entry.set("kind", static_cast<std::int64_t>(step.what.index()));
    if (const auto* learned = std::get_if<murim::Learned>(&step.what)) {
      entry.set("person", static_cast<std::int64_t>(learned->person));
      entry.set("art", static_cast<std::int64_t>(learned->art));
      entry.set("skill", static_cast<std::int64_t>(learned->skill));
      entry.set("by", static_cast<std::int64_t>(learned->by));
    } else {
      const auto& gained = std::get<murim::RealmGained>(step.what);
      entry.set("person", static_cast<std::int64_t>(gained.person));
      entry.set("rank", static_cast<std::int64_t>(gained.rank));
    }
    kept.push_back(entry);
  }
  return kept;
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
  if (!terrain_.empty()) {
    for (const std::optional<std::size_t> owner : world().territory()) {
      owners.push_back(owner ? static_cast<std::int64_t>(*owner) : -1);
    }
  }
  return owners;
}

godot::TypedArray<godot::Dictionary> WorldAdapter::villages() const {
  godot::TypedArray<godot::Dictionary> villages;
  const auto add = [this, &villages](
                       const std::string& name, const std::string& kind,
                       murim::Cell corner, murim::Footprint area) {
    const auto index = static_cast<std::size_t>(villages.size());
    godot::Dictionary fields;
    fields.set("name", godot::String(name.c_str()));
    fields.set("kind", godot::String(kind.c_str()));
    fields.set("faction", static_cast<std::int64_t>(world().owner(index)));
    fields.set("corner", godot::Vector2i{corner.x, corner.y});
    fields.set("area", godot::Vector2i{area.width, area.height});
    villages.push_back(fields);
  };
  for (const murim::Village& village : villages_) {
    add(village.name, village.kind, village.corner, village.area);
  }
  // Those founded since, on the land the core says each was founded on.
  for (std::size_t village = villages_.size();
       village < world().village_count(); ++village) {
    const std::optional<murim::Settlement> land = world().settlement(village);
    CRASH_COND_MSG(!land, "a village added as the world runs was not founded");
    add(land->kind + " " + std::to_string(village + 1), land->kind,
        land->corner, land->area);
  }
  return villages;
}

std::int64_t WorldAdapter::faction_count() const {
  return static_cast<std::int64_t>(faction_count_);
}

std::int64_t WorldAdapter::leader(std::int64_t faction) const {
  const std::optional<std::size_t> person =
      world().leader(faction_index(faction));
  return person ? static_cast<std::int64_t>(*person) : -1;
}

bool WorldAdapter::elects(std::int64_t faction) const {
  return world().elects(faction_index(faction));
}

std::int64_t WorldAdapter::challenge_refusal(std::int64_t army,
                                             std::int64_t faction) const {
  return static_cast<std::int64_t>(
      world().challenge_refusal(army_index(army), faction_index(faction)));
}

godot::Dictionary WorldAdapter::duel(std::int64_t faction) const {
  godot::Dictionary fields;
  const std::optional<murim::Duellists> fought =
      world().duel(faction_index(faction));
  if (fought) {
    fields.set("challenger", static_cast<std::int64_t>(fought->challenger));
    fields.set("leader", static_cast<std::int64_t>(fought->leader));
  }
  return fields;
}

bool WorldAdapter::hostile(std::int64_t from, std::int64_t target) const {
  return world().hostile(faction_index(from), faction_index(target));
}

std::int64_t WorldAdapter::relation(std::int64_t faction,
                                    std::int64_t other) const {
  return static_cast<std::int64_t>(
      world().relation(faction_index(faction), faction_index(other)));
}

godot::Dictionary WorldAdapter::declaration(std::int64_t faction,
                                            std::int64_t other) const {
  godot::Dictionary fields;
  const std::optional<murim::Declared> standing =
      world().declaration(faction_index(faction), faction_index(other));
  if (standing) {
    fields.set("what", static_cast<std::int64_t>(standing->what));
    fields.set("by", static_cast<std::int64_t>(standing->by));
    fields.set("tick", static_cast<std::int64_t>(standing->tick));
    const murim::Date made = world().date_at(standing->tick);
    fields.set("year", static_cast<std::int64_t>(made.year));
    fields.set("season", static_cast<std::int64_t>(made.season));
  }
  return fields;
}

murim::Declaration WorldAdapter::declaration_of(std::int64_t what) {
  CRASH_COND_MSG(
      what != static_cast<std::int64_t>(murim::Declaration::WAR) &&
          what != static_cast<std::int64_t>(murim::Declaration::PEACE),
      "no such declaration");
  return static_cast<murim::Declaration>(what);
}

std::int64_t WorldAdapter::declaration_refusal(std::int64_t person,
                                               std::int64_t faction,
                                               std::int64_t what) const {
  return static_cast<std::int64_t>(world().declaration_refusal(
      person_index(person), faction_index(faction), declaration_of(what)));
}

bool WorldAdapter::declare(std::int64_t person, std::int64_t faction,
                           std::int64_t what) {
  // The one handler for the core's refusal: it must not cross into Godot.
  try {
    world().declare(person_index(person), faction_index(faction),
                    declaration_of(what));
    return true;
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.declare: ", error.what());
    return false;
  }
}

void WorldAdapter::steer(std::int64_t army, bool steered) {
  world().steer(army_index(army), steered);
}

bool WorldAdapter::steered(std::int64_t army) const {
  return world().steered(army_index(army));
}

godot::PackedInt64Array WorldAdapter::roster(std::int64_t army) const {
  godot::PackedInt64Array members;
  for (const std::size_t member : world().roster(army_index(army))) {
    members.push_back(static_cast<std::int64_t>(member));
  }
  return members;
}

godot::Vector2 WorldAdapter::person_position(std::int64_t person) const {
  const murim::Position position =
      world().person(person_index(person)).position;
  return {static_cast<float>(position.x), static_cast<float>(position.y)};
}

std::int64_t WorldAdapter::person_health(std::int64_t person) const {
  return static_cast<std::int64_t>(world().person(person_index(person)).health);
}

godot::Dictionary WorldAdapter::person_name(std::int64_t person) const {
  godot::Dictionary fields;
  const std::optional<murim::Name> name = world().name(person_index(person));
  if (name) {
    fields.set("surname", godot::String::utf8(name->surname.korean.c_str()));
    fields.set("given", godot::String::utf8(name->given.korean.c_str()));
    fields.set("surname_en", godot::String(name->surname.romanised.c_str()));
    fields.set("given_en", godot::String(name->given.romanised.c_str()));
  }
  return fields;
}

godot::Dictionary WorldAdapter::names() const {
  const auto entries = [](const std::vector<murim::NameEntry>& from) {
    godot::Array entries;
    for (const murim::NameEntry& entry : from) {
      godot::Dictionary fields;
      fields.set("korean", godot::String::utf8(entry.korean.c_str()));
      fields.set("romanised", godot::String(entry.romanised.c_str()));
      entries.push_back(fields);
    }
    return entries;
  };
  godot::Dictionary names;
  names.set("surnames", entries(names_.surnames));
  names.set("female", entries(names_.female));
  names.set("male", entries(names_.male));
  return names;
}

godot::PackedInt64Array WorldAdapter::person_parents(
    std::int64_t person) const {
  return indices(world().person(person_index(person)).parents);
}

godot::PackedInt64Array WorldAdapter::person_children(
    std::int64_t person) const {
  return indices(world().person(person_index(person)).children);
}

std::int64_t WorldAdapter::person_spouse(std::int64_t person) const {
  const std::vector<std::size_t>& spouses =
      world().person(person_index(person)).spouses;
  return spouses.empty() ? -1 : static_cast<std::int64_t>(spouses.back());
}

std::int64_t WorldAdapter::person_family(std::int64_t person) const {
  return static_cast<std::int64_t>(world().person(person_index(person)).family);
}

std::int64_t WorldAdapter::person_age(std::int64_t person) const {
  return world().age_in_years(person_index(person));
}

std::int64_t WorldAdapter::person_faction(std::int64_t person) const {
  const std::optional<std::size_t> faction =
      world().faction_of(person_index(person));
  return faction ? static_cast<std::int64_t>(*faction) : -1;
}

std::size_t WorldAdapter::sect_index(std::int64_t sect) const {
  CRASH_COND_MSG(
      sect < 0 || static_cast<std::uint64_t>(sect) >= world().sect_count(),
      "sect index out of range");
  return static_cast<std::size_t>(sect);
}

std::int64_t WorldAdapter::sect_count() const {
  return world_ ? static_cast<std::int64_t>(world().sect_count()) : 0;
}

std::int64_t WorldAdapter::application_refusal(std::int64_t person,
                                               std::int64_t sect) const {
  return static_cast<std::int64_t>(
      world().application_refusal(person_index(person), sect_index(sect)));
}

bool WorldAdapter::apply(std::int64_t person, std::int64_t sect) {
  // The one handler for the core's refusal: it must not cross into Godot.
  try {
    world().apply(person_index(person), sect_index(sect));
    return true;
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.apply: ", error.what());
    return false;
  }
}

bool WorldAdapter::cast(std::int64_t person, std::int64_t skill,
                        const godot::Variant& target) {
  // The one handler for the core's refusal: it must not cross into Godot.
  try {
    world().cast(person_index(person), narrow<std::size_t>(skill, "skill"),
                 target_of(target));
    return true;
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.cast: ", error.what());
    return false;
  }
}

std::int64_t WorldAdapter::cast_refusal(std::int64_t person, std::int64_t skill,
                                        const godot::Variant& target) const {
  try {
    return static_cast<std::int64_t>(world().cast_refusal(
        person_index(person), narrow<std::size_t>(skill, "skill"),
        target_of(target)));
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.cast_refusal: ",
                                        error.what());
    return -1;
  }
}

std::int64_t WorldAdapter::aim(std::int64_t person, std::int64_t skill,
                               const godot::Vector2& facing) const {
  try {
    const murim::Position way = units(facing, "facing");
    const std::optional<std::size_t> aimed =
        world().aim(person_index(person), narrow<std::size_t>(skill, "skill"),
                    murim::Step{way.x, way.y});
    return aimed ? static_cast<std::int64_t>(*aimed) : -1;
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.aim: ", error.what());
    return -1;
  }
}

std::int64_t WorldAdapter::talk_refusal(std::int64_t person,
                                        std::int64_t other) const {
  return static_cast<std::int64_t>(
      world().talk_refusal(person_index(person), person_index(other)));
}

godot::PackedInt64Array WorldAdapter::offers(std::int64_t person) const {
  godot::PackedInt64Array offered;
  for (const murim::Offer offer : world().offers(person_index(person))) {
    offered.push_back(static_cast<std::int64_t>(offer));
  }
  return offered;
}

godot::TypedArray<godot::Dictionary> WorldAdapter::wares() const {
  godot::TypedArray<godot::Dictionary> wares;
  for (const murim::Ware& ware : world().wares()) {
    godot::Dictionary fields;
    fields.set("name", godot::String(ware.name.c_str()));
    fields.set("price", static_cast<std::int64_t>(ware.price));
    fields.set("health", static_cast<std::int64_t>(ware.health));
    fields.set("qi", static_cast<std::int64_t>(ware.qi));
    fields.set("discipline", static_cast<std::int64_t>(ware.discipline));
    wares.push_back(fields);
  }
  return wares;
}

std::int64_t WorldAdapter::person_silver(std::int64_t person) const {
  return static_cast<std::int64_t>(world().person(person_index(person)).silver);
}

godot::PackedInt64Array WorldAdapter::person_held(std::int64_t person) const {
  godot::PackedInt64Array held;
  for (const std::uint64_t count : world().person(person_index(person)).held) {
    held.push_back(static_cast<std::int64_t>(count));
  }
  return held;
}

bool WorldAdapter::sells(std::int64_t person, std::int64_t ware) const {
  return world().sells(person_index(person), ware_index(ware));
}

std::int64_t WorldAdapter::buy_refusal(std::int64_t buyer, std::int64_t seller,
                                       std::int64_t ware) const {
  return static_cast<std::int64_t>(world().buy_refusal(
      person_index(buyer), person_index(seller), ware_index(ware)));
}

bool WorldAdapter::buy(std::int64_t buyer, std::int64_t seller,
                       std::int64_t ware) {
  // The one handler for the core's refusal: it must not cross into Godot.
  try {
    world().buy(person_index(buyer), person_index(seller), ware_index(ware));
    return true;
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.buy: ", error.what());
    return false;
  }
}

std::int64_t WorldAdapter::consume_refusal(std::int64_t person,
                                           std::int64_t ware) const {
  return static_cast<std::int64_t>(
      world().consume_refusal(person_index(person), ware_index(ware)));
}

bool WorldAdapter::consume(std::int64_t person, std::int64_t ware) {
  // The one handler for the core's refusal: it must not cross into Godot.
  try {
    world().consume(person_index(person), ware_index(ware));
    return true;
  } catch (const std::invalid_argument& error) {
    godot::UtilityFunctions::push_error("WorldAdapter.consume: ", error.what());
    return false;
  }
}

godot::TypedArray<godot::Dictionary> WorldAdapter::rumours(
    std::int64_t person) const {
  godot::TypedArray<godot::Dictionary> heard;
  for (const murim::Event& event : world().rumours(person_index(person))) {
    godot::Dictionary fields;
    fields.set("sequence", static_cast<std::int64_t>(event.sequence));
    fields.set("tick", static_cast<std::int64_t>(event.tick));
    fields.set("cell", godot::Vector2i{event.cell.x, event.cell.y});
    const auto index = [](std::size_t value) {
      return static_cast<std::int64_t>(value);
    };
    if (const auto* founded = std::get_if<murim::Founded>(&event.what)) {
      fields.set("kind", "founded");
      fields.set("village", index(founded->village));
      fields.set("faction", index(founded->faction));
    } else if (const auto* raided = std::get_if<murim::Raided>(&event.what)) {
      fields.set("kind", "raided");
      fields.set("village", index(raided->village));
      fields.set("owner", index(raided->owner));
      fields.set("raider", index(raided->raider));
    } else if (const auto* ruined = std::get_if<murim::Ruined>(&event.what)) {
      fields.set("kind", "ruined");
      fields.set("village", index(ruined->village));
      fields.set("owner", index(ruined->owner));
      fields.set("army", index(ruined->destroyer.army));
      fields.set("destroyer", index(ruined->destroyer.faction));
    } else if (const auto* fell = std::get_if<murim::SectFell>(&event.what)) {
      fields.set("kind", "sect_fell");
      fields.set("sect", index(fell->sect));
      fields.set("faction", index(fell->faction));
    } else if (const auto* changed =
                   std::get_if<murim::LeaderChanged>(&event.what)) {
      fields.set("kind", "leader_changed");
      fields.set("faction", index(changed->faction));
      fields.set("leader", index(changed->leader));
      fields.set("by",
                 changed->by == murim::ChosenBy::DUEL ? "duel" : "election");
    } else if (const auto* born = std::get_if<murim::Born>(&event.what)) {
      fields.set("kind", "born");
      fields.set("person", index(born->person));
    } else if (const auto* died = std::get_if<murim::Died>(&event.what)) {
      fields.set("kind", "died");
      fields.set("person", index(died->person));
    }
    heard.append(fields);
  }
  return heard;
}

godot::PackedInt64Array WorldAdapter::person_attainment(
    std::int64_t person) const {
  godot::PackedInt64Array attained;
  for (const std::uint32_t points :
       world().person(person_index(person)).attainment) {
    attained.append(points);
  }
  return attained;
}

godot::PackedStringArray WorldAdapter::discipline_names() const {
  godot::PackedStringArray names;
  for (const std::string& name : disciplines_) {
    names.append(name.c_str());
  }
  return names;
}

std::int64_t WorldAdapter::person_sect(std::int64_t person) const {
  const std::optional<std::size_t> sect = world().sect_of(person_index(person));
  return sect ? static_cast<std::int64_t>(*sect) : -1;
}

std::int64_t WorldAdapter::person_qi(std::int64_t person) const {
  return world().person(person_index(person)).qi;
}

std::int64_t WorldAdapter::person_qi_capacity(std::int64_t person) const {
  return world().person(person_index(person)).qi_capacity;
}

std::int64_t WorldAdapter::person_realm(std::int64_t person) const {
  return world().rank(person_index(person));
}

godot::Dictionary WorldAdapter::person_nature(std::int64_t person) const {
  godot::Dictionary fields;
  if (const std::optional<murim::Nature> nature =
          world().nature(person_index(person))) {
    fields.set("orientation", static_cast<std::int64_t>(nature->orientation));
    fields.set("polarity", static_cast<std::int64_t>(nature->polarity));
  }
  return fields;
}

godot::TypedArray<godot::Dictionary> WorldAdapter::person_skills(
    std::int64_t person) const {
  const std::size_t index = person_index(person);
  const murim::World& core = world();
  const std::size_t count = core.skill_count(index);
  const std::vector<murim::Known>& known = core.person(index).known;
  const std::size_t drilled = count - std::min(count, known.size());
  godot::TypedArray<godot::Dictionary> skills;
  for (std::size_t skill = 0; skill < count; ++skill) {
    const murim::Skill& used = core.skill_of(index, skill);
    godot::Dictionary fields;
    fields.set("range", used.range);
    fields.set("radius", used.radius);
    fields.set("body_damage", static_cast<std::int64_t>(used.body_damage));
    fields.set("structure_damage", used.structure_damage);
    fields.set("cooldown", static_cast<std::int64_t>(used.cooldown));
    fields.set("qi_cost", used.qi_cost);
    fields.set("shape", static_cast<std::int64_t>(used.shape));
    fields.set("ready_at",
               static_cast<std::int64_t>(core.ready_at(index, skill)));
    fields.set("art", -1);
    if (skill >= drilled) {
      const murim::Known& learned = known.at(skill - drilled);
      const murim::Art& art = core.arts().at(learned.art);
      const murim::ArtSkill& taught = art.skills.at(learned.skill);
      fields.set("art", static_cast<std::int64_t>(learned.art));
      fields.set("skill", static_cast<std::int64_t>(learned.skill));
      fields.set("rank", taught.rank);
      fields.set("korean", godot::String::utf8(taught.name.korean.c_str()));
      fields.set("romanised",
                 godot::String::utf8(taught.name.romanised.c_str()));
      fields.set("orientation",
                 static_cast<std::int64_t>(art.nature.orientation));
      fields.set("polarity", static_cast<std::int64_t>(art.nature.polarity));
    }
    skills.push_back(fields);
  }
  return skills;
}

godot::TypedArray<godot::Dictionary> WorldAdapter::arts() const {
  godot::TypedArray<godot::Dictionary> arts;
  if (!world_) {
    return arts;
  }
  for (const murim::Art& art : world().arts()) {
    godot::Dictionary fields;
    fields.set("korean", godot::String::utf8(art.name.korean.c_str()));
    fields.set("romanised", godot::String::utf8(art.name.romanised.c_str()));
    fields.set("orientation",
               static_cast<std::int64_t>(art.nature.orientation));
    fields.set("polarity", static_cast<std::int64_t>(art.nature.polarity));
    godot::TypedArray<godot::Dictionary> skills;
    for (const murim::ArtSkill& taught : art.skills) {
      godot::Dictionary skill;
      skill.set("korean", godot::String::utf8(taught.name.korean.c_str()));
      skill.set("romanised",
                godot::String::utf8(taught.name.romanised.c_str()));
      skill.set("rank", taught.rank);
      skill.set("range", taught.skill.range);
      skill.set("radius", taught.skill.radius);
      skill.set("cooldown", static_cast<std::int64_t>(taught.skill.cooldown));
      skill.set("qi_cost", taught.skill.qi_cost);
      skills.push_back(skill);
    }
    fields.set("skills", skills);
    arts.push_back(fields);
  }
  return arts;
}

godot::TypedArray<godot::Dictionary> WorldAdapter::realms() const {
  godot::TypedArray<godot::Dictionary> realms;
  if (!world_) {
    return realms;
  }
  for (const murim::Realm& realm : world().realms()) {
    godot::Dictionary fields;
    fields.set("korean", godot::String::utf8(realm.name.korean.c_str()));
    fields.set("romanised", godot::String::utf8(realm.name.romanised.c_str()));
    fields.set("qi", realm.qi);
    fields.set("might", realm.might);
    realms.push_back(fields);
  }
  return realms;
}

godot::String WorldAdapter::sect_name(std::int64_t sect) const {
  return {world().sect_name(sect_index(sect)).c_str()};
}

std::int64_t WorldAdapter::sect_faction(std::int64_t sect) const {
  return static_cast<std::int64_t>(world().sect_faction(sect_index(sect)));
}

std::int64_t WorldAdapter::sect_seat(std::int64_t sect) const {
  return static_cast<std::int64_t>(world().sect_seat(sect_index(sect)));
}

bool WorldAdapter::sect_seat_held(std::int64_t sect) const {
  return world().seat_held(sect_index(sect));
}

godot::PackedInt64Array WorldAdapter::sect_members(std::int64_t sect) const {
  return packed(world().sect_members(sect_index(sect)));
}

godot::PackedInt64Array WorldAdapter::sect_elders(std::int64_t sect) const {
  return packed(world().sect_elders(sect_index(sect)));
}

std::int64_t WorldAdapter::sect_master(std::int64_t sect) const {
  const std::optional<std::size_t> master =
      world().sect_master(sect_index(sect));
  return master ? static_cast<std::int64_t>(*master) : -1;
}

std::int64_t WorldAdapter::sect_cap(std::int64_t sect) const {
  return static_cast<std::int64_t>(world().sect_cap(sect_index(sect)));
}

bool WorldAdapter::sect_stands(std::int64_t sect) const {
  return world().sect_stands(sect_index(sect));
}

godot::TypedArray<godot::Dictionary> WorldAdapter::manuals() const {
  godot::TypedArray<godot::Dictionary> manuals;
  if (!world_) {
    return manuals;
  }
  for (const murim::Manual& manual : world().manuals()) {
    godot::Dictionary fields;
    fields.set("art", static_cast<std::int64_t>(manual.art));
    if (const auto* held = std::get_if<murim::HeldBySect>(&manual.place)) {
      fields.set("place", "sect");
      fields.set("sect", static_cast<std::int64_t>(held->sect));
    } else if (const auto* cell = std::get_if<murim::Cell>(&manual.place)) {
      fields.set("place", "cell");
      fields.set("cell", godot::Vector2i{cell->x, cell->y});
    } else {
      fields.set("place", "lost");
    }
    manuals.push_back(fields);
  }
  return manuals;
}

void WorldAdapter::_bind_methods() {
  const godot::StringName& name = WorldAdapter::get_class_static();
  godot::ClassDB::bind_static_method(
      name,
      godot::D_METHOD("create", "seed", "map_rows", "armies", "structures",
                      "rules", "leadership"),
      &WorldAdapter::create, DEFVAL(godot::TypedArray<godot::Dictionary>()));
  godot::ClassDB::bind_static_method(name, godot::D_METHOD("create_m1"),
                                     &WorldAdapter::create_m1);
  godot::ClassDB::bind_static_method(
      name,
      godot::D_METHOD("create_region", "seed", "village", "ticks_per_day"),
      &WorldAdapter::create_region, DEFVAL(0), DEFVAL(murim::TICKS_PER_DAY));
  godot::ClassDB::bind_static_method(name, godot::D_METHOD("m1_centre"),
                                     &WorldAdapter::m1_centre);
  godot::ClassDB::bind_static_method(
      name, godot::D_METHOD("load", "slot", "directory"), &WorldAdapter::load,
      DEFVAL(FileSaves::DEFAULT_DIRECTORY));
  godot::ClassDB::bind_static_method(
      name, godot::D_METHOD("load_refusal", "slot", "directory"),
      &WorldAdapter::load_refusal, DEFVAL(FileSaves::DEFAULT_DIRECTORY));
  godot::ClassDB::bind_static_method(name, godot::D_METHOD("saves_directory"),
                                     &WorldAdapter::saves_directory);
  godot::ClassDB::bind_method(godot::D_METHOD("save", "slot", "directory"),
                              &WorldAdapter::save,
                              DEFVAL(FileSaves::DEFAULT_DIRECTORY));
  godot::ClassDB::bind_method(godot::D_METHOD("player_army"),
                              &WorldAdapter::player_army);
  godot::ClassDB::bind_method(godot::D_METHOD("region_seed"),
                              &WorldAdapter::region_seed);
  godot::ClassDB::bind_method(godot::D_METHOD("order", "army", "objective",
                                              "skills", "challenge", "target"),
                              &WorldAdapter::order, DEFVAL(-1), DEFVAL(-1));
  godot::ClassDB::bind_method(
      godot::D_METHOD("attack_refusal", "army", "person"),
      &WorldAdapter::attack_refusal);
  godot::ClassDB::bind_method(godot::D_METHOD("target", "army"),
                              &WorldAdapter::target);
  godot::ClassDB::bind_method(godot::D_METHOD("foes_in_sight", "person"),
                              &WorldAdapter::foes_in_sight);
  godot::ClassDB::bind_method(godot::D_METHOD("faction_count"),
                              &WorldAdapter::faction_count);
  godot::ClassDB::bind_method(godot::D_METHOD("leader", "faction"),
                              &WorldAdapter::leader);
  godot::ClassDB::bind_method(godot::D_METHOD("elects", "faction"),
                              &WorldAdapter::elects);
  godot::ClassDB::bind_method(
      godot::D_METHOD("challenge_refusal", "army", "faction"),
      &WorldAdapter::challenge_refusal);
  godot::ClassDB::bind_method(godot::D_METHOD("duel", "faction"),
                              &WorldAdapter::duel);
  godot::ClassDB::bind_method(godot::D_METHOD("hostile", "from", "target"),
                              &WorldAdapter::hostile);
  godot::ClassDB::bind_method(godot::D_METHOD("relation", "faction", "other"),
                              &WorldAdapter::relation);
  godot::ClassDB::bind_method(
      godot::D_METHOD("declaration", "faction", "other"),
      &WorldAdapter::declaration);
  godot::ClassDB::bind_method(
      godot::D_METHOD("declaration_refusal", "person", "faction", "what"),
      &WorldAdapter::declaration_refusal);
  godot::ClassDB::bind_method(
      godot::D_METHOD("declare", "person", "faction", "what"),
      &WorldAdapter::declare);
  godot::ClassDB::bind_method(godot::D_METHOD("steer", "army", "steered"),
                              &WorldAdapter::steer);
  godot::ClassDB::bind_method(godot::D_METHOD("steered", "army"),
                              &WorldAdapter::steered);
  godot::ClassDB::bind_method(godot::D_METHOD("roster", "army"),
                              &WorldAdapter::roster);
  godot::ClassDB::bind_method(godot::D_METHOD("person_position", "person"),
                              &WorldAdapter::person_position);
  godot::ClassDB::bind_method(godot::D_METHOD("person_health", "person"),
                              &WorldAdapter::person_health);
  godot::ClassDB::bind_method(godot::D_METHOD("person_name", "person"),
                              &WorldAdapter::person_name);
  godot::ClassDB::bind_method(godot::D_METHOD("names"), &WorldAdapter::names);
  godot::ClassDB::bind_method(godot::D_METHOD("person_parents", "person"),
                              &WorldAdapter::person_parents);
  godot::ClassDB::bind_method(godot::D_METHOD("person_children", "person"),
                              &WorldAdapter::person_children);
  godot::ClassDB::bind_method(godot::D_METHOD("person_spouse", "person"),
                              &WorldAdapter::person_spouse);
  godot::ClassDB::bind_method(godot::D_METHOD("person_family", "person"),
                              &WorldAdapter::person_family);
  godot::ClassDB::bind_method(godot::D_METHOD("person_age", "person"),
                              &WorldAdapter::person_age);
  godot::ClassDB::bind_method(godot::D_METHOD("person_faction", "person"),
                              &WorldAdapter::person_faction);
  godot::ClassDB::bind_method(godot::D_METHOD("sect_count"),
                              &WorldAdapter::sect_count);
  godot::ClassDB::bind_method(
      godot::D_METHOD("application_refusal", "person", "sect"),
      &WorldAdapter::application_refusal);
  godot::ClassDB::bind_method(godot::D_METHOD("apply", "person", "sect"),
                              &WorldAdapter::apply);
  godot::ClassDB::bind_method(
      godot::D_METHOD("cast", "person", "skill", "target"),
      &WorldAdapter::cast);
  godot::ClassDB::bind_method(
      godot::D_METHOD("cast_refusal", "person", "skill", "target"),
      &WorldAdapter::cast_refusal);
  godot::ClassDB::bind_method(
      godot::D_METHOD("aim", "person", "skill", "facing"), &WorldAdapter::aim);
  godot::ClassDB::bind_method(godot::D_METHOD("rumours", "person"),
                              &WorldAdapter::rumours);
  godot::ClassDB::bind_method(
      godot::D_METHOD("talk_refusal", "person", "other"),
      &WorldAdapter::talk_refusal);
  godot::ClassDB::bind_method(godot::D_METHOD("offers", "person"),
                              &WorldAdapter::offers);
  godot::ClassDB::bind_method(godot::D_METHOD("wares"), &WorldAdapter::wares);
  godot::ClassDB::bind_method(godot::D_METHOD("person_silver", "person"),
                              &WorldAdapter::person_silver);
  godot::ClassDB::bind_method(godot::D_METHOD("person_held", "person"),
                              &WorldAdapter::person_held);
  godot::ClassDB::bind_method(godot::D_METHOD("sells", "person", "ware"),
                              &WorldAdapter::sells);
  godot::ClassDB::bind_method(
      godot::D_METHOD("buy_refusal", "buyer", "seller", "ware"),
      &WorldAdapter::buy_refusal);
  godot::ClassDB::bind_method(godot::D_METHOD("buy", "buyer", "seller", "ware"),
                              &WorldAdapter::buy);
  godot::ClassDB::bind_method(
      godot::D_METHOD("consume_refusal", "person", "ware"),
      &WorldAdapter::consume_refusal);
  godot::ClassDB::bind_method(godot::D_METHOD("consume", "person", "ware"),
                              &WorldAdapter::consume);
  godot::ClassDB::bind_method(godot::D_METHOD("person_attainment", "person"),
                              &WorldAdapter::person_attainment);
  godot::ClassDB::bind_method(godot::D_METHOD("discipline_names"),
                              &WorldAdapter::discipline_names);
  godot::ClassDB::bind_method(godot::D_METHOD("person_sect", "person"),
                              &WorldAdapter::person_sect);
  godot::ClassDB::bind_method(godot::D_METHOD("person_qi", "person"),
                              &WorldAdapter::person_qi);
  godot::ClassDB::bind_method(godot::D_METHOD("person_qi_capacity", "person"),
                              &WorldAdapter::person_qi_capacity);
  godot::ClassDB::bind_method(godot::D_METHOD("person_realm", "person"),
                              &WorldAdapter::person_realm);
  godot::ClassDB::bind_method(godot::D_METHOD("person_nature", "person"),
                              &WorldAdapter::person_nature);
  godot::ClassDB::bind_method(godot::D_METHOD("person_skills", "person"),
                              &WorldAdapter::person_skills);
  godot::ClassDB::bind_method(godot::D_METHOD("realms"), &WorldAdapter::realms);
  godot::ClassDB::bind_method(godot::D_METHOD("arts"), &WorldAdapter::arts);
  godot::ClassDB::bind_method(godot::D_METHOD("sect_name", "sect"),
                              &WorldAdapter::sect_name);
  godot::ClassDB::bind_method(godot::D_METHOD("sect_faction", "sect"),
                              &WorldAdapter::sect_faction);
  godot::ClassDB::bind_method(godot::D_METHOD("sect_seat", "sect"),
                              &WorldAdapter::sect_seat);
  godot::ClassDB::bind_method(godot::D_METHOD("sect_seat_held", "sect"),
                              &WorldAdapter::sect_seat_held);
  godot::ClassDB::bind_method(godot::D_METHOD("sect_members", "sect"),
                              &WorldAdapter::sect_members);
  godot::ClassDB::bind_method(godot::D_METHOD("sect_elders", "sect"),
                              &WorldAdapter::sect_elders);
  godot::ClassDB::bind_method(godot::D_METHOD("sect_master", "sect"),
                              &WorldAdapter::sect_master);
  godot::ClassDB::bind_method(godot::D_METHOD("sect_cap", "sect"),
                              &WorldAdapter::sect_cap);
  godot::ClassDB::bind_method(godot::D_METHOD("sect_stands", "sect"),
                              &WorldAdapter::sect_stands);
  godot::ClassDB::bind_method(godot::D_METHOD("manuals"),
                              &WorldAdapter::manuals);
  godot::ClassDB::bind_method(godot::D_METHOD("tick"), &WorldAdapter::tick);
  godot::ClassDB::bind_method(godot::D_METHOD("map_rows"),
                              &WorldAdapter::map_rows);
  godot::ClassDB::bind_method(godot::D_METHOD("date"), &WorldAdapter::date);
  godot::ClassDB::bind_method(godot::D_METHOD("tick_count"),
                              &WorldAdapter::tick_count);
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
  godot::ClassDB::bind_method(godot::D_METHOD("strikes"),
                              &WorldAdapter::strikes);
  godot::ClassDB::bind_method(godot::D_METHOD("structure_stage", "structure"),
                              &WorldAdapter::structure_stage);
  godot::ClassDB::bind_method(
      godot::D_METHOD("structure_field_state", "structure"),
      &WorldAdapter::structure_field_state);
  godot::ClassDB::bind_method(godot::D_METHOD("village_cap", "village"),
                              &WorldAdapter::village_cap);
  godot::ClassDB::bind_method(godot::D_METHOD("village_stock", "village"),
                              &WorldAdapter::village_stock);
  godot::ClassDB::bind_method(godot::D_METHOD("village_population", "village"),
                              &WorldAdapter::village_population);
  godot::ClassDB::bind_method(godot::D_METHOD("army_faction", "army"),
                              &WorldAdapter::army_faction);
  godot::ClassDB::bind_method(godot::D_METHOD("army_strength", "army"),
                              &WorldAdapter::army_strength);
  godot::ClassDB::bind_method(godot::D_METHOD("body_strengths", "army"),
                              &WorldAdapter::body_strengths);
  godot::ClassDB::bind_method(godot::D_METHOD("body_ready", "army"),
                              &WorldAdapter::body_ready);
  godot::ClassDB::bind_method(godot::D_METHOD("events", "from"),
                              &WorldAdapter::events);
  godot::ClassDB::bind_method(godot::D_METHOD("progress", "from"),
                              &WorldAdapter::progress);
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
  godot::ClassDB::bind_integer_constant(name, "", "ARC_HALF_WIDTH_DEGREES",
                                        murim::ARC_HALF_WIDTH_DEGREES);
  godot::ClassDB::bind_integer_constant(name, "", "STAGE_INTACT",
                                        static_cast<int>(murim::Stage::INTACT));
  godot::ClassDB::bind_integer_constant(
      name, "", "STAGE_DAMAGED", static_cast<int>(murim::Stage::DAMAGED));
  godot::ClassDB::bind_integer_constant(
      name, "", "STAGE_DESTROYED", static_cast<int>(murim::Stage::DESTROYED));
  godot::ClassDB::bind_integer_constant(name, "", "STAGE_RUBBLE",
                                        static_cast<int>(murim::Stage::RUBBLE));
  const std::array<std::pair<const char*, murim::FieldState>, 5> fields{{
      {"FIELD_SOWN", murim::FieldState::SOWN},
      {"FIELD_GROWING", murim::FieldState::GROWING},
      {"FIELD_RIPE", murim::FieldState::RIPE},
      {"FIELD_FALLOW", murim::FieldState::FALLOW},
      {"FIELD_BURNT", murim::FieldState::BURNT},
  }};
  for (const auto& [constant, state] : fields) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(state));
  }
  // Each kind is its alternative's index in murim::Happening.
  const std::array<std::pair<const char*, murim::Happening>, 7> kinds{{
      {"EVENT_FOUNDED", murim::Founded{}},
      {"EVENT_RAIDED", murim::Raided{}},
      {"EVENT_RUINED", murim::Ruined{}},
      {"EVENT_SECT_FELL", murim::SectFell{}},
      {"EVENT_LEADER_CHANGED", murim::LeaderChanged{}},
      {"EVENT_BORN", murim::Born{}},
      {"EVENT_DIED", murim::Died{}},
  }};
  const std::array<std::pair<const char*, murim::LearnedBy>, 3> lessons{{
      {"LEARNED_BY_COMPREHENSION", murim::LearnedBy::COMPREHENSION},
      {"LEARNED_BY_MANUAL", murim::LearnedBy::MANUAL},
      {"LEARNED_BY_TEACHING", murim::LearnedBy::TEACHING},
  }};
  for (const auto& [constant, lesson] : lessons) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(lesson));
  }
  for (const auto& [constant, happening] : kinds) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(happening.index()));
  }
  // Each kind is its alternative's index in murim::Advance.
  const std::array<std::pair<const char*, murim::Advance>, 2> advances{{
      {"PROGRESS_LEARNED", murim::Learned{}},
      {"PROGRESS_REALM_GAINED", murim::RealmGained{}},
  }};
  for (const auto& [constant, advance] : advances) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(advance.index()));
  }
  const std::array<std::pair<const char*, murim::ChallengeRefusal>, 7> refusals{
      {
          {"CHALLENGE_NONE", murim::ChallengeRefusal::NONE},
          {"CHALLENGE_NO_SUCCESSION", murim::ChallengeRefusal::NO_SUCCESSION},
          {"CHALLENGE_NO_LEADER", murim::ChallengeRefusal::NO_LEADER},
          {"CHALLENGE_NO_CHALLENGER", murim::ChallengeRefusal::NO_CHALLENGER},
          {"CHALLENGE_NOT_A_MEMBER", murim::ChallengeRefusal::NOT_A_MEMBER},
          {"CHALLENGE_IS_LEADER", murim::ChallengeRefusal::IS_LEADER},
          {"CHALLENGE_DUEL_IN_PROGRESS",
           murim::ChallengeRefusal::DUEL_IN_PROGRESS},
      }};
  for (const auto& [constant, refusal] : refusals) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(refusal));
  }
  const std::array<std::pair<const char*, murim::Relation>, 5> relations{{
      {"RELATION_OWN", murim::Relation::OWN},
      {"RELATION_ALLY", murim::Relation::ALLY},
      {"RELATION_NEUTRAL", murim::Relation::NEUTRAL},
      {"RELATION_HOSTILE", murim::Relation::HOSTILE},
      {"RELATION_AT_WAR", murim::Relation::AT_WAR},
  }};
  for (const auto& [constant, relation] : relations) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(relation));
  }
  const std::array<std::pair<const char*, murim::Declaration>, 2> declared{{
      {"DECLARATION_WAR", murim::Declaration::WAR},
      {"DECLARATION_PEACE", murim::Declaration::PEACE},
  }};
  for (const auto& [constant, what] : declared) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(what));
  }
  const std::array<std::pair<const char*, murim::DeclarationRefusal>, 7>
      declarations{{
          {"DECLARE_NONE", murim::DeclarationRefusal::NONE},
          {"DECLARE_DEAD", murim::DeclarationRefusal::DEAD},
          {"DECLARE_NOT_THE_LEADER", murim::DeclarationRefusal::NOT_THE_LEADER},
          {"DECLARE_SAME_FACTION", murim::DeclarationRefusal::SAME_FACTION},
          {"DECLARE_ALREADY_AT_WAR", murim::DeclarationRefusal::ALREADY_AT_WAR},
          {"DECLARE_ALREADY_AT_PEACE",
           murim::DeclarationRefusal::ALREADY_AT_PEACE},
          {"DECLARE_TOO_SOON", murim::DeclarationRefusal::TOO_SOON},
      }};
  for (const auto& [constant, refusal] : declarations) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(refusal));
  }
  const std::array<std::pair<const char*, murim::TalkRefusal>, 5> talks{{
      {"TALK_NONE", murim::TalkRefusal::NONE},
      {"TALK_DEAD", murim::TalkRefusal::DEAD},
      {"TALK_SELF", murim::TalkRefusal::SELF},
      {"TALK_OUT_OF_REACH", murim::TalkRefusal::OUT_OF_REACH},
      {"TALK_HOSTILE", murim::TalkRefusal::HOSTILE},
  }};
  for (const auto& [constant, refusal] : talks) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(refusal));
  }
  const std::array<std::pair<const char*, murim::AttackRefusal>, 5> attacks{{
      {"ATTACK_NONE", murim::AttackRefusal::NONE},
      {"ATTACK_NO_MEMBER", murim::AttackRefusal::NO_MEMBER},
      {"ATTACK_NO_TARGET", murim::AttackRefusal::NO_TARGET},
      {"ATTACK_NOT_A_FOE", murim::AttackRefusal::NOT_A_FOE},
      {"ATTACK_OUT_OF_SIGHT", murim::AttackRefusal::OUT_OF_SIGHT},
  }};
  for (const auto& [constant, refusal] : attacks) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(refusal));
  }
  godot::ClassDB::bind_integer_constant(name, "", "OFFER_NEWS",
                                        static_cast<int>(murim::Offer::NEWS));
  godot::ClassDB::bind_integer_constant(name, "", "OFFER_TRADE",
                                        static_cast<int>(murim::Offer::TRADE));
  const std::array<std::pair<const char*, murim::BuyRefusal>, 5> buys{{
      {"BUY_NONE", murim::BuyRefusal::NONE},
      {"BUY_DEAD", murim::BuyRefusal::DEAD},
      {"BUY_CANNOT_TALK", murim::BuyRefusal::CANNOT_TALK},
      {"BUY_NOT_SOLD", murim::BuyRefusal::NOT_SOLD},
      {"BUY_NO_SILVER", murim::BuyRefusal::NO_SILVER},
  }};
  for (const auto& [constant, refusal] : buys) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(refusal));
  }
  const std::array<std::pair<const char*, murim::ConsumeRefusal>, 4> consumed{{
      {"CONSUME_NONE", murim::ConsumeRefusal::NONE},
      {"CONSUME_DEAD", murim::ConsumeRefusal::DEAD},
      {"CONSUME_NONE_HELD", murim::ConsumeRefusal::NONE_HELD},
      {"CONSUME_FULL", murim::ConsumeRefusal::FULL},
  }};
  for (const auto& [constant, refusal] : consumed) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(refusal));
  }
  const std::array<std::pair<const char*, murim::ApplicationRefusal>, 11>
      applications{{
          {"APPLICATION_NONE", murim::ApplicationRefusal::NONE},
          {"APPLICATION_DEAD", murim::ApplicationRefusal::DEAD},
          {"APPLICATION_IN_A_SECT", murim::ApplicationRefusal::IN_A_SECT},
          {"APPLICATION_NO_HOME", murim::ApplicationRefusal::NO_HOME},
          {"APPLICATION_NOT_OF_FACTION",
           murim::ApplicationRefusal::NOT_OF_FACTION},
          {"APPLICATION_TOO_YOUNG", murim::ApplicationRefusal::TOO_YOUNG},
          {"APPLICATION_UNPROVEN", murim::ApplicationRefusal::UNPROVEN},
          {"APPLICATION_TOO_FAR", murim::ApplicationRefusal::TOO_FAR},
          {"APPLICATION_SEAT_LOST", murim::ApplicationRefusal::SEAT_LOST},
          {"APPLICATION_NO_ELDER", murim::ApplicationRefusal::NO_ELDER},
          {"APPLICATION_FULL", murim::ApplicationRefusal::FULL},
      }};
  for (const auto& [constant, refusal] : applications) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(refusal));
  }
  const std::array<std::pair<const char*, murim::CastRefusal>, 7> casts{{
      {"CAST_NONE", murim::CastRefusal::NONE},
      {"CAST_DEAD", murim::CastRefusal::DEAD},
      {"CAST_NO_SKILL", murim::CastRefusal::NO_SKILL},
      {"CAST_ON_COOLDOWN", murim::CastRefusal::ON_COOLDOWN},
      {"CAST_NO_QI", murim::CastRefusal::NO_QI},
      {"CAST_NO_TARGET", murim::CastRefusal::NO_TARGET},
      {"CAST_OUT_OF_RANGE", murim::CastRefusal::OUT_OF_RANGE},
  }};
  for (const auto& [constant, refusal] : casts) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(refusal));
  }
  const std::array<std::pair<const char*, murim::Shape>, 5> shapes{{
      {"SHAPE_AREA", murim::Shape::AREA},
      {"SHAPE_ARC", murim::Shape::ARC},
      {"SHAPE_LINE", murim::Shape::LINE},
      {"SHAPE_SELF", murim::Shape::SELF},
      {"SHAPE_TARGETED", murim::Shape::TARGETED},
  }};
  for (const auto& [constant, shape] : shapes) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(shape));
  }
  const std::array<std::pair<const char*, murim::Orientation>, 3> orientations{{
      {"ORIENTATION_ORTHODOX", murim::Orientation::ORTHODOX},
      {"ORIENTATION_UNORTHODOX", murim::Orientation::UNORTHODOX},
      {"ORIENTATION_DEMONIC", murim::Orientation::DEMONIC},
  }};
  for (const auto& [constant, orientation] : orientations) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(orientation));
  }
  godot::ClassDB::bind_integer_constant(
      name, "", "POLARITY_YANG", static_cast<int>(murim::Polarity::YANG));
  godot::ClassDB::bind_integer_constant(name, "", "POLARITY_YIN",
                                        static_cast<int>(murim::Polarity::YIN));
  godot::ClassDB::bind_integer_constant(name, "", "LOAD_NONE", LOAD_NONE);
  const std::array<std::pair<const char*, murim::LoadError>, 5> load_errors{{
      {"LOAD_MISSING", murim::LoadError::MISSING},
      {"LOAD_NOT_A_SAVE", murim::LoadError::NOT_A_SAVE},
      {"LOAD_UNSUPPORTED_VERSION", murim::LoadError::UNSUPPORTED_VERSION},
      {"LOAD_TRUNCATED", murim::LoadError::TRUNCATED},
      {"LOAD_CORRUPTED", murim::LoadError::CORRUPTED},
  }};
  for (const auto& [constant, error] : load_errors) {
    godot::ClassDB::bind_integer_constant(name, "", constant,
                                          static_cast<int>(error));
  }
}

}  // namespace murim_godot
