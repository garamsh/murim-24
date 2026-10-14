#include "progress.hpp"

#include <cstddef>
#include <variant>
#include <vector>

#include "arts.hpp"

namespace murim {

bool names_held(const Advance& what, std::size_t people,
                const std::vector<Art>& arts,
                const std::vector<Realm>& realms) {
  if (const auto* learned = std::get_if<Learned>(&what)) {
    return learned->person < people && learned->art < arts.size() &&
           learned->skill < arts.at(learned->art).skills.size();
  }
  const auto& gained = std::get<RealmGained>(what);
  return gained.person < people && gained.rank != 0 &&
         gained.rank <= realms.size();
}

}  // namespace murim
