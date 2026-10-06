#pragma once

#include <cstdint>

namespace murim {

/// Seed a world is created from; the same seed gives the same world.
enum class Seed : std::uint64_t {};

/// Digest of a world's whole state; equal states give equal hashes.
enum class StateHash : std::uint64_t {};

/// The simulated world, advanced one deterministic step at a time.
class World {
 public:
  explicit World(Seed seed);

  /// Advances the world by one step.
  void tick();

  /// Returns the hash of the current state.
  [[nodiscard]] StateHash hash() const;

 private:
  std::uint64_t rng_state_;
  std::uint64_t tick_count_ = 0;
};

}  // namespace murim
