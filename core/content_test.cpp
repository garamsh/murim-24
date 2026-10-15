#include "content.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "json_packs.hpp"
#include "region.hpp"
#include "skill.hpp"
#include "world.hpp"

namespace murim {
namespace {

using murim_packs::DirectoryPacks;
using Kind = ContentNode::Kind;

// Builds the fields of one map or the items of one list of a ContentTree,
// one call a field.
class Node {
 public:
  Node(ContentTree& tree, std::size_t node) : tree_(&tree), node_(node) {}

  Node& set(const std::string& key, std::int64_t value) {
    ContentNode made;
    made.kind = Kind::INTEGER;
    made.integer = value;
    add(key, std::move(made));
    return *this;
  }

  // A number with a fraction, as an adapter gives one.
  Node& fraction(const std::string& key) {
    ContentNode made;
    made.kind = Kind::FRACTION;
    add(key, std::move(made));
    return *this;
  }

  Node& flag(const std::string& key, bool value) {
    ContentNode made;
    made.kind = Kind::BOOLEAN;
    made.boolean = value;
    add(key, std::move(made));
    return *this;
  }

  Node& text(const std::string& key, std::string_view value) {
    ContentNode made;
    made.kind = Kind::STRING;
    made.text = std::string{value};
    add(key, std::move(made));
    return *this;
  }

  // A list of strings.
  Node& words(const std::string& key, const std::vector<std::string>& values) {
    Node list = child(key, Kind::LIST);
    for (const std::string& value : values) {
      list.text("", value);
    }
    return *this;
  }

  Node map(const std::string& key) { return child(key, Kind::MAP); }

  Node list(const std::string& key) { return child(key, Kind::LIST); }

  // A new map at the end of this list.
  Node item() { return child("", Kind::MAP); }

 private:
  Node child(const std::string& key, Kind kind) {
    ContentNode made;
    made.kind = kind;
    return Node{*tree_, add(key, std::move(made))};
  }

  // Adds `made` under `key` of this map, or at the end of this list.
  std::size_t add(const std::string& key, ContentNode made) {
    const std::size_t index = tree_->nodes.size();
    tree_->nodes.push_back(std::move(made));
    ContentNode& parent = tree_->nodes.at(node_);
    parent.children.push_back(index);
    if (parent.kind == Kind::MAP) {
      parent.keys.push_back(key);
    }
    return index;
  }

  ContentTree* tree_;
  std::size_t node_;
};

// The in-memory pack source for tests: each pack's documents held as the
// core's value trees, built a field at a time.
class MemoryPacks : public PackSource {
 public:
  // The manifest of pack `pack` at `version`, with its id and name.
  Node manifest(const std::string& pack, std::int64_t version) {
    ContentTree& tree = document(pack, MANIFEST, Kind::MAP);
    Node root{tree, 0};
    root.text("id", pack)
        .set("version", version)
        .text("name", "pack." + pack + ".name");
    return root;
  }

  // A new entry `name` of kind `kind` in pack `pack`.
  Node entry(const std::string& pack, std::string_view kind,
             const std::string& name) {
    Node made = Node{document(pack, std::string{kind}, Kind::LIST), 0}.item();
    made.text("id", name);
    return made;
  }

  // Makes pack `pack`'s document of kind `kind` one the source cannot read.
  void unreadable(const std::string& pack, std::string_view kind) {
    unreadable_.emplace(pack, std::string{kind});
    static_cast<void>(document(pack, std::string{kind}, Kind::LIST));
  }

  [[nodiscard]] ContentRead read(const std::string& pack,
                                 const std::string& name) const override {
    const auto found = documents_.find({pack, name});
    if (found == documents_.end() || unreadable_.count({pack, name}) != 0) {
      return ContentRead{std::nullopt, ContentRefusal::UNREADABLE};
    }
    return ContentRead{found->second, ContentRefusal::UNREADABLE};
  }

