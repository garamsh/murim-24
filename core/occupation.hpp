#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace murim {

/// Where an occupation's schedule sends a person in a band of hours
/// (ADR 0081): its home, its workplace, or its village's square.
enum class Where : std::uint8_t { HOME, WORK, SQUARE };

/// A band of a day's schedule: from hour `from`, 0 to 23, until the next
/// band's, a person is at `where`.
struct Band {
  std::uint32_t from = 0;
  Where where = Where::HOME;
};

/// One occupation, as data (ADR 0081's trade): its `name`; the structure kind
/// it works at, or none for the village's square; the openings each standing
/// structure of that kind gives, or, working on the square, one opening for
/// every `per_residents` people whose home the village is, rounded up; its
/// schedule by hour; and the wares, by index, it sells.
struct Occupation {
  std::string name;
  std::optional<std::string> workplace;
  std::uint32_t per_workplace = 0;
  std::uint32_t per_residents = 0;
  std::vector<Band> schedule;
  std::vector<std::size_t> wares;
};

/// The occupations people take, as data, one rule for every person: the
/// occupations in the order openings are filled, the age in years from which
/// a person takes one, the occupation, by index, of an adult left without an
/// opening, and the schedule of anyone with none.
struct Occupations {
  std::vector<Occupation> list;
  std::uint32_t working_age = 0;
  std::size_t fallback = 0;
  std::vector<Band> idle;
};

/// One person whose home is a village, as the day's assignment reads it:
/// whether it has reached the working age, and the occupation it holds.
struct Candidate {
  bool of_age = false;
  std::optional<std::size_t> occupation;
};

/// Returns the occupation each of a village's `candidates` holds after the
/// day's assignment, in their order, given the kinds of the village's
/// standing structures, one entry each. Each occupation opens its places;
/// a candidate of age keeps the occupation it holds while one of its places
/// is left, in the candidates' order; the places left are filled in the
/// occupations' order, then the candidates', by those of age holding none or
/// the fallback; and every candidate of age still holding none takes the
/// fallback. A candidate under age holds none.
[[nodiscard]] std::vector<std::optional<std::size_t>> assign(
    const Occupations& occupations, const std::vector<Candidate>& candidates,
    const std::vector<std::string>& standing);

/// Throws std::invalid_argument when `occupations` lists no occupation, its
/// fallback is none of them, an occupation sells a ware outside `wares`, or
/// a schedule is empty, does not begin at hour 0, or its hours are not
/// ascending below 24.
void check_occupations(const Occupations& occupations, std::size_t wares);

}  // namespace murim
