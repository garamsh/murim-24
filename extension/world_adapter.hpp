#pragma once

#include <cstddef>
#include <cstdint>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <optional>
#include <string>
#include <vector>

#include "map.hpp"
#include "region.hpp"
#include "save.hpp"
#include "world.hpp"

namespace murim_godot {

/// A core world exposed to Godot; GDScript gets one from
/// `WorldAdapter.create()` or `WorldAdapter.create_m1()`, the only ways to
/// make one.
// GDCLASS, godot-cpp's code, declares a copy assignment and expands into code
// these checks reject.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class WorldAdapter : public godot::RefCounted {
  // NOLINTNEXTLINE(misc-const-correctness,modernize-use-auto)
  GDCLASS(WorldAdapter, godot::RefCounted)

 public:
  /// Returns an adapter over a world created from `seed`, a map of
  /// `map_rows`, `armies`, `structures`, `rules` and `leadership`, as the
  /// core's constructor takes them, each struct as a Dictionary keyed by its
  /// field names:
  /// - army: `objective` (Vector2i cell), `members` (Array of members),
  ///   `faction` (int) and `skills` (Array of skills);
  /// - member: `cell` (Vector2i) and `health` (int), a new person;
  /// - skill: `range`, `radius`, `body_damage`, `structure_damage`,
  ///   `cooldown` (int);
  /// - structure: `cell` (Vector2i) and `durability` (int);
  /// - rules: `separation` (int), `hostile` (Array of Arrays of bool) and
  ///   `stage_floors` (Array of three ints);
  /// - leadership, one per faction, may be left out: `leader` (int, a
  ///   person) and `duel` (`members_only` and `allies_turn_hostile`, bool,
  ///   and `yield_health`, int), each left out when the faction has none.
  /// Returns null, after push_error, when the core or the conversion to its
  /// types refuses the input.
  static godot::Ref<WorldAdapter> create(
      std::int64_t seed, const godot::PackedStringArray& map_rows,
      const godot::TypedArray<godot::Dictionary>& armies,
      const godot::TypedArray<godot::Dictionary>& structures,
      const godot::Dictionary& rules,
      const godot::TypedArray<godot::Dictionary>& leadership = {});

  /// Returns an adapter over the core's M1 benchmark scenario.
  static godot::Ref<WorldAdapter> create_m1();

  /// Returns an adapter over the region the core generates from `seed`, with
  /// each faction's leadership, and one person born into village `village`
  /// as the last army, its days `ticks_per_day` ticks long. Returns null,
  /// after push_error, when the region has no such village or the day is not
  /// a positive number of ticks.
  static godot::Ref<WorldAdapter> create_region(
      std::int64_t seed, std::int64_t village = 0,
      std::int64_t ticks_per_day = murim::TICKS_PER_DAY);

  /// Returns an adapter over the world saved in slot `slot`, as save() wrote
  /// it. Returns null, after push_error naming why, when the slot holds no
  /// save, the save is refused, or it holds no world made by create_region().
  static godot::Ref<WorldAdapter> load(std::int64_t slot);

  /// Saves the world in slot `slot`, replacing what it held. Returns false,
  /// after push_error, when the world was not made by create_region() or the
  /// save could not be written.
  [[nodiscard]] bool save(std::int64_t slot) const;

  /// Returns the army the living person carrying the player's soul serves
  /// in, as the core holds it, or -1 when no such person lives.
  [[nodiscard]] std::int64_t player_army() const;

  /// Returns the point the M1 scenario's battle is fought around, in
  /// sub-cell units.
  static godot::Vector2i m1_centre();

  /// Gives army `army` the order to march toward `objective` and strike with
  /// one of `skills`, indices into its skills; with `challenge` 0 or more, its
  /// lead also challenges that faction's leader to a duel. Returns false,
  /// after push_error, when the core or the conversion to its types refuses
  /// it.
  bool order(std::int64_t army, const godot::Vector2i& objective,
             const godot::PackedInt64Array& skills,
             std::int64_t challenge = -1);

  /// Returns how many factions the world's rules name.
  [[nodiscard]] std::int64_t faction_count() const;

  /// Returns the person leading faction `faction`, or -1 when none does.
  [[nodiscard]] std::int64_t leader(std::int64_t faction) const;

  /// Returns whether faction `faction` elects its leader.
  [[nodiscard]] bool elects(std::int64_t faction) const;

