#include "save.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "flow_field.hpp"
#include "map.hpp"
#include "world.hpp"

namespace murim {
namespace {

// "MURIMSAV": the bytes every save begins with.
constexpr std::array<std::uint8_t, 8> MAGIC{0x4D, 0x55, 0x52, 0x49,
                                            0x4D, 0x53, 0x41, 0x56};
constexpr std::size_t BYTE_BITS = 8;
constexpr std::uint64_t FNV_OFFSET = 0xCBF29CE484222325ULL;
constexpr std::uint64_t FNV_PRIME = 0x100000001B3ULL;
// The magic, the version and the payload's length.
constexpr std::size_t HEADER_BYTES =
    MAGIC.size() + sizeof(std::uint32_t) + sizeof(std::uint64_t);
constexpr std::size_t CHECKSUM_BYTES = sizeof(std::uint64_t);

// FNV-1a over `bytes` from `first` up to `last`.
std::uint64_t checksum(const std::vector<std::uint8_t>& bytes,
                       std::size_t first, std::size_t last) {
  std::uint64_t digest = FNV_OFFSET;
  for (std::size_t index = first; index < last; ++index) {
    digest = (digest ^ bytes.at(index)) * FNV_PRIME;
  }
  return digest;
}

// A save refused while it is read, and why.
struct Refusal {
  LoadError error;
};

// Appends values to a save, little-endian and of fixed width.
class Writer {
 public:
  void u8(std::uint8_t value) { bytes_.push_back(value); }

  void u32(std::uint32_t value) { little_endian(value); }

  void u64(std::uint64_t value) { little_endian(value); }

  void i32(std::int32_t value) { u32(static_cast<std::uint32_t>(value)); }

  void i64(std::int64_t value) { u64(static_cast<std::uint64_t>(value)); }

  void size(std::size_t value) { u64(value); }

  void flag(bool value) { u8(value ? 1 : 0); }

  void text(const std::string& value) {
    size(value.size());
    for (const char character : value) {
      u8(static_cast<std::uint8_t>(character));
    }
  }

  [[nodiscard]] std::vector<std::uint8_t>& bytes() { return bytes_; }

 private:
  template <typename Unsigned>
  void little_endian(Unsigned value) {
    for (std::size_t byte = 0; byte < sizeof(Unsigned); ++byte) {
      bytes_.push_back(static_cast<std::uint8_t>(value >> (byte * BYTE_BITS)));
    }
  }

  std::vector<std::uint8_t> bytes_;
};

// Reads values back from a save's payload, refusing it as corrupted where it
// runs short or holds a value no writer gives.
class Reader {
 public:
  Reader(const std::vector<std::uint8_t>& bytes, std::size_t first,
         std::size_t last)
      : bytes_(&bytes), next_(first), last_(last) {}

  std::uint8_t u8() {
    take(1);
    return bytes_->at(next_ - 1);
  }

  std::uint32_t u32() { return little_endian<std::uint32_t>(); }

  std::uint64_t u64() { return little_endian<std::uint64_t>(); }

  std::int32_t i32() { return static_cast<std::int32_t>(u32()); }

  std::int64_t i64() { return static_cast<std::int64_t>(u64()); }

  std::size_t size() {
    const std::uint64_t value = u64();
    if (value > SIZE_MAX) {
      throw Refusal{LoadError::CORRUPTED};
    }
    return static_cast<std::size_t>(value);
  }

  // A count of items each at least one byte long, so no count asks for more
  // items than bytes remain.
  std::size_t count() {
    const std::size_t value = size();
    if (value > last_ - next_) {
      throw Refusal{LoadError::CORRUPTED};
    }
    return value;
  }

  // A tag below `bound`.
  std::uint8_t tag(std::uint8_t bound) {
    const std::uint8_t value = u8();
    if (value >= bound) {
      throw Refusal{LoadError::CORRUPTED};
    }
    return value;
  }

  bool flag() { return tag(2) == 1; }

  std::string text() {
    std::string value(count(), '\0');
    for (char& character : value) {
      character = static_cast<char>(u8());
    }
    return value;
  }

  [[nodiscard]] bool done() const { return next_ == last_; }

 private:
  void take(std::size_t count) {
    if (count > last_ - next_) {
      throw Refusal{LoadError::CORRUPTED};
    }
    next_ += count;
  }

