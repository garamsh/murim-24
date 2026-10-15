#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <tuple>
#include <vector>

#include "coarse_graph.hpp"
#include "flow_field.hpp"
#include "map.hpp"

namespace murim {

/// The fields people step by in live and stepped areas (ADR 0043): one for
/// each area and objective, over the area and a cell's margin, seeded on the
/// objective when it lies in the area and on the far side of every way out of
/// the area, each such cell at its exit's coarse cost to the objective and the
/// octile cost to that exit's node. Each component of the area is reached
/// only from its own exits, the objective in it, or margin cells joined to
/// them, all of them walks, so the field needs no person's component. A field
/// is a function of the map, the graph and its key alone, so what is cached
/// changes no step. Fields are built when first asked for in a tick and kept
/// while asked for every tick; a field asked for by no one in a tick returns
/// its buffers to a pool, so a tick asking for the fields of the last allocates
/// nothing.
class AreaFields {
 public:
  /// Returns the index of the field a person standing on `cell` steps by
  /// toward `objective`, building it if no field of that key is held.
  std::size_t field_for(const Map& map, const CoarseGraph& graph, Cell cell,
                        Cell objective);

  /// Returns the field at index `index`, which field_for gave this tick.
  [[nodiscard]] const FlowField& field(std::size_t index) const;

  /// Ends a tick: every field no one asked for in it returns to the pool, and
  /// the searches toward objectives no build read are dropped.
  void end_tick();

  /// Drops every field and cost, as after the map or the graph changes.
  void clear();

  /// How many fields are held, and how many were built since the last
  /// end_tick().
  [[nodiscard]] std::size_t held() const;
  [[nodiscard]] std::size_t built() const;

 private:
  using Key = std::tuple<std::size_t, std::int32_t, std::int32_t>;
  using Objective = std::pair<std::int32_t, std::int32_t>;
  struct Held {
    std::size_t slot;
    bool asked;
  };
  struct Search {
    GoalSearch search;
    bool read = false;
  };

  [[nodiscard]] std::vector<FieldSeed> seeds(const Map& map,
                                             const CoarseGraph& graph,
                                             std::size_t area, Cell objective);

  std::vector<FlowField> pool_;
  std::vector<std::size_t> free_;
  std::map<Key, Held> held_;
  std::map<Objective, Search> searches_;
  std::vector<FieldSeed> scratch_;
  std::size_t built_ = 0;
};

}  // namespace murim
