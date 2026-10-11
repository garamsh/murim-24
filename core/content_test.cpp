#include "content.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "json_packs.hpp"
#include "region.hpp"
#include "world.hpp"

namespace murim {
namespace {

using murim_packs::BytePacks;
using murim_packs::DirectoryPacks;

// A pack source serving the base pack from the packs directory and every
// other pack from `mods`.
class BaseAndMods : public PackSource {
 public:
  explicit BaseAndMods(const BytePacks& mods)
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
  const BytePacks* mods_;
};

// A base pack small enough to read, which the merged checks accept: one
// terrain, one faction with one skill, one structure kind and a hamlet of
// one house, and the rules of life.
constexpr const char* TINY = "tiny";

void add_tiny_base(BytePacks& packs) {
  packs.add(TINY, "pack.json",
            R"({"id": "tiny", "version": 2, "name": "pack.tiny.name",
                "base": true})");
  packs.add(TINY, "data/terrain.json",
            R"([{"id": "plain", "footing": "buildable", "weight": 1}])");
  packs.add(TINY, "data/skills.json",
            R"([{"id": "jab", "range": 384, "radius": 0, "body_damage": 10,
                 "structure_damage": 0, "cooldown": 15, "qi_cost": 0,
                 "shape": "targeted"}])");
  packs.add(TINY, "data/factions.json",
            R"([{"id": "north", "name": "North", "hostile_to": [],
                 "skills": ["jab"], "stance": []}])");
  packs.add(TINY, "data/structure_kinds.json",
            R"([{"id": "house", "housing": 8, "storage": 0, "yield": 0,
                 "blocks": true, "pupils": 0}])");
  packs.add(TINY, "data/villages.json",
            R"([{"id": "hamlet", "width": 7, "height": 9,
                 "buildings": [{"x": 0, "y": 0, "width": 3, "height": 3,
                                "durability": 100, "kind": "house",
                                "households": 1}]}])");
  packs.add(TINY, "data/life.json",
            R"([{"id": "life", "newborn_health": 120,
                 "learning": {"comprehension": 0, "teaching": 0,
                              "reading_cost": 0, "reading_reach": 0,
                              "orientation_ease": 1000, "polarity_ease": 1000,
                              "orientation_potency": 1000,
                              "polarity_potency": 1000}}])");
}

// A mod pack `id` with the manifest fields `manifest` beside its id,
// version and name.
void add_mod(BytePacks& packs, const std::string& name,
             const std::string& manifest) {
  packs.add(name, "pack.json",
            R"({"id": ")" + name + R"(", "version": 1, "name": "pack.)" + name +
                R"(.name")" + (manifest.empty() ? "" : ", " + manifest) + "}");
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
  BytePacks packs;
  add_tiny_base(packs);
  const LoadedContent loaded = load_content(packs, TINY, {});
  EXPECT_TRUE(loaded.errors.empty());
  ASSERT_TRUE(loaded.data.has_value());
  EXPECT_EQ(loaded.data.value_or(RegionData{}).factions.size(), 1U);
  EXPECT_EQ(loaded.data.value_or(RegionData{})
                .factions.front()
                .skills.front()
                .body_damage,
            10U);
  EXPECT_EQ(loaded.order, std::vector<std::string>{TINY});
}

TEST(Content, AnUnknownFieldRefusesItsPack) {
  BytePacks packs;
  add_tiny_base(packs);
  add_mod(packs, "colours", "");
  packs.add("colours", "data/factions.json",
            R"([{"id": "north", "colour": 3}])");
  const LoadedContent loaded = load_content(packs, TINY, {"colours"});
  EXPECT_TRUE(
      refused_for(loaded, "colours", "colour", ContentRefusal::UNKNOWN_FIELD));
  EXPECT_EQ(loaded.refused, std::vector<std::string>{"colours"});
  // The base still loads without it.
  EXPECT_TRUE(loaded.data.has_value());

  // Control: the same mod giving a field the kind has loads.
  BytePacks control;
  add_tiny_base(control);
  add_mod(control, "colours", "");
  control.add("colours", "data/factions.json",
              R"([{"id": "north", "name": "Northmen"}])");
  const LoadedContent named = load_content(control, TINY, {"colours"});
  EXPECT_TRUE(named.errors.empty());
  ASSERT_TRUE(named.data.has_value());
  EXPECT_EQ(named.data.value_or(RegionData{}).factions.front().name,
            "Northmen");
}

TEST(Content, AFractionIsRefused) {
  BytePacks packs;
  add_tiny_base(packs);
  add_mod(packs, "heavier", "");
  packs.add("heavier", "data/terrain.json",
            R"([{"id": "plain", "weight": 1.5}])");
  const LoadedContent loaded = load_content(packs, TINY, {"heavier"});
  EXPECT_TRUE(
      refused_for(loaded, "heavier", "weight", ContentRefusal::FRACTION));
  EXPECT_EQ(loaded.refused, std::vector<std::string>{"heavier"});

  // Control: the same weight as an integer loads.
  BytePacks control;
  add_tiny_base(control);
  add_mod(control, "heavier", "");
  control.add("heavier", "data/terrain.json",
              R"([{"id": "plain", "weight": 2}])");
  const LoadedContent whole = load_content(control, TINY, {"heavier"});
  EXPECT_TRUE(whole.errors.empty());
  ASSERT_TRUE(whole.data.has_value());
  EXPECT_EQ(whole.data.value_or(RegionData{}).terrain.front().weight, 2);
}