  template <typename Unsigned>
  Unsigned little_endian() {
    take(sizeof(Unsigned));
    Unsigned value = 0;
    for (std::size_t byte = 0; byte < sizeof(Unsigned); ++byte) {
      value |= static_cast<Unsigned>(
          static_cast<Unsigned>(bytes_->at(next_ - sizeof(Unsigned) + byte))
          << (byte * BYTE_BITS));
    }
    return value;
  }

  const std::vector<std::uint8_t>* bytes_;
  std::size_t next_;
  std::size_t last_;
};

template <typename Item, typename Write>
void put_all(Writer& out, const std::vector<Item>& items, Write write) {
  out.size(items.size());
  for (const Item& item : items) {
    write(item);
  }
}

template <typename Item, typename Read>
std::vector<Item> get_all(Reader& input, Read read) {
  const std::size_t count = input.count();
  std::vector<Item> items;
  items.reserve(count);
  for (std::size_t item = 0; item < count; ++item) {
    items.push_back(read());
  }
  return items;
}

template <typename Item, typename Write>
void put_maybe(Writer& out, const std::optional<Item>& item, Write write) {
  out.flag(item.has_value());
  if (item) {
    write(*item);
  }
}

template <typename Item, typename Read>
std::optional<Item> get_maybe(Reader& input, Read read) {
  if (!input.flag()) {
    return std::nullopt;
  }
  return read();
}

void put_sizes(Writer& out, const std::vector<std::size_t>& sizes) {
  put_all(out, sizes, [&out](std::size_t value) { out.size(value); });
}

std::vector<std::size_t> get_sizes(Reader& input) {
  return get_all<std::size_t>(input, [&input] { return input.size(); });
}

void put_index(Writer& out, const std::optional<std::size_t>& index) {
  put_maybe(out, index, [&out](std::size_t value) { out.size(value); });
}

std::optional<std::size_t> get_index(Reader& input) {
  return get_maybe<std::size_t>(input, [&input] { return input.size(); });
}

void put_cell(Writer& out, Cell cell) {
  out.i32(cell.x);
  out.i32(cell.y);
}

Cell get_cell(Reader& input) {
  const std::int32_t column = input.i32();
  return Cell{column, input.i32()};
}

void put_footprint(Writer& out, Footprint footprint) {
  out.i32(footprint.width);
  out.i32(footprint.height);
}

Footprint get_footprint(Reader& input) {
  const std::int32_t width = input.i32();
  return Footprint{width, input.i32()};
}

void put_spread(Writer& out, Spread spread) {
  out.u32(spread.mean);
  out.u32(spread.spread);
}

Spread get_spread(Reader& input) {
  const std::uint32_t mean = input.u32();
  return Spread{mean, input.u32()};
}

void put_range(Writer& out, Range range) {
  out.u32(range.low);
  out.u32(range.high);
}

Range get_range(Reader& input) {
  const std::uint32_t low = input.u32();
  return Range{low, input.u32()};
}

void put_holding(Writer& out, const Holding& holding) {
  for (const std::uint32_t value :
       {holding.housing, holding.storage, holding.harvest, holding.pupils,
        holding.arms, holding.tends, holding.edge, holding.healing}) {
    out.u32(value);
  }
}

Holding get_holding(Reader& input) {
  Holding holding;
  for (std::uint32_t* value :
       {&holding.housing, &holding.storage, &holding.harvest, &holding.pupils,
        &holding.arms, &holding.tends, &holding.edge, &holding.healing}) {
    *value = input.u32();
  }
  return holding;
}

void put_skill(Writer& out, const Skill& skill) {
  out.i32(skill.range);
  out.i32(skill.radius);
  out.u64(skill.body_damage);
  out.i32(skill.structure_damage);
  out.u64(skill.cooldown);
}

Skill get_skill(Reader& input) {
  Skill skill{};
  skill.range = input.i32();
  skill.radius = input.i32();
  skill.body_damage = input.u64();
  skill.structure_damage = input.i32();
  skill.cooldown = input.u64();
  return skill;
}

void put_duel(Writer& out, const Duel& duel) {
  out.flag(duel.members_only);
  out.u64(duel.yield_health);
  out.flag(duel.allies_turn_hostile);
}

Duel get_duel(Reader& input) {
  Duel duel;
  duel.members_only = input.flag();
  duel.yield_health = input.u64();
  duel.allies_turn_hostile = input.flag();
  return duel;
}

void put_names(Writer& out, const std::vector<NameEntry>& names) {
  put_all(out, names, [&out](const NameEntry& name) {
    out.text(name.korean);
    out.text(name.romanised);
  });
}

std::vector<NameEntry> get_names(Reader& input) {
  return get_all<NameEntry>(input, [&input] {
    std::string korean = input.text();
    return NameEntry{std::move(korean), input.text()};
  });
}

void put_growth(Writer& out, const Growth& growth) {
  out.u32(growth.interval);
  out.u32(growth.pressure);
  out.i32(growth.reach);
  out.i32(growth.vicinity);
  out.i32(growth.spacing);
  out.i32(growth.claim);
  put_all(out, growth.plans, [&out](const Plan& plan) {
    out.text(plan.kind);
    put_footprint(out, plan.footprint);
    out.flag(plan.blocks);
    put_holding(out, plan.holding);
  });
  put_sizes(out, growth.builds);
  put_footprint(out, growth.settlement);
  out.text(growth.settlement_kind);
  put_all(out, growth.layout, [&out](const Placement& placement) {
    out.size(placement.plan);
    put_cell(out, placement.offset);
  });
  out.u32(growth.settlers);
  out.u32(growth.raiders);
  put_all(out, growth.stances, [&out](const std::vector<Expansion>& stance) {
    put_all(out, stance, [&out](Expansion expansion) {
      out.u8(static_cast<std::uint8_t>(expansion));
    });
  });
  out.u32(growth.self_reliance);
}

Growth get_growth(Reader& input) {
  constexpr std::uint8_t EXPANSIONS = 3;
  Growth growth;
  growth.interval = input.u32();
  growth.pressure = input.u32();
  growth.reach = input.i32();
  growth.vicinity = input.i32();
  growth.spacing = input.i32();
  growth.claim = input.i32();
  growth.plans = get_all<Plan>(input, [&input] {
    Plan plan;
    plan.kind = input.text();
    plan.footprint = get_footprint(input);
    plan.blocks = input.flag();
    plan.holding = get_holding(input);
    return plan;
  });
  growth.builds = get_sizes(input);
  growth.settlement = get_footprint(input);
  growth.settlement_kind = input.text();
  growth.layout = get_all<Placement>(input, [&input] {
    const std::size_t plan = input.size();
    return Placement{plan, get_cell(input)};
  });
  growth.settlers = input.u32();
  growth.raiders = input.u32();
  growth.stances = get_all<std::vector<Expansion>>(input, [&input] {
    return get_all<Expansion>(input, [&input] {
      return static_cast<Expansion>(input.tag(EXPANSIONS));
    });
  });
  growth.self_reliance = input.u32();
  return growth;
}

void put_life(Writer& out, const Life& life) {
  put_all(out, life.disciplines, [&out](const DisciplineDraw& draw) {
    out.text(draw.name);
    put_spread(out, draw.growth);
    put_spread(out, draw.limit);
  });
  out.u64(life.newborn_health);
  put_maybe(out, life.lifespan,
            [&out](Spread spread) { put_spread(out, spread); });
  out.u32(life.days_per_attainment);
  out.u32(life.discipline_widening);
  out.u32(life.upper_widening);
  put_maybe(out, life.founding_age,
            [&out](Spread spread) { put_spread(out, spread); });
  put_maybe(out, life.kinship, [&out](const Kinship& kinship) {
    put_range(out, kinship.marrying_age);
    out.i32(kinship.neighbourhood);
    out.u32(kinship.marriage_chance);
    put_range(out, kinship.bearing_age);
    out.u32(kinship.birth_chance);
    out.u32(kinship.birth_spacing);
    out.flag(kinship.fathers_family);
    out.flag(kinship.balanced_sexes);
  });
  put_names(out, life.names.surnames);
  put_names(out, life.names.female);
  put_names(out, life.names.male);
  put_maybe(out, life.husbandry, [&out](const Husbandry& husbandry) {
    out.u32(husbandry.consumption);
    out.u32(husbandry.hunger_days);
    out.i32(husbandry.migration_reach);
  });
  put_maybe(out, life.growth,
            [&out](const Growth& growth) { put_growth(out, growth); });
  put_maybe(out, life.manuals, [&out](const Manuals& manuals) {
    put_all(out, manuals.held, [&out](const std::vector<std::size_t>& held) {
      put_sizes(out, held);
    });
    out.u32(manuals.scatter_chance);
    out.i32(manuals.scatter_reach);
  });
}

Life get_life(Reader& input) {
  Life life;
  life.disciplines = get_all<DisciplineDraw>(input, [&input] {
    DisciplineDraw draw;
    draw.name = input.text();
    draw.growth = get_spread(input);
    draw.limit = get_spread(input);
    return draw;
  });
  life.newborn_health = input.u64();
  life.lifespan =
      get_maybe<Spread>(input, [&input] { return get_spread(input); });
  life.days_per_attainment = input.u32();
  life.discipline_widening = input.u32();
  life.upper_widening = input.u32();
  life.founding_age =
      get_maybe<Spread>(input, [&input] { return get_spread(input); });
  life.kinship = get_maybe<Kinship>(input, [&input] {
    Kinship kinship;
    kinship.marrying_age = get_range(input);
    kinship.neighbourhood = input.i32();
    kinship.marriage_chance = input.u32();
    kinship.bearing_age = get_range(input);
    kinship.birth_chance = input.u32();
    kinship.birth_spacing = input.u32();
    kinship.fathers_family = input.flag();
    kinship.balanced_sexes = input.flag();
    return kinship;
  });
  life.names.surnames = get_names(input);
  life.names.female = get_names(input);
  life.names.male = get_names(input);
  life.husbandry = get_maybe<Husbandry>(input, [&input] {
    Husbandry husbandry;
    husbandry.consumption = input.u32();
    husbandry.hunger_days = input.u32();
    husbandry.migration_reach = input.i32();
    return husbandry;
  });
  life.growth =
      get_maybe<Growth>(input, [&input] { return get_growth(input); });
  life.manuals = get_maybe<Manuals>(input, [&input] {
    Manuals manuals;
    manuals.held = get_all<std::vector<std::size_t>>(
        input, [&input] { return get_sizes(input); });
    manuals.scatter_chance = input.u32();
    manuals.scatter_reach = input.i32();
    return manuals;
  });
  return life;
}

void put_rules(Writer& out, const Rules& rules) {
  out.i32(rules.separation);
  put_all(out, rules.hostile, [&out](const std::vector<bool>& row) {
    put_all(out, row, [&out](bool hostile) { out.flag(hostile); });
  });
  for (const std::uint32_t floor : rules.stage_floors) {
    out.u32(floor);
  }
  out.u64(rules.ticks_per_day);
  out.i32(rules.rumour_reach);
}

Rules get_rules(Reader& input) {
  Rules rules{};
  rules.separation = input.i32();
  rules.hostile = get_all<std::vector<bool>>(input, [&input] {
    return get_all<bool>(input, [&input] { return input.flag(); });
  });
  for (std::uint32_t& floor : rules.stage_floors) {
    floor = input.u32();
  }
  rules.ticks_per_day = input.u64();
  rules.rumour_reach = input.i32();
  return rules;
}

void put_map(Writer& out, const Map& map) {
  out.i32(map.width());
  out.i32(map.height());
  for (Cell cell{0, 0}; cell.y < map.height(); ++cell.y) {
    for (cell.x = 0; cell.x < map.width(); ++cell.x) {
      Footing footing = Footing::BUILDABLE;
      if (!map.passable(cell)) {
        footing = Footing::BLOCKED;
      } else if (!map.buildable(cell)) {
        footing = Footing::ROUGH;
      }
      out.u8(static_cast<std::uint8_t>(footing));
    }
  }
}

Map get_map(Reader& input) {
  constexpr std::uint8_t FOOTINGS = 3;
  const std::int32_t width = input.i32();
  const std::int32_t height = input.i32();
  if (width <= 0 || height <= 0) {
    throw Refusal{LoadError::CORRUPTED};
  }
  std::vector<std::string> rows;
  for (std::int32_t row = 0; row < height; ++row) {
    std::string tiles;
    for (std::int32_t column = 0; column < width; ++column) {
      tiles.push_back(tile_of(static_cast<Footing>(input.tag(FOOTINGS))));
    }
    rows.push_back(std::move(tiles));
  }
  return Map{rows};
}

void put_person(Writer& out, const Person& person) {
  out.i32(person.position.x);
  out.i32(person.position.y);
  out.u64(person.health);
  out.u64(person.ready_at);
  put_index(out, person.home);
  out.i64(person.born);
  out.size(person.family);
  put_sizes(out, person.parents);
  out.u8(static_cast<std::uint8_t>(person.sex));
  out.size(person.given);
  put_sizes(out, person.spouses);
  put_sizes(out, person.children);
  out.u32(person.hunger);
  put_all(out, person.talent, [&out](Aptitude aptitude) {
    out.u32(aptitude.growth);
    out.u32(aptitude.limit);
  });
  put_all(out, person.attainment,
          [&out](std::uint32_t attainment) { out.u32(attainment); });
  put_maybe(out, person.lifespan,
            [&out](std::uint64_t lifespan) { out.u64(lifespan); });
  put_index(out, person.soul);
  put_index(out, person.sect);
  put_maybe(out, person.casting, [&out](const Cast& cast) {
    out.size(cast.skill);
    out.u8(static_cast<std::uint8_t>(cast.target.index()));
    if (const auto* point = std::get_if<Point>(&cast.target)) {
      out.i32(point->position.x);
      out.i32(point->position.y);
    } else if (const auto* aimed = std::get_if<AtPerson>(&cast.target)) {
      out.size(aimed->person);
    }
  });
}

Person get_person(Reader& input) {
  constexpr std::uint8_t SEXES = 2;
  Person person;
  person.position.x = input.i32();
  person.position.y = input.i32();
  person.health = input.u64();
  person.ready_at = input.u64();
  person.home = get_index(input);
  person.born = input.i64();
  person.family = input.size();
  person.parents = get_sizes(input);
  person.sex = static_cast<Sex>(input.tag(SEXES));
  person.given = input.size();
  person.spouses = get_sizes(input);
  person.children = get_sizes(input);
  person.hunger = input.u32();
  person.talent = get_all<Aptitude>(input, [&input] {
    const std::uint32_t growth = input.u32();
    return Aptitude{growth, input.u32()};
  });
  person.attainment =
      get_all<std::uint32_t>(input, [&input] { return input.u32(); });
  person.lifespan =
      get_maybe<std::uint64_t>(input, [&input] { return input.u64(); });
  person.soul = get_index(input);
  person.sect = get_index(input);
  person.casting = get_maybe<Cast>(input, [&input] {
    constexpr std::uint8_t TARGETS = std::variant_size_v<Target>;
    Cast cast;
    cast.skill = input.size();
    switch (input.tag(TARGETS)) {
      case 0: {
        const std::int32_t across = input.i32();
        cast.target = Point{Position{across, input.i32()}};
        break;
      }
      case 1:
        cast.target = AtPerson{input.size()};
        break;
      default:
        cast.target = Itself{};
        break;
    }
    return cast;
  });
  return person;
}

void put_village_spec(Writer& out, const VillageSpec& spec) {
  put_sizes(out, spec.structures);
  out.size(spec.residents);
  put_all(out, spec.holdings,
          [&out](const Holding& holding) { put_holding(out, holding); });
  out.u32(spec.households);
  put_all(out, spec.clearings, [&out](const Clearing& clearing) {
    put_cell(out, clearing.corner);
    put_footprint(out, clearing.area);
  });
}

VillageSpec get_village_spec(Reader& input) {
  VillageSpec spec{};
  spec.structures = get_sizes(input);
  spec.residents = input.size();
  spec.holdings =
      get_all<Holding>(input, [&input] { return get_holding(input); });
  spec.households = input.u32();
  spec.clearings = get_all<Clearing>(input, [&input] {
    const Cell corner = get_cell(input);
    return Clearing{corner, get_footprint(input)};
  });
  return spec;
}

void put_leadership(Writer& out, const Leadership& faction) {
  put_index(out, faction.leader);
  put_maybe(out, faction.succession,
            [&out](const Duel& duel) { put_duel(out, duel); });
  put_maybe(out, faction.admission, [&out](const Admission& admission) {
    out.u32(admission.age);
    out.u32(admission.talent);
    out.u32(admission.attainment);
    out.i32(admission.reach);
    out.u32(admission.chance);
    out.u32(admission.elder_share);
  });
  put_maybe(out, faction.election,
            [&out](Election election) { out.u32(election.term); });
}

Leadership get_leadership(Reader& input) {
  Leadership faction;
  faction.leader = get_index(input);
  faction.succession =
      get_maybe<Duel>(input, [&input] { return get_duel(input); });
  faction.admission = get_maybe<Admission>(input, [&input] {
    Admission admission;
    admission.age = input.u32();
    admission.talent = input.u32();
    admission.attainment = input.u32();
    admission.reach = input.i32();
    admission.chance = input.u32();
    admission.elder_share = input.u32();
    return admission;
  });
  faction.election =
      get_maybe<Election>(input, [&input] { return Election{input.u32()}; });
  return faction;
}

void put_manual(Writer& out, const Manual& manual) {
  out.size(manual.discipline);
  out.u8(static_cast<std::uint8_t>(manual.place.index()));
  if (const auto* held = std::get_if<HeldBySect>(&manual.place)) {
    out.size(held->sect);
  } else if (const auto* cell = std::get_if<Cell>(&manual.place)) {
    put_cell(out, *cell);
  }
}

Manual get_manual(Reader& input) {
  constexpr std::uint8_t PLACES = 3;
  Manual manual;
  manual.discipline = input.size();
  switch (input.tag(PLACES)) {
    case 0:
      manual.place = HeldBySect{input.size()};
      break;
    case 1:
      manual.place = get_cell(input);
      break;
    default:
      manual.place = Lost{};
      break;
  }
  return manual;
}

void put_event(Writer& out, const Event& event) {
  out.u64(event.sequence);
  out.u64(event.tick);
  put_cell(out, event.cell);
  out.u8(static_cast<std::uint8_t>(event.what.index()));
  std::visit(
      [&out](const auto& what) {
        using Kind = std::decay_t<decltype(what)>;
        if constexpr (std::is_same_v<Kind, Founded>) {
          out.size(what.village);
          out.size(what.faction);
        } else if constexpr (std::is_same_v<Kind, Raided>) {
          out.size(what.village);
          out.size(what.owner);
          out.size(what.raider);
        } else if constexpr (std::is_same_v<Kind, Ruined>) {
          out.size(what.village);
          out.size(what.owner);
          out.size(what.destroyer.army);
          out.size(what.destroyer.faction);
        } else if constexpr (std::is_same_v<Kind, SectFell>) {
          out.size(what.sect);
          out.size(what.faction);
        } else if constexpr (std::is_same_v<Kind, LeaderChanged>) {
          out.size(what.faction);
          out.size(what.leader);
          out.u8(static_cast<std::uint8_t>(what.by));
        } else {
          out.size(what.person);
        }
      },
      event.what);
}

Event get_event(Reader& input) {
  constexpr std::uint8_t HAPPENINGS = std::variant_size_v<Happening>;
  constexpr std::uint8_t CHOICES = 2;
  // The happenings' tags, as their index in Happening.
  constexpr std::uint8_t FOUNDED = 0;
  constexpr std::uint8_t RAIDED = 1;
  constexpr std::uint8_t RUINED = 2;
  constexpr std::uint8_t SECT_FELL = 3;
  constexpr std::uint8_t LEADER_CHANGED = 4;
  constexpr std::uint8_t BORN = 5;
  Event event;
  event.sequence = input.u64();
  event.tick = input.u64();
  event.cell = get_cell(input);
  switch (input.tag(HAPPENINGS)) {
    case FOUNDED: {
      const std::size_t village = input.size();
      event.what = Founded{village, input.size()};
      break;
    }
    case RAIDED: {
      const std::size_t village = input.size();
      const std::size_t owner = input.size();
      event.what = Raided{village, owner, input.size()};
      break;
    }
    case RUINED: {
      const std::size_t village = input.size();
      const std::size_t owner = input.size();
      const std::size_t army = input.size();
      event.what = Ruined{village, owner, Destroyer{army, input.size()}};
      break;
    }
    case SECT_FELL: {
      const std::size_t sect = input.size();
      event.what = SectFell{sect, input.size()};
      break;
    }
    case LEADER_CHANGED: {
      const std::size_t faction = input.size();
      const std::size_t leader = input.size();
      event.what = LeaderChanged{faction, leader,
                                 static_cast<ChosenBy>(input.tag(CHOICES))};
      break;
    }
    case BORN:
      event.what = Born{input.size()};
      break;
    default:
      event.what = Died{input.size()};
      break;
  }
  return event;
}

}  // namespace

/// Writes a world's state to a save's payload and reads it back.
class SaveCodec {
 public:
  static void write(Writer& out, const World& world) {
    out.u64(world.rng_state_);
    out.u64(world.tick_count_);
    put_map(out, world.map_);
    put_rules(out, world.rules_);
    put_life(out, world.life_);
    put_all(out, world.people_,
            [&out](const Person& person) { put_person(out, person); });
    put_all(out, world.armies_, [&out](const World::Army& army) {
      out.size(army.faction);
      put_all(out, army.skills,
              [&out](const Skill& skill) { put_skill(out, skill); });
      put_cell(out, army.order.objective);
      put_sizes(out, army.order.skills);
      put_sizes(out, army.roster);
    });
    put_all(out, world.structures_, [&out](const Structure& structure) {
      put_cell(out, structure.cell);
      out.u32(structure.durability);
      put_footprint(out, structure.footprint);
      out.flag(structure.blocks);
    });
    put_all(out, world.villages_, [&out](const World::Village& village) {
      put_village_spec(out, village.spec);
      put_maybe(out, village.destroyer, [&out](Destroyer destroyer) {
        out.size(destroyer.army);
        out.size(destroyer.faction);
      });
      out.u64(village.stock);
      put_index(out, village.party);
      put_index(out, village.raiding);
      put_maybe(out, village.founded,
                [&out](Cell cell) { put_cell(out, cell); });
    });
    put_all(out, world.leadership_, [&out](const Leadership& faction) {
      put_leadership(out, faction);
    });
    put_all(out, world.sects_, [&out](const World::Sect& sect) {
      out.text(sect.name);
      out.size(sect.faction);
      out.size(sect.seat);
    });
    put_all(out, world.manuals_,
            [&out](const Manual& manual) { put_manual(out, manual); });
    put_all(out, world.duels_, [&out](const World::Fight& fight) {
      out.size(fight.faction);
      out.size(fight.challenger);
      out.size(fight.leader);
      put_duel(out, fight.rule);
    });
    put_all(out, world.events_,
            [&out](const Event& event) { put_event(out, event); });
    put_all(out, world.souls_, [&out](const Soul& soul) {
      out.u32(soul.lives);
      put_all(out, soul.practised,
              [&out](bool practised) { out.flag(practised); });
    });
    put_sizes(out, world.surnames_);
    put_all(out, world.village_of_, [&out](std::optional<std::size_t> village) {
      put_index(out, village);
    });
    put_all(out, world.plans_,
            [&out](std::optional<std::size_t> plan) { put_index(out, plan); });
  }