  [[nodiscard]] std::vector<std::string> documents(
      const std::string& pack) const override {
    std::vector<std::string> names;
    for (const auto& [key, tree] : documents_) {
      if (key.first == pack && key.second != MANIFEST) {
        names.push_back(key.second);
      }
    }
    return names;
  }

 private:
  ContentTree& document(const std::string& pack, const std::string& name,
                        Kind root) {
    ContentTree& tree = documents_[{pack, name}];
    if (tree.nodes.empty()) {
      ContentNode made;
      made.kind = root;
      tree.nodes.push_back(std::move(made));
    }
    return tree;
  }

  std::map<std::pair<std::string, std::string>, ContentTree> documents_;
  std::set<std::pair<std::string, std::string>> unreadable_;
};

// A pack source serving the base pack from the packs directory and every
// other pack from `mods`.
class BaseAndMods : public PackSource {
 public:
  explicit BaseAndMods(const MemoryPacks& mods)
      : base_(MURIM_PACKS_DIR), mods_(&mods) {}

  [[nodiscard]] ContentRead read(const std::string& pack,
                                 const std::string& document) const override {
    return pack == murim_packs::BASE_PACK ? base_.read(pack, document)
                                          : mods_->read(pack, document);
  }

  [[nodiscard]] std::vector<std::string> documents(
      const std::string& pack) const override {
    return pack == murim_packs::BASE_PACK ? base_.documents(pack)
                                          : mods_->documents(pack);
  }

 private:
  DirectoryPacks base_;
  const MemoryPacks* mods_;
};

// A base pack small enough to read, which the merged checks accept: one
// terrain, one faction with one skill, one structure kind and a hamlet of
// one house, and the rules of life.
constexpr const char* TINY = "tiny";
constexpr std::int64_t TINY_VERSION = 2;
constexpr std::int64_t JAB_DAMAGE = 4;
// The hamlet's area: room for its house and the army its household raises.

// A complete set of rules of life, `name`, in pack `pack`.
// A terrace raised at the labour of a field on buildable cells.
constexpr std::int64_t PER_MILLE_TERRACE = 1'000;

void add_life(MemoryPacks& packs, const std::string& pack,
              const std::string& name) {
  Node life = packs.entry(pack, "life", name).set("newborn_health", 4);
  Node growth = life.map("growth")
                    .set("interval", 1)
                    .set("pressure", 0)
                    .set("reach", 0)
                    .set("spacing", 0)
                    .set("claim", 0)
                    .set("apart", 0)
                    .set("terrace", PER_MILLE_TERRACE)
                    .set("settlers", 0)
                    .set("raiders", 0)
                    .set("self_reliance", 0)
                    .text("settlement", "hamlet");
  growth.list("tiers")
      .item()
      .set("residents", 0)
      .set("width", 1)
      .set("height", 1)
      .set("vicinity", 4);
  growth.list("builds");
  life.map("learning")
      .set("comprehension", 0)
      .set("teaching", 0)
      .set("reading_cost", 0)
      .set("reading_reach", 0)
      .set("orientation_ease", 0)
      .set("polarity_ease", 0)
      .set("orientation_potency", 0)
      .set("polarity_potency", 0);
}

void add_tiny_base(MemoryPacks& packs) {
  packs.manifest(TINY, TINY_VERSION).flag("base", true);
  packs.entry(TINY, "terrain", "plain")
      .text("footing", "buildable")
      .set("weight", 1);
  packs.entry(TINY, "skills", "jab")
      .set("range", 4)
      .set("radius", 0)
      .set("body_damage", JAB_DAMAGE)
      .set("structure_damage", 0)
      .set("cooldown", 3)
      .set("qi_cost", 0)
      .text("shape", "targeted");
  packs.entry(TINY, "factions", "north")
      .text("name", "North")
      .words("hostile_to", {})
      .words("skills", {"jab"})
      .words("stance", {});
  packs.entry(TINY, "structure_kinds", "house")
      .set("housing", 4)
      .set("storage", 0)
      .set("yield", 0)
      .flag("blocks", true)
      .set("pupils", 0)
      .text("role", "home");
  packs.entry(TINY, "villages", "hamlet")
      .list("buildings")
      .item()
      .set("width", 3)
      .set("height", 3)
      .set("durability", 4)
      .text("kind", "house")
      .set("households", 1);
  add_life(packs, TINY, "life");
}

// The manifest of a mod pack `name` at version 1, for its further fields.
Node add_mod(MemoryPacks& packs, const std::string& name) {
  return packs.manifest(name, 1);
}

// Whether `loaded` holds an error of `reason` at `path` in pack `pack`.
bool refused_for(const LoadedContent& loaded, const std::string& pack,
                 const std::string& path, ContentRefusal reason) {
  return std::any_of(loaded.errors.begin(), loaded.errors.end(),
                     [&](const ContentError& error) {
                       return error.pack == pack && error.path == path &&
                              error.reason == reason;
                     });
}

TEST(Content, ATinyBasePackLoadsWithNoError) {
  MemoryPacks packs;
  add_tiny_base(packs);
  const LoadedContent loaded = load_content(packs, TINY, {});
  EXPECT_TRUE(loaded.errors.empty());
  ASSERT_TRUE(loaded.data.has_value());
  const RegionData data = loaded.data.value_or(RegionData{});
  ASSERT_EQ(data.factions.size(), 1U);
  EXPECT_EQ(data.factions.front().skills.front().body_damage,
            static_cast<std::uint64_t>(JAB_DAMAGE));
  EXPECT_EQ(loaded.order, std::vector<std::string>{TINY});
}

TEST(Content, AnUnknownFieldRefusesItsPack) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "colours");
  packs.entry("colours", "factions", "north").set("colour", 3);
  const LoadedContent loaded = load_content(packs, TINY, {"colours"});
  EXPECT_TRUE(
      refused_for(loaded, "colours", "colour", ContentRefusal::UNKNOWN_FIELD));
  EXPECT_EQ(loaded.refused, std::vector<std::string>{"colours"});
  // The base still loads without it.
  EXPECT_TRUE(loaded.data.has_value());

