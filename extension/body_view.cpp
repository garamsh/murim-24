#include "body_view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <limits>
#include <utility>
#include <vector>

#include "map.hpp"
#include "world.hpp"

namespace murim_godot {

namespace {

using godot::Color;
using godot::Rect2;
using godot::Vector2;
using godot::Vector2i;

// The arithmetic below mirrors the GDScript it replaced, world_art.gd,
// combat.gd and relief.gd, so a body lands on the same pixel: a Vector2's
// components are single precision and a float is double, as in GDScript.

// A cell's width and depth on the ground, in pixels, and how far a level
// raises it.
constexpr double CELL_X = 16.0;
constexpr double CELL_Y = 12.0;
constexpr double LEVEL_PX = 16.0;
// The relief's warp of the ground's coordinates, in pixels, and its scale.
constexpr double WARP = 32.0;
constexpr double WARP_SCALE = 48.0;
constexpr double WARP_SALT_X = 7.0;
constexpr double WARP_SALT_Y = 5.0;
constexpr std::int64_t WARP_NOISE_X = 11;
constexpr std::int64_t WARP_NOISE_Y = 12;
constexpr int FBM_OCTAVES = 3;
constexpr double FBM_LACUNARITY = 2.03;
constexpr double FBM_TOTAL = 0.875;
constexpr double SLOPE_BLEND = 0.5;

// Pixels past the screen's edge still drawn; a body's frame; the row of it
// its feet stand on; how far above them its name's anchor is.
constexpr double MARGIN = 64.0;
constexpr std::int64_t FRAME_WIDTH = 16;
constexpr std::int64_t FRAME_HEIGHT = 24;
constexpr std::int64_t FEET_ROW = 22;
constexpr std::int64_t HEAD_RISE = 23;
constexpr auto FRAME_W = static_cast<double>(FRAME_WIDTH);
constexpr auto FRAME_H = static_cast<double>(FRAME_HEIGHT);
constexpr auto FEET = static_cast<double>(FEET_ROW);
constexpr auto HEAD = static_cast<double>(HEAD_RISE);
// How long a body shows its strike frame, and each walking frame, in
// milliseconds.
constexpr std::int64_t BODY_STRIKE_MS = 200;
constexpr std::int64_t WALK_FRAME_MS = 150;
constexpr std::int64_t WALK_FRAMES = 4;
// Pixels the occluders are bucketed by.
constexpr double BUCKET = 32.0;
// How far from the viewer's lead names are read, in cells.
constexpr std::int64_t NAME_CELLS = 8;
constexpr auto NAME_REACH = static_cast<double>(NAME_CELLS);
// Floats per sprite in a band's buffer.
constexpr std::int64_t BODY_FLOATS = 16;
// The body tags, in world_art.gd's BODY_TAGS order.
constexpr std::int64_t TAG_IDLE = 0;
constexpr std::int64_t TAG_WALK = 1;
constexpr std::int64_t TAG_BACK = 2;
constexpr std::int64_t TAG_STRIKE = 3;
constexpr std::array<std::array<std::int64_t, 3>, 2> WOUND_TAGS{
    {{0, 4, 6}, {1, 5, 7}}};

// The fighting's timings, in milliseconds, and sizes, in pixels, as
// combat.gd states them.
constexpr double BODY_HEIGHT = 22.0;
constexpr std::int64_t STRIKE_MS = 160;
constexpr std::int64_t FLASH_MS = 120;
constexpr std::int64_t DEATH_MS = 700;
constexpr std::int64_t NUMBER_MS = 800;
constexpr std::int64_t TARGET_MS = 2000;
constexpr double KNOCK_PIXELS = 2.0;
constexpr double NUMBER_RISE = 12.0;
constexpr std::int64_t WOUNDED = 500;
constexpr std::int64_t MAIMED = 250;
constexpr std::int64_t PER_MILLE = 1000;
constexpr std::int64_t DRIP_TICKS = 45;
constexpr std::int64_t MOST_BLEEDS = 4;
constexpr std::int64_t BLEED_SEED = 7919;
constexpr std::array<std::array<std::int64_t, 2>, 2> HIT_STOP{
    {{20, 2}, {60, 3}}};
constexpr std::int64_t HIT_STOP_MOST = 4;
constexpr std::array<std::array<std::int64_t, 2>, 2> SHAKE{{{20, 1}, {60, 2}}};
constexpr std::int64_t SHAKE_MOST = 3;
constexpr std::int64_t SHAKE_MS = 150;
constexpr std::int64_t SHAKE_STEP_MS = 30;
constexpr std::array<std::array<std::int32_t, 2>, 5> SHAKE_PATTERN{
    {{1, 0}, {-1, 1}, {0, -1}, {1, 1}, {-1, 0}}};
constexpr double LEAST_RADIUS = 4.0;
constexpr std::int64_t NEVER = -(std::int64_t{1} << 40);
constexpr double FILL_ALPHA = 0.35;
constexpr double EDGE_ALPHA = 0.9;
constexpr int ARC_SEGMENTS = 12;
constexpr int DISC_POINTS = 16;
constexpr double PROJECTILE = 1.5;
constexpr double DEATH_FALL = 3.0;
// The target's brackets, round a body standing on their bottom edge.
constexpr double BRACKET_LEFT = -9.0;
constexpr double BRACKET_TOP = 3.0;
constexpr double BRACKET_W = 18.0;
constexpr double BRACKET_H = 21.0;
constexpr double BRACKET_ARM = 4.0;
constexpr double DRIP_SPREAD = 4.0;
// _unit()'s hash, as combat.gd states it.
constexpr std::int64_t UNIT_X = 73856093;
constexpr std::int64_t UNIT_Y = 19349663;
constexpr std::int64_t UNIT_MIX = 1274126177;
constexpr std::int64_t UNIT_RANGE = 10007;
constexpr int UNIT_SHIFT_A = 13;
constexpr int UNIT_SHIFT_B = 16;
// relief.gd's hash01(), on unsigned 32-bit arithmetic.
constexpr std::uint64_t HASH_X = 374761393;
constexpr std::uint64_t HASH_Y = 668265263;
constexpr std::uint64_t HASH_SALT = 2246822519;
constexpr std::uint64_t HASH_MIX = 1274126177;
constexpr std::uint64_t LOW_32 = 0xffffffff;
constexpr std::uint64_t LOW_16 = 0xffff;
constexpr double HASH_RANGE = 65536.0;

// A colour of the palette, as GDScript's Color("rrggbb") reads it.
Color rgb(std::uint32_t hex) {
  constexpr std::uint32_t BYTE = 0xff;
  constexpr float MOST = 255.0F;
  constexpr int RED = 16;
  constexpr int GREEN = 8;
  return {static_cast<float>((hex >> RED) & BYTE) / MOST,
          static_cast<float>((hex >> GREEN) & BYTE) / MOST,
          static_cast<float>(hex & BYTE) / MOST, 1.0F};
}

constexpr std::uint32_t PAPER_HEX = 0xefe3c2;
constexpr std::uint32_t INK_HEX = 0x14101a;
constexpr std::uint32_t JADE_HEX = 0x4fc49a;
constexpr std::uint32_t CRIMSON_HEX = 0xd63a2f;

float single(double value) { return static_cast<float>(value); }

double lerpf(double from, double to, double weight) {
  return from + ((to - from) * weight);
}

double smoothstep(double from, double to, double value) {
  const double share = std::clamp((value - from) / (to - from), 0.0, 1.0);
  return share * share * (3.0 - (2.0 * share));
}

double hash01(std::int64_t x, std::int64_t y, std::int64_t salt) {
  std::uint64_t n = ((static_cast<std::uint64_t>(x) & LOW_32) * HASH_X +
                     (static_cast<std::uint64_t>(y) & LOW_32) * HASH_Y +
                     (static_cast<std::uint64_t>(salt) & LOW_32) * HASH_SALT) &
                    LOW_32;
  n = ((n ^ (n >> UNIT_SHIFT_A)) * HASH_MIX) & LOW_32;
  n = n ^ (n >> UNIT_SHIFT_B);
  return static_cast<double>(n & LOW_16) / HASH_RANGE;
}

double value_noise(Vector2 point, std::int64_t salt) {
  const Vector2 cell = point.floor();
  Vector2 f = point - cell;
  f = f * f * (Vector2(3, 3) - f * 2.0F);
  const auto x = static_cast<std::int64_t>(cell.x);
  const auto y = static_cast<std::int64_t>(cell.y);
  return lerpf(lerpf(hash01(x, y, salt), hash01(x + 1, y, salt), f.x),
               lerpf(hash01(x, y + 1, salt), hash01(x + 1, y + 1, salt), f.x),
               f.y);
}

double fbm(Vector2 point, std::int64_t salt) {
  double sum = 0.0;
  double amplitude = 0.5;
  for (int octave = 0; octave < FBM_OCTAVES; ++octave) {
    sum += amplitude * value_noise(point, salt + octave);
    point *= single(FBM_LACUNARITY);
    amplitude *= 0.5;
  }
  return sum / FBM_TOTAL;
}

std::int64_t floor_div(double value, double by) {
  return static_cast<std::int64_t>(std::floor(value / by));
}

std::int64_t bucket_key(std::int64_t x, std::int64_t y) {
  constexpr std::int64_t SPAN = std::int64_t{1} << 32;
  return (x * SPAN) + y;
}

Color with_alpha(Color colour, double alpha) {
  colour.a = single(alpha);
  return colour;
}

}  // namespace

void BodyView::set_atlas(Vector2 size, const godot::PackedInt64Array& frames,
                         const godot::PackedInt64Array& outlines,
                         std::int64_t faction_width, std::int64_t shadow_x,
                         const godot::PackedInt64Array& sprite_of_faction,
                         const godot::PackedColorArray& accents) {
  atlas_size_ = size;
  // A tag the atlas does not name takes the first frame.
  frames_.assign(TAGS, 0);
  outline_frames_.assign(TAGS, 0);
  std::copy_n(frames.ptr(), std::min<std::size_t>(TAGS, static_cast<std::size_t>(frames.size())),
              frames_.begin());
  std::copy_n(outlines.ptr(),
              std::min<std::size_t>(TAGS, static_cast<std::size_t>(outlines.size())),
              outline_frames_.begin());
  faction_width_ = faction_width;
  shadow_x_ = shadow_x;
  sprite_of_faction_.assign(sprite_of_faction.ptr(),
                            sprite_of_faction.ptr() + sprite_of_faction.size());
  accents_.assign(accents.ptr(), accents.ptr() + accents.size());
}

void BodyView::set_relief(const godot::Dictionary& cells) {
  relief_size_ = cells["size"];
  const godot::PackedInt32Array levels = cells["level"];
  const godot::PackedByteArray slopes = cells["slope"];
  const godot::PackedInt32Array flat = cells["flat"];
  levels_.assign(levels.ptr(), levels.ptr() + levels.size());
  slopes_.assign(slopes.ptr(), slopes.ptr() + slopes.size());
  flat_.assign(flat.ptr(), flat.ptr() + flat.size());
  highest_ = 0;
  for (const std::int32_t level : levels_) {
    highest_ = std::max<std::int64_t>(highest_, level);
  }
}

void BodyView::set_occluders(const godot::PackedFloat32Array& occluders) {
  constexpr std::int64_t FIELDS = 5;
  occluders_.clear();
  for (std::int64_t i = 0; i + FIELDS <= occluders.size(); i += FIELDS) {
    const Occluder occluder{
        Rect2(occluders[i], occluders[i + 1], occluders[i + 2],
              occluders[i + 3]),
        occluders[i + 4]};
    const Rect2 rect = occluder.rect;
    for (std::int64_t bx = floor_div(rect.position.x, BUCKET);
         bx <= floor_div(rect.get_end().x, BUCKET); ++bx) {
      for (std::int64_t by = floor_div(rect.position.y, BUCKET);
           by <= floor_div(rect.get_end().y, BUCKET); ++by) {
        occluders_[bucket_key(bx, by)].push_back(occluder);
      }
    }
  }
}

void BodyView::set_viewer(std::int64_t army,
                          const godot::PackedInt64Array& relation_of_faction) {
  viewer_ = army;
  relation_of_faction_.assign(
      relation_of_faction.ptr(),
      relation_of_faction.ptr() + relation_of_faction.size());
}

void BodyView::set_damage_numbers(std::int64_t which) {
  damage_numbers_ = which;
}

void BodyView::set_steered(std::int64_t army) { steered_army_ = army; }

void BodyView::set_blood(bool on) { blood_ = on; }

void BodyView::set_hit_stop(bool on) { hit_stop_ = on; }

void BodyView::set_selected(std::int64_t person) { selected_ = person; }

void BodyView::set_font(const godot::Ref<godot::Font>& font,
                        std::int64_t size) {
  font_ = font;
  font_size_ = size;
}

void BodyView::clear_people() {
  armies_.clear();
  seen_.clear();
  health_.clear();
  peak_.clear();
  army_.clear();
  body_.clear();
  struck_tick_.clear();
  struck_by_lead_.clear();
  nearest_.clear();
  striker_.clear();
  hit_ms_.clear();
  away_.clear();
  struck_at_.clear();
  held_.clear();
  strikes_.clear();
  deaths_.clear();
  numbers_.clear();
  hit_stops_.clear();
  target_ = -1;
  target_ms_ = 0;
  shake_ms_ = NEVER;
  shake_pixels_ = 0;
  tick_ = 0;
  lead_ = -1;
}

void BodyView::begin(const godot::Dictionary& snapshot) {
  clear_people();
  read_armies(snapshot);
  read_bodies(snapshot, false);
}

void BodyView::read_armies(const godot::Dictionary& snapshot) {
  const godot::PackedInt64Array first = snapshot["first"];
  const godot::PackedInt64Array persons = snapshot["person"];
  const godot::PackedVector2Array positions = snapshot["position"];
  const godot::PackedInt64Array factions = snapshot["faction"];
  const std::int64_t count = factions.size();
  for (std::int64_t army = 0; army < count; ++army) {
    godot::PackedInt64Array roster = persons.slice(first[army], first[army + 1]);
    godot::PackedVector2Array now =
        positions.slice(first[army], first[army + 1]);
    if (army < static_cast<std::int64_t>(armies_.size())) {
      Army& known = armies_.at(static_cast<std::size_t>(army));
      known.before = known.now;
      known.now = now;
      known.same = roster == known.roster;
      known.roster = roster;
      known.faction = factions[army];
    } else {
      armies_.push_back(Army{roster, now, now, true, factions[army]});
    }
  }
}

void BodyView::grow(std::size_t size) {
  const std::size_t from = seen_.size();
  const std::size_t to = std::max(size, from * 2);
  seen_.resize(to, -2);
  health_.resize(to, 0);
  peak_.resize(to, 0);
  army_.resize(to, 0);
  body_.resize(to, 0);
  struck_tick_.resize(to, -1);
  struck_by_lead_.resize(to, false);
  nearest_.resize(to, 0.0);
  striker_.resize(to, 0);
  hit_ms_.resize(to, NEVER);
  away_.resize(to, Vector2());
  struck_at_.resize(to, -BODY_STRIKE_MS);
}

bool BodyView::alive(std::int64_t person) const {
  return person >= 0 && static_cast<std::size_t>(person) < seen_.size() &&
         seen_.at(static_cast<std::size_t>(person)) == tick_;
}

void BodyView::read_bodies(const godot::Dictionary& snapshot, bool compare) {
  const godot::PackedInt64Array first = snapshot["first"];
  const godot::PackedInt64Array persons = snapshot["person"];
  const godot::PackedInt64Array healths = snapshot["health"];
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    const std::int64_t start = first[static_cast<std::int64_t>(army)];
    const godot::PackedVector2Array& now = armies_.at(army).now;
    for (std::int64_t body = 0; body < now.size(); ++body) {
      const std::int64_t person = persons[start + body];
      const auto index = static_cast<std::size_t>(person);
      if (index >= seen_.size()) {
        grow(index + 1);
      }
      const std::int64_t health = healths[start + body];
      if (compare && seen_.at(index) == tick_ - 1 &&
          health < health_.at(index)) {
        hit(person, health_.at(index) - health, to_screen(now[body]));
      }
      seen_.at(index) = tick_;
      health_.at(index) = health;
      peak_.at(index) = std::max(peak_.at(index), health);
      army_.at(index) = static_cast<std::int64_t>(army);
      body_.at(index) = body;
    }
  }
}