  /// Returns why army `army`'s lead may not challenge faction `faction`'s
  /// leader, one of the CHALLENGE_* constants; CHALLENGE_NONE when it may.
  [[nodiscard]] std::int64_t challenge_refusal(std::int64_t army,
                                               std::int64_t faction) const;

  /// Returns the duel fought for faction `faction`'s lead as a Dictionary of
  /// `challenger` and `leader`, person indices; empty when none is fought.
  [[nodiscard]] godot::Dictionary duel(std::int64_t faction) const;

  /// Returns whether people of faction `from` attack those of `target`.
  [[nodiscard]] bool hostile(std::int64_t from, std::int64_t target) const;

  /// Returns the living members of army `army`, as person indices, its lead
  /// first.
  [[nodiscard]] godot::PackedInt64Array roster(std::int64_t army) const;

  /// Returns person `person`'s position, in sub-cell units.
  [[nodiscard]] godot::Vector2 person_position(std::int64_t person) const;

  /// Returns person `person`'s health; 0 once dead.
  [[nodiscard]] std::int64_t person_health(std::int64_t person) const;

  /// Returns person `person`'s name as `surname` and `given` in Korean and
  /// `surname_en` and `given_en` romanised, or an empty Dictionary when the
  /// world names no one.
  [[nodiscard]] godot::Dictionary person_name(std::int64_t person) const;

  /// Returns every name the world's people may bear, as its rules of life
  /// give them: `surnames`, `female` and `male`, each an Array of
  /// Dictionaries of `korean` and `romanised`; each Array is empty when the
  /// world names no one.
  [[nodiscard]] godot::Dictionary names() const;

  /// Returns person `person`'s parents, as person indices.
  [[nodiscard]] godot::PackedInt64Array person_parents(
      std::int64_t person) const;

  /// Returns the people born to person `person`, as person indices, in
  /// order of birth.
  [[nodiscard]] godot::PackedInt64Array person_children(
      std::int64_t person) const;

  /// Returns the person `person` last married, or -1 if it never married.
  [[nodiscard]] std::int64_t person_spouse(std::int64_t person) const;

  /// Returns person `person`'s family.
  [[nodiscard]] std::int64_t person_family(std::int64_t person) const;

  /// Returns person `person`'s age in whole years of the core's calendar.
  [[nodiscard]] std::int64_t person_age(std::int64_t person) const;

  /// Returns the faction of the army person `person` serves in, or -1 when
  /// it serves in none, as the dead do.
  [[nodiscard]] std::int64_t person_faction(std::int64_t person) const;

  /// Advances the world by one step.
  void tick();

  /// Returns the world's state hash, its bits read as a signed int.
  [[nodiscard]] std::int64_t hash() const;

  /// Returns the map's rows, `#` for a blocked cell and `.` for a passable
  /// one.
  [[nodiscard]] godot::PackedStringArray map_rows() const;

  /// Returns how many armies the world holds, those its villages' growth
  /// formed included.
  [[nodiscard]] std::int64_t army_count() const;

  /// Returns the world's date: `year` and `day` counted from 1, `season`
  /// from 0, as the core's calendar gives them.
  [[nodiscard]] godot::Dictionary date() const;

  /// Returns how many skills army `army` has now.
  [[nodiscard]] std::int64_t skill_count(std::int64_t army) const;

  /// Returns the positions of army `army`'s living members, each one person,
  /// in sub-cell units and roster order; exact while coordinates stay within
  /// 2^24. Its size is the roster's.
  [[nodiscard]] godot::PackedVector2Array body_positions(
      std::int64_t army) const;

  /// Returns how many structures the world holds, those its villages' growth
  /// laid included.
  [[nodiscard]] std::int64_t structure_count() const;

  /// Returns the cell structure `structure` stands on.
  [[nodiscard]] godot::Vector2i structure_cell(std::int64_t structure) const;

  /// Returns the footprint of structure `structure`, its width and height in
  /// cells, from structure_cell() as its top-left cell.
  [[nodiscard]] godot::Vector2i structure_footprint(
      std::int64_t structure) const;

  /// Returns the kind of building structure `structure` is, such as `house`,
  /// a structure growth laid taking its plan's; empty for a world made by
  /// create(), whose structures carry no kind.
  [[nodiscard]] godot::String structure_kind(std::int64_t structure) const;

