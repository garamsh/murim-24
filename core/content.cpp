#include "content.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "arts.hpp"
#include "diplomacy.hpp"
#include "growth.hpp"
#include "kinship.hpp"
#include "leadership.hpp"
#include "map.hpp"
#include "names.hpp"
#include "region.hpp"
#include "skill.hpp"
#include "talent.hpp"
#include "trade.hpp"
#include "village.hpp"

namespace murim {
namespace {

using Kind = ContentNode::Kind;

constexpr std::array<std::string_view, 3> FOOTINGS{"buildable", "rough",
                                                   "blocked"};
constexpr std::array<std::string_view, 5> SHAPES{"area", "arc", "line", "self",
                                                 "targeted"};
constexpr std::array<std::string_view, 3> EXPANSIONS{"build", "found", "raid"};
constexpr std::array<std::string_view, 3> ORIENTATIONS{"orthodox", "unorthodox",
                                                       "demonic"};
constexpr std::array<std::string_view, 2> POLARITIES{"yang", "yin"};

// The kinds of content a pack may hold, in the order their tables are built,
// so a kind is built after every kind it names.
constexpr std::array<std::string_view, 15> KINDS{
    "terrain",  "skills",       "factions",   "structure_kinds", "villages",
    "seats",    "disciplines",  "arts",       "sects",           "realms",
    "surnames", "female_names", "male_names", "wares",           "life"};

// Whether `name` is lowercase ASCII letters, digits and underscores, and not
// empty.
bool well_formed_id(const std::string& name) {
  return !name.empty() && std::all_of(name.begin(), name.end(), [](char each) {
    return (each >= 'a' && each <= 'z') || (each >= '0' && each <= '9') ||
           each == '_';
  });
}

// Whether `name` is the name of a kind of content the core knows.
bool known_kind(const std::string& name) {
  return std::find(KINDS.begin(), KINDS.end(), name) != KINDS.end();
}

// Where a reader reports what it finds, and how it treats what it reads: in
// a pack's own check, a missing field and an unresolved id are not yet
// errors, since a later pack's entry may give the rest of an entry an
// earlier one began.
struct Context {
  std::vector<ContentError>* errors = nullptr;
  bool building = false;
  // In building, the packs in load order, by the index a node's `pack`
  // holds, and each kind's ids in index order.
  const std::vector<std::string>* order = nullptr;
  const std::map<std::string, std::vector<std::string>>* ids = nullptr;
  std::string pack;
  std::string document;
  std::string entry;
};

// Reads the fields of one map node, recording an error for each field of
// the wrong type, outside its limits, naming an id no entry has, or, when
// building, missing; and on finish for each field it never read.
class Reader {
 public:
  Reader(const ContentTree& tree, std::size_t node, const Context& context,
         std::string path)
      : tree_(&tree), node_(node), context_(&context), path_(std::move(path)) {}

  [[nodiscard]] bool has(const std::string& key) const {
    return member(*tree_, node_, key).has_value();
  }

  // The integer field `key`, between `low` and `high`; `fallback` when it
  // is optional and absent, or anything is wrong with it.
  [[nodiscard]] std::int64_t integer(const std::string& key, std::int64_t low,
                                     std::int64_t high,
                                     std::optional<std::int64_t> fallback) {
    const std::optional<std::size_t> field = take(key, fallback.has_value());
    const std::int64_t otherwise = fallback.value_or(low);
    if (!field) {
      return otherwise;
    }
    const ContentNode& value = at(*field);
    if (value.kind == Kind::FRACTION) {
      fail(key, ContentRefusal::FRACTION, value);
      return otherwise;
    }
    if (value.kind != Kind::INTEGER) {
      fail(key, ContentRefusal::WRONG_TYPE, value);
      return otherwise;
    }
    if (value.integer < low || value.integer > high) {
      fail(key, ContentRefusal::OUT_OF_RANGE, value);
      return otherwise;
    }
    return value.integer;
  }

  // The integer field `key` as a `Number`, at least `low` and at most the
  // type's greatest value.
  template <typename Number>
  [[nodiscard]] Number number(const std::string& key, Number low,
                              std::optional<Number> fallback = std::nullopt) {
    constexpr auto GREATEST = std::numeric_limits<Number>::max();
    constexpr std::int64_t HIGH =
        static_cast<std::uint64_t>(GREATEST) >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max())
            ? std::numeric_limits<std::int64_t>::max()
            : static_cast<std::int64_t>(GREATEST);
    const std::optional<std::int64_t> given =
        fallback
            ? std::optional<std::int64_t>{static_cast<std::int64_t>(*fallback)}
            : std::nullopt;
    return static_cast<Number>(
        integer(key, static_cast<std::int64_t>(low), HIGH, given));
  }

  [[nodiscard]] bool boolean(const std::string& key,
                             std::optional<bool> fallback = std::nullopt) {
    const std::optional<std::size_t> field = take(key, fallback.has_value());
    if (!field) {
      return fallback.value_or(false);
    }
    const ContentNode& value = at(*field);
    if (value.kind != Kind::BOOLEAN) {
      fail(key, ContentRefusal::WRONG_TYPE, value);
      return fallback.value_or(false);
    }
    return value.boolean;
  }

  [[nodiscard]] std::string text(const std::string& key) {
    const std::optional<std::size_t> field = take(key, false);
    if (!field) {
      return {};
    }
    const ContentNode& value = at(*field);
    if (value.kind != Kind::STRING) {
      fail(key, ContentRefusal::WRONG_TYPE, value);
      return {};
    }
    return value.text;
  }

