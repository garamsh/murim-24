#pragma once

#include <cstddef>
#include <cstdint>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
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
  /// world_art.gd's BODY_TAGS names them, then the atlas's width for one
  /// faction and where the shadow's slot starts, in `slots`, the sprite set
  /// of each faction, and the accent of each sprite set.
  void set_atlas(const godot::Vector2& size,
                 const godot::PackedInt64Array& frames,
                 const godot::PackedInt64Array& outlines,
                 const godot::Vector2i& slots,
                 const godot::PackedInt64Array& sprite_of_faction,
                 const godot::PackedColorArray& accents);

  /// Sets the colours the fighting is drawn in: paper, for others' losses
  /// and a use by an army of no accent; ink, for a number's shadow; jade,
  /// for the steered person's uses; and crimson, for its own losses and the
  /// target's brackets.
  void set_palette(const godot::PackedColorArray& colours);

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

  /// Sets the army whose lead carries the player's markers, or -1 for none.
  void set_steered(std::int64_t army);
  /// Sets whose losses float up: 0 none, 1 the steered person's own and
  /// those its uses struck, 2 every loss.
  void set_damage_numbers(std::int64_t which);
  /// Sets whether a hit draws blood and whether a hit on or from the
  /// steered person holds bodies and shakes the screen.
  void set_blood(bool shown);
  void set_hit_stop(bool shown);
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
  [[nodiscard]] godot::Dictionary note_tick(const godot::Dictionary& snapshot,
                                            const godot::PackedInt64Array& died,
                                            std::int64_t steered_army,
                                            std::int64_t now);

  /// Packs the bodies within `view`, in pixels, `between` of the way from
  /// the tick before last to the last, at `now` in milliseconds, into
  /// `bands`, one buffer per row from `first_row`, shadows first, with the
  /// falling dead; and `outlines`. Also gives `drawn`, `outlined`, the
  /// people near the viewer's lead nearest first as `near_person`,
  /// `near_head`, `near_relation` and `near_cells`, and where the steered
  /// lead and the target are drawn, `lead_at` and `target_at`, with `lead`
  /// and `target` false when there is none.
  [[nodiscard]] godot::Dictionary fill(const godot::Rect2& view, double between,
                                       std::int64_t now);

  /// Draws on `canvas_item` the uses of skills, the target's brackets and
  /// the damage numbers shown at the last fill.
  void draw_effects(const godot::RID& canvas_item) const;

  /// Returns where body `body` of army `army` stands as drawn, its feet, in
  /// pixels, `between` of the way from where the tick before last left it
  /// to where the last tick did, raised with the ground.
  [[nodiscard]] godot::Vector2 body_at(std::int64_t army, std::int64_t body,
                                       double between) const;
  /// Returns where a point in sub-cell units is drawn, in pixels.
  [[nodiscard]] godot::Vector2 to_screen(const godot::Vector2& sub) const;

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
  /// Adds a damage number, as a hit at `when` adds one.
  void add_number(std::int64_t when, const godot::String& text,
                  const godot::Vector2& where, bool own, bool mine);
  /// Sets the view and the time the shown effects are taken at, and where
  /// the target is drawn, as fill() sets them.
  void show_at(const godot::Rect2& view, std::int64_t now,
               const godot::Vector2& target_at);
  /// Returns the view effects were last shown in, in pixels.
  [[nodiscard]] godot::Rect2 view() const;

  /// Returns a number in [0, 1) from `seed` and `salt`, the same for the
  /// same pair.
  [[nodiscard]] static double unit(std::int64_t seed, std::int64_t salt);
  /// Returns the offsets a shaken screen steps through, one each step.
  [[nodiscard]] static godot::TypedArray<godot::Vector2i> shake_pattern();

  /// A point or a size in pixels or sub-cell units, in single precision as
  /// Godot's Vector2 holds it.
  struct Point {
    float x = 0.0F;
    float y = 0.0F;
  };
  /// A rectangle, as Godot's Rect2 holds it.
  struct Box {
    Point position;
    Point size;
  };
  /// A colour, as Godot's Color holds it.
  struct Rgba {
    float red = 0.0F;
    float green = 0.0F;
    float blue = 0.0F;
    float alpha = 1.0F;
  };

 protected:
  static void _bind_methods();

 private:
  struct Strike {
    std::int64_t person = -1;
    std::int64_t shape = 0;
    std::int64_t range = 0;
    std::int64_t radius = 0;
    std::int64_t body_damage = 0;
    Point origin;
    Point toward;
    Point end;
    std::vector<std::int64_t> struck;
    std::int64_t ms = 0;
    bool steered = false;
  };
  struct Death {
    std::int64_t person = -1;
    std::int64_t army = -1;
    Point at;
    Point ground;
    std::int64_t ms = 0;
  };
  struct Number {
    std::int64_t ms = 0;
    godot::String text;
    Point at;
    bool own = false;
    bool mine = false;
    double width = 0.0;
  };
  struct Stop {
    std::int64_t person = -1;
    std::int64_t frames = 0;
  };
  // A frame of the body atlas, its flip, and how far toward white it is
  // drawn.
  struct Look {
    std::int64_t tag = 0;
    Box frame;
    double white = 0.0;
    bool flip = false;
  };
  // A body a hit stop holds: the view frames it still holds, and from its
  // first drawn frame on, where and how that frame drew it.
  struct Hold {
    std::int64_t left = 0;
    bool drawn = false;
    Point at;
    Look look;
  };
  struct Occluder {
    Box rect;
    double sort_y = 0.0;
  };
  // An army's bodies on the last tick and the one before it, and whether its
  // roster stayed the same between them.
  struct Army {
    std::vector<std::int64_t> roster;
    std::vector<Point> now;
    std::vector<Point> before;
    bool same = true;
    std::int64_t faction = 0;
  };
  // A sprite to pack: its frame in the atlas, its centre, its flip, how far
  // toward white and how opaque it is drawn, how far it is turned about its
  // bottom edge, and for an outline, where the body it outlines lies.
  struct Sprite {
    Box frame;
    Point centre;
    bool flip = false;
    double white = 0.0;
    double alpha = 1.0;
    double turn = 0.0;
    Point outline_of{-1.0F, -1.0F};
  };
  // What a frame packs into, and how it is laid out in rows.
  struct Frame {
    Box shown;
    std::int64_t first_row = 0;
    std::int64_t last_row = 0;
    std::int64_t now = 0;
    float share = 0.0F;
    std::vector<std::vector<float>> shadows;
    std::vector<std::vector<float>> bodies;
    std::vector<float> outlines;
    std::int64_t drawn = 0;
    std::int64_t outlined = 0;
  };
  struct Near {
    std::int64_t person = -1;
    Point head;
    std::int64_t relation = -1;
    double cells = 0.0;
  };
  // The arrays of a snapshot the bodies are read from.
  struct Read {
    std::vector<std::int64_t> first;
    std::vector<std::int64_t> persons;
    std::vector<Point> positions;
    std::vector<std::int64_t> healths;
    std::vector<std::int64_t> factions;
  };

  static Read read(const godot::Dictionary& snapshot);
  void read_armies(const Read& snapshot);
  void read_bodies(const Read& snapshot, bool compare);
  void read_strikes(const godot::Dictionary& snapshot,
                    const std::vector<std::vector<Point>>& stood);
  void note_struck(const Strike& strike,
                   const std::vector<std::vector<Point>>& stood);
  void note_deaths(const godot::PackedInt64Array& died,
                   const std::vector<std::vector<Point>>& stood);
  void hit(std::int64_t person, std::int64_t lost, Point where);
  void bleed(std::int64_t person, Point where, bool mine);
  // A hit's person and the health it lost.
  struct Loss {
    std::int64_t person = -1;
    std::int64_t lost = 0;
  };
  void stop_hit(const Loss& loss);
  void start_holds();
  [[nodiscard]] godot::Dictionary drips() const;
  void grow(std::size_t size);
  void forget(std::int64_t now);
  void clear_people();
  [[nodiscard]] bool alive(std::int64_t person) const;
  [[nodiscard]] static Point sub_at(const Army& army, std::size_t body,
                                    double between);
  [[nodiscard]] Point screen_point(Point sub) const;
  [[nodiscard]] double lift_at(Point point) const;
  [[nodiscard]] double height_at(Point point) const;
  [[nodiscard]] std::size_t cell_index(std::int64_t column,
                                       std::int64_t row) const;
  [[nodiscard]] bool hidden(Box rect, double sort_y) const;
  [[nodiscard]] std::int64_t sprite_of(std::size_t army) const;
  [[nodiscard]] std::int64_t sprite_for(std::int64_t faction) const;
  [[nodiscard]] Rgba accent(std::int64_t person) const;
  [[nodiscard]] Rgba paint(std::size_t which) const;
  [[nodiscard]] Point knocked(std::int64_t person, std::int64_t now) const;
  [[nodiscard]] Look look_of(const Frame& frame, const Army& army,
                             std::size_t body) const;
  void pack(std::vector<float>& buffer, const Sprite& sprite) const;
  void pack_body(Frame& frame, const Army& army, std::size_t body);
  void pack_dead(Frame& frame) const;
  void count_holds();
  [[nodiscard]] std::vector<Near> near(double between) const;
  void draw_strike(const godot::RID& canvas_item, const Strike& strike,
                   double left) const;
  void draw_target(const godot::RID& canvas_item, Point where) const;
  [[nodiscard]] bool shows(const Strike& strike, std::int64_t now) const;

  // The atlas: until it is set, every frame is its first.
  static constexpr std::size_t TAGS = 8;
  Point atlas_size_{1.0F, 1.0F};
  std::vector<std::int64_t> frames_ = std::vector<std::int64_t>(TAGS, 0);
  std::vector<std::int64_t> outline_frames_ =
      std::vector<std::int64_t>(TAGS, 0);
  std::int64_t faction_width_ = 0;
  std::int64_t shadow_x_ = 0;
  std::vector<std::int64_t> sprite_of_faction_;
  std::vector<Rgba> accents_;
  std::vector<Rgba> palette_;
  // The relief.
  std::int64_t relief_width_ = 0;
  std::int64_t relief_height_ = 0;
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
  std::int64_t font_size_ = 0;

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
  std::vector<Point> away_;
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
  std::vector<Point> bleed_at_;
  std::vector<Point> bleed_dir_;
  std::vector<std::int64_t> bleed_seed_;

  // What the last fill showed.
  Box view_;
  std::int64_t now_ = 0;
  Point target_at_;
};

}  // namespace murim_godot