godot::Dictionary BodyView::note_tick(const godot::Dictionary& snapshot,
                                      const godot::PackedInt64Array& died,
                                      std::int64_t steered_army,
                                      std::int64_t now) {
  // What a dead person or one struck is compared with is where it stood on
  // the tick before, so the armies are read anew only after the uses and
  // deaths.
  std::vector<godot::PackedVector2Array> stood;
  stood.reserve(armies_.size());
  for (const Army& army : armies_) {
    stood.push_back(army.now);
  }
  ++tick_;
  bleeds_ = 0;
  ms_ = now;
  steered_army_ = steered_army;
  hit_stops_.clear();
  bleed_at_.clear();
  bleed_dir_.clear();
  bleed_seed_.clear();
  lead_ = -1;
  {
    const godot::PackedInt64Array first = snapshot["first"];
    const godot::PackedInt64Array persons = snapshot["person"];
    if (steered_army >= 0 && steered_army + 1 < first.size() &&
        first[steered_army] < first[steered_army + 1]) {
      lead_ = persons[first[steered_army]];
    }
  }
  // A use that strikes only structures shows in the structures' stages
  // alone.
  const godot::PackedInt64Array strike_person = snapshot["strike_person"];
  const godot::PackedVector2Array origins = snapshot["strike_origin"];
  const godot::PackedVector2Array towards = snapshot["strike_toward"];
  const godot::PackedVector2Array ends = snapshot["strike_end"];
  const godot::PackedInt64Array strike_first = snapshot["strike_first"];
  const godot::PackedInt64Array struck = snapshot["struck"];
  const godot::PackedInt64Array shapes = snapshot["strike_shape"];
  const godot::PackedInt64Array ranges = snapshot["strike_range"];
  const godot::PackedInt64Array radii = snapshot["strike_radius"];
  const godot::PackedInt64Array damages = snapshot["strike_body_damage"];
  for (std::int64_t i = 0; i < strike_person.size(); ++i) {
    if (damages[i] <= 0) {
      continue;
    }
    Strike strike{strike_person[i],
                  shapes[i],
                  ranges[i],
                  radii[i],
                  damages[i],
                  origins[i],
                  towards[i],
                  ends[i],
                  struck.slice(strike_first[i], strike_first[i + 1]),
                  now,
                  strike_person[i] == lead_};
    const auto striker = static_cast<std::size_t>(strike.person);
    if (striker >= struck_at_.size()) {
      grow(striker + 1);
    }
    struck_at_.at(striker) = now;
    note_struck(strike, stood);
    strikes_.push_back(std::move(strike));
  }
  // The dead have left their rosters, so each is read as it stood on the
  // tick before; one no use struck died of hunger or age, and does not fall.
  for (const std::int64_t person : died) {
    const auto index = static_cast<std::size_t>(person);
    if (index < seen_.size() && seen_.at(index) == tick_ - 1 &&
        struck_tick_.at(index) == tick_) {
      const Vector2 where =
          stood.at(static_cast<std::size_t>(army_.at(index)))[body_.at(index)];
      const Vector2 at = to_screen(where);
      hit(person, health_.at(index), at);
      deaths_.push_back(Death{
          person, army_.at(index), at,
          where * Vector2(single(CELL_X), single(CELL_Y)) /
              static_cast<float>(murim::CELL_SIZE),
          now});
    }
  }
  read_armies(snapshot);
  read_bodies(snapshot, true);
  godot::PackedVector2Array drip_at;
  godot::PackedInt64Array drip_shape;
  if (blood_) {
    // Each maimed person on screen drips a drop at its feet every
    // DRIP_TICKS ticks, its own tick apart.
    for (const Army& army : armies_) {
      for (std::int64_t body = 0; body < army.now.size(); ++body) {
        const std::int64_t person = army.roster[body];
        if ((person + tick_) % DRIP_TICKS != 0 || wound(person) < 2) {
          continue;
        }
        const Vector2 at = to_screen(army.now[body]);
        if (view_.has_point(at)) {
          drip_at.push_back(
              at + Vector2(single((unit(person, tick_) * DRIP_SPREAD) -
                                  (DRIP_SPREAD / 2.0)),
                           0.0F));
          drip_shape.push_back(person);
        }
      }
    }
  }
  if (selected_ >= 0 && alive(selected_)) {
    target_ = selected_;
  } else if (target_ >= 0 &&
             (!alive(target_) || now - target_ms_ > TARGET_MS)) {
    target_ = -1;
  }
  // A hit stop starts holding its person afresh from the next fill.
  for (const Stop& stop : hit_stops_) {
    if (stop.person >= 0) {
      const auto found = held_.find(stop.person);
      const std::int64_t left =
          found == held_.end() ? 0 : found->second.left;
      Hold hold;
      hold.left = std::max(stop.frames, left);
      held_[stop.person] = hold;
    }
  }
  forget(now);
  godot::Dictionary laid;
  laid.set("bleed_at", bleed_at_);
  laid.set("bleed_dir", bleed_dir_);
  laid.set("bleed_seed", bleed_seed_);
  laid.set("drip_at", drip_at);
  laid.set("drip_shape", drip_shape);
  return laid;
}

