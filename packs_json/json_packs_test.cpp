#include "json_packs.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
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
      parse_document(R"({"whole": 9007199254740993, "part": 1.5,
                         "power": 1e3})");
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

TEST(JsonPacks, BytePacksServeTheirDataDocumentsInPathOrder) {
  BytePacks packs;
  packs.add("one", "data/villages.json", "[]");
  packs.add("one", "pack.json", "{}");
  packs.add("one", "data/factions.json", "[]");
  packs.add("two", "data/terrain.json", "[]");
  EXPECT_EQ(
      packs.data_documents("one"),
      (std::vector<std::string>{"data/factions.json", "data/villages.json"}));
  EXPECT_TRUE(packs.data_documents("three").empty());
  EXPECT_EQ(packs.read("one", "data/absent.json").refusal,
            ContentRefusal::UNREADABLE);
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
