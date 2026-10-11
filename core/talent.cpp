#include "talent.hpp"

#include <algorithm>

namespace murim {

Range range_of(Spread values) {
  return Range{values.mean - std::min(values.mean, values.spread),
               values.mean + values.spread};
}

}  // namespace murim
