#include "cultivation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "arts.hpp"
#include "life.hpp"
#include "limits.hpp"
#include "person.hpp"

namespace murim {

std::uint32_t rank_of(const Person& person, const Life& life) {
  return static_cast<std::uint32_t>(std::count_if(
      life.realms.begin(), life.realms.end(), [&person](const Realm& realm) {
        return person.qi_capacity >= realm.qi;
      }));
}

std::optional<Nature> nature_of(const Person& person, const Life& life) {
  return person.known.empty()
             ? std::nullopt
             : std::optional<Nature>{
                   life.arts.at(person.known.front().art).nature};
}

std::optional<std::size_t> heart_of(const Person& person) {
  return person.known.empty()
             ? std::nullopt
             : std::optional<std::size_t>{person.known.front().art};
}

bool within_realm(const Person& person, std::size_t art, std::size_t skill,
                  const Life& life) {
  return life.arts.at(art).skills.at(skill).rank <= rank_of(person, life);
}

std::uint64_t agreement(const Person& person, std::size_t art, bool potency,
                        const Life& life) {
  if (person.known.empty()) {
    return PER_MILLE;
  }
  const Nature& heart = life.arts.at(person.known.front().art).nature;
  const Nature& nature = life.arts.at(art).nature;
  const Learning& learning = life.learning;
  std::uint64_t agreed = PER_MILLE;
  if (heart.orientation != nature.orientation) {
    agreed =
        agreed *
        (potency ? learning.orientation_potency : learning.orientation_ease) /
        PER_MILLE;
  }
  if (heart.polarity != nature.polarity) {
    agreed = agreed *
             (potency ? learning.polarity_potency : learning.polarity_ease) /
             PER_MILLE;
  }
  return agreed;
}

std::optional<std::size_t> lesson(const Person& person, std::size_t art,
                                  Lesson how, std::uint32_t rank,
                                  const Life& life) {
  const std::vector<ArtSkill>& skills = life.arts.at(art).skills;
  for (std::size_t skill = 0; skill < skills.size(); ++skill) {
    const bool known = std::any_of(
        person.known.begin(), person.known.end(), [&](const Known& held) {
          return held.art == art && held.skill == skill;
        });
    if (!known && skills.at(skill).rank <= rank &&
        (how == Lesson::TAUGHT || skills.at(skill).comprehensible)) {
      return skill;
    }
  }
  return std::nullopt;
}

void cultivate(Person& person) {
  person.qi_capacity =
      std::max(person.qi_capacity,
               std::min(person.cultivation.limit,
                        person.qi_capacity + person.cultivation.growth));
}

void recover(Person& person, const Cultivation& cultivation) {
  const std::uint64_t regained =
      ((std::uint64_t{person.qi_capacity} * cultivation.recovery) + PER_MILLE -
       1) /
      PER_MILLE;
  person.qi = static_cast<std::uint32_t>(
      std::min<std::uint64_t>(person.qi_capacity, person.qi + regained));
}

std::vector<std::size_t> practised(const Person& person) {
  std::vector<std::size_t> arts;
  for (const Known& known : person.known) {
    if (std::find(arts.begin(), arts.end(), known.art) == arts.end()) {
      arts.push_back(known.art);
    }
  }
  return arts;
}

std::uint64_t comprehension_chance(const Person& person, std::size_t art,
                                   const Life& life) {
  const std::size_t discipline = life.arts.at(art).discipline;
  const std::uint64_t mean =
      std::max<std::uint64_t>(life.disciplines.at(discipline).limit.mean, 1);
  const std::uint64_t talent =
      person.talent.empty() ? mean : person.talent.at(discipline).limit;
  return life.learning.comprehension * talent / mean *
         agreement(person, art, false, life) / PER_MILLE;
}

bool may_read(const Person& reader, std::size_t art, const Life& life) {
  const std::uint64_t ease = agreement(reader, art, false, life);
  return ease != 0 &&
         lesson(reader, art, Lesson::TAUGHT, rank_of(reader, life), life) &&
         std::uint64_t{reader.qi} * ease >=
             std::uint64_t{life.learning.reading_cost} * PER_MILLE;
}

std::uint32_t reading_cost(const Person& reader, std::size_t art,
                           const Life& life) {
  const std::uint64_t ease = agreement(reader, art, false, life);
  if (ease == 0) {
    throw std::domain_error("the art wholly disagrees with the reader");
  }
  return static_cast<std::uint32_t>(std::uint64_t{life.learning.reading_cost} *
                                    PER_MILLE / ease);
}

std::uint64_t teaching_chance(const Person& pupil, std::size_t art,
                              std::uint32_t teacher, const Life& life) {
  return std::uint64_t{life.learning.teaching} * teacher *
         agreement(pupil, art, false, life) / PER_MILLE;
}

}  // namespace murim