TEST(Content, AnUnresolvedIdRefusesTheMergedSet) {
  BytePacks packs;
  add_tiny_base(packs);
  add_mod(packs, "feud", "");
  packs.add("feud", "data/factions.json",
            R"([{"id": "north", "hostile_to": ["south"]}])");
  const LoadedContent loaded = load_content(packs, TINY, {"feud"});
  EXPECT_TRUE(refused_for(loaded, "feud", "hostile_to.0",
                          ContentRefusal::UNRESOLVED_ID));
  EXPECT_FALSE(loaded.data.has_value());

  // Control: with the faction it names added, it resolves.
  BytePacks control;
  add_tiny_base(control);
  add_mod(control, "feud", "");
  control.add("feud", "data/factions.json",
              R"([{"id": "north", "hostile_to": ["south"]},
                  {"id": "south", "name": "South", "hostile_to": ["north"],
                   "skills": ["jab"], "stance": []}])");
  const LoadedContent resolved = load_content(control, TINY, {"feud"});
  EXPECT_TRUE(resolved.errors.empty());
  ASSERT_TRUE(resolved.data.has_value());
  EXPECT_EQ(resolved.data.value_or(RegionData{}).factions.front().hostile_to,
            std::vector<std::size_t>{1});
}

TEST(Content, PacksLoadingAfterEachOtherInACycleAreRefused) {
  BytePacks packs;
  add_tiny_base(packs);
  add_mod(packs, "first", R"("after": ["second"])");
  add_mod(packs, "second", R"("after": ["first"])");
  const LoadedContent loaded = load_content(packs, TINY, {"first", "second"});
  EXPECT_TRUE(refused_for(loaded, "first", "", ContentRefusal::CYCLE));
  EXPECT_TRUE(refused_for(loaded, "second", "", ContentRefusal::CYCLE));
  EXPECT_EQ(loaded.order, std::vector<std::string>{TINY});

  // Control: one way only, the order follows it, though asked otherwise.
  BytePacks control;
  add_tiny_base(control);
  add_mod(control, "first", R"("after": ["second"])");
  add_mod(control, "second", "");
  const LoadedContent ordered =
      load_content(control, TINY, {"first", "second"});
  EXPECT_TRUE(ordered.errors.empty());
  EXPECT_EQ(ordered.order, (std::vector<std::string>{TINY, "second", "first"}));
}

TEST(Content, AMissingRequirementRefusesThePackAndThoseRequiringIt) {
  BytePacks packs;
  add_tiny_base(packs);
  add_mod(packs, "needy", R"("requires": [{"id": "absent", "version": 1}])");
  add_mod(packs, "needier", R"("requires": [{"id": "needy", "version": 1}])");
  const LoadedContent loaded = load_content(packs, TINY, {"needy", "needier"});
  EXPECT_TRUE(refused_for(loaded, "needy", "requires.absent",
                          ContentRefusal::MISSING_REQUIREMENT));
  EXPECT_TRUE(refused_for(loaded, "needier", "requires.needy",
                          ContentRefusal::MISSING_REQUIREMENT));
  EXPECT_EQ(loaded.refused, (std::vector<std::string>{"needy", "needier"}));
  // A version above the base's is as missing.
  BytePacks newer;
  add_tiny_base(newer);
  add_mod(newer, "needy", R"("requires": [{"id": "tiny", "version": 3}])");
  EXPECT_TRUE(refused_for(load_content(newer, TINY, {"needy"}), "needy",
                          "requires.tiny",
                          ContentRefusal::MISSING_REQUIREMENT));

  // Control: requiring the base at its version loads.
  BytePacks control;
  add_tiny_base(control);
  add_mod(control, "needy", R"("requires": [{"id": "tiny", "version": 2}])");
  const LoadedContent met = load_content(control, TINY, {"needy"});
  EXPECT_TRUE(met.errors.empty());
  EXPECT_EQ(met.order, (std::vector<std::string>{TINY, "needy"}));
}

TEST(Content, ARemovedEntryTakesNoIndex) {
  BytePacks packs;
  add_tiny_base(packs);
  add_mod(packs, "rocky", "");
  packs.add("rocky", "data/terrain.json",
            R"([{"id": "hills", "footing": "rough", "weight": 2},
                {"id": "plain", "remove": true},
                {"id": "meadow", "footing": "buildable", "weight": 3}])");
  const LoadedContent loaded = load_content(packs, TINY, {"rocky"});
  EXPECT_TRUE(loaded.errors.empty());
  ASSERT_TRUE(loaded.data.has_value());
  ASSERT_EQ(loaded.data.value_or(RegionData{}).terrain.size(), 2U);
  EXPECT_EQ(loaded.data.value_or(RegionData{}).terrain.at(0).name, "hills");
  EXPECT_EQ(loaded.data.value_or(RegionData{}).terrain.at(1).name, "meadow");
}

TEST(Content, ADocumentNotWellFormedRefusesItsPack) {
  BytePacks packs;
  add_tiny_base(packs);
  add_mod(packs, "broken", "");
  packs.add("broken", "data/terrain.json", R"([{"id": "plain", )");
  const LoadedContent loaded = load_content(packs, TINY, {"broken"});
  EXPECT_TRUE(std::any_of(loaded.errors.begin(), loaded.errors.end(),
                          [](const ContentError& error) {
                            return error.pack == "broken" &&
                                   error.document == "terrain" &&
                                   error.reason == ContentRefusal::UNREADABLE;
                          }));
  EXPECT_EQ(loaded.refused, std::vector<std::string>{"broken"});
}

// The damage the test mod gives the guard strike.
constexpr std::uint64_t SHARPER_DAMAGE = 40;

TEST(Content, AModOverridingOneSkillsDamageChangesOnlyThatField) {
  BytePacks mods;
  add_mod(mods, "sharper", "");
  mods.add("sharper", "data/skills.json",
           R"([{"id": "guard", "body_damage": 40}])");
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
