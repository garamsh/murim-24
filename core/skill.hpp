#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include "map.hpp"

namespace murim {

/// A cast's target: a point, in sub-cell units.
struct Point {
  Position position;
};

/// A cast's target: a person, by index.
struct AtPerson {
  std::size_t person = 0;
};

/// A cast's target: the caster itself.
struct Itself {};

/// What a cast is aimed at.
using Target = std::variant<Point, AtPerson, Itself>;

/// A use of one skill, by index among the caster's skills, its army's
/// followed by those it knows, at a target, made on the next tick in place of
/// the automatic strike.
struct Cast {
  std::size_t skill = 0;
  Target target;
};

/// The ground a skill covers, aimed toward a point: the nearest living
/// hostile body within its range when used on its own, or the target of a
/// cast.
enum class Shape : std::uint8_t {
  /// The disc of `radius` around the point, brought within `range`.
  AREA,
  /// A sweep: every point within `range` of the user ahead of the way to the
  /// point, within ARC_HALF_WIDTH_DEGREES either side, or all around when the
  /// point is the user's own.
  ARC,
  /// Every point within `radius` of the line `range` long from the user
  /// toward the point.
  LINE,
  /// The disc of `radius` around the user, wherever it is aimed.
  SELF,
  /// The one person aimed at, within `range`, and nothing else; a cast of it
  /// must name a person.
  TARGETED,
};

/// How far either side of the way it faces an ARC sweeps, in degrees: a
/// quarter turn, so the arc is the half-disc ahead.
constexpr std::int32_t ARC_HALF_WIDTH_DEGREES = 90;

/// An ability as data. A body uses it toward the nearest living hostile body
/// within `range`, or where it stands when its shape is SELF or `range` is 0;
/// every hostile body its `shape` covers takes `body_damage`, and every
/// structure whose footprint the shape reaches loses `structure_damage` of
/// durability. A negative `structure_damage` restores durability instead, up
/// to the intact stage's floor. A village with living residents takes damage
/// only from factions hostile to its owner, and repair only from the others.
/// The user waits `cooldown` ticks between uses, and spends `qi_cost` qi on
/// each.
struct Skill {
  std::int32_t range = 0;
  std::int32_t radius = 0;
  std::uint64_t body_damage = 0;
  std::int32_t structure_damage = 0;
  std::uint64_t cooldown = 0;
  std::uint32_t qi_cost = 0;
  Shape shape = Shape::AREA;
};

/// One use of a skill on the last tick, for a view to draw: person
/// `person` used its skill `skill`, of shape `shape`, `range` and
/// `radius`, from `origin` toward `toward`, its shape reaching as far as
/// `end`, all in sub-cell units: a LINE's far end, and for any other shape
/// `toward`; the skill deals `body_damage` before its user's edges, none for
/// a skill that strikes only structures, and the use hit the people
/// `struck`.
struct Strike {
  std::size_t person = 0;
  std::size_t skill = 0;
  Shape shape = Shape::AREA;
  std::int32_t range = 0;
  std::int32_t radius = 0;
  std::uint64_t body_damage = 0;
  Position origin{};
  Position toward{};
  Position end{};
  std::vector<std::size_t> struck;
};

/// Why a person may not cast a skill at a target, the first that holds in
/// this order; `NONE` when it may.
enum class CastRefusal : std::uint8_t {
  NONE,
  /// The caster is dead.
  DEAD,
  /// The caster's army has no skill of that index.
  NO_SKILL,
  /// The caster may not use the skill again on the next tick.
  ON_COOLDOWN,
  /// The caster holds less qi than the skill costs.
  NO_QI,
  /// The target is a person the world lacks or a dead one, or not a person
  /// for a targeted skill.
  NO_TARGET,
  /// The target is a person farther from the caster than the skill's range.
  OUT_OF_RANGE,
};

/// Why an army may not take a person as its attack intent's target, the first
/// that holds in this order; `NONE` when it may.
enum class AttackRefusal : std::uint8_t {
  NONE,
  /// The army has no living member.
  NO_MEMBER,
  /// The person is one the world lacks, or dead.
  NO_TARGET,
  /// The person is no foe of the army's lead: an ally, a neutral or one of
  /// its own.
  NOT_A_FOE,
  /// No member of the army has the person within the rules' sight.
  OUT_OF_SIGHT,
};

/// Whether a skill lies outside the limits a world accepts.
[[nodiscard]] bool invalid_skill(const Skill& skill);

}  // namespace murim