  // Control: the same mod giving a field the kind has loads.
  MemoryPacks control;
  add_tiny_base(control);
  add_mod(control, "colours");
  control.entry("colours", "factions", "north").text("name", "Northmen");
  const LoadedContent named = load_content(control, TINY, {"colours"});
  EXPECT_TRUE(named.errors.empty());
  ASSERT_TRUE(named.data.has_value());
  EXPECT_EQ(named.data.value_or(RegionData{}).factions.front().name,
            "Northmen");
}

TEST(Content, AFractionIsRefused) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "heavier");
  packs.entry("heavier", "terrain", "plain").fraction("weight");
  const LoadedContent loaded = load_content(packs, TINY, {"heavier"});
  EXPECT_TRUE(
      refused_for(loaded, "heavier", "weight", ContentRefusal::FRACTION));
  EXPECT_EQ(loaded.refused, std::vector<std::string>{"heavier"});

  // Control: the same weight as an integer loads.
  MemoryPacks control;
  add_tiny_base(control);
  add_mod(control, "heavier");
  control.entry("heavier", "terrain", "plain").set("weight", 2);
  const LoadedContent whole = load_content(control, TINY, {"heavier"});
  EXPECT_TRUE(whole.errors.empty());
  ASSERT_TRUE(whole.data.has_value());
  EXPECT_EQ(whole.data.value_or(RegionData{}).terrain.front().weight, 2);
}

