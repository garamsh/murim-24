#include "chronicle.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace murim {
namespace {

// An entry carrying only what a chronicle reads, and a mark to refuse it by.
struct Note {
  std::uint64_t sequence = 0;
  std::uint64_t tick = 0;
  bool marked = false;
};

constexpr std::size_t KEPT = 3;
using Notes = Chronicle<Note, KEPT>;

// Whether `notes` are kept at tick `now`, every unmarked note accepted.
bool kept_at(const Notes& notes, std::uint64_t now) {
  return notes.kept(now, [](const Note& note) { return !note.marked; });
}

TEST(Chronicle, KeepsTheNewestNumberedOnPastItsCapacity) {
  Notes notes;
  for (std::uint64_t tick = 0; tick <= KEPT; ++tick) {
    notes.add(Note{0, tick, false});
  }
  ASSERT_EQ(notes.entries().size(), KEPT);
  EXPECT_EQ(notes.entries().front().sequence, 1U);
  EXPECT_EQ(notes.entries().front().tick, 1U);
  EXPECT_EQ(notes.entries().back().sequence, KEPT);
  EXPECT_TRUE(kept_at(notes, KEPT));
}

TEST(Chronicle, RefusesEntriesNotAsAddKeepsThem) {
  // Control: entries as add() makes them are kept.
  const Notes made{std::vector<Note>{{0, 1, false}, {1, 2, false}}};
  EXPECT_TRUE(kept_at(made, 2));

  // A gap in the numbering.
  EXPECT_FALSE(
      kept_at(Notes{std::vector<Note>{{0, 1, false}, {2, 2, false}}}, 2));
  // A tick earlier than the entry before it.
  EXPECT_FALSE(
      kept_at(Notes{std::vector<Note>{{0, 2, false}, {1, 1, false}}}, 2));
  // A tick later than now.
  EXPECT_FALSE(kept_at(made, 1));
  // An entry the check refuses.
  EXPECT_FALSE(
      kept_at(Notes{std::vector<Note>{{0, 1, false}, {1, 2, true}}}, 2));
  // More than the capacity.
  EXPECT_FALSE(
      kept_at(Notes{std::vector<Note>{
                  {0, 1, false}, {1, 1, false}, {2, 1, false}, {3, 1, false}}},
              1));
}

}  // namespace
}  // namespace murim
