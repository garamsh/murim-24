#pragma once

#include <cstdint>

namespace murim {

/// The world's calendar: days in a season, seasons in a year. How many ticks
/// make a day is the world's data (Rules).
struct Calendar {
  std::uint64_t days_per_season;
  std::uint64_t seasons_per_year;
};

/// Thirty days to a season, four seasons a year.
constexpr Calendar CALENDAR{30, 4};

/// Days in a year.
constexpr std::uint64_t DAYS_PER_YEAR =
    CALENDAR.days_per_season * CALENDAR.seasons_per_year;

/// A minute of play to a day.
constexpr std::uint64_t TICKS_PER_DAY = 1'800;

/// A day of the calendar, each part counted from 1 but `season`, from 0.
struct Date {
  std::uint64_t year;
  std::uint64_t season;
  std::uint64_t day;
};

}  // namespace murim