TEST(Content, AnUnresolvedIdRefusesTheMergedSet) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "feud");
  packs.entry("feud", "factions", "north").words("hostile_to", {"south"});
  const LoadedContent loaded = load_content(packs, TINY, {"feud"});
  EXPECT_TRUE(refused_for(loaded, "feud", "hostile_to.0",
                          ContentRefusal::UNRESOLVED_ID));
  EXPECT_FALSE(loaded.data.has_value());

  // Control: with the faction it names added, it resolves.
  MemoryPacks control;
  add_tiny_base(control);
  add_mod(control, "feud");
  control.entry("feud", "factions", "north").words("hostile_to", {"south"});
  control.entry("feud", "factions", "south")
      .text("name", "South")
      .words("hostile_to", {"north"})
      .words("skills", {"jab"})
      .words("stance", {});
  const LoadedContent resolved = load_content(control, TINY, {"feud"});
  EXPECT_TRUE(resolved.errors.empty());
  ASSERT_TRUE(resolved.data.has_value());
  EXPECT_EQ(resolved.data.value_or(RegionData{}).factions.front().hostile_to,
            std::vector<std::size_t>{1});
}

TEST(Content, PacksLoadingAfterEachOtherInACycleAreRefused) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "first").words("after", {"second"});
  add_mod(packs, "second").words("after", {"first"});
  const LoadedContent loaded = load_content(packs, TINY, {"first", "second"});
  EXPECT_TRUE(refused_for(loaded, "first", "", ContentRefusal::CYCLE));
  EXPECT_TRUE(refused_for(loaded, "second", "", ContentRefusal::CYCLE));
  EXPECT_EQ(loaded.order, std::vector<std::string>{TINY});

  // Control: one way only, the order follows it, though asked otherwise.
  MemoryPacks control;
  add_tiny_base(control);
  add_mod(control, "first").words("after", {"second"});
  add_mod(control, "second");
  const LoadedContent ordered =
      load_content(control, TINY, {"first", "second"});
  EXPECT_TRUE(ordered.errors.empty());
  EXPECT_EQ(ordered.order, (std::vector<std::string>{TINY, "second", "first"}));
}

TEST(Content, AMissingRequirementRefusesThePackAndThoseRequiringIt) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "needy")
      .list("requires")
      .item()
      .text("id", "absent")
      .set("version", 1);
  add_mod(packs, "needier")
      .list("requires")
      .item()
      .text("id", "needy")
      .set("version", 1);
  const LoadedContent loaded = load_content(packs, TINY, {"needy", "needier"});
  EXPECT_TRUE(refused_for(loaded, "needy", "requires.absent",
                          ContentRefusal::MISSING_REQUIREMENT));
  EXPECT_TRUE(refused_for(loaded, "needier", "requires.needy",
                          ContentRefusal::MISSING_REQUIREMENT));
  EXPECT_EQ(loaded.refused, (std::vector<std::string>{"needy", "needier"}));
  // A version above the base's is as missing.
  MemoryPacks newer;
  add_tiny_base(newer);
  add_mod(newer, "needy")
      .list("requires")
      .item()
      .text("id", TINY)
      .set("version", TINY_VERSION + 1);
  EXPECT_TRUE(refused_for(load_content(newer, TINY, {"needy"}), "needy",
                          "requires.tiny",
                          ContentRefusal::MISSING_REQUIREMENT));

  // Control: requiring the base at its version loads.
  MemoryPacks control;
  add_tiny_base(control);
  add_mod(control, "needy")
      .list("requires")
      .item()
      .text("id", TINY)
      .set("version", TINY_VERSION);
  const LoadedContent met = load_content(control, TINY, {"needy"});
  EXPECT_TRUE(met.errors.empty());
  EXPECT_EQ(met.order, (std::vector<std::string>{TINY, "needy"}));
}

TEST(Content, ARemovedEntryTakesNoIndex) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "rocky");
  packs.entry("rocky", "terrain", "hills")
      .text("footing", "rough")
      .set("weight", 2);
  packs.entry("rocky", "terrain", "plain").flag("remove", true);
  packs.entry("rocky", "terrain", "meadow")
      .text("footing", "buildable")
      .set("weight", 3);
  const LoadedContent loaded = load_content(packs, TINY, {"rocky"});
  EXPECT_TRUE(loaded.errors.empty());
  ASSERT_TRUE(loaded.data.has_value());
  const RegionData data = loaded.data.value_or(RegionData{});
  ASSERT_EQ(data.terrain.size(), 2U);
  EXPECT_EQ(data.terrain.at(0).name, "hills");
  EXPECT_EQ(data.terrain.at(1).name, "meadow");
}