void BodyView::note_struck(
    const Strike& strike, const std::vector<godot::PackedVector2Array>& stood) {
  double best = std::numeric_limits<double>::infinity();
  for (const std::int64_t person : strike.struck) {
    const auto index = static_cast<std::size_t>(person);
    if (index >= seen_.size() || seen_.at(index) != tick_ - 1) {
      continue;
    }
    const Vector2 where =
        stood.at(static_cast<std::size_t>(army_.at(index)))[body_.at(index)];
    const double near = where.distance_squared_to(strike.origin);
    if (struck_tick_.at(index) != tick_) {
      struck_tick_.at(index) = tick_;
      struck_by_lead_.at(index) = false;
      nearest_.at(index) = std::numeric_limits<double>::infinity();
    }
    if (near < nearest_.at(index)) {
      nearest_.at(index) = near;
      away_.at(index) = (where - strike.origin).normalized();
      striker_.at(index) = strike.person;
    }
    if (strike.steered) {
      struck_by_lead_.at(index) = true;
      const double toward = where.distance_squared_to(strike.toward);
      if (toward < best) {
        best = toward;
        target_ = person;
        target_ms_ = ms_;
      }
    }
  }
}

void BodyView::hit(std::int64_t person, std::int64_t lost, Vector2 at) {
  const auto index = static_cast<std::size_t>(person);
  const bool struck = struck_tick_.at(index) == tick_;
  hit_ms_.at(index) = ms_;
  if (!struck) {
    away_.at(index) = Vector2();
  }
  const bool own = person == lead_;
  const bool by_lead = struck && struck_by_lead_.at(index);
  const bool mine = own || by_lead;
  if (struck && blood_ &&
      (mine || (bleeds_ < MOST_BLEEDS &&
                view_.grow(single(CELL_X)).has_point(at)))) {
    if (!mine) {
      ++bleeds_;
    }
    const std::int64_t seed = (person * BLEED_SEED) + tick_;
    bleed_at_.push_back(at);
    bleed_dir_.push_back(away_.at(index));
    bleed_seed_.push_back(seed);
  }
  // A hit on or from the steered person holds the struck, and the one whose
  // use struck it nearest.
  if (hit_stop_ && struck && (own || by_lead)) {
    std::int64_t frames = HIT_STOP_MOST;
    for (const auto& step : HIT_STOP) {
      if (lost < step.at(0)) {
        frames = step.at(1);
        break;
      }
    }
    hit_stops_.push_back(Stop{person, frames});
    hit_stops_.push_back(Stop{striker_.at(index), frames});
    std::int64_t pixels = SHAKE_MOST;
    for (const auto& step : SHAKE) {
      if (lost < step.at(0)) {
        pixels = step.at(1);
        break;
      }
    }
    if (shake_ms_ != ms_) {
      shake_pixels_ = 0;
    }
    shake_ms_ = ms_;
    shake_pixels_ = std::max(shake_pixels_, pixels);
  }
  if (damage_numbers_ == 2 || (damage_numbers_ == 1 && mine)) {
    add_number(ms_, godot::String::num_int64(lost), at, own, mine);
  }
}

