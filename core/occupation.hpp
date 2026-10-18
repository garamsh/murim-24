#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace murim {

/// The hours of a day, by which a schedule's bands are given.
constexpr std::uint32_t HOURS_PER_DAY = 24;

/// Where an occupation's schedule sends a person in a band of hours
/// (ADR 0081): its home, its workplace, or its village's square.
enum class Where : std::uint8_t { HOME, WORK, SQUARE };

/// A band of a day's schedule: from hour `from`, 0 to 23, until the next
/// band's, a person is at `where`.
struct Band {
  std::uint32_t from = 0;
  Where where = Where::HOME;
};

/// An occupation that works at structures of `kind`: each standing one opens
/// `per` places.
struct AtWorkplace {
  std::string kind;
  std::uint32_t per = 0;
};

/// An occupation that works on its village's square: one place for every
/// `per_residents` people whose home the village is, rounded up.
struct OnSquare {
  std::uint32_t per_residents = 0;
};

/// An occupation that opens no place, held only as the fallback.
struct Nowhere {};

/// Where an occupation's places open, one of the three.
using Openings = std::variant<AtWorkplace, OnSquare, Nowhere>;

/// One occupation, as data (ADR 0081's trade): its `name`, where its places
/// open, its schedule by hour, and the wares, by index, it sells.
struct Occupation {
  std::string name;
  Openings openings;
  std::vector<Band> schedule;
  std::vector<std::size_t> wares;
};

/// The hours a person is awake, from hour `from` until hour `to`, 0 to 24.
struct Waking {
  std::uint32_t from = 0;
  std::uint32_t to = HOURS_PER_DAY;
};

/// The occupations people take, as data, one rule for every person: the
/// occupations in the order openings are filled, the age in years from which
/// a person takes one, the occupation, by index, of an adult left without an
/// opening, the schedule of anyone with none, and the hours people are awake.
struct Occupations {
  std::vector<Occupation> list;
  std::uint32_t working_age = 0;
  std::size_t fallback = 0;
  std::vector<Band> idle;
  Waking waking;
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
/// fallback is none of them, an occupation opens places at no kind or none
/// for each workplace or for no residents, sells a ware outside `wares`, a
/// schedule is empty, does not begin at hour 0, or its hours are not
/// ascending below 24, or its waking hours do not end after they begin, by 24.
void check_occupations(const Occupations& occupations, std::size_t wares);

}  // namespace murim
