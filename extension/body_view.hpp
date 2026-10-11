#pragma once

#include <cstdint>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <unordered_map>
#include <vector>

namespace murim_godot {

/// The bodies of a world as the view draws them, and what the fighting does
/// to them, kept natively so neither a tick nor a frame loops over every
/// person in GDScript. Fed each tick one WorldAdapter.snapshot(), it compares
/// each person with the tick before: who lost health, flashing and knocked
/// from the nearest use that struck it, whose loss floats up as a number,
/// who fell struck, who a hit on or from the steered person holds still and
/// how far the screen shakes. Each frame it packs every body on screen into
/// one MultiMesh buffer per row of cells, with its shadow, its frame by how
/// it moves, struck and is wounded, and an outline where something stands in
/// front of it, and lists the people near the viewer; and it draws the uses
/// of skills, the target's brackets and the numbers in one call. It reads
/// the world and holds no game rule.
// GDCLASS, godot-cpp's code, declares a copy assignment and expands into code
// these checks reject.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class BodyView : public godot::RefCounted {
  // NOLINTNEXTLINE(misc-const-correctness,modernize-use-auto)
  GDCLASS(BodyView, godot::RefCounted)

 public:
  /// Sets the body atlas: its size in pixels, the first frame of each body
  /// tag in `frames` and of its outline in `outlines`, both in the order
  /// world_art.gd's BODY_TAGS names them, the atlas's width for one
  /// faction, where the shadow's slot starts, the sprite set of each
  /// faction, and the accent of each sprite set.
  void set_atlas(godot::Vector2 size, const godot::PackedInt64Array& frames,
                 const godot::PackedInt64Array& outlines,
                 std::int64_t faction_width, std::int64_t shadow_x,
                 const godot::PackedInt64Array& sprite_of_faction,
                 const godot::PackedColorArray& accents);

  /// Sets the ground bodies stand on, as Relief.view_cells() gives it: the
  /// cells' `size`, each cell's `level`, whether it rises as a `slope`,
  /// and its `flat` level plus one where the ground round it is flat, else
  /// 0. Without it, bodies stand on the unraised ground.
  void set_relief(const godot::Dictionary& cells);

  /// Sets what can stand in front of a body: five floats each, its
  /// rectangle's position and size and the y it is sorted by, in pixels.
  void set_occluders(const godot::PackedFloat32Array& occluders);

  /// Sets the army whose lead names are read around, or -1 for none, and
  /// how each faction stands toward that army's, by faction.
  void set_viewer(std::int64_t army,
                  const godot::PackedInt64Array& relation_of_faction);

  /// Sets whose losses float up: 0 none, 1 the steered person's own and
  /// those its uses struck, 2 every loss.
  void set_damage_numbers(std::int64_t which);
  /// Sets the army whose lead carries the player's markers, or -1 for none.
  void set_steered(std::int64_t army);
  /// Sets whether a hit draws blood and whether a hit on or from the
  /// steered person holds bodies and shakes the screen.
  void set_blood(bool on);
  void set_hit_stop(bool on);
  /// Sets the person the player selected, or -1 for none.
  void set_selected(std::int64_t person);
  /// Sets the font damage numbers are drawn in, and its size.
  void set_font(const godot::Ref<godot::Font>& font, std::int64_t size);

  /// Starts on `snapshot` as it stands, showing nothing and holding no one:
  /// what each person holds is what the next tick is compared with.
  void begin(const godot::Dictionary& snapshot);

  /// Reads what the tick just run did, at `now` in milliseconds, from its
  /// `snapshot` and the people who `died` on it, for `steered_army`'s lead
  /// (-1 for none). Returns what GDScript lays as blood: each hit that
  /// bleeds, `bleed_at`, `bleed_dir` and `bleed_seed`, and each drop a
  /// maimed body on screen lets fall, `drip_at` and `drip_shape`.
  [[nodiscard]] godot::Dictionary note_tick(
      const godot::Dictionary& snapshot, const godot::PackedInt64Array& died,
      std::int64_t steered_army, std::int64_t now);

  /// Packs the bodies within `view`, in pixels, `between` of the way from
  /// the tick before last to the last, at `now` in milliseconds, into
  /// `bands`, one buffer per row from `first_row`, shadows first, with the
  /// falling dead; and `outlines`. Also gives `drawn`, `outlined`, the
  /// people near the viewer's lead nearest first as `near_person`,
  /// `near_head`, `near_relation` and `near_cells`, and where the steered
  /// lead and the target are drawn, `lead_at` and `target_at`, with `lead`
  /// and `target` false when there is none.
  [[nodiscard]] godot::Dictionary fill(godot::Rect2 view, double between,
                                       std::int64_t now);

  /// Draws on `canvas_item` the uses of skills, the target's brackets and
  /// the damage numbers shown at the last fill.
  void draw_effects(godot::RID canvas_item) const;

