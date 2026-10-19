#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace murim {

/// What a Reader throws where the bytes it reads run short or hold a value
/// no Writer gives.
struct Malformed {};

/// Returns the FNV-1a digest of `bytes` from `first` up to `last`.
[[nodiscard]] inline std::uint64_t checksum(
    const std::vector<std::uint8_t>& bytes, std::size_t first,
    std::size_t last) {
  constexpr std::uint64_t FNV_OFFSET = 0xCBF29CE484222325ULL;
  constexpr std::uint64_t FNV_PRIME = 0x100000001B3ULL;
  std::uint64_t digest = FNV_OFFSET;
  for (std::size_t index = first; index < last; ++index) {
    digest = (digest ^ bytes.at(index)) * FNV_PRIME;
  }
  return digest;
}

/// The bits of a byte, by which a value is split into bytes and put back.
constexpr std::size_t BYTE_BITS = 8;

/// Appends values to bytes the core writes (ADR 0030), little-endian and of
/// fixed width.
class Writer {
 public:
  void u8(std::uint8_t value) { bytes_.push_back(value); }

  void u16(std::uint16_t value) { little_endian(value); }

  void u32(std::uint32_t value) { little_endian(value); }

  void u64(std::uint64_t value) { little_endian(value); }

  void i32(std::int32_t value) { u32(static_cast<std::uint32_t>(value)); }

  void i64(std::int64_t value) { u64(static_cast<std::uint64_t>(value)); }

  void size(std::size_t value) { u64(value); }

  void flag(bool value) { u8(value ? 1 : 0); }

  void text(const std::string& value) {
    size(value.size());
    for (const char character : value) {
      u8(static_cast<std::uint8_t>(character));
    }
  }

  [[nodiscard]] std::vector<std::uint8_t>& bytes() { return bytes_; }

 private:
  template <typename Unsigned>
  void little_endian(Unsigned value) {
    for (std::size_t byte = 0; byte < sizeof(Unsigned); ++byte) {
      bytes_.push_back(static_cast<std::uint8_t>(value >> (byte * BYTE_BITS)));
    }
  }

  std::vector<std::uint8_t> bytes_;
};

/// Reads values back from bytes `first` up to `last` of what a Writer wrote,
/// throwing Malformed where they run short or hold a value no writer gives.
class Reader {
 public:
  Reader(const std::vector<std::uint8_t>& bytes, std::size_t first,
         std::size_t last)
      : bytes_(&bytes), next_(first), last_(last) {}

  std::uint8_t u8() {
    take(1);
    return bytes_->at(next_ - 1);
  }

  std::uint16_t u16() { return little_endian<std::uint16_t>(); }

  std::uint32_t u32() { return little_endian<std::uint32_t>(); }

  std::uint64_t u64() { return little_endian<std::uint64_t>(); }

  std::int32_t i32() { return static_cast<std::int32_t>(u32()); }

  std::int64_t i64() { return static_cast<std::int64_t>(u64()); }

  std::size_t size() {
    const std::uint64_t value = u64();
    if (value > SIZE_MAX) {
      throw Malformed{};
    }
    return static_cast<std::size_t>(value);
  }

  /// A count of items each at least one byte long, so no count asks for more
  /// items than bytes remain.
  std::size_t count() {
    const std::size_t value = size();
    if (value > last_ - next_) {
      throw Malformed{};
    }
    return value;
  }

  /// A tag below `bound`.
  std::uint8_t tag(std::uint8_t bound) {
    const std::uint8_t value = u8();
    if (value >= bound) {
      throw Malformed{};
    }
    return value;
  }

  bool flag() { return tag(2) == 1; }

  std::string text() {
    std::string value(count(), '\0');
    for (char& character : value) {
      character = static_cast<char>(u8());
    }
    return value;
  }

  [[nodiscard]] bool done() const { return next_ == last_; }

  /// The index of the next byte to be read.
  [[nodiscard]] std::size_t next() const { return next_; }

 private:
  void take(std::size_t count) {
    if (count > last_ - next_) {
      throw Malformed{};
    }
    next_ += count;
  }

  template <typename Unsigned>
  Unsigned little_endian() {
    take(sizeof(Unsigned));
    Unsigned value = 0;
    for (std::size_t byte = 0; byte < sizeof(Unsigned); ++byte) {
      value |= static_cast<Unsigned>(
          static_cast<Unsigned>(bytes_->at(next_ - sizeof(Unsigned) + byte))
          << (byte * BYTE_BITS));
    }
    return value;
  }

  const std::vector<std::uint8_t>* bytes_;
  std::size_t next_;
  std::size_t last_;
};

}  // namespace murim