  static World read(Reader& input) {
    const std::uint64_t rng_state = input.u64();
    const std::uint64_t tick_count = input.u64();
    Map map = get_map(input);
    Rules rules = get_rules(input);
    World world{std::move(map), std::move(rules), get_life(input)};
    world.rng_state_ = rng_state;
    world.tick_count_ = tick_count;
    world.people_ =
        get_all<Person>(input, [&input] { return get_person(input); });
    world.armies_ = get_all<World::Army>(input, [&input, &world] {
      const std::size_t faction = input.size();
      std::vector<Skill> skills =
          get_all<Skill>(input, [&input] { return get_skill(input); });
      const Cell objective = get_cell(input);
      std::vector<std::size_t> order_skills = get_sizes(input);
      if (!world.map_.passable(objective)) {
        throw Refusal{LoadError::CORRUPTED};
      }
      return World::Army{faction, std::move(skills),
                         Order{objective, std::move(order_skills)},
                         FlowField{world.map_, objective}, get_sizes(input)};
    });
    world.structures_ = get_all<Structure>(input, [&input] {
      Structure structure;
      structure.cell = get_cell(input);
      structure.durability = input.u32();
      structure.footprint = get_footprint(input);
      structure.blocks = input.flag();
      return structure;
    });
    world.villages_ = get_all<World::Village>(input, [&input] {
      World::Village village{get_village_spec(input),
                             std::nullopt,
                             0,
                             std::nullopt,
                             std::nullopt,
                             std::nullopt};
      village.destroyer = get_maybe<Destroyer>(input, [&input] {
        const std::size_t army = input.size();
        return Destroyer{army, input.size()};
      });
      village.stock = input.u64();
      village.party = get_index(input);
      village.raiding = get_index(input);
      village.founded =
          get_maybe<Cell>(input, [&input] { return get_cell(input); });
      return village;
    });
    world.leadership_ =
        get_all<Leadership>(input, [&input] { return get_leadership(input); });
    world.sects_ = get_all<World::Sect>(input, [&input] {
      World::Sect sect;
      sect.name = input.text();
      sect.faction = input.size();
      sect.seat = input.size();
      return sect;
    });
    world.manuals_ =
        get_all<Manual>(input, [&input] { return get_manual(input); });
    world.duels_ = get_all<World::Fight>(input, [&input] {
      World::Fight fight;
      fight.faction = input.size();
      fight.challenger = input.size();
      fight.leader = input.size();
      fight.rule = get_duel(input);
      return fight;
    });
    world.events_ =
        get_all<Event>(input, [&input] { return get_event(input); });
    world.souls_ = get_all<Soul>(input, [&input] {
      Soul soul;
      soul.lives = input.u32();
      soul.practised = get_all<bool>(input, [&input] { return input.flag(); });
      return soul;
    });
    world.surnames_ = get_sizes(input);
    world.village_of_ = get_all<std::optional<std::size_t>>(
        input, [&input] { return get_index(input); });
    world.plans_ = get_all<std::optional<std::size_t>>(
        input, [&input] { return get_index(input); });
    world.restore();
    return world;
  }
};

bool save(const Saved& saved, SaveStore& store, Slot slot) {
  Writer payload;
  put_maybe(payload, saved.origin, [&payload](Origin origin) {
    payload.u64(static_cast<std::uint64_t>(origin.region));
    payload.size(origin.village);
  });
  SaveCodec::write(payload, saved.world);
  Writer out;
  for (const std::uint8_t byte : MAGIC) {
    out.u8(byte);
  }
  out.u32(SAVE_VERSION);
  out.size(payload.bytes().size());
  std::vector<std::uint8_t>& bytes = out.bytes();
  bytes.insert(bytes.end(), payload.bytes().begin(), payload.bytes().end());
  out.u64(checksum(bytes, 0, bytes.size()));
  return store.put(slot, bytes);
}

std::variant<Saved, LoadError> load(const SaveStore& store, Slot slot) {
  const std::optional<std::vector<std::uint8_t>> held = store.get(slot);
  if (!held) {
    return LoadError::MISSING;
  }
  const std::vector<std::uint8_t>& bytes = *held;
  // The one handler for a refused save: every refusal while it is read ends
  // here as its LoadError.
  try {
    for (std::size_t index = 0; index < MAGIC.size() && index < bytes.size();
         ++index) {
      if (bytes.at(index) != MAGIC.at(index)) {
        return LoadError::NOT_A_SAVE;
      }
    }
    if (bytes.size() < HEADER_BYTES) {
      return LoadError::TRUNCATED;
    }
    Reader header{bytes, MAGIC.size(), HEADER_BYTES};
    if (header.u32() != SAVE_VERSION) {
      return LoadError::UNSUPPORTED_VERSION;
    }
    const std::uint64_t length = header.u64();
    const std::size_t after = bytes.size() - HEADER_BYTES;
    if (length > after || after - length < CHECKSUM_BYTES) {
      return LoadError::TRUNCATED;
    }
    const std::size_t end = HEADER_BYTES + static_cast<std::size_t>(length);
    if (after - length > CHECKSUM_BYTES ||
        Reader{bytes, end, end + CHECKSUM_BYTES}.u64() !=
            checksum(bytes, 0, end)) {
      return LoadError::CORRUPTED;
    }
    Reader input{bytes, HEADER_BYTES, end};
    const std::optional<Origin> origin = get_maybe<Origin>(input, [&input] {
      const Seed region{input.u64()};
      return Origin{region, input.size()};
    });
    World world = SaveCodec::read(input);
    if (!input.done()) {
      return LoadError::CORRUPTED;
    }
    return Saved{origin, std::move(world)};
  } catch (const Refusal& refusal) {
    return refusal.error;
  } catch (const std::invalid_argument&) {
    return LoadError::CORRUPTED;
  }
}

}  // namespace murim
