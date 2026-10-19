#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "map.hpp"
#include "skill.hpp"
#include "talent.hpp"

namespace murim {

/// A person's sex, drawn at birth.
enum class Sex : std::uint8_t { FEMALE, MALE };

/// A person's temperament, drawn at birth (ADR 0071): it names the ratios of
/// threat to own strength above which the person backs off and flees.
enum class Temperament : std::uint8_t { BOLD, CAUTIOUS, COWARDLY };

/// What a person does about the danger it sees (ADR 0071): close on a foe,
/// hold its home ground, back off from the threat, flee to its refuge, or
/// regroup toward its group.
enum class Stance : std::uint8_t { CLOSE, HOLD, BACK_OFF, FLEE, REGROUP };

/// A skill a person knows: skill `skill` of art `art`, which it may use again
/// from tick `ready_at`.
struct Known {
  std::size_t art = 0;
  std::size_t skill = 0;
  std::uint64_t ready_at = 0;
};

/// A person of the world. A person keeps its identity, its index among the
/// world's people, for as long as the world runs, through every army it
/// serves in and after it dies; a dead person has no health and stands in no
/// army. It may use a skill again from tick `ready_at`, and returns to
/// village `home`, if it has one, when its army disbands. It was born on
/// tick `born`, before the world began for the people it starts with, into
/// `family`, of `parents` if it had any, of sex `sex` with given name `given`
/// among its sex's names, of temperament `temperament`, with `talent` and
/// `attainment` per discipline. It holds stance `stance` since tick
/// `stance_tick`, backing off from `away` while it backs off, and
/// lives `lifespan` days, extended by its attainment, if it ages at all. A
/// founder of a village, and the children settling with it, take the new
/// family it heads.
/// `spouses` are the people it married, in order, and `children` the people
/// born to it. It has gone `hunger` days unfed. A person carrying soul `soul`
/// is born again when it dies. It belongs to sect `sect`, if any, and keeps it
/// while it serves in an army of the sect's faction. It casts `casting` on
/// the next tick, if it was given a cast. It cultivates qi by `cultivation`,
/// a growth a day and a limit, holds `qi` of a store of `qi_capacity`, and
/// knows the art skills `known`, in the order it learned them, each with a
/// ready tick of its own; its army's skills share `ready_at`. It holds
/// `silver`, and `held` of each of the trade's wares, by index.
struct Person {
  Position position{};
  std::uint64_t health = 0;
  std::uint64_t ready_at = 0;
  std::optional<std::size_t> home;
  std::int64_t born = 0;
  std::size_t family = 0;
  std::vector<std::size_t> parents;
  Sex sex = Sex::FEMALE;
  Temperament temperament = Temperament::CAUTIOUS;
  Stance stance = Stance::CLOSE;
  std::uint64_t stance_tick = 0;
  Position away{};
  std::size_t given = 0;
  std::vector<std::size_t> spouses;
  std::vector<std::size_t> children;
  std::uint32_t hunger = 0;
  std::vector<Aptitude> talent;
  std::vector<std::uint32_t> attainment;
  std::optional<std::uint64_t> lifespan;
  std::optional<std::size_t> soul;
  std::optional<std::size_t> sect;
  std::optional<Cast> casting;
  Aptitude cultivation;
  std::uint32_t qi = 0;
  std::uint32_t qi_capacity = 0;
  std::vector<Known> known;
  std::uint64_t silver = 0;
  std::vector<std::uint64_t> held;
  std::optional<std::size_t> occupation;
};

}  // namespace murim
