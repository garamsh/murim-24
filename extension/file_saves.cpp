#include "file_saves.hpp"

#include <cstddef>
#include <cstdint>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <optional>
#include <utility>
#include <vector>

#include "save.hpp"

namespace murim_godot {

FileSaves::FileSaves(godot::String directory)
    : directory_(std::move(directory)) {}

godot::String FileSaves::path_of(murim::Slot slot) const {
  return directory_ + godot::String("/slot_") +
         godot::String::num_uint64(static_cast<std::uint32_t>(slot)) + ".sav";
}

bool FileSaves::put(murim::Slot slot, const std::vector<std::uint8_t>& bytes) {
  if (godot::DirAccess::make_dir_recursive_absolute(directory_) != godot::OK) {
    return false;
  }
  const godot::Ref<godot::FileAccess> file =
      godot::FileAccess::open(path_of(slot), godot::FileAccess::WRITE);
  if (file.is_null()) {
    return false;
  }
  godot::PackedByteArray buffer;
  buffer.resize(static_cast<std::int64_t>(bytes.size()));
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    buffer.set(static_cast<std::int64_t>(index), bytes.at(index));
  }
  return file->store_buffer(buffer) && file->get_error() == godot::OK;
}

std::optional<std::vector<std::uint8_t>> FileSaves::get(
    murim::Slot slot) const {
  const godot::String path = path_of(slot);
  if (!godot::FileAccess::file_exists(path)) {
    return std::nullopt;
  }
  const godot::PackedByteArray buffer =
      godot::FileAccess::get_file_as_bytes(path);
  std::vector<std::uint8_t> bytes;
  bytes.reserve(static_cast<std::size_t>(buffer.size()));
  for (std::int64_t index = 0; index < buffer.size(); ++index) {
    bytes.push_back(static_cast<std::uint8_t>(buffer.get(index)));
  }
  return bytes;
}

}  // namespace murim_godot
