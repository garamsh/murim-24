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
constexpr std::uint32_t SAVE_VERSION = 4;

/// Writes `saved` to `slot` of `store`. Returns whether the store kept it.
bool save(const Saved& saved, SaveStore& store, Slot slot);

/// Reads the save in `slot` of `store`: the world it holds, which hashes and
/// ticks on as the saved one did, or why it was refused.
[[nodiscard]] std::variant<Saved, LoadError> load(const SaveStore& store,
                                                  Slot slot);

}  // namespace murim
