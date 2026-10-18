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
/// changes no step. A field is built when first asked for, and held while a
/// person bound for its objective stands in its area or in one of the eight a
/// step can cross into it from; one no such person stood near in a tick
/// returns its buffers to a pool, so a tick asking for the fields of the last
/// allocates nothing.
class AreaFields {
 public:
  /// Returns the index of the field a person standing on `cell` steps by
  /// toward `objective`, building it if no field of that key is held.
  std::size_t field_for(const Map& map, const CoarseGraph& graph, Cell cell,
                        Cell objective);

  /// Returns the field at index `index`, which field_for gave this tick.
  [[nodiscard]] const FlowField& field(std::size_t index) const;

  /// Ends a tick: every field no person bound for its objective stood in or
  /// beside returns to the pool, and the search toward an objective is
  /// dropped once no field toward it is held and no build read it in the
  /// tick.
  void end_tick();

  /// Drops what blocking `cells` on the map, and the graph's rebuild for
  /// them, can change, `map` and `graph` being the new ones: every search,
  /// the walks within an area holding one of the cells, and each held field
  /// whose area or margin holds one or whose seeds, sown again, differ from
  /// those it was built from. A field is a function of its window's cells,
  /// its seeds and its objective, so each field kept is the one a fresh build
  /// gives.
  void changed(const Map& map, const CoarseGraph& graph,
               const std::vector<Cell>& cells);

  /// How many fields are held, how many were built since the last
  /// end_tick(), and how many were built in all.
  [[nodiscard]] std::size_t held() const;
  [[nodiscard]] std::size_t built() const;
  [[nodiscard]] std::size_t built_in_all() const;

  /// Returns the held searches toward objectives, by objective.
  [[nodiscard]] std::vector<const GoalSearch*> searches() const;

  /// Returns how many bytes the walks within areas that seed the fields
  /// hold.
  [[nodiscard]] std::size_t walk_bytes() const;

 private:
  using Key = std::tuple<std::size_t, std::int32_t, std::int32_t>;
  using Objective = std::pair<std::int32_t, std::int32_t>;
  struct Search {
    GoalSearch search;
    bool read = false;
    // The walks within the objective's area from the objective.
    std::vector<CoarseCost> near;
  };
  // How a seed was sown: its walk over the graph to the objective, and
  // whether its line to the objective is walked at the octile cost.
  struct Sowing {
    CoarseCost through;
    bool line;
  };
  // The seeds a field was built from, and how each was sown.
  struct Sown {
    std::vector<FieldSeed> seeds;
    std::vector<Sowing> sowing;
  };
  // The cells a field is built over: its area and a cell's margin round it,
  // on the map.
  struct Window {
    Cell corner;
    Footprint size;
  };
  // An area and a cell in it, a walk within the area starts from.
  using Start = std::tuple<std::size_t, std::int32_t, std::int32_t>;

  [[nodiscard]] const std::vector<CoarseCost>& walks(const Map& map,
                                                     const CoarseGraph& graph,
                                                     NodeRef node);
  [[nodiscard]] const std::vector<FieldSeed>& seeds(const Map& map,
                                                    const CoarseGraph& graph,
                                                    std::size_t area,
                                                    Cell objective);
  [[nodiscard]] static Window window_of(const Map& map, std::size_t columns,
                                        std::size_t area);
  // Whether the field sown as `sown` toward `objective` is sown the same
  // after `cells` were blocked, the graph having changed only within
  // `rebuilt`, without sowing it again.
  [[nodiscard]] static bool unchanged(const Map& map, const Sown& sown,
                                      Cell objective,
                                      const std::vector<Cell>& cells,
                                      const std::vector<Window>& rebuilt);
  // Whether a person bound for the key's objective stood this tick in its
  // area or one of the eight around it.
  [[nodiscard]] bool stood_near(const Key& key) const;
  // Seeds the far side of one exit toward the objective into the scratch list.
  void sow(const Map& map, const CoarseGraph& graph, const CoarseExit& exit,
           Cell objective, Search& held);

  std::vector<FlowField> pool_;
  std::vector<Sown> sown_;
  std::vector<std::size_t> free_;
  // The fields held, by key, and the keys asked for this tick, each the
  // area a person bound for the objective stands in.
  std::map<Key, std::size_t> held_;
  std::vector<Key> standing_;
  std::map<Objective, Search> searches_;
  std::vector<FieldSeed> scratch_;
  std::vector<Sowing> sowing_;
  // The walks within an area from each node, by area and the node's cell,
  // kept while a held field's area is the area or borders it, and the
  // graph's columns of areas, to tell which do.
  std::map<Start, std::vector<CoarseCost>> walks_;
  std::size_t columns_ = 0;
  std::size_t rows_ = 0;
  std::size_t built_ = 0;
  std::size_t built_in_all_ = 0;
};

}  // namespace murim