  // The index among `options` of the word field `key` gives.
  template <std::size_t COUNT>
  [[nodiscard]] std::size_t choice(
      const std::string& key,
      const std::array<std::string_view, COUNT>& options) {
    const std::optional<std::size_t> field = take(key, false);
    return field ? choice_of(key, at(*field), options) : 0;
  }

  // The indices among `options` of the words list field `key` gives.
  template <std::size_t COUNT>
  [[nodiscard]] std::vector<std::size_t> choices(
      const std::string& key,
      const std::array<std::string_view, COUNT>& options) {
    std::vector<std::size_t> found;
    const std::optional<std::size_t> field = list(key);
    for (std::size_t place = 0; field && place < at(*field).children.size();
         ++place) {
      found.push_back(choice_of(item(key, place),
                                at(at(*field).children.at(place)), options));
    }
    return found;
  }

  // The index among kind `kind`'s entries of the id field `key` names.
  [[nodiscard]] std::size_t reference(const std::string& key,
                                      const std::string& kind) {
    const std::optional<std::size_t> field = take(key, false);
    return field ? reference_of(key, at(*field), kind) : 0;
  }

  // The indices among kind `kind`'s entries of the ids list field `key`
  // names.
  [[nodiscard]] std::vector<std::size_t> references(const std::string& key,
                                                    const std::string& kind) {
    std::vector<std::size_t> found;
    const std::optional<std::size_t> field = list(key);
    for (std::size_t place = 0; field && place < at(*field).children.size();
         ++place) {
      found.push_back(reference_of(item(key, place),
                                   at(at(*field).children.at(place)), kind));
    }
    return found;
  }

  // Reads the map field `key`, required unless `optional`, with `read`.
  // Returns whether it was there.
  bool object(const std::string& key, bool optional,
              const std::function<void(Reader&)>& read) {
    const std::optional<std::size_t> field = take(key, optional);
    if (!field) {
      return false;
    }
    if (at(*field).kind != Kind::MAP) {
      fail(key, ContentRefusal::WRONG_TYPE, at(*field));
      return false;
    }
    Reader inner(*tree_, *field, *context_, join(key));
    read(inner);
    inner.finish();
    return true;
  }

  // Reads each map of the list field `key`, required unless `optional`, with
  // `read`.
  void each(const std::string& key, bool optional,
            const std::function<void(Reader&)>& read) {
    const std::optional<std::size_t> field =
        optional && !has(key) ? std::nullopt : list(key);
    read_.insert(key);
    for (std::size_t place = 0; field && place < at(*field).children.size();
         ++place) {
      const std::size_t child = at(*field).children.at(place);
      if (at(child).kind != Kind::MAP) {
        fail(item(key, place), ContentRefusal::WRONG_TYPE, at(child));
        continue;
      }
      Reader inner(*tree_, child, *context_, join(item(key, place)));
      read(inner);
      inner.finish();
    }
  }

  // Marks field `key` read, without reading it.
  void skip(const std::string& key) { read_.insert(key); }

  // Records each field never read as unknown.
  void finish() const {
    const ContentNode& map = at(node_);
    for (std::size_t place = 0; place < map.keys.size(); ++place) {
      if (read_.count(map.keys.at(place)) == 0) {
        fail(map.keys.at(place), ContentRefusal::UNKNOWN_FIELD,
             at(map.children.at(place)));
      }
    }
  }

 private:
  [[nodiscard]] const ContentNode& at(std::size_t node) const {
    return tree_->nodes.at(node);
  }

  [[nodiscard]] std::string join(const std::string& key) const {
    return path_.empty() ? key : path_ + "." + key;
  }

  [[nodiscard]] static std::string item(const std::string& key,
                                        std::size_t place) {
    return key + "." + std::to_string(place);
  }

  std::optional<std::size_t> take(const std::string& key, bool optional) {
    read_.insert(key);
    const std::optional<std::size_t> field = member(*tree_, node_, key);
    if (!field && !optional && context_->building) {
      fail(key, ContentRefusal::MISSING_FIELD, at(node_));
    }
    return field;
  }

  std::optional<std::size_t> list(const std::string& key) {
    const std::optional<std::size_t> field = take(key, false);
    if (field && at(*field).kind != Kind::LIST) {
      fail(key, ContentRefusal::WRONG_TYPE, at(*field));
      return std::nullopt;
    }
    return field;
  }

  template <std::size_t COUNT>
  [[nodiscard]] std::size_t choice_of(
      const std::string& key, const ContentNode& value,
      const std::array<std::string_view, COUNT>& options) const {
    if (value.kind != Kind::STRING) {
      fail(key, ContentRefusal::WRONG_TYPE, value);
      return 0;
    }
    const auto found = std::find(options.begin(), options.end(), value.text);
    if (found == options.end()) {
      fail(key, ContentRefusal::OUT_OF_RANGE, value);
      return 0;
    }
    return static_cast<std::size_t>(found - options.begin());
  }

  [[nodiscard]] std::size_t reference_of(const std::string& key,
                                         const ContentNode& value,
                                         const std::string& kind) const {
    if (value.kind != Kind::STRING) {
      fail(key, ContentRefusal::WRONG_TYPE, value);
      return 0;
    }
    if (!well_formed_id(value.text)) {
      fail(key, ContentRefusal::BAD_ID, value);
      return 0;
    }
    if (!context_->building) {
      return 0;
    }
    const std::vector<std::string>& kind_ids = context_->ids->at(kind);
    const auto found = std::find(kind_ids.begin(), kind_ids.end(), value.text);
    if (found == kind_ids.end()) {
      fail(key, ContentRefusal::UNRESOLVED_ID, value);
      return 0;
    }
    return static_cast<std::size_t>(found - kind_ids.begin());
  }

