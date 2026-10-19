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

#include "areas.hpp"
#include "arts.hpp"
#include "attack.hpp"
#include "chart.hpp"
#include "diplomacy.hpp"
#include "encoding.hpp"
#include "event.hpp"
#include "growth.hpp"
#include "kinship.hpp"
#include "leadership.hpp"
#include "life.hpp"
#include "map.hpp"
#include "names.hpp"
#include "occupation.hpp"
#include "person.hpp"
#include "progress.hpp"
#include "skill.hpp"
#include "talent.hpp"
#include "trade.hpp"
#include "village.hpp"
#include "world.hpp"

namespace murim {
namespace {

// The roles a structure or a plan may take, Role's members.
constexpr std::uint8_t ROLES = 3;
// The tiers an area may be at.
constexpr std::uint8_t AREA_TIERS = 3;
// Where a schedule's band may send a person, Where's members.
constexpr std::uint8_t WHERES = 3;
// Where an occupation's places may open, Openings' alternatives.
constexpr std::uint8_t OPENINGS = 3;

// "MURIMSAV": the bytes every save begins with.
constexpr std::array<std::uint8_t, 8> MAGIC{0x4D, 0x55, 0x52, 0x49,
                                            0x4D, 0x53, 0x41, 0x56};
// The magic, the version and the payload's length.
constexpr std::size_t HEADER_BYTES =
    MAGIC.size() + sizeof(std::uint32_t) + sizeof(std::uint64_t);
constexpr std::size_t CHECKSUM_BYTES = sizeof(std::uint64_t);

// A save refused while it is read, and why.
struct Refusal {
  LoadError error;
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

void put_bands(Writer& out, const std::vector<Band>& schedule) {
  put_all(out, schedule, [&out](const Band& band) {
    out.u32(band.from);
    out.u8(static_cast<std::uint8_t>(band.where));
  });
}

void put_openings(Writer& out, const Openings& openings) {
  out.u8(static_cast<std::uint8_t>(openings.index()));
  if (const auto* workplace = std::get_if<AtWorkplace>(&openings)) {
    out.text(workplace->kind);
    out.u32(workplace->per);
  } else if (const auto* square = std::get_if<OnSquare>(&openings)) {
    out.u32(square->per_residents);
  }
}

Openings get_openings(Reader& input) {
  switch (input.tag(OPENINGS)) {
    case 0: {
      std::string kind = input.text();
      return AtWorkplace{std::move(kind), input.u32()};
    }
    case 1:
      return OnSquare{input.u32()};
    default:
      return Nowhere{};
  }
}

std::vector<Band> get_bands(Reader& input) {
  return get_all<Band>(input, [&input] {
    const std::uint32_t from = input.u32();
    return Band{from, static_cast<Where>(input.tag(WHERES))};
  });
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
  out.u32(skill.qi_cost);
  out.u8(static_cast<std::uint8_t>(skill.shape));
}

Skill get_skill(Reader& input) {
  Skill skill{};
  skill.range = input.i32();
  skill.radius = input.i32();
  skill.body_damage = input.u64();
  skill.structure_damage = input.i32();
  skill.cooldown = input.u64();
  skill.qi_cost = input.u32();
  constexpr std::uint8_t SHAPES = 5;
  skill.shape = static_cast<Shape>(input.tag(SHAPES));
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

void put_name(Writer& out, const NameEntry& name) {
  out.text(name.korean);
  out.text(name.romanised);
}

NameEntry get_name(Reader& input) {
  std::string korean = input.text();
  return NameEntry{std::move(korean), input.text()};
}

void put_names(Writer& out, const std::vector<NameEntry>& names) {
  put_all(out, names, [&out](const NameEntry& name) { put_name(out, name); });
}

std::vector<NameEntry> get_names(Reader& input) {
  return get_all<NameEntry>(input, [&input] { return get_name(input); });
}

void put_growth(Writer& out, const Growth& growth) {
  out.u32(growth.interval);
  out.u32(growth.pressure);
  out.i32(growth.reach);
  out.i32(growth.spacing);
  out.i32(growth.claim);
  out.i32(growth.apart);
  out.u32(growth.terrace);
  put_all(out, growth.tiers, [&out](const Tier& tier) {
    out.u32(tier.residents);
    put_footprint(out, tier.square);
    out.i32(tier.vicinity);
    put_sizes(out, tier.square_kinds);
  });
  put_all(out, growth.plans, [&out](const Plan& plan) {
    out.text(plan.kind);
    put_footprint(out, plan.footprint);
    out.flag(plan.blocks);
    put_holding(out, plan.holding);
    out.u8(static_cast<std::uint8_t>(plan.role));
  });
  put_sizes(out, growth.builds);
  out.text(growth.settlement_kind);
  put_sizes(out, growth.core);
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
  growth.spacing = input.i32();
  growth.claim = input.i32();
  growth.apart = input.i32();
  growth.terrace = input.u32();
  growth.tiers = get_all<Tier>(input, [&input] {
    Tier tier;
    tier.residents = input.u32();
    tier.square = get_footprint(input);
    tier.vicinity = input.i32();
    tier.square_kinds = get_sizes(input);
    return tier;
  });
  growth.plans = get_all<Plan>(input, [&input] {
    Plan plan;
    plan.kind = input.text();
    plan.footprint = get_footprint(input);
    plan.blocks = input.flag();
    plan.holding = get_holding(input);
    plan.role = static_cast<Role>(input.tag(ROLES));
    return plan;
  });
  growth.builds = get_sizes(input);
  growth.settlement_kind = input.text();
  growth.core = get_sizes(input);
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
  put_maybe(out, life.cultivation, [&out](const Cultivation& cultivation) {
    put_spread(out, cultivation.growth);
    put_spread(out, cultivation.limit);
    out.u32(cultivation.recovery);
    out.u64(cultivation.recovery_ticks);
  });
  put_all(out, life.realms, [&out](const Realm& realm) {
    put_name(out, realm.name);
    out.u32(realm.qi);
    out.u32(realm.might);
  });
  put_all(out, life.arts, [&out](const Art& art) {
    put_name(out, art.name);
    out.size(art.discipline);
    out.u8(static_cast<std::uint8_t>(art.nature.orientation));
    out.u8(static_cast<std::uint8_t>(art.nature.polarity));
    put_all(out, art.skills, [&out](const ArtSkill& taught) {
      put_name(out, taught.name);
      put_skill(out, taught.skill);
      out.u32(taught.rank);
      out.flag(taught.comprehensible);
    });
  });
  const Learning& learning = life.learning;
  for (const std::uint32_t value :
       {learning.comprehension, learning.teaching, learning.reading_cost,
        learning.orientation_ease, learning.polarity_ease,
        learning.orientation_potency, learning.polarity_potency}) {
    out.u32(value);
  }
  out.i32(learning.reading_reach);
  put_maybe(out, life.diplomacy, [&out](const Diplomacy& diplomacy) {
    out.u32(diplomacy.war_margin);
    out.u32(diplomacy.war_chance);
    out.u32(diplomacy.peace_chance);
    out.u32(diplomacy.hold_days);
  });
  put_maybe(out, life.trade, [&out](const Trade& trade) {
    put_all(out, trade.wares, [&out](const Ware& ware) {
      out.text(ware.name);
      out.u64(ware.price);
      out.u64(ware.health);
      out.u32(ware.qi);
    });
    out.u64(trade.purse);
    out.u64(trade.wage);
    out.u32(trade.wounded);
    out.u32(trade.drained);
  });
  put_maybe(out, life.occupations, [&out](const Occupations& occupations) {
    put_all(out, occupations.list, [&out](const Occupation& occupation) {
      out.text(occupation.name);
      put_openings(out, occupation.openings);
      put_bands(out, occupation.schedule);
      put_sizes(out, occupation.wares);
    });
    out.u32(occupations.working_age);
    out.size(occupations.fallback);
    put_bands(out, occupations.idle);
    out.u32(occupations.waking.from);
    out.u32(occupations.waking.to);
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
  life.cultivation = get_maybe<Cultivation>(input, [&input] {
    Cultivation cultivation;
    cultivation.growth = get_spread(input);
    cultivation.limit = get_spread(input);
    cultivation.recovery = input.u32();
    cultivation.recovery_ticks = input.u64();
    return cultivation;
  });
  life.realms = get_all<Realm>(input, [&input] {
    Realm realm;
    realm.name = get_name(input);
    realm.qi = input.u32();
    realm.might = input.u32();
    return realm;
  });
  life.arts = get_all<Art>(input, [&input] {
    constexpr std::uint8_t ORIENTATIONS = 3;
    constexpr std::uint8_t POLARITIES = 2;
    Art art;
    art.name = get_name(input);
    art.discipline = input.size();
    art.nature.orientation = static_cast<Orientation>(input.tag(ORIENTATIONS));
    art.nature.polarity = static_cast<Polarity>(input.tag(POLARITIES));
    art.skills = get_all<ArtSkill>(input, [&input] {
      ArtSkill taught;
      taught.name = get_name(input);
      taught.skill = get_skill(input);
      taught.rank = input.u32();
      taught.comprehensible = input.flag();
      return taught;
    });
    return art;
  });
  Learning& learning = life.learning;
  for (std::uint32_t* value :
       {&learning.comprehension, &learning.teaching, &learning.reading_cost,
        &learning.orientation_ease, &learning.polarity_ease,
        &learning.orientation_potency, &learning.polarity_potency}) {
    *value = input.u32();
  }
  learning.reading_reach = input.i32();
  life.diplomacy = get_maybe<Diplomacy>(input, [&input] {
    Diplomacy diplomacy;
    diplomacy.war_margin = input.u32();
    diplomacy.war_chance = input.u32();
    diplomacy.peace_chance = input.u32();
    diplomacy.hold_days = input.u32();
    return diplomacy;
  });
  life.trade = get_maybe<Trade>(input, [&input] {
    Trade trade;
    trade.wares = get_all<Ware>(input, [&input] {
      Ware ware;
      ware.name = input.text();
      ware.price = input.u64();
      ware.health = input.u64();
      ware.qi = input.u32();
      return ware;
    });
    trade.purse = input.u64();
    trade.wage = input.u64();
    trade.wounded = input.u32();
    trade.drained = input.u32();
    return trade;
  });
  life.occupations = get_maybe<Occupations>(input, [&input] {
    Occupations occupations;
    occupations.list = get_all<Occupation>(input, [&input] {
      Occupation occupation;
      occupation.name = input.text();
      occupation.openings = get_openings(input);
      occupation.schedule = get_bands(input);
      occupation.wares = get_sizes(input);
      return occupation;
    });
    occupations.working_age = input.u32();
    occupations.fallback = input.size();
    occupations.idle = get_bands(input);
    occupations.waking.from = input.u32();
    occupations.waking.to = input.u32();
    return occupations;
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
  out.i32(rules.talk_reach);
  out.i32(rules.sight);
  out.u64(rules.people_capacity);
  out.i32(rules.map_sight);
  put_maybe(out, rules.tokens, [&out](const Tokens& tokens) {
    put_all(out, tokens.by_rank,
            [&out](std::uint32_t granted) { out.u32(granted); });
    out.u64(tokens.circling);
    out.u64(tokens.lease);
  });
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
  rules.talk_reach = input.i32();
  rules.sight = input.i32();
  rules.people_capacity = input.u64();
  rules.map_sight = input.i32();
  rules.tokens = get_maybe<Tokens>(input, [&input] {
    Tokens tokens;
    tokens.by_rank =
        get_all<std::uint32_t>(input, [&input] { return input.u32(); });
    tokens.circling = input.u64();
    tokens.lease = input.u64();
    return tokens;
  });
  return rules;
}

// A chart: each seen area's index and bits, in order of area, then each
// village by index as last seen, or nothing for one never seen.
void put_chart(Writer& out, const Chart& chart) {
  put_all(out, chart.blocks(), [&out](const ChartBlock& block) {
    out.u32(block.area);
    for (const std::uint8_t bits : block.bits) {
      out.u8(bits);
    }
  });
  put_all(out, chart.villages(),
          [&out](const std::optional<VillageMemory>& village) {
            put_maybe(out, village, [&out](VillageMemory memory) {
              out.size(memory.owner);
              out.flag(memory.peopled);
              out.i32(memory.centre.x);
              out.i32(memory.centre.y);
              out.u8(static_cast<std::uint8_t>(memory.stage));
            });
          });
}

// Reads a chart over a map of `size` cells, refusing one no chart gives or
// one remembering an owner at or past `factions` or a centre off the map.
Chart get_chart(Reader& input, Footprint size, std::size_t factions) {
  constexpr std::uint8_t STAGES = 3;
  const std::vector<ChartBlock> blocks = get_all<ChartBlock>(input, [&input] {
    ChartBlock block{input.u32(), {}};
    for (std::uint8_t& bits : block.bits) {
      bits = input.u8();
    }
    return block;
  });
  const std::vector<std::optional<VillageMemory>> remembered =
      get_all<std::optional<VillageMemory>>(input, [&input] {
        return get_maybe<VillageMemory>(input, [&input] {
          const std::size_t owner = input.size();
          const bool peopled = input.flag();
          const Cell centre{input.i32(), input.i32()};
          return VillageMemory{owner, peopled,
                               static_cast<VillageStage>(input.tag(STAGES)),
                               centre};
        });
      });
  std::optional<Chart> chart =
      Chart::restore(size, blocks, remembered, factions);
  if (!chart) {
    throw Refusal{LoadError::CORRUPTED};
  }
  return std::move(*chart);
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
  out.u32(person.cultivation.growth);
  out.u32(person.cultivation.limit);
  out.u32(person.qi);
  out.u32(person.qi_capacity);
  put_all(out, person.known, [&out](const Known& known) {
    out.size(known.art);
    out.size(known.skill);
    out.u64(known.ready_at);
  });
  out.u64(person.silver);
  put_all(out, person.held, [&out](std::uint64_t held) { out.u64(held); });
  put_index(out, person.occupation);
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
  const std::uint32_t growth = input.u32();
  person.cultivation = Aptitude{growth, input.u32()};
  person.qi = input.u32();
  person.qi_capacity = input.u32();
  person.known = get_all<Known>(input, [&input] {
    Known known;
    known.art = input.size();
    known.skill = input.size();
    known.ready_at = input.u64();
    return known;
  });
  person.silver = input.u64();
  person.held = get_all<std::uint64_t>(input, [&input] { return input.u64(); });
  person.occupation = get_index(input);
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
  put_all(out, spec.kinds, [&out](const std::string& kind) { out.text(kind); });
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
  spec.kinds = get_all<std::string>(input, [&input] { return input.text(); });
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
  out.size(manual.art);
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
  manual.art = input.size();
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

void put_progress(Writer& out, const Progress& step) {
  out.u64(step.sequence);
  out.u64(step.tick);
  out.u8(static_cast<std::uint8_t>(step.what.index()));
  if (const auto* learned = std::get_if<Learned>(&step.what)) {
    out.size(learned->person);
    out.size(learned->art);
    out.size(learned->skill);
    out.u8(static_cast<std::uint8_t>(learned->by));
  } else {
    out.size(std::get<RealmGained>(step.what).person);
    out.size(std::get<RealmGained>(step.what).rank);
  }
}

Progress get_progress(Reader& input) {
  constexpr std::uint8_t ADVANCES = std::variant_size_v<Advance>;
  constexpr std::uint8_t LESSONS = 3;
  // The advances' tags, as their index in Advance.
  constexpr std::uint8_t LEARNED = 0;
  Progress step;
  step.sequence = input.u64();
  step.tick = input.u64();
  const std::uint8_t tag = input.tag(ADVANCES);
  const std::size_t person = input.size();
  if (tag == LEARNED) {
    const std::size_t art = input.size();
    const std::size_t skill = input.size();
    step.what =
        Learned{person, art, skill, static_cast<LearnedBy>(input.tag(LESSONS))};
  } else {
    step.what = RealmGained{person, input.size()};
  }
  return step;
}

}  // namespace

/// Writes a world's state to a save's payload and reads it back.
class SaveCodec {
 public:
  static void write(Writer& out, const World& world) {
    out.u64(world.rng_state_);
    out.u64(world.seed_);
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
      put_index(out, army.order.target);
      put_sizes(out, army.roster);
      out.flag(army.steered);
      const auto* far = std::get_if<FarAnchor>(&army.course);
      out.flag(far != nullptr);
      if (far != nullptr) {
        out.size(far->area);
        put_cell(out, far->cell);
        out.u32(far->progress);
      }
    });
    put_all(out, world.areas_.tiers(),
            [&out](AreaTier tier) { out.u8(static_cast<std::uint8_t>(tier)); });
    put_all(out, world.structures_, [&out](const Structure& structure) {
      put_cell(out, structure.cell);
      out.u32(structure.durability);
      put_footprint(out, structure.footprint);
      out.flag(structure.blocks);
      out.u8(static_cast<std::uint8_t>(structure.role));
    });
    put_all(out, world.villages_.records(),
            [&out](const VillageRecord& village) {
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
              out.size(village.tier);
              put_cell(out, village.centre);
            });
    put_all(out, world.leaders_.factions(), [&out](const Leadership& faction) {
      put_leadership(out, faction);
    });
    put_all(out, world.sects_, [&out](const World::Sect& sect) {
      out.text(sect.name);
      out.size(sect.faction);
      out.size(sect.seat);
    });
    put_all(out, world.manuals_,
            [&out](const Manual& manual) { put_manual(out, manual); });
    put_all(out, world.leaders_.duels(), [&out](const Fight& fight) {
      out.size(fight.faction);
      out.size(fight.challenger);
      out.size(fight.leader);
      put_duel(out, fight.rule);
    });
    put_all(out, world.declarations_.slots(),
            [&out](const std::optional<Declared>& standing) {
              put_maybe(out, standing, [&out](Declared declared) {
                out.u8(static_cast<std::uint8_t>(declared.what));
                out.size(declared.by);
                out.u64(declared.tick);
              });
            });
    put_all(out, world.progress_.entries(),
            [&out](const Progress& step) { put_progress(out, step); });
    put_all(out, world.events_.entries(),
            [&out](const Event& event) { put_event(out, event); });
    put_all(out, world.souls_, [&out](const Soul& soul) {
      out.u32(soul.lives);
      put_all(out, soul.practised,
              [&out](bool practised) { out.flag(practised); });
    });
    put_sizes(out, world.surnames_);
    put_all(out, world.villages_.membership(),
            [&out](std::optional<std::size_t> village) {
              put_index(out, village);
            });
    put_all(out, world.plans_,
            [&out](std::optional<std::size_t> plan) { put_index(out, plan); });
    put_all(out, world.attacks_.engaged(), [&out](const Engaged& each) {
      out.size(each.person);
      out.size(each.engagement.foe);
      out.u8(static_cast<std::uint8_t>(each.engagement.ring));
      put_maybe(out, each.engagement.slot,
                [&out](std::uint8_t slot) { out.u8(slot); });
      out.flag(each.engagement.token);
      out.u32(each.engagement.waiting_since);
      out.u32(each.engagement.token_since);
    });
    put_all(out, world.charts_,
            [&out](const Chart& chart) { put_chart(out, chart); });
  }

  // Reads a world from `input`, noting in `parts`, if given, where its parts
  // end.
  static World read(Reader& input, SaveParts* parts) {
    const std::uint64_t rng_state = input.u64();
    const std::uint64_t seed = input.u64();
    const std::uint64_t tick_count = input.u64();
    Map map = get_map(input);
    Rules rules = get_rules(input);
    World world{std::move(map), std::move(rules), get_life(input)};
    world.rng_state_ = rng_state;
    world.seed_ = seed;
    world.tick_count_ = tick_count;
    world.people_ =
        get_all<Person>(input, [&input] { return get_person(input); });
    world.armies_ = get_all<World::Army>(input, [&input, &world] {
      const std::size_t faction = input.size();
      std::vector<Skill> skills =
          get_all<Skill>(input, [&input] { return get_skill(input); });
      const Cell objective = get_cell(input);
      std::vector<std::size_t> order_skills = get_sizes(input);
      const std::optional<std::size_t> target = get_index(input);
      if (!world.map_.passable(objective)) {
        throw Refusal{LoadError::CORRUPTED};
      }
      std::vector<std::size_t> roster = get_sizes(input);
      const bool steered = input.flag();
      // A far army holds its anchor; any other is bound for its objective.
      const auto course = [&input, &world]() -> World::Course {
        if (!input.flag()) {
          return World::Bound{};
        }
        const std::size_t area = input.size();
        const Cell cell = get_cell(input);
        if (area >= world.areas_.count() || !world.map_.passable(cell) ||
            world.areas_.area_of(cell) != area) {
          throw Refusal{LoadError::CORRUPTED};
        }
        return FarAnchor{area, cell, input.u32()};
      };
      return World::Army{faction,
                         std::move(skills),
                         Order{objective, std::move(order_skills), target},
                         course(),
                         std::move(roster),
                         steered};
    });
    std::vector<AreaTier> tiers = get_all<AreaTier>(input, [&input] {
      return static_cast<AreaTier>(input.tag(AREA_TIERS));
    });
    if (tiers.size() != world.areas_.count()) {
      throw Refusal{LoadError::CORRUPTED};
    }
    world.areas_ = Areas{world.areas_.columns(), std::move(tiers)};
    world.structures_ = get_all<Structure>(input, [&input] {
      Structure structure;
      structure.cell = get_cell(input);
      structure.durability = input.u32();
      structure.footprint = get_footprint(input);
      structure.blocks = input.flag();
      structure.role = static_cast<Role>(input.tag(ROLES));
      return structure;
    });
    std::vector<VillageRecord> villages =
        get_all<VillageRecord>(input, [&input] {
          VillageRecord village{get_village_spec(input),
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
          village.tier = input.size();
          village.centre = get_cell(input);
          return village;
        });
    std::vector<Leadership> leadership =
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
    std::vector<Fight> duels = get_all<Fight>(input, [&input] {
      Fight fight;
      fight.faction = input.size();
      fight.challenger = input.size();
      fight.leader = input.size();
      fight.rule = get_duel(input);
      return fight;
    });
    world.leaders_ = Leaders{std::move(leadership), std::move(duels)};
    std::vector<std::optional<Declared>> declarations =
        get_all<std::optional<Declared>>(input, [&input] {
          return get_maybe<Declared>(input, [&input] {
            constexpr std::uint8_t DECLARATIONS = 2;
            Declared declared;
            declared.what = static_cast<Declaration>(input.tag(DECLARATIONS));
            declared.by = input.size();
            declared.tick = input.u64();
            return declared;
          });
        });
    world.declarations_ =
        Declarations{world.rules_.hostile.size(), std::move(declarations)};
    world.progress_ = decltype(world.progress_){
        get_all<Progress>(input, [&input] { return get_progress(input); })};
    world.events_ = decltype(world.events_){
        get_all<Event>(input, [&input] { return get_event(input); })};
    world.souls_ = get_all<Soul>(input, [&input] {
      Soul soul;
      soul.lives = input.u32();
      soul.practised = get_all<bool>(input, [&input] { return input.flag(); });
      return soul;
    });
    world.surnames_ = get_sizes(input);
    std::vector<std::optional<std::size_t>> village_of =
        get_all<std::optional<std::size_t>>(
            input, [&input] { return get_index(input); });
    world.villages_ = Villages{std::move(villages), std::move(village_of)};
    world.plans_ = get_all<std::optional<std::size_t>>(
        input, [&input] { return get_index(input); });
    if (parts != nullptr) {
      parts->plans_end = input.next() - HEADER_BYTES;
    }
    const std::vector<Engaged> engaged = get_all<Engaged>(input, [&input] {
      Engaged each;
      each.person = input.size();
      each.engagement.foe = input.size();
      each.engagement.ring = static_cast<Ring>(input.tag(RING_KINDS));
      each.engagement.slot =
          get_maybe<std::uint8_t>(input, [&input] { return input.u8(); });
      each.engagement.token = input.flag();
      each.engagement.waiting_since = input.u32();
      each.engagement.token_since = input.u32();
      return each;
    });
    std::optional<Attacks> attacks = Attacks::of(world.people_.size(), engaged);
    if (!attacks) {
      throw Refusal{LoadError::CORRUPTED};
    }
    world.attacks_ = std::move(*attacks);
    if (parts != nullptr) {
      parts->engaged_end = input.next() - HEADER_BYTES;
    }
    const Footprint size{world.map_.width(), world.map_.height()};
    world.charts_ = get_all<Chart>(input, [&input, &world, size] {
      Chart chart = get_chart(input, size, world.rules_.hostile.size());
      // A chart remembers no village past the world's.
      if (chart.villages().size() > world.villages_.size()) {
        throw Refusal{LoadError::CORRUPTED};
      }
      return chart;
    });
    if (parts != nullptr) {
      parts->charts_end = input.next() - HEADER_BYTES;
    }
    if (world.charts_.size() != world.souls_.size()) {
      throw Refusal{LoadError::CORRUPTED};
    }
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

namespace {

// Reads the save `bytes`, noting in `parts`, if given, where its parts end.
std::variant<Saved, LoadError> decode(const std::vector<std::uint8_t>& bytes,
                                      SaveParts* parts) {
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
    World world = SaveCodec::read(input, parts);
    if (!input.done()) {
      return LoadError::CORRUPTED;
    }
    return Saved{origin, std::move(world)};
  } catch (const Refusal& refusal) {
    return refusal.error;
  } catch (const Malformed&) {
    return LoadError::CORRUPTED;
  } catch (const std::invalid_argument&) {
    return LoadError::CORRUPTED;
  }
}

}  // namespace

std::variant<Saved, LoadError> load(const SaveStore& store, Slot slot) {
  const std::optional<std::vector<std::uint8_t>> held = store.get(slot);
  if (!held) {
    return LoadError::MISSING;
  }
  return decode(*held, nullptr);
}

std::variant<SaveParts, LoadError> save_parts(
    const std::vector<std::uint8_t>& bytes) {
  SaveParts parts;
  const std::variant<Saved, LoadError> decoded = decode(bytes, &parts);
  if (const auto* error = std::get_if<LoadError>(&decoded)) {
    return *error;
  }
  return parts;
}

}  // namespace murim