void BodyView::add_number(std::int64_t ms, const godot::String& text,
                          Vector2 at, bool own, bool mine) {
  double width = 0.0;
  if (font_.is_valid()) {
    width = font_
                ->get_string_size(text, godot::HORIZONTAL_ALIGNMENT_LEFT, -1,
                                  static_cast<std::int32_t>(font_size_))
                .x;
  }
  numbers_.push_back(Number{ms, text, at, own, mine, width});
}

void BodyView::forget(std::int64_t now) {
  strikes_.erase(std::remove_if(strikes_.begin(), strikes_.end(),
                                [now](const Strike& strike) {
                                  return now - strike.ms >= STRIKE_MS;
                                }),
                 strikes_.end());
  deaths_.erase(std::remove_if(deaths_.begin(), deaths_.end(),
                               [now](const Death& death) {
                                 return now - death.ms >= DEATH_MS;
                               }),
                deaths_.end());
  numbers_.erase(std::remove_if(numbers_.begin(), numbers_.end(),
                                [now](const Number& number) {
                                  return now - number.ms >= NUMBER_MS;
                                }),
                 numbers_.end());
}

double BodyView::flash(std::int64_t person, std::int64_t now) const {
  if (person < 0 || static_cast<std::size_t>(person) >= hit_ms_.size()) {
    return 0.0;
  }
  return std::clamp(
      1.0 - (static_cast<double>(now - hit_ms_.at(
                                           static_cast<std::size_t>(person))) /
             static_cast<double>(FLASH_MS)),
      0.0, 1.0);
}

Vector2 BodyView::knock(std::int64_t person, std::int64_t now) const {
  const double left = flash(person, now);
  if (left == 0.0) {
    return {};
  }
  return away_.at(static_cast<std::size_t>(person)) * single(KNOCK_PIXELS) *
         single(left);
}

std::int64_t BodyView::wound(std::int64_t person) const {
  if (person < 0 || static_cast<std::size_t>(person) >= peak_.size()) {
    return 0;
  }
  const auto index = static_cast<std::size_t>(person);
  if (peak_.at(index) <= 0) {
    return 0;
  }
  const std::int64_t share = health_.at(index) * PER_MILLE / peak_.at(index);
  if (share < MAIMED) {
    return 2;
  }
  return share < WOUNDED ? 1 : 0;
}

Vector2i BodyView::place_of(std::int64_t person) const {
  if (!alive(person)) {
    return {-1, -1};
  }
  const auto index = static_cast<std::size_t>(person);
  return {static_cast<std::int32_t>(army_.at(index)),
          static_cast<std::int32_t>(body_.at(index))};
}

std::int64_t BodyView::target() const { return target_; }

Vector2i BodyView::shake(std::int64_t now) const {
  if (!hit_stop_ || now - shake_ms_ >= SHAKE_MS || now < shake_ms_) {
    return {};
  }
  const auto step = static_cast<std::size_t>(
      ((now - shake_ms_) / SHAKE_STEP_MS) %
      static_cast<std::int64_t>(SHAKE_PATTERN.size()));
  const auto pixels = static_cast<std::int32_t>(shake_pixels_);
  return {SHAKE_PATTERN.at(step).at(0) * pixels,
          SHAKE_PATTERN.at(step).at(1) * pixels};
}

std::int64_t BodyView::held_count() const {
  return static_cast<std::int64_t>(held_.size());
}

bool BodyView::held(std::int64_t person) const {
  return held_.find(person) != held_.end();
}

double BodyView::unit(std::int64_t seed, std::int64_t k) {
  // GDScript's integers wrap on overflow, which unsigned arithmetic does here.
  auto wrap = [](std::uint64_t value) { return static_cast<std::int64_t>(value); };
  std::int64_t h = wrap(static_cast<std::uint64_t>(seed) * UNIT_X) ^
                   wrap(static_cast<std::uint64_t>(k) * UNIT_Y);
  h = wrap(static_cast<std::uint64_t>(h ^ (h >> UNIT_SHIFT_A)) *
           static_cast<std::uint64_t>(UNIT_MIX));
  const std::int64_t mixed = h ^ (h >> UNIT_SHIFT_B);
  const std::int64_t magnitude = mixed < 0 ? -mixed : mixed;
  return static_cast<double>(magnitude % UNIT_RANGE) /
         static_cast<double>(UNIT_RANGE);
}

godot::TypedArray<Vector2i> BodyView::shake_pattern() {
  godot::TypedArray<Vector2i> steps;
  for (const auto& step : SHAKE_PATTERN) {
    steps.push_back(Vector2i(step.at(0), step.at(1)));
  }
  return steps;
}

std::int64_t BodyView::cell_index(Vector2i at) const {
  const std::int64_t x = std::clamp<std::int64_t>(at.x, 0, relief_size_.x - 1);
  const std::int64_t y = std::clamp<std::int64_t>(at.y, 0, relief_size_.y - 1);
  return (y * relief_size_.x) + x;
}

Vector2 BodyView::warped(Vector2 point) const {
  const Vector2 scaled = point / single(WARP_SCALE);
  const Vector2 noise(
      single(fbm(scaled + Vector2(single(WARP_SALT_X), 0.0F), WARP_NOISE_X)),
      single(fbm(scaled + Vector2(0.0F, single(WARP_SALT_Y)), WARP_NOISE_Y)));
  return point + (noise - Vector2(0.5F, 0.5F)) * 2.0F * single(WARP);
}

double BodyView::height_at(Vector2 point) const {
  const Vector2 cells = (point / Vector2(single(CELL_X), single(CELL_Y))).floor();
  const std::int64_t on = cell_index(
      Vector2i(static_cast<std::int32_t>(cells.x),
               static_cast<std::int32_t>(cells.y)));
  if (flat_.at(static_cast<std::size_t>(on)) > 0) {
    return static_cast<double>(flat_.at(static_cast<std::size_t>(on)) - 1);
  }
  const Vector2 q = warped(point);
  const Vector2 uv = Vector2(single(q.x / CELL_X), single(q.y / CELL_Y)) -
                     Vector2(0.5F, 0.5F);
  const Vector2 corner_of = uv.floor();
  const Vector2i c(static_cast<std::int32_t>(corner_of.x),
                   static_cast<std::int32_t>(corner_of.y));
  const Vector2 f = uv - corner_of;
  const std::array<Vector2i, 4> corners{c, c + Vector2i(1, 0),
                                        c + Vector2i(0, 1), c + Vector2i(1, 1)};
  std::array<double, 4> levels{};
  std::array<double, 4> slopes{};
  for (std::size_t n = 0; n < corners.size(); ++n) {
    const auto index = static_cast<std::size_t>(cell_index(corners.at(n)));
    levels.at(n) = static_cast<double>(levels_.at(index));
    slopes.at(n) = slopes_.at(index) == 1 ? 1.0 : 0.0;
  }
  const double level = lerpf(lerpf(levels[0], levels[1], f.x),
                             lerpf(levels[2], levels[3], f.x), f.y);
  const double slope = lerpf(lerpf(slopes[0], slopes[1], f.x),
                             lerpf(slopes[2], slopes[3], f.x), f.y);
  double best = levels[0];
  for (std::size_t n = 1; n < levels.size(); ++n) {
    const double here = std::abs(levels.at(n) - level);
    const double there = std::abs(best - level);
    if (here < there || (here == there && levels.at(n) > best)) {
      best = levels.at(n);
    }
  }
  return lerpf(best, level, smoothstep(0.0, SLOPE_BLEND, slope));
}

