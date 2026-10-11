#include "skill.hpp"

#include "limits.hpp"

namespace murim {

bool invalid_skill(const Skill& skill) {
  return !reachable(skill.range) || !reachable(skill.radius) ||
         skill.body_damage > MAX_BODY_DAMAGE || skill.cooldown > MAX_COOLDOWN ||
         skill.qi_cost > MAX_TALENT;
}

}  // namespace murim
