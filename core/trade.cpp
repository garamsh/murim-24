#include "trade.hpp"

#include <algorithm>
#include <stdexcept>

#include "limits.hpp"

namespace murim {

void check_trade(const Trade& trade) {
  const auto stray_ware = [](const Ware& ware) {
    return ware.price == 0 || ware.price > MAX_TALENT ||
           ware.health > MAX_HEALTH || ware.qi > MAX_TALENT;
  };
  if (std::any_of(trade.wares.begin(), trade.wares.end(), stray_ware) ||
      trade.purse > MAX_TALENT || trade.wage > MAX_TALENT ||
      trade.wounded > PER_MILLE || trade.drained > PER_MILLE) {
    throw std::invalid_argument(
        "a ware must cost 1 to MAX_TALENT silver, restore health within "
        "MAX_HEALTH and qi within MAX_TALENT; a purse and wage lie within "
        "MAX_TALENT and the shares within PER_MILLE");
  }
}

}  // namespace murim