double BodyView::lift_at(Vector2 point) const {
  if (flat_.empty()) {
    return 0.0;
  }
  const std::int64_t y = std::clamp<std::int64_t>(
      static_cast<std::int64_t>(point.y / CELL_Y), 0, relief_size_.y - 1);
  const std::int64_t x = std::clamp<std::int64_t>(
      static_cast<std::int64_t>(point.x / CELL_X), 0, relief_size_.x - 1);
  const std::int32_t flat =
      flat_.at(static_cast<std::size_t>((y * relief_size_.x) + x));
  if (flat > 0) {
    return static_cast<double>(flat - 1) * LEVEL_PX;
  }
  return height_at(point) * LEVEL_PX;
}

Vector2 BodyView::to_screen(Vector2 sub) const {
  const Vector2 on_ground = sub * Vector2(single(CELL_X), single(CELL_Y)) /
                            static_cast<float>(murim::CELL_SIZE);
  return on_ground - Vector2(0.0F, single(lift_at(on_ground)));
}

Vector2 BodyView::sub_at(std::int64_t army, std::int64_t body,
                         double between) const {
  const Army& known = armies_.at(static_cast<std::size_t>(army));
  Vector2 now = known.now[body];
  if (known.same && body < known.before.size()) {
    now = known.before[body].lerp(now, single(std::clamp(between, 0.0, 1.0)));
  }
  return now;
}

Vector2 BodyView::body_at(std::int64_t army, std::int64_t body,
                          double between) const {
  return to_screen(sub_at(army, body, between));
}

std::int64_t BodyView::sprite_of(std::int64_t army) const {
  if (sprite_of_faction_.empty()) {
    return 0;
  }
  const std::int64_t faction = armies_.at(static_cast<std::size_t>(army)).faction;
  return sprite_of_faction_.at(static_cast<std::size_t>(
      faction % static_cast<std::int64_t>(sprite_of_faction_.size())));
}

Color BodyView::accent(std::int64_t person) const {
  const std::int64_t army = place_of(person).x;
  if (army < 0) {
    return rgb(PAPER_HEX);
  }
  const auto sprite = static_cast<std::size_t>(sprite_of(army));
  return sprite < accents_.size() ? accents_.at(sprite) : rgb(PAPER_HEX);
}

bool BodyView::hidden(Rect2 rect, double sort_y) const {
  for (std::int64_t bx = floor_div(rect.position.x, BUCKET);
       bx <= floor_div(rect.get_end().x, BUCKET); ++bx) {
    for (std::int64_t by = floor_div(rect.position.y, BUCKET);
         by <= floor_div(rect.get_end().y, BUCKET); ++by) {
      const auto found = occluders_.find(bucket_key(bx, by));
      if (found == occluders_.end()) {
        continue;
      }
      for (const Occluder& occluder : found->second) {
        if (occluder.sort_y > sort_y && occluder.rect.intersects(rect)) {
          return true;
        }
      }
    }
  }
  return false;
}

void BodyView::pack(godot::PackedFloat32Array& buffer, Rect2 frame,
                    Vector2 centre, bool flip, double white, double alpha,
                    double turn, Vector2 outline_of) const {
  const double width = frame.size.x * (flip ? -1.0 : 1.0);
  const double height = frame.size.y;
  const double cos_turn = std::cos(turn);
  const double sin_turn = std::sin(turn);
  const Vector2 local =
      centre.round() + Vector2(single(height / 2.0 * sin_turn),
                               single(height / 2.0 * (1.0 - cos_turn)));
  const bool body = outline_of.x < 0.0F;
  const Vector2 custom = body ? Vector2(single(white), single(alpha))
                              : outline_of / atlas_size_;
  const Color colour = body ? Color(1, 1, 1, 1) : Color(single(white), 0, 0, 1);
  const std::int64_t at = buffer.size();
  buffer.resize(at + BODY_FLOATS);
  float* out = buffer.ptrw() + at;
  const std::array<float, BODY_FLOATS> values{
      single(width * cos_turn),
      single(height * sin_turn),
      0.0F,
      local.x,
      single(width * sin_turn),
      single(-height * cos_turn),
      0.0F,
      local.y,
      colour.r,
      colour.g,
      colour.b,
      colour.a,
      frame.position.x / atlas_size_.x,
      frame.position.y / atlas_size_.y,
      custom.x,
      custom.y};
  std::copy(values.begin(), values.end(), out);
}