  void fail(const std::string& key, ContentRefusal reason,
            const ContentNode& value) const {
    const std::string pack =
        context_->building ? context_->order->at(value.pack) : context_->pack;
    context_->errors->push_back(ContentError{
        pack, context_->document, context_->entry, join(key), reason});
  }

  const ContentTree* tree_;
  std::size_t node_;
  const Context* context_;
  std::string path_;
  std::set<std::string> read_;
};

Spread read_spread(Reader& reader) {
  const auto mean = reader.number<std::uint32_t>("mean", 0);
  return Spread{mean, reader.number<std::uint32_t>("spread", 0)};
}

Range read_range(Reader& reader) {
  const auto low = reader.number<std::uint32_t>("low", 0);
  return Range{low, reader.number<std::uint32_t>("high", 0)};
}

Footprint read_footprint(Reader& reader) {
  const auto width = reader.number<std::int32_t>("width", 1);
  return Footprint{width, reader.number<std::int32_t>("height", 1)};
}

Cell read_cell(Reader& reader) {
  constexpr std::int32_t LOWEST = std::numeric_limits<std::int32_t>::min();
  const auto column = reader.number<std::int32_t>("x", LOWEST);
  return Cell{column, reader.number<std::int32_t>("y", LOWEST)};
}

NameEntry read_name(Reader& reader) {
  std::string korean = reader.text("korean");
  return NameEntry{std::move(korean), reader.text("romanised")};
}

NameEntry read_named(Reader& reader, const std::string& key) {
  NameEntry name;
  reader.object(key, false,
                [&name](Reader& inner) { name = read_name(inner); });
  return name;
}

Skill read_skill(Reader& reader) {
  Skill skill;
  skill.range = reader.number<std::int32_t>("range", 0);
  skill.radius = reader.number<std::int32_t>("radius", 0);
  skill.body_damage = reader.number<std::uint64_t>("body_damage", 0);
  skill.structure_damage = reader.number<std::int32_t>(
      "structure_damage", std::numeric_limits<std::int32_t>::min());
  skill.cooldown = reader.number<std::uint64_t>("cooldown", 0);
  skill.qi_cost = reader.number<std::uint32_t>("qi_cost", 0, 0U);
  skill.shape = static_cast<Shape>(reader.choice("shape", SHAPES));
  return skill;
}

// The tables built so far, kind by kind in KINDS' order.
struct Built {
  RegionData data;
  std::vector<Skill> skills;
  std::vector<std::vector<std::size_t>> manuals;
  std::size_t lives = 0;
};

// The id of entry `index` of kind `kind`, when building; none otherwise.
std::string id_of(const Context& context, const std::string& kind,
                  std::size_t index) {
  if (!context.building) {
    return {};
  }
  const std::vector<std::string>& kind_ids = context.ids->at(kind);
  return index < kind_ids.size() ? kind_ids.at(index) : std::string{};
}

// A template of a village or a seat, by id `name`; the kinds of its
// buildings name structure kinds by id.
VillageTemplate read_template(Reader& reader, const std::string& name,
                              const Context& context) {
  VillageTemplate made{};
  made.name = name;
  made.area = read_footprint(reader);
  reader.each("buildings", false, [&made, &context](Reader& building) {
    const Cell cell = read_cell(building);
    const Footprint footprint = read_footprint(building);
    const auto durability = building.number<std::uint32_t>("durability", 1);
    const std::size_t kind = building.reference("kind", "structure_kinds");
    const auto households = building.number<std::uint32_t>("households", 0, 0U);
    made.buildings.push_back(Building{Structure{cell, durability, footprint},
                                      id_of(context, "structure_kinds", kind),
                                      households});
  });
  reader.each("clearings", true, [&made](Reader& clearing) {
    const Cell corner = read_cell(clearing);
    made.clearings.push_back(Clearing{corner, read_footprint(clearing)});
  });
  return made;
}

Faction read_faction(Reader& reader, const Built& built) {
  Faction faction;
  faction.name = reader.text("name");
  faction.hostile_to = reader.references("hostile_to", "factions");
  for (const std::size_t skill : reader.references("skills", "skills")) {
    if (skill < built.skills.size()) {
      faction.skills.push_back(built.skills.at(skill));
    }
  }
  reader.object("succession", true, [&faction](Reader& inner) {
    const bool members_only = inner.boolean("members_only");
    const auto yield_health = inner.number<std::uint64_t>("yield_health", 0);
    faction.succession =
        Duel{members_only, yield_health, inner.boolean("allies_turn_hostile")};
  });
  for (const std::size_t expansion : reader.choices("stance", EXPANSIONS)) {
    faction.stance.push_back(static_cast<Expansion>(expansion));
  }
  reader.object("admission", true, [&faction](Reader& inner) {
    Admission admission;
    admission.age = inner.number<std::uint32_t>("age", 0);
    admission.talent = inner.number<std::uint32_t>("talent", 0);
    admission.attainment = inner.number<std::uint32_t>("attainment", 0);
    admission.reach = inner.number<std::int32_t>("reach", 0);
    admission.chance = inner.number<std::uint32_t>("chance", 0);
    admission.elder_share = inner.number<std::uint32_t>("elder_share", 0);
    faction.admission = admission;
  });
  reader.object("election", true, [&faction](Reader& inner) {
    faction.election = Election{inner.number<std::uint32_t>("term", 0)};
  });
  return faction;
}

StructureKind read_structure_kind(Reader& reader, const std::string& name) {
  StructureKind made{};
  made.name = name;
  made.housing = reader.number<std::uint32_t>("housing", 0, 0U);
  made.storage = reader.number<std::uint32_t>("storage", 0, 0U);
  made.yield = reader.number<std::uint32_t>("yield", 0, 0U);
  made.blocks = reader.boolean("blocks", true);
  made.pupils = reader.number<std::uint32_t>("pupils", 0, 0U);
  made.arms = reader.number<std::uint32_t>("arms", 0, 0U);
  made.tends = reader.number<std::uint32_t>("tends", 0, 0U);
  made.edge = reader.number<std::uint32_t>("edge", 0, 0U);
  made.healing = reader.number<std::uint32_t>("healing", 0, 0U);
  return made;
}

Art read_art(Reader& reader) {
  Art art;
  art.name = read_named(reader, "name");
  art.discipline = reader.reference("discipline", "disciplines");
  reader.object("nature", false, [&art](Reader& inner) {
    const auto orientation =
        static_cast<Orientation>(inner.choice("orientation", ORIENTATIONS));
    art.nature =
        Nature{orientation,
               static_cast<Polarity>(inner.choice("polarity", POLARITIES))};
  });
  reader.each("skills", false, [&art](Reader& inner) {
    ArtSkill skill;
    skill.name = read_named(inner, "name");
    skill.skill = read_skill(inner);
    skill.rank = inner.number<std::uint32_t>("rank", 0);
    skill.comprehensible = inner.boolean("comprehensible");
    art.skills.push_back(std::move(skill));
  });
  return art;
}

Kinship read_kinship(Reader& reader) {
  Kinship kinship;
  reader.object("marrying_age", false, [&kinship](Reader& age) {
    kinship.marrying_age = read_range(age);
  });
  kinship.neighbourhood = reader.number<std::int32_t>("neighbourhood", 0);
  kinship.marriage_chance = reader.number<std::uint32_t>("marriage_chance", 0);
  reader.object("bearing_age", false, [&kinship](Reader& age) {
    kinship.bearing_age = read_range(age);
  });
  kinship.birth_chance = reader.number<std::uint32_t>("birth_chance", 0);
  kinship.birth_spacing = reader.number<std::uint32_t>("birth_spacing", 0);
  kinship.fathers_family = reader.boolean("fathers_family", true);
  kinship.balanced_sexes = reader.boolean("balanced_sexes", false);
  return kinship;
}

// The rules of growth, its plans laid for the builds and the settlement
// template the entry names, once the villages are known.
void read_growth(Reader& reader, Built& built, const Context& context) {
  Growth growth;
  growth.interval = reader.number<std::uint32_t>("interval", 1);
  growth.pressure = reader.number<std::uint32_t>("pressure", 0);
  growth.reach = reader.number<std::int32_t>("reach", 0);
  growth.vicinity = reader.number<std::int32_t>("vicinity", 0);
  growth.spacing = reader.number<std::int32_t>("spacing", 0);
  growth.claim = reader.number<std::int32_t>("claim", 0);
  growth.settlers = reader.number<std::uint32_t>("settlers", 0);
  growth.raiders = reader.number<std::uint32_t>("raiders", 0);
  growth.self_reliance = reader.number<std::uint32_t>("self_reliance", 0);
  std::vector<Building> builds;
  reader.each("builds", false, [&builds, &context](Reader& build) {
    const std::size_t kind = build.reference("kind", "structure_kinds");
    builds.push_back(Building{Structure{Cell{0, 0}, 0, read_footprint(build)},
                              id_of(context, "structure_kinds", kind), 0});
  });
  const std::size_t settlement = reader.reference("settlement", "villages");
  const RegionData& data = built.data;
  if (context.building && settlement < data.villages.size()) {
    built.data.life.growth = growth_for(data, std::move(growth), builds,
                                        data.villages.at(settlement));
  }
}

Learning read_learning(Reader& reader) {
  Learning learning;
  learning.comprehension = reader.number<std::uint32_t>("comprehension", 0);
  learning.teaching = reader.number<std::uint32_t>("teaching", 0);
  learning.reading_cost = reader.number<std::uint32_t>("reading_cost", 0);
  learning.reading_reach = reader.number<std::int32_t>("reading_reach", 0);
  learning.orientation_ease =
      reader.number<std::uint32_t>("orientation_ease", 0);
  learning.polarity_ease = reader.number<std::uint32_t>("polarity_ease", 0);
  learning.orientation_potency =
      reader.number<std::uint32_t>("orientation_potency", 0);
  learning.polarity_potency =
      reader.number<std::uint32_t>("polarity_potency", 0);
  return learning;
}

// The rules of life, the one entry of kind `life`.
void read_life(Reader& reader, Built& built, const Context& context) {
  Life& life = built.data.life;
  life.newborn_health = reader.number<std::uint64_t>("newborn_health", 1);
  reader.object("lifespan", true,
                [&life](Reader& inner) { life.lifespan = read_spread(inner); });
  life.days_per_attainment =
      reader.number<std::uint32_t>("days_per_attainment", 0, 0U);
  life.discipline_widening =
      reader.number<std::uint32_t>("discipline_widening", 0, 0U);
  life.upper_widening = reader.number<std::uint32_t>("upper_widening", 0, 0U);
  reader.object("founding_age", true, [&life](Reader& inner) {
    life.founding_age = read_spread(inner);
  });
  reader.object("kinship", true,
                [&life](Reader& inner) { life.kinship = read_kinship(inner); });
  reader.object("husbandry", true, [&life](Reader& inner) {
    const auto consumption = inner.number<std::uint32_t>("consumption", 0);
    const auto hunger_days = inner.number<std::uint32_t>("hunger_days", 0);
    life.husbandry =
        Husbandry{consumption, hunger_days,
                  inner.number<std::int32_t>("migration_reach", 0)};
  });
  reader.object("growth", true, [&built, &context](Reader& inner) {
    read_growth(inner, built, context);
  });
  reader.object("manuals", true, [&built](Reader& inner) {
    const auto chance = inner.number<std::uint32_t>("scatter_chance", 0);
    built.data.life.manuals = Manuals{
        built.manuals, chance, inner.number<std::int32_t>("scatter_reach", 0)};
  });
  reader.object("cultivation", true, [&life](Reader& inner) {
    Cultivation cultivation;
    inner.object("growth", false, [&cultivation](Reader& spread) {
      cultivation.growth = read_spread(spread);
    });
    inner.object("limit", false, [&cultivation](Reader& spread) {
      cultivation.limit = read_spread(spread);
    });
    cultivation.recovery = inner.number<std::uint32_t>("recovery", 0);
    cultivation.recovery_ticks =
        inner.number<std::uint64_t>("recovery_ticks", 1);
    life.cultivation = cultivation;
  });
  reader.object("learning", false, [&life](Reader& inner) {
    life.learning = read_learning(inner);
  });
  reader.object("diplomacy", true, [&life](Reader& inner) {
    Diplomacy diplomacy;
    diplomacy.war_margin = inner.number<std::uint32_t>("war_margin", 0);
    diplomacy.war_chance = inner.number<std::uint32_t>("war_chance", 0);
    diplomacy.peace_chance = inner.number<std::uint32_t>("peace_chance", 0);
    diplomacy.hold_days = inner.number<std::uint32_t>("hold_days", 0);
    life.diplomacy = diplomacy;
  });
  reader.object("trade", true, [&life](Reader& inner) {
    Trade trade = life.trade.value_or(Trade{});
    trade.purse = inner.number<std::uint64_t>("purse", 0);
    trade.wage = inner.number<std::uint64_t>("wage", 0);
    trade.wounded = inner.number<std::uint32_t>("wounded", 0);
    trade.drained = inner.number<std::uint32_t>("drained", 0);
    life.trade = trade;
  });
}

Realm read_realm(Reader& reader) {
  Realm realm;
  realm.name = read_named(reader, "name");
  realm.qi = reader.number<std::uint32_t>("qi", 0);
  realm.might = reader.number<std::uint32_t>("might", 0);
  return realm;
}

Ware read_ware(Reader& reader, const std::string& name) {
  Ware ware;
  ware.name = name;
  ware.price = reader.number<std::uint64_t>("price", 0);
  ware.health = reader.number<std::uint64_t>("health", 0, std::uint64_t{0});
  ware.qi = reader.number<std::uint32_t>("qi", 0, 0U);
  ware.discipline = reader.reference("discipline", "disciplines");
  return ware;
}

DisciplineDraw read_discipline(Reader& reader) {
  DisciplineDraw draw;
  draw.name = reader.text("name");
  reader.object("growth", false,
                [&draw](Reader& inner) { draw.growth = read_spread(inner); });
  reader.object("limit", false,
                [&draw](Reader& inner) { draw.limit = read_spread(inner); });
  return draw;
}

// Reads the entry the context names, of kind `kind`, into `built`.
void read_entry(const std::string& kind, Reader& reader, Built& built,
                const Context& context) {
  RegionData& data = built.data;
  const std::string& name = context.entry;
  if (kind == "terrain") {
    const auto footing =
        static_cast<Footing>(reader.choice("footing", FOOTINGS));
    data.terrain.push_back(
        TerrainKind{name, footing, reader.number<std::int32_t>("weight", 1)});
  } else if (kind == "skills") {
    built.skills.push_back(read_skill(reader));
  } else if (kind == "factions") {
    data.factions.push_back(read_faction(reader, built));
  } else if (kind == "structure_kinds") {
    data.kinds.push_back(read_structure_kind(reader, name));
  } else if (kind == "villages") {
    data.villages.push_back(read_template(reader, name, context));
  } else if (kind == "seats") {
    data.seat = read_template(reader, name, context);
  } else if (kind == "disciplines") {
    data.life.disciplines.push_back(read_discipline(reader));
  } else if (kind == "arts") {
    data.life.arts.push_back(read_art(reader));
  } else if (kind == "sects") {
    std::string sect = reader.text("name");
    data.sects.push_back(
        SectData{std::move(sect), reader.reference("faction", "factions")});
    built.manuals.push_back(reader.references("manuals", "arts"));
  } else if (kind == "realms") {
    data.life.realms.push_back(read_realm(reader));
  } else if (kind == "surnames") {
    data.life.names.surnames.push_back(read_name(reader));
  } else if (kind == "female_names") {
    data.life.names.female.push_back(read_name(reader));
  } else if (kind == "male_names") {
    data.life.names.male.push_back(read_name(reader));
  } else if (kind == "wares") {
    if (!data.life.trade) {
      data.life.trade = Trade{};
    }
    data.life.trade->wares.push_back(read_ware(reader, name));
  } else if (kind == "life") {
    ++built.lives;
    read_life(reader, built, context);
  }
}

// A pack's manifest, as read.
struct Manifest {
  std::int64_t version = 0;
  bool base = false;
  std::vector<std::pair<std::string, std::int64_t>> requires_packs;
  std::vector<std::string> after;
};

// One entry as a pack gives it: its kind, its id, the document it is in and
// its node in that document's tree.
struct PackEntry {
  std::string kind;
  std::string name;
  std::size_t document = 0;
  std::size_t node = 0;
};

// A pack as read: its manifest, its documents' trees and the kinds they
// hold, and its entries.
struct ReadPack {
  Manifest manifest;
  std::vector<ContentTree> documents;
  std::vector<std::string> kinds;
  std::vector<PackEntry> entries;
};

// An error at the manifest of pack `pack`.
ContentError manifest_error(const std::string& pack, const std::string& path,
                            ContentRefusal reason) {
  return ContentError{pack, MANIFEST, "", path, reason};
}

// Reads pack `pack`'s manifest `tree` into `manifest`, recording each error.
void read_manifest(const ContentTree& tree, const std::string& pack,
                   Manifest& manifest, std::vector<ContentError>& errors) {
  if (tree.nodes.front().kind != Kind::MAP) {
    errors.push_back(manifest_error(pack, "", ContentRefusal::WRONG_TYPE));
    return;
  }
  const std::vector<std::string> order{pack};
  const std::map<std::string, std::vector<std::string>> none;
  Context context;
  context.errors = &errors;
  context.building = true;
  context.order = &order;
  context.ids = &none;
  context.pack = pack;
  context.document = MANIFEST;
  Reader reader(tree, 0, context, "");
  if (reader.text("id") != pack) {
    errors.push_back(manifest_error(pack, "id", ContentRefusal::OUT_OF_RANGE));
  }
  constexpr std::int64_t HIGHEST = std::numeric_limits<std::int64_t>::max();
  manifest.version = reader.integer("version", 0, HIGHEST, std::nullopt);
  static_cast<void>(reader.text("name"));
  manifest.base = reader.boolean("base", false);
  reader.each("requires", true, [&manifest](Reader& inner) {
    std::string need = inner.text("id");
    manifest.requires_packs.emplace_back(
        std::move(need), inner.integer("version", 0, HIGHEST, 0));
  });
  reader.skip("after");
  const std::optional<std::size_t> after = member(tree, 0, "after");
  if (after && tree.nodes.at(*after).kind != Kind::LIST) {
    errors.push_back(manifest_error(pack, "after", ContentRefusal::WRONG_TYPE));
  } else if (after) {
    for (const std::size_t child : tree.nodes.at(*after).children) {
      const ContentNode& other = tree.nodes.at(child);
      if (other.kind == Kind::STRING && well_formed_id(other.text)) {
        manifest.after.push_back(other.text);
      } else {
        errors.push_back(manifest_error(pack, "after", ContentRefusal::BAD_ID));
      }
    }
  }
  reader.finish();
}

// Checks entry `entry` of pack `pack` alone, as the pack gives it.
void check_entry(const ReadPack& read, const PackEntry& entry,
                 const std::string& pack, std::vector<ContentError>& errors) {
  const ContentTree& tree = read.documents.at(entry.document);
  Context context;
  context.errors = &errors;
  context.pack = pack;
  context.document = read.kinds.at(entry.document);
  context.entry = entry.name;
  Reader reader(tree, entry.node, context, "");
  reader.skip("id");
  if (member(tree, entry.node, "remove")) {
    // A removal names nothing else.
    static_cast<void>(reader.boolean("remove", false));
    reader.finish();
    return;
  }
  Built scratch;
  read_entry(entry.kind, reader, scratch, context);
  reader.finish();
}

// Records the entries of document `document` of `read`, and an error for
// each item that is not a map with a well-formed id, or whose id comes twice.
void list_entries(ReadPack& read, std::size_t document, const std::string& pack,
                  std::vector<ContentError>& errors) {
  const ContentTree& tree = read.documents.at(document);
  const std::string kind = read.kinds.at(document);
  std::set<std::string> seen;
  for (const std::size_t item : tree.nodes.front().children) {
    const ContentNode& value = tree.nodes.at(item);
    const std::optional<std::size_t> name =
        value.kind == Kind::MAP ? member(tree, item, "id") : std::nullopt;
    if (value.kind != Kind::MAP) {
      errors.push_back(
          ContentError{pack, kind, "", "", ContentRefusal::WRONG_TYPE});
    } else if (!name) {
      errors.push_back(
          ContentError{pack, kind, "", "id", ContentRefusal::MISSING_FIELD});
    } else if (const ContentNode& named = tree.nodes.at(*name);
               named.kind != Kind::STRING || !well_formed_id(named.text)) {
      errors.push_back(
          ContentError{pack, kind, "", "id", ContentRefusal::BAD_ID});
    } else if (!seen.insert(named.text).second) {
      errors.push_back(ContentError{pack, kind, named.text, "id",
                                    ContentRefusal::DUPLICATE_ID});
    } else {
      read.entries.push_back(PackEntry{kind, named.text, document, item});
    }
  }
}

// Reads every content document of pack `pack` from `source` into `read`,
// recording each error.
void read_documents(const PackSource& source, const std::string& pack,
                    ReadPack& read, std::vector<ContentError>& errors) {
  std::vector<std::string> names = source.documents(pack);
  std::sort(names.begin(), names.end());
  for (const std::string& kind : names) {
    if (!known_kind(kind)) {
      errors.push_back(
          ContentError{pack, kind, "", "", ContentRefusal::UNKNOWN_FIELD});
      continue;
    }
    ContentRead document = source.read(pack, kind);
    if (!document.tree) {
      errors.push_back(ContentError{pack, kind, "", "", document.refusal});
    } else if (document.tree->nodes.front().kind != Kind::LIST) {
      errors.push_back(
          ContentError{pack, kind, "", "", ContentRefusal::WRONG_TYPE});
    } else {
      read.documents.push_back(std::move(*document.tree));
      read.kinds.push_back(kind);
    }
  }
  for (std::size_t document = 0; document < read.documents.size(); ++document) {
    list_entries(read, document, pack, errors);
  }
  for (const PackEntry& entry : read.entries) {
    check_entry(read, entry, pack, errors);
  }
}

// Copies the node `root` of `from`, and everything under it, to the end of
// `into`, as set by pack `pack`. Returns its index in `into`.
std::size_t copy_tree(const ContentTree& from, std::size_t root,
                      ContentTree& into, std::size_t pack) {
  const std::size_t top = into.nodes.size();
  into.nodes.emplace_back();
  std::vector<std::pair<std::size_t, std::size_t>> waiting{{root, top}};
  while (!waiting.empty()) {
    const auto [source, target] = waiting.back();
    waiting.pop_back();
    ContentNode made = from.nodes.at(source);
    made.pack = pack;
    for (std::size_t& child : made.children) {
      const std::size_t slot = into.nodes.size();
      into.nodes.emplace_back();
      waiting.emplace_back(child, slot);
      child = slot;
    }
    into.nodes.at(target) = std::move(made);
  }
  return top;
}

// Merges the map `later` of `from`, set by pack `pack`, into the map at the
// root of `into` field by field: a map merges the same way, and anything
// else, a list included, replaces the field.
void merge_into(ContentTree& into, std::size_t later, const ContentTree& from,
                std::size_t pack) {
  std::vector<std::pair<std::size_t, std::size_t>> waiting{{0, later}};
  while (!waiting.empty()) {
    const auto [target, source] = waiting.back();
    waiting.pop_back();
    const ContentNode& given = from.nodes.at(source);
    for (std::size_t place = 0; place < given.keys.size(); ++place) {
      const std::string& key = given.keys.at(place);
      const std::size_t child = given.children.at(place);
      const std::optional<std::size_t> held = member(into, target, key);
      if (held && into.nodes.at(*held).kind == Kind::MAP &&
          from.nodes.at(child).kind == Kind::MAP) {
        waiting.emplace_back(*held, child);
      } else if (held) {
        const std::size_t copied = copy_tree(from, child, into, pack);
        ContentNode& map = into.nodes.at(target);
        const auto slot = static_cast<std::size_t>(
            std::find(map.keys.begin(), map.keys.end(), key) -
            map.keys.begin());
        map.children.at(slot) = copied;
      } else {
        const std::size_t copied = copy_tree(from, child, into, pack);
        into.nodes.at(target).keys.push_back(key);
        into.nodes.at(target).children.push_back(copied);
      }
    }
  }
}

// One kind's merged entries, in the order their ids first appeared, each
// with the tree of its fields.
using Merged = std::vector<std::pair<std::string, ContentTree>>;

// Merges the entries of the packs of `order` kind by kind.
std::map<std::string, Merged> merge_packs(
    const std::vector<std::string>& order,
    const std::map<std::string, ReadPack>& read) {
  std::map<std::string, Merged> merged;
  for (std::size_t pack = 0; pack < order.size(); ++pack) {
    const ReadPack& source = read.at(order.at(pack));
    for (const PackEntry& entry : source.entries) {
      const ContentTree& tree = source.documents.at(entry.document);
      Merged& kind = merged[entry.kind];
      const auto found = std::find_if(
          kind.begin(), kind.end(),
          [&entry](const auto& each) { return each.first == entry.name; });
      const std::optional<std::size_t> remove =
          member(tree, entry.node, "remove");
      if (remove && tree.nodes.at(*remove).boolean) {
        if (found != kind.end()) {
          kind.erase(found);
        }
      } else if (found == kind.end()) {
        ContentTree fresh;
        copy_tree(tree, entry.node, fresh, pack);
        kind.emplace_back(entry.name, std::move(fresh));
      } else {
        merge_into(found->second, entry.node, tree, pack);
      }
    }
  }
  return merged;
}

// Builds the tables from `merged`, recording each error.
RegionData build_tables(const std::map<std::string, Merged>& merged,
                        const std::vector<std::string>& order,
                        std::vector<ContentError>& errors) {
  std::map<std::string, std::vector<std::string>> ids;
  for (const std::string_view kind : KINDS) {
    std::vector<std::string>& kind_ids = ids[std::string{kind}];
    const auto found = merged.find(std::string{kind});
    if (found != merged.end()) {
      for (const auto& [name, tree] : found->second) {
        kind_ids.push_back(name);
      }
    }
  }
  Built built;
  for (const std::string_view kind : KINDS) {
    const auto found = merged.find(std::string{kind});
    if (found == merged.end()) {
      continue;
    }
    for (const auto& [name, tree] : found->second) {
      Context context;
      context.errors = &errors;
      context.building = true;
      context.order = &order;
      context.ids = &ids;
      context.document = std::string{kind};
      context.entry = name;
      context.pack = order.at(tree.nodes.front().pack);
      Reader reader(tree, 0, context, "");
      reader.skip("id");
      read_entry(std::string{kind}, reader, built, context);
      reader.finish();
    }
  }
  const std::size_t seats = ids.at("seats").size();
  if (built.lives != 1 || seats > 1) {
    errors.push_back(ContentError{order.back(), seats > 1 ? "seats" : "life",
                                  "", "", ContentRefusal::INCONSISTENT});
  }
  return built.data;
}

// Orders the packs of `wanted` not yet refused so that each follows the
// packs it requires and names to load after, keeping `wanted`'s order where
// it can; refuses the rest as in a cycle.
std::vector<std::string> order_packs(
    const std::vector<std::string>& wanted,
    const std::map<std::string, ReadPack>& read, std::set<std::string>& refused,
    std::vector<ContentError>& errors) {
  std::vector<std::string> placed;
  std::vector<std::string> waiting;
  for (const std::string& pack : wanted) {
    if (refused.count(pack) == 0) {
      waiting.push_back(pack);
    }
  }
  const auto ready = [&](const std::string& pack) {
    const Manifest& manifest = read.at(pack).manifest;
    const auto loaded = [&placed](const std::string& other) {
      return std::find(placed.begin(), placed.end(), other) != placed.end();
    };
    const auto present = [&waiting](const std::string& other) {
      return std::find(waiting.begin(), waiting.end(), other) != waiting.end();
    };
    return std::all_of(
               manifest.requires_packs.begin(), manifest.requires_packs.end(),
               [&loaded](const auto& need) { return loaded(need.first); }) &&
           std::all_of(manifest.after.begin(), manifest.after.end(),
                       [&](const std::string& other) {
                         return loaded(other) || !present(other);
                       });
  };
  while (!waiting.empty()) {
    const auto next = std::find_if(waiting.begin(), waiting.end(), ready);
    if (next == waiting.end()) {
      for (const std::string& pack : waiting) {
        errors.push_back(manifest_error(pack, "", ContentRefusal::CYCLE));
        refused.insert(pack);
      }
      break;
    }
    placed.push_back(*next);
    waiting.erase(next);
  }
  return placed;
}

// Refuses every pack not refused that requires a pack missing, refused or
// of too low a version, until none does.
void refuse_unmet(const std::vector<std::string>& wanted,
                  const std::map<std::string, ReadPack>& read,
                  std::set<std::string>& refused,
                  std::vector<ContentError>& errors) {
  bool changed = true;
  while (changed) {
    changed = false;
    for (const std::string& pack : wanted) {
      if (refused.count(pack) != 0) {
        continue;
      }
      for (const auto& [need, version] :
           read.at(pack).manifest.requires_packs) {
        const auto found = read.find(need);
        const bool met = found != read.end() && refused.count(need) == 0 &&
                         found->second.manifest.version >= version;
        if (!met) {
          errors.push_back(manifest_error(pack, "requires." + need,
                                          ContentRefusal::MISSING_REQUIREMENT));
          refused.insert(pack);
          changed = true;
          break;
        }
      }
    }
  }
}

// Reads pack `pack`'s manifest and documents from `source` into `made`,
// with `is_base` whether it is to be the base. Returns whether it found
// no error.
bool read_pack(const PackSource& source, const std::string& pack, bool is_base,
               ReadPack& made, std::vector<ContentError>& errors) {
  const std::size_t before = errors.size();
  const ContentRead manifest = source.read(pack, MANIFEST);
  if (!manifest.tree) {
    errors.push_back(manifest_error(pack, "", manifest.refusal));
  } else {
    read_manifest(*manifest.tree, pack, made.manifest, errors);
    if (made.manifest.base != is_base) {
      errors.push_back(manifest_error(pack, "base", ContentRefusal::BASE));
    }
    if (is_base && !made.manifest.requires_packs.empty()) {
      errors.push_back(manifest_error(pack, "requires", ContentRefusal::BASE));
    }
  }
  read_documents(source, pack, made, errors);
  return errors.size() == before;
}

}  // namespace

