#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace murim {

/// A bounded record of what happened: the latest `CAPACITY` entries, oldest
/// first, numbered one after another; the oldest gives way to the newest.
/// An entry carries its `sequence` and the `tick` it was made on.
template <typename Entry, std::size_t CAPACITY>
class Chronicle {
 public:
  Chronicle() = default;

  /// A chronicle holding `entries` as a save gives them, unchecked until
  /// kept() is asked.
  explicit Chronicle(std::vector<Entry> entries)
      : entries_(std::move(entries)) {}

  /// Adds `entry`, numbering it one after the newest, letting the oldest go
  /// once CAPACITY are kept.
  void add(Entry entry) {
    entry.sequence = entries_.empty() ? 0 : entries_.back().sequence + 1;
    if (entries_.size() == CAPACITY) {
      entries_.erase(entries_.begin());
    }
    entries_.push_back(std::move(entry));
  }

  /// The entries kept, oldest first.
  [[nodiscard]] const std::vector<Entry>& entries() const { return entries_; }

  /// Whether the entries are as add() keeps them: at most CAPACITY, oldest
  /// first, numbered one after another, none later than tick `now`, and
  /// each one `valid` accepts.
  template <typename Valid>
  [[nodiscard]] bool kept(std::uint64_t now, Valid valid) const {
    bool bad = entries_.size() > CAPACITY;
    for (std::size_t index = 0; index < entries_.size(); ++index) {
      const Entry& entry = entries_.at(index);
      const bool unordered =
          index > 0 && (entry.sequence != entries_.at(index - 1).sequence + 1 ||
                        entry.tick < entries_.at(index - 1).tick);
      bad = bad || unordered || entry.tick > now || !valid(entry);
    }
    return !bad;
  }

 private:
  std::vector<Entry> entries_;
};

}  // namespace murim
