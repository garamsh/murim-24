#include "json_packs.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <variant>
#include <vector>

#include "content.hpp"

namespace murim_packs {
namespace {

using murim::ContentNode;
using murim::ContentRead;
using murim::ContentRefusal;
using Kind = ContentNode::Kind;

TEST(JsonPacks, AnIntegerIsReadExactlyAndAFractionIsMarked) {
  const ContentRead read =
      parse_document(R"({"whole": 9007199254740993, "below": -9007199254740993,
                         "part": 1.5, "power": 1e3})");
  ASSERT_TRUE(read.tree.has_value());
  const murim::ContentTree tree = read.tree.value_or(murim::ContentTree{});
  const auto kind_of = [&tree](const std::string& key) {
    const std::size_t node = murim::member(tree, 0, key).value_or(0);
    return tree.nodes.at(node).kind;
  };
  // Past a double's 53 bits, so a parser reading doubles would lose it.
  EXPECT_EQ(kind_of("whole"), Kind::INTEGER);
  EXPECT_EQ(tree.nodes.at(murim::member(tree, 0, "whole").value_or(0)).integer,
            9007199254740993);
  EXPECT_EQ(kind_of("below"), Kind::INTEGER);
  EXPECT_EQ(tree.nodes.at(murim::member(tree, 0, "below").value_or(0)).integer,
            -9007199254740993);
  EXPECT_EQ(kind_of("part"), Kind::FRACTION);
  EXPECT_EQ(kind_of("power"), Kind::FRACTION);
}

TEST(JsonPacks, ADocumentItCannotTakeIsRefusedWithItsReason) {
  EXPECT_EQ(parse_document(R"([{"id": "plain", )").refusal,
            ContentRefusal::UNREADABLE);
  EXPECT_EQ(parse_document(R"({"nothing": null})").refusal,
            ContentRefusal::WRONG_TYPE);
  EXPECT_EQ(parse_document(R"({"huge": 18446744073709551615})").refusal,
            ContentRefusal::WRONG_TYPE);
  EXPECT_EQ(parse_document(std::string(DOCUMENT_LIMIT + 1, ' ')).refusal,
            ContentRefusal::TOO_LARGE);
  // Control: the same documents well formed and in bounds are read.
  EXPECT_TRUE(parse_document(R"([{"id": "plain"}])").tree.has_value());
  EXPECT_TRUE(parse_document(R"({"nothing": 0})").tree.has_value());
  EXPECT_TRUE(
      parse_document(R"({"huge": 9223372036854775807})").tree.has_value());
}

TEST(JsonPacks, BytePacksServeEachDataFileAsTheDocumentItNames) {
  BytePacks packs;
  packs.add("one", "data/villages.json", "[]");
  packs.add("one", "pack.json", R"({"id": "one"})");
  packs.add("one", "data/factions.json", "[]");
  packs.add("one", "data/notes.txt", "[]");
  packs.add("two", "data/terrain.json", "[]");
  // A file of another type keeps its whole name, which names no kind.
  EXPECT_EQ(packs.documents("one"),
            (std::vector<std::string>{"factions", "notes.txt", "villages"}));
  EXPECT_TRUE(packs.documents("three").empty());
  EXPECT_TRUE(packs.read("one", murim::MANIFEST).tree.has_value());
  EXPECT_TRUE(packs.read("one", "factions").tree.has_value());
  EXPECT_EQ(packs.read("one", "terrain").refusal, ContentRefusal::UNREADABLE);
}

TEST(JsonPacks, EachSourceServesAMapFileAsTheMapDocumentItNames) {
  BytePacks bytes;
  bytes.add("one", "data/terrain.json", "[]");
  bytes.add("one", "map/places.json", "[]");
  // Control: a file under neither directory is no document.
  bytes.add("one", "view/places.json", "[]");
  const std::filesystem::path root =
      std::filesystem::path{testing::TempDir()} / "json_packs_map";
  std::filesystem::remove_all(root);
  for (const char* directory : {"data", "map", "view"}) {
    std::filesystem::create_directories(root / "one" / directory);
  }
  std::ofstream(root / "one" / "data" / "terrain.json") << "[]";
  std::ofstream(root / "one" / "map" / "places.json") << "[]";
  std::ofstream(root / "one" / "view" / "places.json") << "[]";
  const DirectoryPacks directories{root};
  const std::vector<std::string> served{"map/places", "terrain"};
  EXPECT_EQ(bytes.documents("one"), served);
  EXPECT_EQ(directories.documents("one"), served);
  EXPECT_EQ(file_of("map/places"), "map/places.json");
  EXPECT_TRUE(bytes.read("one", "map/places").tree.has_value());
  EXPECT_TRUE(directories.read("one", "map/places").tree.has_value());
  std::filesystem::remove_all(root);
}

// Bytes with their high bit set, which a char holds negative.
constexpr std::uint8_t HIGH_BIT = 0x80;
constexpr std::uint8_t ALL_BITS = 0xFF;

// A shipped graph's bytes as a file holds them.
std::string shipped_bytes() { return std::string{"\x01\x80\xff", 3}; }

// The same bytes as a source serves them.
std::vector<std::uint8_t> served_bytes() { return {1, HIGH_BIT, ALL_BITS}; }

// The bytes `read` serves; none where it serves no graph.
std::vector<std::uint8_t> bytes_of(const murim::GraphRead& read) {
  const auto* held = std::get_if<std::vector<std::uint8_t>>(&read);
  return held != nullptr ? *held : std::vector<std::uint8_t>{};
}

TEST(JsonPacks, BytePacksServeAShippedGraphAsBytesAndNameNoDocumentForIt) {
  BytePacks packs;
  packs.add("one", "map/places.json", "[]");
  packs.add("one", GRAPH_FILE, shipped_bytes());
  packs.add("two", "map/places.json", "[]");

  EXPECT_EQ(bytes_of(packs.read_graph("one")), served_bytes());
  EXPECT_EQ(packs.documents("one"), std::vector<std::string>{"map/places"});
  // Control: a pack shipping no graph serves none.
  EXPECT_TRUE(std::holds_alternative<murim::NoGraph>(packs.read_graph("two")));
}

TEST(JsonPacks, DirectoryPacksServeAShippedGraphAsBytesAndNameNoDocumentForIt) {
  const std::filesystem::path root =
      std::filesystem::path{testing::TempDir()} / "json_packs_graph";
  std::filesystem::remove_all(root);
  for (const char* pack : {"one", "two"}) {
    std::filesystem::create_directories(root / pack / "map");
    std::ofstream(root / pack / "map" / "places.json") << "[]";
  }
  std::ofstream(root / "one" / GRAPH_FILE, std::ios::binary) << shipped_bytes();
  const DirectoryPacks packs{root};

  EXPECT_EQ(bytes_of(packs.read_graph("one")), served_bytes());
  EXPECT_EQ(packs.documents("one"), std::vector<std::string>{"map/places"});
  // Control: a pack shipping no graph serves none.
  EXPECT_TRUE(std::holds_alternative<murim::NoGraph>(packs.read_graph("two")));
  std::filesystem::remove_all(root);
}

TEST(JsonPacks, ADirectoryFilePastTheLimitIsRefusedAsTooLarge) {
  const std::filesystem::path root =
      std::filesystem::path{testing::TempDir()} / "json_packs_limit";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "big" / "data");
  const std::string document = R"([{"id": "plain"}])";
  const auto write = [&root, &document](const std::string& kind,
                                        std::size_t size) {
    std::ofstream file(root / "big" / "data" / (kind + ".json"),
                       std::ios::binary);
    file << document << std::string(size - document.size(), ' ');
  };
  write("terrain", DOCUMENT_LIMIT + 1);
  // Control: the same document padded to the limit exactly is read.
  write("villages", DOCUMENT_LIMIT);
  ASSERT_EQ(std::filesystem::file_size(root / "big" / "data" / "terrain.json"),
            DOCUMENT_LIMIT + 1);
  const DirectoryPacks source{root};
  EXPECT_EQ(source.read("big", "terrain").refusal, ContentRefusal::TOO_LARGE);
  EXPECT_TRUE(source.read("big", "villages").tree.has_value());
  EXPECT_EQ(source.read("big", "absent").refusal, ContentRefusal::UNREADABLE);
  std::filesystem::remove_all(root);
}

TEST(JsonPacks, TheBasePackLoadsFromItsDirectoryWithNoError) {
  const DirectoryPacks source{MURIM_PACKS_DIR};
  const murim::LoadedContent loaded =
      murim::load_content(source, BASE_PACK, {});
  EXPECT_TRUE(loaded.errors.empty());
  EXPECT_TRUE(loaded.data.has_value());
  EXPECT_EQ(loaded.order, std::vector<std::string>{BASE_PACK});
}

}  // namespace
}  // namespace murim_packs