std::optional<std::size_t> member(const ContentTree& tree, std::size_t node,
                                  const std::string& key) {
  const ContentNode& map = tree.nodes.at(node);
  const auto found = std::find(map.keys.begin(), map.keys.end(), key);
  if (map.kind != ContentNode::Kind::MAP || found == map.keys.end()) {
    return std::nullopt;
  }
  return map.children.at(static_cast<std::size_t>(found - map.keys.begin()));
}

LoadedContent load_content(const PackSource& source, const std::string& base,
                           const std::vector<std::string>& mods) {
  LoadedContent loaded;
  std::vector<std::string> wanted{base};
  for (const std::string& mod : mods) {
    if (std::find(wanted.begin(), wanted.end(), mod) == wanted.end()) {
      wanted.push_back(mod);
    }
  }
  std::map<std::string, ReadPack> read;
  std::set<std::string> refused;
  for (const std::string& pack : wanted) {
    if (!read_pack(source, pack, pack == base, read[pack], loaded.errors)) {
      refused.insert(pack);
    }
  }
  refuse_unmet(wanted, read, refused, loaded.errors);
  if (refused.count(base) == 0) {
    loaded.order = order_packs(wanted, read, refused, loaded.errors);
  }
  for (const std::string& pack : wanted) {
    if (refused.count(pack) != 0) {
      loaded.refused.push_back(pack);
    }
  }
  if (refused.count(base) != 0) {
    loaded.order.clear();
    return loaded;
  }
  const std::size_t before = loaded.errors.size();
  RegionData data = build_tables(merge_packs(loaded.order, read), loaded.order,
                                 loaded.errors);
  if (loaded.errors.size() == before && !consistent(data)) {
    loaded.errors.push_back(ContentError{loaded.order.back(), "", "", "",
                                         ContentRefusal::INCONSISTENT});
  }
  if (loaded.errors.size() == before) {
    loaded.data = std::move(data);
  }
  return loaded;
}

}  // namespace murim