godot::Dictionary BodyView::fill(Rect2 view, double between,
                                 std::int64_t now) {
  const Rect2 shown = view.grow(single(MARGIN));
  const std::int64_t rows =
      relief_size_.y > 0 ? relief_size_.y : std::numeric_limits<std::int32_t>::max();
  const std::int64_t first_row =
      std::max<std::int64_t>(static_cast<std::int64_t>(shown.position.y / CELL_Y), 0);
  const std::int64_t last_row = std::min<std::int64_t>(
      static_cast<std::int64_t>(
          (shown.get_end().y + (static_cast<double>(highest_) * LEVEL_PX)) /
          CELL_Y) +
          1,
      rows + 1);
  const std::int64_t band_count = std::max<std::int64_t>(last_row - first_row + 1, 0);
  std::vector<godot::PackedFloat32Array> shadows(
      static_cast<std::size_t>(band_count));
  std::vector<godot::PackedFloat32Array> buffers(
      static_cast<std::size_t>(band_count));
  godot::PackedFloat32Array outlines;
  const std::int64_t walk = (now / WALK_FRAME_MS) % WALK_FRAMES;
  std::int64_t drawn = 0;
  std::int64_t outlined = 0;
  struct Near {
    std::int64_t person;
    Vector2 head;
    std::int64_t relation;
    double cells;
  };
  std::vector<Near> near;
  const bool named = viewer_ >= 0 &&
                     viewer_ < static_cast<std::int64_t>(armies_.size()) &&
                     !armies_.at(static_cast<std::size_t>(viewer_)).now.is_empty();
  const Vector2 named_from = named ? sub_at(viewer_, 0, between) : Vector2();
  const float share = single(std::clamp(between, 0.0, 1.0));
  const Vector2 pixels = Vector2(single(CELL_X), single(CELL_Y)) /
                         static_cast<float>(murim::CELL_SIZE);
  const Vector2 frame_size(single(FRAME_W), single(FRAME_H));
  auto band_y = [](std::int64_t row) {
    return (static_cast<double>(row) * CELL_Y) + (CELL_Y / 2.0);
  };
  // A hit stop lasts its frames whether or not its body is drawn, and holds
  // no one dead.
  for (auto it = held_.begin(); it != held_.end();) {
    if (!alive(it->first)) {
      it = held_.erase(it);
    } else {
      ++it;
    }
  }
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    const Army& known = armies_.at(army);
    const std::int64_t sprite = sprite_of(static_cast<std::int64_t>(army));
    const std::int64_t relation =
        known.faction >= 0 && static_cast<std::size_t>(known.faction) <
                                  relation_of_faction_.size()
            ? relation_of_faction_.at(static_cast<std::size_t>(known.faction))
            : -1;
    for (std::int64_t body = 0; body < known.now.size(); ++body) {
      const std::int64_t person = known.roster[body];
      const bool moving = known.same && body < known.before.size();
      const Vector2 sub = moving ? known.before[body].lerp(known.now[body], share)
                                 : known.now[body];
      const Vector2 on_ground = sub * pixels;
      Vector2 at = on_ground - Vector2(0.0F, single(lift_at(on_ground))) +
                   knock(person, now);
      const auto holding = held_.find(person);
      if (holding != held_.end() && holding->second.drawn) {
        at = holding->second.at;
      }
      if (!shown.has_point(at)) {
        continue;
      }
      const double cells =
          static_cast<double>((sub - named_from).length()) /
          static_cast<double>(murim::CELL_SIZE);
      if (named && cells <= NAME_REACH &&
          (static_cast<std::int64_t>(army) != viewer_ || body > 0)) {
        near.push_back(Near{person, at - Vector2(0.0F, single(HEAD)),
                            relation, cells});
      }
      const std::int64_t row = std::clamp<std::int64_t>(
          static_cast<std::int64_t>(on_ground.y / CELL_Y), first_row, last_row);
      if (band_count == 0) {
        continue;
      }
      const Vector2 heading = body < known.before.size()
                                  ? known.now[body] - known.before[body]
                                  : Vector2();
      std::int64_t tag = TAG_IDLE;
      std::int64_t index = 0;
      const auto who = static_cast<std::size_t>(person);
      const std::int64_t struck =
          who < struck_at_.size() ? struck_at_.at(who) : -BODY_STRIKE_MS;
      if (struck > 0 && now - struck < BODY_STRIKE_MS) {
        tag = TAG_STRIKE;
        index = now - struck < BODY_STRIKE_MS / 2 ? 0 : 1;
      } else if (heading != Vector2()) {
        tag = heading.y < 0.0F && -heading.y > std::abs(heading.x) ? TAG_BACK
                                                                    : TAG_WALK;
        index = walk;
      }
      // Facing the viewer, standing or walking, a wounded body stoops and is
      // stained.
      if (tag == TAG_IDLE || tag == TAG_WALK) {
        tag = WOUND_TAGS.at(static_cast<std::size_t>(tag))
                  .at(static_cast<std::size_t>(wound(person)));
      }
      const Vector2 centre(at.x, single(at.y - FEET + (FRAME_H / 2.0)));
      Rect2 frame(
          Vector2(static_cast<float>((sprite * faction_width_) +
                                     ((frames_.at(static_cast<std::size_t>(tag)) +
                                       index) *
                                      static_cast<std::int64_t>(FRAME_W))),
                  0.0F),
          frame_size);
      const std::int64_t band = row - first_row;
      const Vector2 origin(0.0F, single(band_y(row)));
      // Rounded before the band's origin is taken off, so a body stands on
      // the same whole pixel in every band.
      pack(shadows.at(static_cast<std::size_t>(band)),
           Rect2(Vector2(static_cast<float>(shadow_x_), 0.0F), frame_size),
           at.round() - origin, false);
      double white = flash(person, now);
      bool flip = heading.x < 0.0F;
      if (holding != held_.end()) {
        Hold& hold = holding->second;
        if (hold.drawn) {
          tag = hold.tag;
          frame = hold.frame;
          white = hold.white;
          flip = hold.flip;
        } else {
          hold.drawn = true;
          hold.at = at;
          hold.tag = tag;
          hold.frame = frame;
          hold.white = white;
          hold.flip = flip;
        }
      }
      pack(buffers.at(static_cast<std::size_t>(band)), frame,
           centre.round() - origin, flip, white);
      if (hidden(Rect2(centre - frame_size / 2.0F, frame_size).grow(-2.0F),
                 origin.y)) {
        Rect2 outline = frame;
        outline.position.x += static_cast<float>(
            (outline_frames_.at(static_cast<std::size_t>(tag)) -
             frames_.at(static_cast<std::size_t>(tag))) *
            static_cast<std::int64_t>(FRAME_W));
        pack(outlines, outline, centre, flip, white, 1.0, 0.0, frame.position);
        ++outlined;
      }
      ++drawn;
    }
  }
  // A hold counts down once a frame, drawn or not.
  for (auto it = held_.begin(); it != held_.end();) {
    it->second.left -= 1;
    if (it->second.left <= 0) {
      it = held_.erase(it);
    } else {
      ++it;
    }
  }
  // The dead fall a quarter turn about their feet and fade where they last
  // stood, flashing as their killing blow lands.
  for (const Death& death : deaths_) {
    const double age = static_cast<double>(now - death.ms) /
                       static_cast<double>(DEATH_MS);
    if (age < 0.0 || age >= 1.0 || !shown.has_point(death.at) ||
        band_count == 0) {
      continue;
    }
    const std::int64_t row = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(death.ground.y / CELL_Y), first_row,
        last_row);
    const Rect2 frame(
        Vector2(static_cast<float>(sprite_of(death.army) * faction_width_), 0.0F),
        frame_size);
    pack(buffers.at(static_cast<std::size_t>(row - first_row)), frame,
         Vector2(death.at.x, single(death.at.y - FEET + (FRAME_H / 2.0)))
                 .round() -
             Vector2(0.0F, single(band_y(row))),
         false, flash(death.person, now), 1.0 - age,
         std::min(age * DEATH_FALL, 1.0) * godot::Math::PI / 2.0);
  }
  godot::Array bands;
  for (std::size_t band = 0; band < shadows.size(); ++band) {
    shadows.at(band).append_array(buffers.at(band));
    bands.push_back(shadows.at(band));
  }
  std::stable_sort(near.begin(), near.end(),
                   [](const Near& a, const Near& b) { return a.cells < b.cells; });
  godot::PackedInt64Array near_person;
  godot::PackedVector2Array near_head;
  godot::PackedInt64Array near_relation;
  godot::PackedFloat64Array near_cells;
  for (const Near& one : near) {
    near_person.push_back(one.person);
    near_head.push_back(one.head);
    near_relation.push_back(one.relation);
    near_cells.push_back(one.cells);
  }
  const bool lead = steered_army_ >= 0 &&
                    steered_army_ < static_cast<std::int64_t>(armies_.size()) &&
                    !armies_.at(static_cast<std::size_t>(steered_army_)).now.is_empty();
  const Vector2i aimed = place_of(target_);
  const Vector2 target_at =
      aimed.x >= 0 ? body_at(aimed.x, aimed.y, between) : Vector2();
  show_at(view, now, target_at);
  godot::Dictionary filled;
  filled.set("bands", bands);
  filled.set("first_row", first_row);
  filled.set("outlines", outlines);
  filled.set("drawn", drawn);
  filled.set("outlined", outlined);
  filled.set("near_person", near_person);
  filled.set("near_head", near_head);
  filled.set("near_relation", near_relation);
  filled.set("near_cells", near_cells);
  filled.set("lead", lead);
  filled.set("lead_at", lead ? body_at(steered_army_, 0, between).round() : Vector2());
  filled.set("target", aimed.x >= 0);
  filled.set("target_at", target_at);
  return filled;
}

void BodyView::show_at(Rect2 view, std::int64_t now, Vector2 target_at) {
  view_ = view;
  now_ = now;
  target_at_ = target_at;
}

Rect2 BodyView::view() const { return view_; }

