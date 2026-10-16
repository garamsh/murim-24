#include "schedule.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "map.hpp"
#include "occupation.hpp"
#include "person.hpp"
#include "village.hpp"

namespace murim {
namespace {

// SplitMix64's constants (Steele, Lea and Flood, 2014).
constexpr std::uint64_t GOLDEN = 0x9E3779B97F4A7C15ULL;
constexpr std::uint64_t MIX_A = 0xBF58476D1CE4E5B9ULL;
constexpr std::uint64_t MIX_B = 0x94D049BB133111EBULL;
constexpr int SHIFT_A = 30;
constexpr int SHIFT_B = 27;
constexpr int SHIFT_C = 31;

constexpr std::size_t NONE = std::numeric_limits<std::size_t>::max();

std::uint64_t mix(std::uint64_t value) {
  value += GOLDEN;
  value = (value ^ (value >> SHIFT_A)) * MIX_A;
  value = (value ^ (value >> SHIFT_B)) * MIX_B;
  return value ^ (value >> SHIFT_C);
}

// Whether person `person` of `people` is unmarried or widowed.
bool unwed(const std::vector<Person>& people, std::size_t person) {
  const std::vector<std::size_t>& spouses = people.at(person).spouses;
  return spouses.empty() || people.at(spouses.back()).health == 0;
}

}  // namespace

std::uint32_t hour_of(std::uint64_t tick, std::uint64_t ticks_per_day) {
  return static_cast<std::uint32_t>((tick % ticks_per_day) * HOURS_PER_DAY /
                                    ticks_per_day);
}

std::uint64_t wander_ticks(std::uint64_t ticks_per_day) {
  return std::max<std::uint64_t>(ticks_per_day / HOURS_PER_DAY, 1);
}

std::size_t band_at(const std::vector<Band>& schedule, std::uint32_t hour) {
  std::size_t band = 0;
  while (band + 1 < schedule.size() && schedule.at(band + 1).from <= hour) {
    ++band;
  }
  return band;
}

bool awake(const Waking& waking, std::uint32_t hour) {
  return hour >= waking.from && hour < waking.to;
}

std::vector<std::vector<std::size_t>> households(
    const std::vector<std::size_t>& residents,
    const std::vector<Person>& people) {
  // Each resident's position in `residents`, and the position of the first
  // member of the household it joins.
  std::vector<std::size_t> position(people.size(), NONE);
  for (std::size_t index = 0; index < residents.size(); ++index) {
    position.at(residents.at(index)) = index;
  }
  const auto resident = [&position](std::size_t person) {
    return position.at(person) != NONE;
  };
  // The couple's first member, for a resident married to a living resident.
  const auto couple = [&](std::size_t person) -> std::optional<std::size_t> {
    const std::vector<std::size_t>& spouses = people.at(person).spouses;
    if (spouses.empty() || people.at(spouses.back()).health == 0 ||
        !resident(spouses.back())) {
      return std::nullopt;
    }
    return std::min(position.at(person), position.at(spouses.back()));
  };
  std::vector<std::size_t> head(residents.size());
  for (std::size_t index = 0; index < residents.size(); ++index) {
    const std::size_t person = residents.at(index);
    head.at(index) = couple(person).value_or(index);
    if (couple(person) || !unwed(people, person)) {
      continue;
    }
    for (const std::size_t parent : people.at(person).parents) {
      const std::optional<std::size_t> parents =
          resident(parent) ? couple(parent) : std::nullopt;
      if (parents) {
        head.at(index) = *parents;
        break;
      }
    }
  }
  // A household is ordered by its first member, which may be a child placed
  // before its parents.
  std::vector<std::size_t> first(residents.size(), NONE);
  for (std::size_t index = 0; index < residents.size(); ++index) {
    first.at(head.at(index)) = std::min(first.at(head.at(index)), index);
  }
  std::vector<std::size_t> heads;
  for (std::size_t index = 0; index < residents.size(); ++index) {
    if (first.at(index) != NONE) {
      heads.push_back(index);
    }
  }
  std::stable_sort(heads.begin(), heads.end(),
                   [&first](std::size_t one, std::size_t other) {
                     return first.at(one) < first.at(other);
                   });
  std::vector<std::size_t> rank(residents.size(), NONE);
  for (std::size_t order = 0; order < heads.size(); ++order) {
    rank.at(heads.at(order)) = order;
  }
  std::vector<std::vector<std::size_t>> found(heads.size());
  for (std::size_t index = 0; index < residents.size(); ++index) {
    found.at(rank.at(head.at(index))).push_back(residents.at(index));
  }
  return found;
}

std::vector<std::optional<std::size_t>> house(
    const std::vector<std::size_t>& sizes,
    const std::vector<std::uint32_t>& housing) {
  std::vector<std::size_t> left(housing.begin(), housing.end());
  std::vector<std::optional<std::size_t>> homes;
  homes.reserve(sizes.size());
  for (const std::size_t size : sizes) {
    const auto room =
        std::find_if(left.begin(), left.end(),
                     [size](std::size_t free) { return free >= size; });
    if (room == left.end()) {
      homes.emplace_back();
      continue;
    }
    *room -= size;
    homes.emplace_back(static_cast<std::size_t>(room - left.begin()));
  }
  return homes;
}

std::optional<Cell> door(const Map& map, const Structure& structure,
                         Cell centre) {
  const Cell corner = structure.cell;
  const Footprint size = structure.footprint;
  std::optional<Cell> nearest;
  std::int64_t best = 0;
  const auto consider = [&](Cell cell) {
    if (!map.passable(cell)) {
      return;
    }
    const std::int64_t off_x = std::int64_t{cell.x} - centre.x;
    const std::int64_t off_y = std::int64_t{cell.y} - centre.y;
    const std::int64_t distance = (off_x * off_x) + (off_y * off_y);
    const bool earlier =
        nearest &&
        (cell.y < nearest->y || (cell.y == nearest->y && cell.x < nearest->x));
    if (!nearest || distance < best || (distance == best && earlier)) {
      nearest = cell;
      best = distance;
    }
  };
  for (std::int32_t column = corner.x; column < corner.x + size.width;
       ++column) {
    consider(Cell{column, corner.y - 1});
    consider(Cell{column, corner.y + size.height});
  }
  for (std::int32_t row = corner.y; row < corner.y + size.height; ++row) {
    consider(Cell{corner.x - 1, row});
    consider(Cell{corner.x + size.width, row});
  }
  return nearest;
}

std::uint64_t errand_draw(std::uint64_t seed, std::size_t person,
                          std::uint64_t stream, std::uint64_t counter) {
  return mix(mix(mix(mix(seed) ^ person) ^ stream) ^ counter);
}

}  // namespace murim
