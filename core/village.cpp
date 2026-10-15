#include "village.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "calendar.hpp"
#include "map.hpp"

namespace murim {

namespace {

// Whether `index`, if there is one, names one of `count` records.
bool below(std::optional<std::size_t> index, std::size_t count) {
  return !index || *index < count;
}

}  // namespace

Stages::Stages(const std::vector<Structure>& structures,
               std::array<std::uint32_t, 3> floors)
    : structures_(&structures), floors_(floors) {}

Stage Stages::of(std::size_t structure) const {
  const std::uint32_t durability = structures_->at(structure).durability;
  return static_cast<Stage>(std::count_if(
      floors_.begin(), floors_.end(),
      [durability](std::uint32_t floor) { return durability < floor; }));
}

Villages::Villages(std::size_t structures) : village_of_(structures) {}

Villages::Villages(std::vector<VillageRecord> records,
                   std::vector<std::optional<std::size_t>> village_of)
    : records_(std::move(records)), village_of_(std::move(village_of)) {}

std::size_t Villages::size() const { return records_.size(); }

bool Villages::empty() const { return records_.empty(); }

const VillageRecord& Villages::at(std::size_t village) const {
  return records_.at(village);
}

const std::vector<VillageRecord>& Villages::records() const { return records_; }

const std::vector<std::optional<std::size_t>>& Villages::membership() const {
  return village_of_;
}

const std::optional<std::size_t>& Villages::village_of(
    std::size_t structure) const {
  return village_of_.at(structure);
}

bool Villages::houses(std::size_t army) const {
  return std::any_of(records_.begin(), records_.end(),
                     [army](const VillageRecord& village) {
                       return village.spec.residents == army;
                     });
}

std::size_t Villages::add(VillageSpec spec, std::size_t armies,
                          std::size_t roster, const Stages& stages) {
  const auto claimed = [this](std::size_t structure) {
    return structure >= village_of_.size() ||
           village_of_.at(structure).has_value();
  };
  const auto unmatched = [&spec](std::size_t count) {
    return count != 0 && count != spec.structures.size();
  };
  const bool unheld =
      unmatched(spec.holdings.size()) || unmatched(spec.kinds.size());
  if (spec.residents >= armies ||
      std::any_of(spec.structures.begin(), spec.structures.end(), claimed) ||
      houses(spec.residents) || unheld ||
      std::size_t{2} * spec.households > roster) {
    throw std::invalid_argument(
        "a village must name existing structures no other village names, "
        "an existing army no other village houses as its residents, what "
        "each of its structures holds and its kind, if anything, and no more "
        "households than its residents pair into");
  }
  const std::size_t village = records_.size();
  for (const std::size_t structure : spec.structures) {
    village_of_.at(structure) = village;
  }
  records_.push_back(VillageRecord{std::move(spec), std::nullopt, 0,
                                   std::nullopt, std::nullopt, std::nullopt});
  // A village starts with its storage full.
  records_.back().stock = held(village, Stage::DAMAGED, stages).storage;
  return village;
}

std::size_t Villages::found(std::size_t residents, Cell corner) {
  records_.push_back(VillageRecord{VillageSpec{{}, residents, {}, 0, {}, {}},
                                   std::nullopt, 0, std::nullopt, std::nullopt,
                                   corner});
  return records_.size() - 1;
}

void Villages::place(std::size_t village, Cell centre) {
  records_.at(village).centre = centre;
}

void Villages::reach(Reached reached) {
  std::size_t& tier = records_.at(reached.village).tier;
  tier = std::max(tier, reached.tier);
}

void Villages::add_structure() { village_of_.emplace_back(); }

void Villages::attach(std::size_t village, std::size_t structure,
                      Holding holding, std::string kind) {
  VillageSpec& spec = records_.at(village).spec;
  spec.holdings.resize(spec.structures.size());
  spec.kinds.resize(spec.structures.size());
  village_of_.at(structure) = village;
  spec.structures.push_back(structure);
  spec.holdings.push_back(holding);
  spec.kinds.push_back(std::move(kind));
}

void Villages::send(std::size_t village, Raid raid) {
  VillageRecord& raiding = records_.at(village);
  raiding.party = raid.party;
  raiding.raiding = raid.target;
}

void Villages::end_raid(std::size_t village) {
  records_.at(village).raiding.reset();
}

void Villages::release(std::size_t village) {
  VillageRecord& raiding = records_.at(village);
  raiding.party.reset();
  raiding.raiding.reset();
}

Holding Villages::held(std::size_t village, Stage worst,
                       const Stages& stages) const {
  const VillageSpec& spec = records_.at(village).spec;
  Holding total;
  for (std::size_t index = 0; index < spec.holdings.size(); ++index) {
    const Holding& holding = spec.holdings.at(index);
    const Stage now = stages.of(spec.structures.at(index));
    if (now <= worst) {
      total.housing += holding.housing;
      total.storage += holding.storage;
      total.pupils += holding.pupils;
      total.arms += holding.arms;
      total.tends += holding.tends;
      total.edge = std::max(total.edge, holding.edge);
      total.healing = std::max(total.healing, holding.healing);
    }
    total.harvest += now == Stage::INTACT ? holding.harvest : 0;
  }
  return total;
}

bool Villages::all_in(std::size_t village, Stage stage,
                      const Stages& stages) const {
  const std::vector<std::size_t>& built = records_.at(village).spec.structures;
  return std::all_of(built.begin(), built.end(),
                     [&stages, stage](std::size_t structure) {
                       return stages.of(structure) == stage;
                     });
}

std::size_t Villages::cap(std::size_t village, const Stages& stages,
                          const std::optional<Husbandry>& husbandry,
                          std::size_t capacity) const {
  if (!husbandry) {
    return capacity;
  }
  const Holding standing = held(village, Stage::DAMAGED, stages);
  const std::uint64_t yearly = std::max<std::uint64_t>(
      std::uint64_t{husbandry->consumption} * DAYS_PER_YEAR, 1);
  return static_cast<std::size_t>(
      std::min({std::uint64_t{standing.housing}, standing.harvest / yearly,
                standing.storage / yearly}));
}

Service Villages::served(const std::vector<std::size_t>& population,
                         std::uint64_t reliance, const Stages& stages) const {
  Service service{std::vector<std::uint32_t>(records_.size(), 0),
                  std::vector<std::uint32_t>(records_.size(), 0)};
  for (std::size_t village = 0; village < records_.size(); ++village) {
    const Holding standing = held(village, Stage::DAMAGED, stages);
    const std::uint64_t people = population.at(village);
    if (reliance + standing.arms >= people) {
      service.edge.at(village) = standing.edge;
    }
    if (reliance + standing.tends >= people) {
      service.healing.at(village) = standing.healing;
    }
  }
  return service;
}

std::vector<bool> Villages::feed(const std::vector<std::size_t>& population,
                                 std::uint32_t consumption, bool harvest,
                                 const Stages& stages) {
  std::vector<bool> fed(records_.size(), false);
  for (std::size_t village = 0; village < records_.size(); ++village) {
    const Holding holds = held(village, Stage::DAMAGED, stages);
    VillageRecord& feeding = records_.at(village);
    feeding.stock = std::min<std::uint64_t>(
        feeding.stock + (harvest ? holds.harvest : 0), holds.storage);
    const std::uint64_t need =
        std::uint64_t{consumption} * population.at(village);
    fed.at(village) = feeding.stock >= need;
    feeding.stock -= fed.at(village) ? need : feeding.stock;
  }
  return fed;
}

bool Villages::shelters(std::size_t village, std::size_t people,
                        const Stages& stages,
                        const std::optional<Husbandry>& husbandry,
                        std::size_t capacity) const {
  return people < cap(village, stages, husbandry, capacity) &&
         records_.at(village).stock > 0;
}

std::optional<FieldState> Villages::field_state(std::size_t structure,
                                                const Stages& stages,
                                                std::uint64_t season) const {
  const std::optional<std::size_t>& village = village_of_.at(structure);
  if (!village) {
    return std::nullopt;
  }
  const VillageSpec& spec = records_.at(*village).spec;
  const auto place = static_cast<std::size_t>(
      std::find(spec.structures.begin(), spec.structures.end(), structure) -
      spec.structures.begin());
  if (place >= spec.holdings.size() || spec.holdings.at(place).harvest == 0) {
    return std::nullopt;
  }
  if (stages.of(structure) != Stage::INTACT) {
    return FieldState::BURNT;
  }
  return static_cast<FieldState>(season);
}

VillageChange Villages::shifted(std::size_t village, bool stood, bool raised,
                                bool peopled, Destroyer striker,
                                const Stages& stages) {
  VillageRecord& struck = records_.at(village);
  if (stood && all_in(village, Stage::RUBBLE, stages)) {
    struck.destroyer = striker;
    return VillageChange::RUINED;
  }
  if (raised && !peopled) {
    struck.spec.residents = striker.army;
    return VillageChange::RETAKEN;
  }
  return VillageChange::NONE;
}

bool Villages::kept(const std::vector<Structure>& structures,
                    std::size_t armies, std::size_t factions) const {
  const std::size_t villages = records_.size();
  const std::size_t built = structures.size();
  const auto stray = [&](const VillageRecord& village) {
    const VillageSpec& spec = village.spec;
    const auto missing = [built](std::size_t structure) {
      return structure >= built;
    };
    return std::any_of(spec.structures.begin(), spec.structures.end(),
                       missing) ||
           spec.residents >= armies ||
           (!spec.holdings.empty() &&
            spec.holdings.size() != spec.structures.size()) ||
           (village.destroyer && (village.destroyer->army >= armies ||
                                  village.destroyer->faction >= factions)) ||
           !below(village.party, armies) || !below(village.raiding, villages);
  };
  const auto unvillaged = [villages](std::optional<std::size_t> village) {
    return !below(village, villages);
  };
  return village_of_.size() == built &&
         std::none_of(village_of_.begin(), village_of_.end(), unvillaged) &&
         std::none_of(records_.begin(), records_.end(), stray);
}

std::vector<std::optional<std::size_t>> claimed(
    Footprint size, std::int32_t claim,
    const std::vector<std::pair<Cell, std::size_t>>& centres) {
  std::vector<std::optional<std::size_t>> held(
      static_cast<std::size_t>(size.width) *
      static_cast<std::size_t>(size.height));
  std::vector<std::int64_t> nearest(held.size(),
                                    std::numeric_limits<std::int64_t>::max());
  for (const auto& [centre, owner] : centres) {
    for (Cell cell{0, std::max(centre.y - claim, 0)};
         cell.y <= std::min(centre.y + claim, size.height - 1); ++cell.y) {
      for (cell.x = std::max(centre.x - claim, 0);
           cell.x <= std::min(centre.x + claim, size.width - 1); ++cell.x) {
        const std::size_t slot = (static_cast<std::size_t>(cell.y) *
                                  static_cast<std::size_t>(size.width)) +
                                 static_cast<std::size_t>(cell.x);
        const std::int64_t offset_x = cell.x - centre.x;
        const std::int64_t offset_y = cell.y - centre.y;
        const std::int64_t distance =
            (offset_x * offset_x) + (offset_y * offset_y);
        if (distance < nearest.at(slot)) {
          nearest.at(slot) = distance;
          held.at(slot) = owner;
        }
      }
    }
  }
  return held;
}

}  // namespace murim