  /// Returns where body `body` of army `army` stands as drawn, its feet, in
  /// pixels, `between` of the way from where the tick before last left it
  /// to where the last tick did, raised with the ground.
  [[nodiscard]] godot::Vector2 body_at(std::int64_t army, std::int64_t body,
                                       double between) const;
  /// Returns where a point in sub-cell units is drawn, in pixels.
  [[nodiscard]] godot::Vector2 to_screen(godot::Vector2 sub) const;

  /// Returns the share of `person`'s pixels drawn white at `now`, how far it
  /// is knocked, in pixels, and how wounded it is drawn: 0, 1 or 2.
  [[nodiscard]] double flash(std::int64_t person, std::int64_t now) const;
  [[nodiscard]] godot::Vector2 knock(std::int64_t person,
                                     std::int64_t now) const;
  [[nodiscard]] std::int64_t wound(std::int64_t person) const;
  /// Returns the army and body index `person` stood at on the last tick, or
  /// (-1, -1) once it is not among the living.
  [[nodiscard]] godot::Vector2i place_of(std::int64_t person) const;
  /// Returns the person marked, or -1.
  [[nodiscard]] std::int64_t target() const;
  /// Returns how far, in whole screen pixels, the screen is shaken at `now`.
  [[nodiscard]] godot::Vector2i shake(std::int64_t now) const;
  /// Returns how many people a hit stop holds now.
  [[nodiscard]] std::int64_t held_count() const;
  /// Returns whether a hit stop holds `person`.
  [[nodiscard]] bool held(std::int64_t person) const;

  /// Returns the last tick's hit stops, each `person` and `frames`.
  [[nodiscard]] godot::TypedArray<godot::Dictionary> hit_stops() const;
  /// Returns each use kept, as WorldAdapter.strikes() gives it, with `ms`
  /// and `steered`; those shown at `now`; each death kept, its `person`,
  /// `army`, `at`, `ground` and `ms`; those shown at `now`, each its
  /// `army`, `at`, `ground`, `fallen`, `alpha` and `flash`; each number
  /// kept, its `ms`, `text`, `at`, `own`, `mine` and `width`; and those
  /// shown at `now`, each its `text`, `at` and `colour`.
  [[nodiscard]] godot::TypedArray<godot::Dictionary> strikes() const;
  [[nodiscard]] godot::TypedArray<godot::Dictionary> shown_strikes(
      std::int64_t now) const;
  [[nodiscard]] godot::TypedArray<godot::Dictionary> deaths() const;
  [[nodiscard]] godot::TypedArray<godot::Dictionary> dying(
      std::int64_t now) const;
  [[nodiscard]] godot::TypedArray<godot::Dictionary> numbers() const;
  [[nodiscard]] godot::TypedArray<godot::Dictionary> shown_numbers(
      std::int64_t now) const;
  /// Adds a damage number, as a hit at `ms` adds one.
  void add_number(std::int64_t ms, const godot::String& text, godot::Vector2 at,
                  bool own, bool mine);
  /// Sets the view and the time the shown effects are taken at, and where
  /// the target is drawn, as fill() sets them.
  void show_at(godot::Rect2 view, std::int64_t now, godot::Vector2 target_at);
  /// Returns the view effects were last shown in, in pixels.
  [[nodiscard]] godot::Rect2 view() const;

  /// Returns a number in [0, 1) from `seed` and `k`, the same for the same
  /// pair.
  [[nodiscard]] static double unit(std::int64_t seed, std::int64_t k);
  /// Returns the offsets a shaken screen steps through, one each step.
  [[nodiscard]] static godot::TypedArray<godot::Vector2i> shake_pattern();

 protected:
  static void _bind_methods();

 private:
  struct Strike {
    std::int64_t person = -1;
    std::int64_t shape = 0;
    std::int64_t range = 0;
    std::int64_t radius = 0;
    std::int64_t body_damage = 0;
    godot::Vector2 origin;
    godot::Vector2 toward;
    godot::Vector2 end;
    godot::PackedInt64Array struck;
    std::int64_t ms = 0;
    bool steered = false;
  };
  struct Death {
    std::int64_t person = -1;
    std::int64_t army = -1;
    godot::Vector2 at;
    godot::Vector2 ground;
    std::int64_t ms = 0;
  };
  struct Number {
    std::int64_t ms = 0;
    godot::String text;
    godot::Vector2 at;
    bool own = false;
    bool mine = false;
    double width = 0.0;
  };
  struct Stop {
    std::int64_t person = -1;
    std::int64_t frames = 0;
  };
  // A body a hit stop holds: the view frames it still holds, and from its
  // first drawn frame on, what that frame drew.
  struct Hold {
    std::int64_t left = 0;
    bool drawn = false;
    godot::Vector2 at;
    std::int64_t tag = 0;
    godot::Rect2 frame;
    double white = 0.0;
    bool flip = false;
  };
  struct Occluder {
    godot::Rect2 rect;
    double sort_y = 0.0;
  };
  // An army's bodies on the last tick and the one before it, and whether its
  // roster stayed the same between them.
  struct Army {
    godot::PackedInt64Array roster;
    godot::PackedVector2Array now;
    godot::PackedVector2Array before;
    bool same = true;
    std::int64_t faction = 0;
  };

