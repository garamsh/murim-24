#include "event.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <variant>
#include <vector>

#include "map.hpp"
#include "person.hpp"

namespace murim {

bool names_held(const Happening& what, const Records& held) {
  if (const auto* founded = std::get_if<Founded>(&what)) {
    return founded->village < held.villages && founded->faction < held.factions;
  }
  if (const auto* raided = std::get_if<Raided>(&what)) {
    return raided->village < held.villages && raided->owner < held.factions &&
           raided->raider < held.factions;
  }
  if (const auto* ruined = std::get_if<Ruined>(&what)) {
    return ruined->village < held.villages && ruined->owner < held.factions &&
           ruined->destroyer.army < held.armies &&
           ruined->destroyer.faction < held.factions;
  }
  if (const auto* fell = std::get_if<SectFell>(&what)) {
    return fell->sect < held.sects && fell->faction < held.factions;
  }
  if (const auto* changed = std::get_if<LeaderChanged>(&what)) {
    return changed->faction < held.factions && changed->leader < held.people;
  }
  if (const auto* born = std::get_if<Born>(&what)) {
    return born->person < held.people;
  }
  const auto* died = std::get_if<Died>(&what);
  return died != nullptr && died->person < held.people;
}

bool related(const std::vector<Person>& people, std::size_t one,
             std::size_t other) {
  const Person& self = people.at(one);
  const auto among = [other](const std::vector<std::size_t>& kin) {
    return std::find(kin.begin(), kin.end(), other) != kin.end();
  };
  return one == other || self.family == people.at(other).family ||
         among(self.spouses) || among(self.parents) || among(self.children);
}

bool heard_by(const Event& event, const Listener& listener,
              const std::vector<Person>& people) {
  const Cell here = cell_of(people.at(listener.person).position);
  if (std::abs(event.cell.x - here.x) <= listener.reach &&
      std::abs(event.cell.y - here.y) <= listener.reach) {
    return true;
  }
  const auto ours = [&listener](std::size_t other) {
    return listener.faction == other;
  };
  if (const auto* born = std::get_if<Born>(&event.what)) {
    return related(people, listener.person, born->person);
  }
  if (const auto* died = std::get_if<Died>(&event.what)) {
    return related(people, listener.person, died->person);
  }
  if (const auto* founded = std::get_if<Founded>(&event.what)) {
    return ours(founded->faction);
  }
  if (const auto* raided = std::get_if<Raided>(&event.what)) {
    return ours(raided->owner) || ours(raided->raider);
  }
  if (const auto* ruined = std::get_if<Ruined>(&event.what)) {
    return ours(ruined->owner) || ours(ruined->destroyer.faction);
  }
  if (const auto* fell = std::get_if<SectFell>(&event.what)) {
    return ours(fell->faction);
  }
  const auto* changed = std::get_if<LeaderChanged>(&event.what);
  return changed != nullptr && ours(changed->faction);
}

std::vector<Event> rumours(const std::vector<Event>& events,
                           const Listener& listener,
                           const std::vector<Person>& people) {
  std::vector<Event> heard;
  for (auto event = events.rbegin(); event != events.rend(); ++event) {
    if (heard_by(*event, listener, people)) {
      heard.push_back(*event);
    }
  }
  return heard;
}

}  // namespace murim