TEST(Content, ADocumentTheSourceCannotReadRefusesItsPack) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "broken");
  packs.unreadable("broken", "terrain");
  const LoadedContent loaded = load_content(packs, TINY, {"broken"});
  EXPECT_TRUE(std::any_of(loaded.errors.begin(), loaded.errors.end(),
                          [](const ContentError& error) {
                            return error.pack == "broken" &&
                                   error.document == "terrain" &&
                                   error.reason == ContentRefusal::UNREADABLE;
                          }));
  EXPECT_EQ(loaded.refused, std::vector<std::string>{"broken"});

  // Control: the same document read loads.
  MemoryPacks control;
  add_tiny_base(control);
  add_mod(control, "broken");
  control.entry("broken", "terrain", "plain").set("weight", 2);
  EXPECT_TRUE(load_content(control, TINY, {"broken"}).errors.empty());
}

TEST(Content, AFieldOfTheWrongTypeIsRefused) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "wordy");
  packs.entry("wordy", "terrain", "plain").text("weight", "heavy");
  const LoadedContent loaded = load_content(packs, TINY, {"wordy"});
  EXPECT_TRUE(
      refused_for(loaded, "wordy", "weight", ContentRefusal::WRONG_TYPE));
  EXPECT_EQ(loaded.refused, std::vector<std::string>{"wordy"});

  // Control: the same weight as a number loads.
  MemoryPacks control;
  add_tiny_base(control);
  add_mod(control, "wordy");
  control.entry("wordy", "terrain", "plain").set("weight", 2);
  EXPECT_TRUE(load_content(control, TINY, {"wordy"}).errors.empty());
}

TEST(Content, AnIdTwiceInOneDocumentIsRefused) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "twice");
  packs.entry("twice", "terrain", "hills").set("weight", 2);
  packs.entry("twice", "terrain", "hills").set("weight", 3);
  const LoadedContent loaded = load_content(packs, TINY, {"twice"});
  EXPECT_TRUE(refused_for(loaded, "twice", "id", ContentRefusal::DUPLICATE_ID));
  EXPECT_EQ(loaded.refused, std::vector<std::string>{"twice"});

  // Control: two ids, each once, load.
  MemoryPacks control;
  add_tiny_base(control);
  add_mod(control, "twice");
  control.entry("twice", "terrain", "hills")
      .text("footing", "rough")
      .set("weight", 2);
  control.entry("twice", "terrain", "dunes")
      .text("footing", "rough")
      .set("weight", 3);
  const LoadedContent once = load_content(control, TINY, {"twice"});
  EXPECT_TRUE(once.errors.empty());
  EXPECT_EQ(once.data.value_or(RegionData{}).terrain.size(), 3U);
}

TEST(Content, AnIdNotInLowercaseLettersDigitsAndUnderscoresIsRefused) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "shouty");
  packs.entry("shouty", "terrain", "Hills")
      .text("footing", "rough")
      .set("weight", 2);
  const LoadedContent loaded = load_content(packs, TINY, {"shouty"});
  EXPECT_TRUE(refused_for(loaded, "shouty", "id", ContentRefusal::BAD_ID));
  EXPECT_EQ(loaded.refused, std::vector<std::string>{"shouty"});

  // Control: the same entry with its id in lowercase loads.
  MemoryPacks control;
  add_tiny_base(control);
  add_mod(control, "shouty");
  control.entry("shouty", "terrain", "hills_2")
      .text("footing", "rough")
      .set("weight", 2);
  EXPECT_TRUE(load_content(control, TINY, {"shouty"}).errors.empty());
}