  void read_armies(const godot::Dictionary& snapshot);
  void read_bodies(const godot::Dictionary& snapshot, bool compare);
  void note_struck(const Strike& strike,
                   const std::vector<godot::PackedVector2Array>& stood);
  void hit(std::int64_t person, std::int64_t lost, godot::Vector2 at);
  void grow(std::size_t size);
  void forget(std::int64_t now);
  void clear_people();
  [[nodiscard]] bool alive(std::int64_t person) const;
  [[nodiscard]] godot::Vector2 sub_at(std::int64_t army, std::int64_t body,
                                      double between) const;
  [[nodiscard]] double lift_at(godot::Vector2 point) const;
  [[nodiscard]] double height_at(godot::Vector2 point) const;
  [[nodiscard]] std::int64_t cell_index(godot::Vector2i at) const;
  [[nodiscard]] godot::Vector2 warped(godot::Vector2 point) const;
  [[nodiscard]] bool hidden(godot::Rect2 rect, double sort_y) const;
  [[nodiscard]] std::int64_t sprite_of(std::int64_t army) const;
  [[nodiscard]] godot::Color accent(std::int64_t person) const;
  void pack(godot::PackedFloat32Array& buffer, godot::Rect2 frame,
            godot::Vector2 centre, bool flip, double white = 0.0,
            double alpha = 1.0, double turn = 0.0,
            godot::Vector2 outline_of = godot::Vector2(-1, -1)) const;
  void draw_strike(godot::RID canvas_item, const Strike& strike,
                   double left) const;
  void draw_target(godot::RID canvas_item, godot::Vector2 at) const;

  // The atlas: until it is set, every frame is its first.
  static constexpr std::size_t TAGS = 8;
  godot::Vector2 atlas_size_{1, 1};
  std::vector<std::int64_t> frames_ = std::vector<std::int64_t>(TAGS, 0);
  std::vector<std::int64_t> outline_frames_ = std::vector<std::int64_t>(TAGS, 0);
  std::int64_t faction_width_ = 0;
  std::int64_t shadow_x_ = 0;
  std::vector<std::int64_t> sprite_of_faction_;
  std::vector<godot::Color> accents_;
  // The relief.
  godot::Vector2i relief_size_;
  std::vector<std::int32_t> levels_;
  std::vector<std::uint8_t> slopes_;
  std::vector<std::int32_t> flat_;
  std::int64_t highest_ = 0;
  // What stands in front of bodies, bucketed.
  std::unordered_map<std::int64_t, std::vector<Occluder>> occluders_;
  // Names and settings.
  std::int64_t viewer_ = -1;
  std::vector<std::int64_t> relation_of_faction_;
  std::int64_t damage_numbers_ = 1;
  bool blood_ = true;
  bool hit_stop_ = true;
  std::int64_t selected_ = -1;
  godot::Ref<godot::Font> font_;
  std::int64_t font_size_ = 10;

  // The armies, and the people, by person: the tick each was last read
  // alive, its health and the most it has had, its army and body index
  // then, the tick a use last struck it, whether one of the steered
  // person's did, how near the nearest that did stood, whose use it was,
  // when it last lost health and the way it is knocked, and when it last
  // struck.
  std::vector<Army> armies_;
  std::vector<std::int64_t> seen_;
  std::vector<std::int64_t> health_;
  std::vector<std::int64_t> peak_;
  std::vector<std::int64_t> army_;
  std::vector<std::int64_t> body_;
  std::vector<std::int64_t> struck_tick_;
  std::vector<bool> struck_by_lead_;
  std::vector<double> nearest_;
  std::vector<std::int64_t> striker_;
  std::vector<std::int64_t> hit_ms_;
  std::vector<godot::Vector2> away_;
  std::vector<std::int64_t> struck_at_;
  std::unordered_map<std::int64_t, Hold> held_;

  std::int64_t tick_ = 0;
  std::int64_t ms_ = 0;
  std::int64_t lead_ = -1;
  std::int64_t steered_army_ = -1;
  std::int64_t bleeds_ = 0;
  std::int64_t target_ = -1;
  std::int64_t target_ms_ = 0;
  std::int64_t shake_ms_ = 0;
  std::int64_t shake_pixels_ = 0;
  std::vector<Strike> strikes_;
  std::vector<Death> deaths_;
  std::vector<Number> numbers_;
  std::vector<Stop> hit_stops_;
  godot::PackedVector2Array bleed_at_;
  godot::PackedVector2Array bleed_dir_;
  godot::PackedInt64Array bleed_seed_;

  // What the last fill showed.
  godot::Rect2 view_;
  std::int64_t now_ = 0;
  godot::Vector2 target_at_;
};

}  // namespace murim_godot