void BodyView::draw_effects(godot::RID canvas_item) const {
  for (const Strike& strike : strikes_) {
    if (now_ - strike.ms < STRIKE_MS &&
        (strike.steered || view_.has_point(to_screen(strike.origin)))) {
      draw_strike(canvas_item, strike,
                  1.0 - (static_cast<double>(now_ - strike.ms) /
                         static_cast<double>(STRIKE_MS)));
    }
  }
  if (target_ >= 0) {
    draw_target(canvas_item, target_at_.round());
  }
  if (!font_.is_valid()) {
    return;
  }
  const godot::TypedArray<godot::Dictionary> shown = shown_numbers(now_);
  for (std::int64_t i = 0; i < shown.size(); ++i) {
    const godot::Dictionary number = shown[i];
    const Vector2 at = number["at"];
    const godot::String text = number["text"];
    const Color colour = number["colour"];
    font_->draw_string(canvas_item, at + Vector2(1, 1), text,
                       godot::HORIZONTAL_ALIGNMENT_LEFT, -1,
                       static_cast<std::int32_t>(font_size_),
                       with_alpha(rgb(INK_HEX), colour.a));
    font_->draw_string(canvas_item, at, text, godot::HORIZONTAL_ALIGNMENT_LEFT,
                       -1, static_cast<std::int32_t>(font_size_), colour);
  }
}

void BodyView::draw_strike(godot::RID canvas_item, const Strike& strike,
                           double left) const {
  godot::RenderingServer* server = godot::RenderingServer::get_singleton();
  const Color colour = strike.steered ? rgb(JADE_HEX) : accent(strike.person);
  const Color fill = with_alpha(colour, FILL_ALPHA * left);
  const Color edge = with_alpha(colour, EDGE_ALPHA * left);
  const Vector2 origin = to_screen(strike.origin);
  const Vector2 toward = to_screen(strike.toward);
  const double scale = CELL_X / static_cast<double>(murim::CELL_SIZE);
  const double reach = static_cast<double>(strike.range) * scale;
  const double radius =
      std::max(static_cast<double>(strike.radius) * scale, LEAST_RADIUS);
  // Distances on the ground are foreshortened down the screen: a disc on it
  // is drawn as an ellipse.
  const Vector2 flat(1.0F, single(CELL_Y / CELL_X));
  const godot::PackedColorArray fills{fill};
  const godot::PackedColorArray edges{edge};
  const auto shape = static_cast<murim::Shape>(strike.shape);
  if (shape == murim::Shape::ARC) {
    // An arc used toward its user's own place sweeps all around.
    const Vector2 way = strike.toward - strike.origin;
    const double facing = way != Vector2() ? way.angle() : 0.0;
    const double sweep =
        way != Vector2()
            ? godot::Math::deg_to_rad(
                  static_cast<double>(murim::ARC_HALF_WIDTH_DEGREES))
            : godot::Math::PI;
    godot::PackedVector2Array points{origin};
    for (int n = 0; n <= ARC_SEGMENTS; ++n) {
      points.push_back(
          origin +
          Vector2::from_angle(single(facing - sweep +
                                     (2.0 * sweep * n / ARC_SEGMENTS))) *
              single(reach) * flat);
    }
    server->canvas_item_add_polygon(canvas_item, points, fills);
    server->canvas_item_add_set_transform(
        canvas_item, godot::Transform2D(0.0F, flat, 0.0F, origin));
    godot::PackedVector2Array line;
    const int count = ARC_SEGMENTS + 1;
    for (int i = 0; i < count; ++i) {
      const double theta =
          (static_cast<double>(i) / (static_cast<double>(count) - 1.0)) *
              std::clamp(2.0 * sweep, -godot::Math::TAU, godot::Math::TAU) +
          (facing - sweep);
      line.push_back(Vector2(single(std::cos(theta)), single(std::sin(theta))) *
                     single(reach));
    }
    server->canvas_item_add_polyline(canvas_item, line, edges, 1.0F);
    server->canvas_item_add_set_transform(canvas_item, godot::Transform2D());
  } else if (shape == murim::Shape::LINE || shape == murim::Shape::TARGETED) {
    const Vector2 end = to_screen(strike.end);
    server->canvas_item_add_line(canvas_item, origin, end, fill,
                                 single(std::max(radius * 2.0 * flat.y, 1.0)));
    server->canvas_item_add_circle(canvas_item, origin.lerp(end, single(1.0 - left)),
                                   single(PROJECTILE), edge);
  } else {
    server->canvas_item_add_set_transform(
        canvas_item, godot::Transform2D(0.0F, flat, 0.0F, toward));
    server->canvas_item_add_circle(canvas_item, Vector2(), single(radius), fill);
    godot::PackedVector2Array line;
    for (int i = 0; i < DISC_POINTS; ++i) {
      const double theta = (static_cast<double>(i) /
                            (static_cast<double>(DISC_POINTS) - 1.0)) *
                           godot::Math::TAU;
      line.push_back(Vector2(single(std::cos(theta)), single(std::sin(theta))) *
                     single(radius));
    }
    server->canvas_item_add_polyline(canvas_item, line, edges, 1.0F);
    server->canvas_item_add_set_transform(canvas_item, godot::Transform2D());
  }
}

void BodyView::draw_target(godot::RID canvas_item, Vector2 at) const {
  godot::RenderingServer* server = godot::RenderingServer::get_singleton();
  const Rect2 box(at + Vector2(single(BRACKET_LEFT), single(BRACKET_TOP - BODY_HEIGHT)),
                  Vector2(single(BRACKET_W), single(BRACKET_H)));
  const std::array<Vector2, 4> corners{
      box.position, Vector2(box.get_end().x, box.position.y),
      Vector2(box.position.x, box.get_end().y), box.get_end()};
  for (const Vector2 corner : corners) {
    const float sx = corner.x == box.position.x ? 1.0F : -1.0F;
    const float sy = corner.y == box.position.y ? 1.0F : -1.0F;
    server->canvas_item_add_line(
        canvas_item, corner, corner + Vector2(single(BRACKET_ARM) * sx, 0.0F),
        rgb(CRIMSON_HEX), 1.0F);
    server->canvas_item_add_line(
        canvas_item, corner, corner + Vector2(0.0F, single(BRACKET_ARM) * sy),
        rgb(CRIMSON_HEX), 1.0F);
  }
}

godot::TypedArray<godot::Dictionary> BodyView::hit_stops() const {
  godot::TypedArray<godot::Dictionary> stops;
  for (const Stop& stop : hit_stops_) {
    godot::Dictionary fields;
    fields.set("person", stop.person);
    fields.set("frames", stop.frames);
    stops.push_back(fields);
  }
  return stops;
}

godot::TypedArray<godot::Dictionary> BodyView::strikes() const {
  godot::TypedArray<godot::Dictionary> kept;
  for (const Strike& strike : strikes_) {
    godot::Dictionary fields;
    fields.set("person", strike.person);
    fields.set("shape", strike.shape);
    fields.set("range", strike.range);
    fields.set("radius", strike.radius);
    fields.set("body_damage", strike.body_damage);
    fields.set("origin", strike.origin);
    fields.set("toward", strike.toward);
    fields.set("end", strike.end);
    fields.set("struck", strike.struck);
    fields.set("ms", strike.ms);
    fields.set("steered", strike.steered);
    kept.push_back(fields);
  }
  return kept;
}

godot::TypedArray<godot::Dictionary> BodyView::shown_strikes(
    std::int64_t now) const {
  const godot::TypedArray<godot::Dictionary> kept = strikes();
  godot::TypedArray<godot::Dictionary> shown;
  for (std::size_t i = 0; i < strikes_.size(); ++i) {
    const Strike& strike = strikes_.at(i);
    if (now - strike.ms < STRIKE_MS &&
        (strike.steered || view_.has_point(to_screen(strike.origin)))) {
      shown.push_back(kept[static_cast<std::int64_t>(i)]);
    }
  }
  return shown;
}

