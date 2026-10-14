#include "leadership.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "limits.hpp"
#include "person.hpp"

namespace murim {

Leaders::Leaders(std::vector<Leadership> leadership, std::size_t factions,
                 const std::vector<Person>& people)
    : factions_(std::move(leadership)) {
  const auto unknown = [&people](const Leadership& faction) {
    return faction.leader && *faction.leader >= people.size();
  };
  if (factions_.size() > factions ||
      std::any_of(factions_.begin(), factions_.end(), unknown)) {
    throw std::invalid_argument(
        "leaders must be people the world creates, of no more factions than "
        "hostility names");
  }
  factions_.resize(factions);
}

Leaders::Leaders(std::vector<Leadership> factions, std::vector<Fight> duels)
    : factions_(std::move(factions)), duels_(std::move(duels)) {}

const std::vector<Leadership>& Leaders::factions() const { return factions_; }

const std::vector<Fight>& Leaders::duels() const { return duels_; }

const Leadership& Leaders::of(std::size_t faction) const {
  return factions_.at(faction);
}

bool Leaders::leads(std::size_t person) const {
  return std::any_of(
      factions_.begin(), factions_.end(),
      [person](const Leadership& faction) { return faction.leader == person; });
}

ChallengeRefusal Leaders::refusal(Challenging challenging,
                                  std::size_t faction) const {
  const Leadership none{};
  const Leadership& led =
      faction < factions_.size() ? factions_.at(faction) : none;
  if (!led.succession) {
    return ChallengeRefusal::NO_SUCCESSION;
  }
  if (!led.leader) {
    return ChallengeRefusal::NO_LEADER;
  }
  if (!challenging.lead) {
    return ChallengeRefusal::NO_CHALLENGER;
  }
  if (led.succession->members_only && challenging.faction != faction) {
    return ChallengeRefusal::NOT_A_MEMBER;
  }
  if (*challenging.lead == *led.leader) {
    return ChallengeRefusal::IS_LEADER;
  }
  if (duel(faction)) {
    return ChallengeRefusal::DUEL_IN_PROGRESS;
  }
  return ChallengeRefusal::NONE;
}

void Leaders::challenge(Challenging challenging, std::size_t faction) {
  if (refusal(challenging, faction) != ChallengeRefusal::NONE) {
    throw std::invalid_argument(
        "a challenge must name a faction that passes its lead by duel and has "
        "a leader, from an army whose lead may challenge it, outside a duel");
  }
  const Leadership& led = factions_.at(faction);
  duels_.push_back(Fight{faction, challenging.lead.value_or(0),
                         led.leader.value_or(0),
                         led.succession.value_or(Duel{})});
}

std::optional<Duellists> Leaders::duel(std::size_t faction) const {
  const auto fought = [faction](const Fight& fight) {
    return fight.faction == faction;
  };
  const auto found = std::find_if(duels_.begin(), duels_.end(), fought);
  if (found == duels_.end()) {
    return std::nullopt;
  }
  return Duellists{found->challenger, found->leader};
}

bool Leaders::duelling(std::size_t one, std::size_t other) const {
  return std::any_of(
      duels_.begin(), duels_.end(), [one, other](const Fight& fight) {
        return (fight.challenger == one && fight.leader == other) ||
               (fight.challenger == other && fight.leader == one);
      });
}

bool Leaders::in_duel(std::size_t person) const {
  return std::any_of(
      duels_.begin(), duels_.end(), [person](const Fight& fight) {
        return fight.challenger == person || fight.leader == person;
      });
}

std::vector<Won> Leaders::settle(const std::vector<Person>& people) {
  const auto down = [&people](const Fight& fight, std::size_t duellist) {
    return people.at(duellist).health <= fight.rule.yield_health;
  };
  const auto fallen = [&down](const Fight& fight) {
    return down(fight, fight.challenger) || down(fight, fight.leader);
  };
  std::vector<Won> won;
  for (const Fight& fight : duels_) {
    if (down(fight, fight.leader) && !down(fight, fight.challenger)) {
      factions_.at(fight.faction).leader = fight.challenger;
      won.push_back(
          Won{fight.faction, fight.challenger, fight.rule.allies_turn_hostile});
    }
  }
  duels_.erase(std::remove_if(duels_.begin(), duels_.end(), fallen),
               duels_.end());
  return won;
}

bool Leaders::elects_on(std::size_t faction, const std::vector<Person>& people,
                        std::uint64_t day) const {
  const Leadership& lead = factions_.at(faction);
  if (!lead.election) {
    return false;
  }
  const bool vacant = !lead.leader || people.at(*lead.leader).health == 0;
  const std::uint32_t term = lead.election->term;
  return vacant || (term > 0 && day % term == 0);
}

bool Leaders::lead(std::size_t faction, std::optional<std::size_t> leader) {
  std::optional<std::size_t>& leading = factions_.at(faction).leader;
  if (leading == leader) {
    return false;
  }
  leading = leader;
  return true;
}

bool Leaders::kept(std::size_t factions,
                   const std::vector<Person>& people) const {
  const std::size_t known = people.size();
  const auto bad_faction = [known](const Leadership& faction) {
    const Admission admission = faction.admission.value_or(Admission{});
    return (faction.leader && *faction.leader >= known) ||
           admission.chance > PER_MILLE || !reachable(admission.reach) ||
           admission.elder_share > PER_MILLE;
  };
  const auto stray_duel = [factions, known](const Fight& fight) {
    return fight.faction >= factions || fight.challenger >= known ||
           fight.leader >= known;
  };
  return factions_.size() == factions &&
         std::none_of(factions_.begin(), factions_.end(), bad_faction) &&
         std::none_of(duels_.begin(), duels_.end(), stray_duel);
}

}  // namespace murim