TEST(Content, ANewEntryMissingARequiredFieldRefusesTheMergedSet) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "south");
  packs.entry("south", "factions", "south")
      .words("hostile_to", {})
      .words("skills", {"jab"})
      .words("stance", {});
  const LoadedContent loaded = load_content(packs, TINY, {"south"});
  EXPECT_TRUE(
      refused_for(loaded, "south", "name", ContentRefusal::MISSING_FIELD));
  EXPECT_FALSE(loaded.data.has_value());

  // Control: the same faction with its name loads.
  MemoryPacks control;
  add_tiny_base(control);
  add_mod(control, "south");
  control.entry("south", "factions", "south")
      .text("name", "South")
      .words("hostile_to", {})
      .words("skills", {"jab"})
      .words("stance", {});
  const LoadedContent named = load_content(control, TINY, {"south"});
  EXPECT_TRUE(named.errors.empty());
  EXPECT_EQ(named.data.value_or(RegionData{}).factions.size(), 2U);
}

TEST(Content, AValueOutsideItsLimitsIsRefused) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "flat");
  packs.entry("flat", "terrain", "plain").set("weight", 0);
  packs.entry("flat", "terrain", "plain_two")
      .text("footing", "swampy")
      .set("weight", 1);
  const LoadedContent loaded = load_content(packs, TINY, {"flat"});
  // A weight under its least, and a word not among its choices.
  EXPECT_TRUE(
      refused_for(loaded, "flat", "weight", ContentRefusal::OUT_OF_RANGE));
  EXPECT_TRUE(
      refused_for(loaded, "flat", "footing", ContentRefusal::OUT_OF_RANGE));
  EXPECT_EQ(loaded.refused, std::vector<std::string>{"flat"});

  // Control: the least weight and a word among the choices load.
  MemoryPacks control;
  add_tiny_base(control);
  add_mod(control, "flat");
  control.entry("flat", "terrain", "plain").set("weight", 1);
  control.entry("flat", "terrain", "plain_two")
      .text("footing", "blocked")
      .set("weight", 1);
  EXPECT_TRUE(load_content(control, TINY, {"flat"}).errors.empty());
}

TEST(Content, AModCallingItselfABaseIsRefused) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "usurper").flag("base", true);
  const LoadedContent loaded = load_content(packs, TINY, {"usurper"});
  EXPECT_TRUE(refused_for(loaded, "usurper", "base", ContentRefusal::BASE));
  EXPECT_EQ(loaded.refused, std::vector<std::string>{"usurper"});
  // A base that does not call itself one is refused as well.
  MemoryPacks unmarked;
  unmarked.manifest(TINY, TINY_VERSION);
  EXPECT_TRUE(refused_for(load_content(unmarked, TINY, {}), TINY, "base",
                          ContentRefusal::BASE));

  // Control: the same mod saying it is no base loads.
  MemoryPacks control;
  add_tiny_base(control);
  add_mod(control, "usurper").flag("base", false);
  const LoadedContent loaded_control = load_content(control, TINY, {"usurper"});
  EXPECT_TRUE(loaded_control.errors.empty());
  EXPECT_EQ(loaded_control.order, (std::vector<std::string>{TINY, "usurper"}));
}

