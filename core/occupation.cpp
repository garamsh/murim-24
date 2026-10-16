#include "occupation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace murim {
namespace {

constexpr std::uint32_t HOURS_PER_DAY = 24;

// The places each occupation opens in a village of `people` whose standing
// structures are of the kinds `standing`.
std::vector<std::size_t> openings(const Occupations& occupations,
                                  std::size_t people,
                                  const std::vector<std::string>& standing) {
  std::vector<std::size_t> open;
  open.reserve(occupations.list.size());
  for (const Occupation& occupation : occupations.list) {
    if (const auto* workplace =
            std::get_if<AtWorkplace>(&occupation.openings)) {
      const auto workplaces = static_cast<std::size_t>(
          std::count(standing.begin(), standing.end(), workplace->kind));
      open.push_back(workplaces * workplace->per);
    } else if (const auto* square =
                   std::get_if<OnSquare>(&occupation.openings)) {
      open.push_back((people + square->per_residents - 1) /
                     square->per_residents);
    } else {
      open.push_back(0);
    }
  }
  return open;
}

bool well_ordered(const std::vector<Band>& schedule) {
  if (schedule.empty() || schedule.front().from != 0) {
    return false;
  }
  for (std::size_t band = 1; band < schedule.size(); ++band) {
    if (schedule.at(band).from <= schedule.at(band - 1).from) {
      return false;
    }
  }
  return schedule.back().from < HOURS_PER_DAY;
}

}  // namespace

std::vector<std::optional<std::size_t>> assign(
    const Occupations& occupations, const std::vector<Candidate>& candidates,
    const std::vector<std::string>& standing) {
  std::vector<std::size_t> open =
      openings(occupations, candidates.size(), standing);
  std::vector<std::optional<std::size_t>> held(candidates.size());
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const Candidate& candidate = candidates.at(index);
    if (candidate.of_age && candidate.occupation &&
        *candidate.occupation != occupations.fallback &&
        open.at(*candidate.occupation) > 0) {
      held.at(index) = candidate.occupation;
      --open.at(*candidate.occupation);
    }
  }
  for (std::size_t occupation = 0; occupation < open.size(); ++occupation) {
    for (std::size_t index = 0;
         index < candidates.size() && open.at(occupation) > 0; ++index) {
      if (candidates.at(index).of_age && !held.at(index)) {
        held.at(index) = occupation;
        --open.at(occupation);
      }
    }
  }
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    if (candidates.at(index).of_age && !held.at(index)) {
      held.at(index) = occupations.fallback;
    }
  }
  return held;
}

void check_occupations(const Occupations& occupations, std::size_t wares) {
  const bool sells_unknown = std::any_of(
      occupations.list.begin(), occupations.list.end(),
      [wares](const Occupation& occupation) {
        return std::any_of(occupation.wares.begin(), occupation.wares.end(),
                           [wares](std::size_t ware) { return ware >= wares; });
      });
  const bool opens_nothing = std::any_of(
      occupations.list.begin(), occupations.list.end(),
      [](const Occupation& occupation) {
        const auto* workplace = std::get_if<AtWorkplace>(&occupation.openings);
        const auto* square = std::get_if<OnSquare>(&occupation.openings);
        return (workplace != nullptr &&
                (workplace->kind.empty() || workplace->per == 0)) ||
               (square != nullptr && square->per_residents == 0);
      });
  const bool schedules_ordered =
      well_ordered(occupations.idle) &&
      std::all_of(occupations.list.begin(), occupations.list.end(),
                  [](const Occupation& occupation) {
                    return well_ordered(occupation.schedule);
                  });
  if (occupations.list.empty() ||
      occupations.fallback >= occupations.list.size() || opens_nothing ||
      sells_unknown || !schedules_ordered) {
    throw std::invalid_argument(
        "occupations must list at least one occupation, a fallback among "
        "them, places at a named kind or on the square that open at least "
        "one, only wares there are, and schedules that begin at hour 0 with "
        "hours ascending below 24");
  }
}

}  // namespace murim
