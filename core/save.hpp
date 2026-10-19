#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "world.hpp"

namespace murim {

/// A place a game is saved in, numbered from 0.
enum class Slot : std::uint32_t {};

/// The save port: where saved games are kept, as bytes, by slot.
class SaveStore {
 public:
  SaveStore() = default;
  SaveStore(const SaveStore&) = default;
  SaveStore(SaveStore&&) = default;
  SaveStore& operator=(const SaveStore&) = default;
  SaveStore& operator=(SaveStore&&) = default;
  virtual ~SaveStore() = default;

  /// Keeps `bytes` in `slot`, replacing what it held. Returns whether they
  /// were kept.
  virtual bool put(Slot slot, const std::vector<std::uint8_t>& bytes) = 0;

  /// Returns the bytes `slot` holds, or nothing when it holds none.
  [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> get(
      Slot slot) const = 0;
};

/// How a world was made from a generated region: the region's seed and the
/// village the player's body was born into.
struct Origin {
  Seed region;
  std::size_t village;
};

/// What a save holds: a world's whole state and, for a world made from a
/// generated region, how it was made.
struct Saved {
  std::optional<Origin> origin;
  World world;
};

/// Why a save was refused.
enum class LoadError : std::uint8_t {
  /// The slot holds nothing.
  MISSING,
  /// The bytes do not begin as a save does.
  NOT_A_SAVE,
  /// The save was written in a format version other than this one.
  UNSUPPORTED_VERSION,
  /// The save ends before the length it gives.
  TRUNCATED,
  /// The save fails its checksum, runs past its length, or holds a state no
  /// world can be in.
  CORRUPTED,
};

/// The format version a save is written in. A change to what a save holds
/// raises it.
constexpr std::uint32_t SAVE_VERSION = 21;

/// Writes `saved` to `slot` of `store`. Returns whether the store kept it.
bool save(const Saved& saved, SaveStore& store, Slot slot);

/// Reads the save in `slot` of `store`: the world it holds, which hashes and
/// ticks on as the saved one did, or why it was refused.
[[nodiscard]] std::variant<Saved, LoadError> load(const SaveStore& store,
                                                  Slot slot);

/// Where parts of a save's payload end, each as the offset from the
/// payload's first byte to just past its last: the plan each structure was
/// laid from, the people engaging a foe from its attack slots, and each soul's
/// chart.
struct SaveParts {
  std::size_t plans_end = 0;
  std::size_t engaged_end = 0;
  std::size_t charts_end = 0;
};

/// Returns where the parts of the save `bytes` end, found by decoding it as
/// load() does, or why load() would refuse it. A test reads a field by the
/// end of its part through this, so a part the format later adds after it
/// leaves the test standing.
[[nodiscard]] std::variant<SaveParts, LoadError> save_parts(
    const std::vector<std::uint8_t>& bytes);

}  // namespace murim