TEST(Content, AMergedSetFailingTheRegionsChecksIsRefused) {
  MemoryPacks packs;
  add_tiny_base(packs);
  add_mod(packs, "cramped");
  // A hamlet whose only house holds no household houses no one.
  packs.entry("cramped", "villages", "hamlet")
      .list("buildings")
      .item()
      .set("width", 3)
      .set("height", 3)
      .set("durability", 4)
      .text("kind", "house")
      .set("households", 0);
  const LoadedContent loaded = load_content(packs, TINY, {"cramped"});
  EXPECT_TRUE(refused_for(loaded, "cramped", "", ContentRefusal::INCONSISTENT));
  EXPECT_FALSE(loaded.data.has_value());
  // A second set of rules of life is as inconsistent.
  MemoryPacks lives;
  add_tiny_base(lives);
  add_mod(lives, "twin");
  add_life(lives, "twin", "other_life");
  EXPECT_TRUE(refused_for(load_content(lives, TINY, {"twin"}), "twin", "",
                          ContentRefusal::INCONSISTENT));

  // Control: the same house holding a household loads.
  MemoryPacks control;
  add_tiny_base(control);
  add_mod(control, "cramped");
  control.entry("cramped", "villages", "hamlet")
      .list("buildings")
      .item()
      .set("width", 3)
      .set("height", 3)
      .set("durability", 4)
      .text("kind", "house")
      .set("households", 1);
  EXPECT_TRUE(load_content(control, TINY, {"cramped"}).errors.empty());
}

// The damage the test mod gives the guard strike.
constexpr std::uint64_t SHARPER_DAMAGE = 40;

TEST(Content, AModOverridingOneSkillsDamageChangesOnlyThatField) {
  MemoryPacks mods;
  add_mod(mods, "sharper");
  mods.entry("sharper", "skills", "guard")
      .set("body_damage", static_cast<std::int64_t>(SHARPER_DAMAGE));
  const BaseAndMods source{mods};
  const LoadedContent base = load_content(source, murim_packs::BASE_PACK, {});
  const LoadedContent sharper =
      load_content(source, murim_packs::BASE_PACK, {"sharper"});
  ASSERT_TRUE(base.errors.empty());
  ASSERT_TRUE(sharper.errors.empty());
  ASSERT_TRUE(base.data.has_value());
  ASSERT_TRUE(sharper.data.has_value());
  const RegionData before_data = base.data.value_or(RegionData{});
  const RegionData after_data = sharper.data.value_or(RegionData{});
  ASSERT_EQ(before_data.factions.size(), after_data.factions.size());
  std::size_t changed = 0;
  for (std::size_t faction = 0; faction < before_data.factions.size();
       ++faction) {
    const std::vector<Skill>& before = before_data.factions.at(faction).skills;
    const std::vector<Skill>& after = after_data.factions.at(faction).skills;
    ASSERT_EQ(before.size(), after.size());
    for (std::size_t skill = 0; skill < before.size(); ++skill) {
      Skill expected = before.at(skill);
      if (expected.shape == Shape::TARGETED) {
        // The guard strike, every faction's first: only its damage moves.
        EXPECT_NE(expected.body_damage, SHARPER_DAMAGE);
        expected.body_damage = SHARPER_DAMAGE;
        ++changed;
      }
      const Skill& made = after.at(skill);
      EXPECT_EQ(made.range, expected.range);
      EXPECT_EQ(made.radius, expected.radius);
      EXPECT_EQ(made.body_damage, expected.body_damage);
      EXPECT_EQ(made.structure_damage, expected.structure_damage);
      EXPECT_EQ(made.cooldown, expected.cooldown);
      EXPECT_EQ(made.qi_cost, expected.qi_cost);
      EXPECT_EQ(made.shape, expected.shape);
    }
  }
  // Every faction strikes with the guard.
  EXPECT_EQ(changed, before_data.factions.size());
  // Nothing else of the tables moves: the region they generate is the same
  // ground, villages and structures.
  constexpr Seed SEED{24};
  const Region before = generate_region(SEED, before_data);
  const Region after = generate_region(SEED, after_data);
  EXPECT_EQ(after.map_rows, before.map_rows);
  EXPECT_EQ(after.terrain, before.terrain);
  EXPECT_EQ(after.territory, before.territory);
  EXPECT_EQ(after.structure_kinds, before.structure_kinds);
  EXPECT_EQ(after.villages.size(), before.villages.size());
  EXPECT_EQ(after_data.life.newborn_health, before_data.life.newborn_health);
}

}  // namespace
}  // namespace murim