godot::TypedArray<godot::Dictionary> BodyView::deaths() const {
  godot::TypedArray<godot::Dictionary> kept;
  for (const Death& death : deaths_) {
    godot::Dictionary fields;
    fields.set("person", death.person);
    fields.set("army", death.army);
    fields.set("at", death.at);
    fields.set("ground", death.ground);
    fields.set("ms", death.ms);
    kept.push_back(fields);
  }
  return kept;
}

godot::TypedArray<godot::Dictionary> BodyView::dying(std::int64_t now) const {
  godot::TypedArray<godot::Dictionary> shown;
  for (const Death& death : deaths_) {
    const double age = static_cast<double>(now - death.ms) /
                       static_cast<double>(DEATH_MS);
    if (age >= 0.0 && age < 1.0) {
      godot::Dictionary fields;
      fields.set("army", death.army);
      fields.set("at", death.at);
      fields.set("ground", death.ground);
      fields.set("fallen", std::min(age * DEATH_FALL, 1.0));
      fields.set("alpha", 1.0 - age);
      fields.set("flash", flash(death.person, now));
      shown.push_back(fields);
    }
  }
  return shown;
}

godot::TypedArray<godot::Dictionary> BodyView::numbers() const {
  godot::TypedArray<godot::Dictionary> kept;
  for (const Number& number : numbers_) {
    godot::Dictionary fields;
    fields.set("ms", number.ms);
    fields.set("text", number.text);
    fields.set("at", number.at);
    fields.set("own", number.own);
    fields.set("mine", number.mine);
    fields.set("width", number.width);
    kept.push_back(fields);
  }
  return kept;
}

godot::TypedArray<godot::Dictionary> BodyView::shown_numbers(
    std::int64_t now) const {
  godot::TypedArray<godot::Dictionary> shown;
  if (damage_numbers_ == 0) {
    return shown;
  }
  for (const Number& number : numbers_) {
    const double age = static_cast<double>(now - number.ms) /
                       static_cast<double>(NUMBER_MS);
    if (age < 0.0 || age >= 1.0 || !view_.has_point(number.at) ||
        (damage_numbers_ == 1 && !number.mine)) {
      continue;
    }
    Color colour = number.own ? rgb(CRIMSON_HEX) : rgb(PAPER_HEX);
    colour.a = single(1.0 - (age * age));
    godot::Dictionary fields;
    fields.set("text", number.text);
    fields.set("colour", colour);
    fields.set("at", (number.at + Vector2(single(-number.width / 2.0),
                                          single(-BODY_HEIGHT -
                                                 (NUMBER_RISE * age))))
                         .round());
    shown.push_back(fields);
  }
  return shown;
}

void BodyView::_bind_methods() {
  using godot::ClassDB;
  using godot::D_METHOD;
  ClassDB::bind_method(D_METHOD("set_atlas", "size", "frames", "outlines",
                                "faction_width", "shadow_x",
                                "sprite_of_faction", "accents"),
                       &BodyView::set_atlas);
  ClassDB::bind_method(D_METHOD("set_relief", "cells"), &BodyView::set_relief);
  ClassDB::bind_method(D_METHOD("set_occluders", "occluders"),
                       &BodyView::set_occluders);
  ClassDB::bind_method(D_METHOD("set_viewer", "army", "relation_of_faction"),
                       &BodyView::set_viewer);
  ClassDB::bind_method(D_METHOD("set_damage_numbers", "which"),
                       &BodyView::set_damage_numbers);
  ClassDB::bind_method(D_METHOD("set_steered", "army"), &BodyView::set_steered);
  ClassDB::bind_method(D_METHOD("set_blood", "on"), &BodyView::set_blood);
  ClassDB::bind_method(D_METHOD("set_hit_stop", "on"), &BodyView::set_hit_stop);
  ClassDB::bind_method(D_METHOD("set_selected", "person"),
                       &BodyView::set_selected);
  ClassDB::bind_method(D_METHOD("set_font", "font", "size"),
                       &BodyView::set_font);
  ClassDB::bind_method(D_METHOD("begin", "snapshot"), &BodyView::begin);
  ClassDB::bind_method(
      D_METHOD("note_tick", "snapshot", "died", "steered_army", "now"),
      &BodyView::note_tick);
  ClassDB::bind_method(D_METHOD("fill", "view", "between", "now"),
                       &BodyView::fill);
  ClassDB::bind_method(D_METHOD("draw_effects", "canvas_item"),
                       &BodyView::draw_effects);
  ClassDB::bind_method(D_METHOD("body_at", "army", "body", "between"),
                       &BodyView::body_at);
  ClassDB::bind_method(D_METHOD("to_screen", "sub"), &BodyView::to_screen);
  ClassDB::bind_method(D_METHOD("flash", "person", "now"), &BodyView::flash);
  ClassDB::bind_method(D_METHOD("knock", "person", "now"), &BodyView::knock);
  ClassDB::bind_method(D_METHOD("wound", "person"), &BodyView::wound);
  ClassDB::bind_method(D_METHOD("place_of", "person"), &BodyView::place_of);
  ClassDB::bind_method(D_METHOD("target"), &BodyView::target);
  ClassDB::bind_method(D_METHOD("shake", "now"), &BodyView::shake);
  ClassDB::bind_method(D_METHOD("held_count"), &BodyView::held_count);
  ClassDB::bind_method(D_METHOD("held", "person"), &BodyView::held);
  ClassDB::bind_method(D_METHOD("hit_stops"), &BodyView::hit_stops);
  ClassDB::bind_method(D_METHOD("strikes"), &BodyView::strikes);
  ClassDB::bind_method(D_METHOD("shown_strikes", "now"),
                       &BodyView::shown_strikes);
  ClassDB::bind_method(D_METHOD("deaths"), &BodyView::deaths);
  ClassDB::bind_method(D_METHOD("dying", "now"), &BodyView::dying);
  ClassDB::bind_method(D_METHOD("numbers"), &BodyView::numbers);
  ClassDB::bind_method(D_METHOD("shown_numbers", "now"),
                       &BodyView::shown_numbers);
  ClassDB::bind_method(
      D_METHOD("add_number", "ms", "text", "at", "own", "mine"),
      &BodyView::add_number);
  ClassDB::bind_method(D_METHOD("show_at", "view", "now", "target_at"),
                       &BodyView::show_at);
  ClassDB::bind_method(D_METHOD("view"), &BodyView::view);
  ClassDB::bind_static_method("BodyView", D_METHOD("unit", "seed", "k"),
                              &BodyView::unit);
  ClassDB::bind_static_method("BodyView", D_METHOD("shake_pattern"),
                              &BodyView::shake_pattern);
  const godot::StringName name = get_class_static();
  ClassDB::bind_integer_constant(name, "", "STRIKE_MS", STRIKE_MS);
  ClassDB::bind_integer_constant(name, "", "FLASH_MS", FLASH_MS);
  ClassDB::bind_integer_constant(name, "", "DEATH_MS", DEATH_MS);
  ClassDB::bind_integer_constant(name, "", "NUMBER_MS", NUMBER_MS);
  ClassDB::bind_integer_constant(name, "", "SHAKE_MS", SHAKE_MS);
  ClassDB::bind_integer_constant(name, "", "MOST_BLEEDS", MOST_BLEEDS);
  ClassDB::bind_integer_constant(name, "", "DRIP_TICKS", DRIP_TICKS);
  ClassDB::bind_integer_constant(name, "", "FRAME_WIDTH", FRAME_WIDTH);
  ClassDB::bind_integer_constant(name, "", "FRAME_HEIGHT", FRAME_HEIGHT);
  ClassDB::bind_integer_constant(name, "", "FEET", FEET_ROW);
  ClassDB::bind_integer_constant(name, "", "HEAD", HEAD_RISE);
  ClassDB::bind_integer_constant(name, "", "BODY_FLOATS", BODY_FLOATS);
  ClassDB::bind_integer_constant(name, "", "NAME_REACH", NAME_CELLS);
}

}  // namespace murim_godot