  /// Returns army `army`'s skills, each a Dictionary keyed as create() takes
  /// it: `range`, `radius`, `body_damage`, `structure_damage` and `cooldown`.
  [[nodiscard]] godot::TypedArray<godot::Dictionary> skills(
      std::int64_t army) const;

  /// Returns the stage structure `structure` is in, one of the STAGE_*
  /// constants.
  [[nodiscard]] std::int64_t structure_stage(std::int64_t structure) const;

  /// Returns what structure `structure` shows as a field, one of the FIELD_*
  /// constants, or -1 when it is no field.
  [[nodiscard]] std::int64_t structure_field_state(
      std::int64_t structure) const;

  /// Returns how many people village `village` can hold, as the core derives
  /// it; villages founded as the world runs follow the region's.
  [[nodiscard]] std::int64_t village_cap(std::int64_t village) const;

  /// Returns the food village `village` holds.
  [[nodiscard]] std::int64_t village_stock(std::int64_t village) const;

  /// Returns how many living people make village `village` their home.
  [[nodiscard]] std::int64_t village_population(std::int64_t village) const;

  /// Returns how many sects the world holds, standing or fallen.
  [[nodiscard]] std::int64_t sect_count() const;

  /// Returns the sect person `person` belongs to, or -1 for none.
  [[nodiscard]] std::int64_t person_sect(std::int64_t person) const;

  /// Returns sect `sect`'s name.
  [[nodiscard]] godot::String sect_name(std::int64_t sect) const;

  /// Returns the faction sect `sect` belongs to.
  [[nodiscard]] std::int64_t sect_faction(std::int64_t sect) const;

  /// Returns the village that is sect `sect`'s seat.
  [[nodiscard]] std::int64_t sect_seat(std::int64_t sect) const;

  /// Returns whether sect `sect`'s seat is held by its faction.
  [[nodiscard]] bool sect_seat_held(std::int64_t sect) const;

  /// Returns sect `sect`'s living members, as person indices, ascending.
  [[nodiscard]] godot::PackedInt64Array sect_members(std::int64_t sect) const;

  /// Returns sect `sect`'s elders, as person indices, ascending.
  [[nodiscard]] godot::PackedInt64Array sect_elders(std::int64_t sect) const;

  /// Returns sect `sect`'s master, or -1 if it has no elder.
  [[nodiscard]] std::int64_t sect_master(std::int64_t sect) const;

  /// Returns how many members sect `sect` may hold.
  [[nodiscard]] std::int64_t sect_cap(std::int64_t sect) const;

  /// Returns whether sect `sect` stands, with a living member.
  [[nodiscard]] bool sect_stands(std::int64_t sect) const;

  /// Returns every manual as a Dictionary: its `discipline`, and `place`, one
  /// of "sect", "cell" or "lost", with its `sect` or its `cell` as a
  /// Vector2i.
  [[nodiscard]] godot::TypedArray<godot::Dictionary> manuals() const;

  /// Returns why person `person` may not join sect `sect`, one of the
  /// APPLICATION_* constants; APPLICATION_NONE when it may.
  [[nodiscard]] std::int64_t application_refusal(std::int64_t person,
                                                 std::int64_t sect) const;

  /// Makes person `person` a member of sect `sect`. Returns false, after
  /// push_error, when the core refuses the application.
  bool apply(std::int64_t person, std::int64_t sect);

  /// Returns the events that reach person `person` as rumours, newest first,
  /// each a Dictionary of `sequence`, `tick`, `cell` (Vector2i) and `kind`,
  /// with the records it names: "founded" with `village` and `faction`;
  /// "raided" with `village`, `owner` and `raider`; "ruined" with `village`,
  /// `owner`, `army` and `destroyer`, the destroyer's faction; "sect_fell"
  /// with `sect` and `faction`; "leader_changed" with `faction`, `leader`
  /// and `by`, "election" or "duel"; "born" and "died" with `person`.
  [[nodiscard]] godot::TypedArray<godot::Dictionary> rumours(
      std::int64_t person) const;

  /// Returns person `person`'s attainment in each discipline, in the order
  /// of discipline_names().
  [[nodiscard]] godot::PackedInt64Array person_attainment(
      std::int64_t person) const;

  /// Returns the names of the disciplines the world's rules of life draw,
  /// by index; empty for a world given none.
  [[nodiscard]] godot::PackedStringArray discipline_names() const;

  /// Returns the faction army `army` fights for now; a soul born again
  /// takes its new family's.
  [[nodiscard]] std::int64_t army_faction(std::int64_t army) const;

