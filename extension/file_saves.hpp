#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "save.hpp"

namespace murim_godot {

/// The save port served by files: slot `n` is `user://saves/slot_<n>.sav`.
class FileSaves : public murim::SaveStore {
 public:
  bool put(murim::Slot slot, const std::vector<std::uint8_t>& bytes) override;

  [[nodiscard]] std::optional<std::vector<std::uint8_t>> get(
      murim::Slot slot) const override;
};

}  // namespace murim_godot
