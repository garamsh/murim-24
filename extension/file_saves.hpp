#pragma once

#include <cstdint>
#include <godot_cpp/variant/string.hpp>
#include <optional>
#include <vector>

#include "save.hpp"

namespace murim_godot {

/// The save port served by files: slot `n` is `slot_<n>.sav` in a
/// directory, `user://saves` unless another is given.
class FileSaves : public murim::SaveStore {
 public:
  /// The directory the game keeps its saves in.
  static constexpr const char* DEFAULT_DIRECTORY = "user://saves";

  explicit FileSaves(godot::String directory = DEFAULT_DIRECTORY);

  bool put(murim::Slot slot, const std::vector<std::uint8_t>& bytes) override;

  [[nodiscard]] std::optional<std::vector<std::uint8_t>> get(
      murim::Slot slot) const override;

 private:
  godot::String directory_;

  [[nodiscard]] godot::String path_of(murim::Slot slot) const;
};

}  // namespace murim_godot