  /// Returns the health army `army`'s members were created with, together; 0
  /// for an army the world formed as it ran.
  [[nodiscard]] std::int64_t army_strength(std::int64_t army) const;

  /// Returns the health of each of army `army`'s living members, in the
  /// order of body_positions().
  [[nodiscard]] godot::PackedInt64Array body_strengths(std::int64_t army) const;

  /// Returns the tick from which each of army `army`'s living members may
  /// strike again, in the order of body_positions(); it moves on each strike.
  [[nodiscard]] godot::PackedInt64Array body_ready(std::int64_t army) const;
  /// Returns the events the core keeps from the `from`-th recorded on, oldest
  /// first, each a Dictionary of its `sequence` and `tick`, its `cell`
  /// (Vector2i) and its `kind`, one of the EVENT_* constants.
  [[nodiscard]] godot::TypedArray<godot::Dictionary> events(
      std::int64_t from) const;
  /// Returns the names of the terrain kinds, by index; empty unless the
  /// world was made by create_region().
  [[nodiscard]] godot::PackedStringArray terrain_names() const;

  /// Returns the terrain kind of each cell, row by row; empty unless the
  /// world was made by create_region().
  [[nodiscard]] godot::PackedInt64Array terrain() const;

  /// Returns the names of the factions, by index; empty unless the world was
  /// made by create_region().
  [[nodiscard]] godot::PackedStringArray faction_names() const;

  /// Returns the faction whose territory each cell lies in, row by row, as
  /// the core derives it now, or -1 for a cell no village holds; empty unless
  /// the world was made by create_region().
  [[nodiscard]] godot::PackedInt64Array territory() const;

  /// Returns each village as a Dictionary of `name`, `kind` (its template's
  /// name), `faction` (int, its owner now), `corner` (Vector2i) and `area`
  /// (Vector2i, its width and height in cells): the region's, then those
  /// founded as the world runs, on the land the core says each was founded
  /// on; empty unless the world was made by create_region().
  [[nodiscard]] godot::TypedArray<godot::Dictionary> villages() const;

 protected:
  static void _bind_methods();

 private:
  // Set by create(), the only way GDScript gets an adapter, since the class is
  // registered abstract.
  std::optional<murim::World> world_;
  std::vector<std::string> map_rows_;
  std::vector<std::string> structure_kinds_;
  std::vector<std::uint64_t> strengths_;
  std::size_t faction_count_ = 0;
  // The names the world's rules of life were given.
  murim::Names names_;
  std::vector<std::string> disciplines_;
  // What create_region() keeps of the region for display; empty otherwise.
  murim::RegionData region_data_;
  std::vector<std::size_t> terrain_;
  std::vector<murim::Village> villages_;
  // How create_region() made the world, which save() keeps beside it.
  std::optional<murim::Origin> origin_;

  // Returns an adapter over a world created from these arguments, as the
  // core's constructor takes them.
  static godot::Ref<WorldAdapter> over(
      murim::Seed seed, std::vector<std::string> map_rows,
      std::vector<murim::ArmySpec> armies,
      std::vector<murim::Structure> structures, murim::Rules rules,
      std::vector<murim::VillageSpec> villages = {},
      std::vector<murim::Leadership> leadership = {}, murim::Life life = {},
      std::vector<murim::SectSpec> sects = {});
  // Returns an adapter over `region`, with one person born into village
  // `village` as the last army; throws std::out_of_range when there is no
  // such village, and std::invalid_argument when the core refuses the
  // region's rules.
  static godot::Ref<WorldAdapter> born_into(murim::Region region,
                                            std::size_t village);
  // Keeps what the view draws of `region`, made by create_region() with the
  // player born into `origin`'s village as its last army.
  void show(murim::Region& region, murim::Origin origin);
  [[nodiscard]] murim::World& world();
  [[nodiscard]] const murim::World& world() const;
  [[nodiscard]] std::size_t army_index(std::int64_t army) const;
  [[nodiscard]] std::size_t structure_index(std::int64_t structure) const;
  [[nodiscard]] std::size_t village_index(std::int64_t village) const;
  [[nodiscard]] std::size_t faction_index(std::int64_t faction) const;
  [[nodiscard]] std::size_t person_index(std::int64_t person) const;
  [[nodiscard]] std::size_t sect_index(std::int64_t sect) const;
};

}  // namespace murim_godot
