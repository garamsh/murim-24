#include "body_view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/math_defs.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "map.hpp"
#include "skill.hpp"

namespace murim_godot {

namespace {

using Point = BodyView::Point;
using Box = BodyView::Box;
using Rgba = BodyView::Rgba;

// The arithmetic below mirrors the GDScript it replaced, world_art.gd,
// combat.gd and relief.gd, so a body lands on the same pixel: a point's
// coordinates are single precision as a Vector2's are, a float is double, and
// each step is Godot's own for its type.

constexpr double HALF = 0.5;
constexpr double TWICE = 2.0;
constexpr float HALF_F = 0.5F;
constexpr float TWICE_F = 2.0F;
constexpr double HALF_TURN_DEGREES = 180.0;
// A cell's width and depth on the ground, in pixels, and how far a level
// raises it.
constexpr double CELL_X = 16.0;
constexpr double CELL_Y = 12.0;
constexpr double LEVEL_PX = 16.0;
// The relief's warp of the ground's coordinates, in pixels, its scale and
// the noise it takes, as relief.gd states them.
constexpr double WARP = 32.0;
constexpr double WARP_SCALE = 48.0;
constexpr float WARP_SALT_X = 7.0F;
constexpr float WARP_SALT_Y = 5.0F;
constexpr std::int64_t WARP_NOISE_X = 11;
constexpr std::int64_t WARP_NOISE_Y = 12;
constexpr int FBM_OCTAVES = 3;
constexpr double FBM_LACUNARITY = 2.03;
constexpr double FBM_TOTAL = 0.875;
constexpr float SMOOTH_F = 3.0F;
constexpr double SMOOTH = 3.0;
constexpr double SLOPE_BLEND = 0.5;

// Pixels past the screen's edge still drawn; a body's frame; the row of it
// its feet stand on; how far above them its name's anchor is.
constexpr double MARGIN = 64.0;
constexpr std::int64_t FRAME_WIDTH = 16;
constexpr std::int64_t FRAME_HEIGHT = 24;
constexpr std::int64_t FEET_ROW = 22;
constexpr std::int64_t HEAD_RISE = 23;
constexpr auto FRAME_W = static_cast<float>(FRAME_WIDTH);
constexpr auto FRAME_H = static_cast<float>(FRAME_HEIGHT);
constexpr auto FEET = static_cast<double>(FEET_ROW);
constexpr auto HEAD = static_cast<float>(HEAD_RISE);
// How far in from its frame a body's own pixels lie, for what hides it.
constexpr float BODY_INSET = 2.0F;
// How long a body shows its strike frame, and each walking frame, in
// milliseconds.
constexpr std::int64_t BODY_STRIKE_MS = 200;
constexpr std::int64_t WALK_FRAME_MS = 150;
constexpr std::int64_t WALK_FRAMES = 4;
// Pixels the occluders are bucketed by, and the fields of one.
constexpr double BUCKET = 32.0;
constexpr std::size_t OCCLUDER_FIELDS = 5;
// How far from the viewer's lead names are read, in cells.
constexpr std::int64_t NAME_CELLS = 8;
// Floats per sprite in a band's buffer.
constexpr std::int64_t BODY_FLOATS = 16;
// The body tags, in world_art.gd's BODY_TAGS order, and the idle and walk
// tags by how wounded a body is.
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
constexpr float KNOCK_PIXELS = 2.0F;
constexpr double NUMBER_RISE = 12.0;
constexpr std::int64_t WOUNDED = 500;
constexpr std::int64_t MAIMED = 250;
constexpr std::int64_t PER_MILLE = 1000;
constexpr std::int64_t DRIP_TICKS = 45;
constexpr double DRIP_SPREAD = 4.0;
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
constexpr std::int64_t NUMBERS_OFF = 0;
constexpr std::int64_t NUMBERS_MINE = 1;
constexpr std::int64_t NUMBERS_EVERYONE = 2;
constexpr double LEAST_RADIUS = 4.0;
constexpr std::int64_t NEVER = -(std::int64_t{1} << 40);
constexpr double FILL_ALPHA = 0.35;
constexpr double EDGE_ALPHA = 0.9;
constexpr int ARC_POINTS = 13;
constexpr int DISC_POINTS = 16;
constexpr float PROJECTILE = 1.5F;
constexpr double DEATH_FALL = 3.0;
// The target's brackets, round a body standing on their bottom edge.
constexpr float BRACKET_LEFT = -9.0F;
constexpr float BRACKET_TOP = 3.0F;
constexpr float BRACKET_W = 18.0F;
constexpr float BRACKET_H = 21.0F;
constexpr float BRACKET_ARM = 4.0F;
// The palette's colours set_palette() takes, in its order.
constexpr std::size_t PAPER = 0;
constexpr std::size_t INK = 1;
constexpr std::size_t JADE = 2;
constexpr std::size_t CRIMSON = 3;
// unit()'s hash, as combat.gd states it.
constexpr std::uint64_t UNIT_X = 73856093;
constexpr std::uint64_t UNIT_Y = 19349663;
constexpr std::uint64_t UNIT_MIX = 1274126177;
constexpr std::int64_t UNIT_RANGE = 10007;
constexpr int SHIFT_A = 13;
constexpr int SHIFT_B = 16;
// relief.gd's hash01(), on unsigned 32-bit arithmetic.
constexpr std::uint64_t HASH_X = 374761393;
constexpr std::uint64_t HASH_Y = 668265263;
constexpr std::uint64_t HASH_SALT = 2246822519;
constexpr std::uint64_t HASH_MIX = 1274126177;
constexpr std::uint64_t LOW_32 = 0xffffffff;
constexpr std::uint64_t LOW_16 = 0xffff;
constexpr double HASH_RANGE = 65536.0;

static_assert(sizeof(Point) == sizeof(godot::Vector2) &&
                  std::is_trivially_copyable_v<Point>,
              "a Vector2 array is read as points byte for byte");
static_assert(sizeof(Rgba) == sizeof(godot::Color) &&
                  std::is_trivially_copyable_v<Rgba>,
              "a Color array is read as colours byte for byte");

float single(double value) { return static_cast<float>(value); }

Point operator+(Point left, Point right) {
  return {left.x + right.x, left.y + right.y};
}

Point operator-(Point left, Point right) {
  return {left.x - right.x, left.y - right.y};
}

Point operator*(Point left, Point right) {
  return {left.x * right.x, left.y * right.y};
}

Point operator*(Point point, float scale) {
  return {point.x * scale, point.y * scale};
}

Point operator/(Point point, float scale) {
  return {point.x / scale, point.y / scale};
}

Point operator/(Point point, Point scale) {
  return {point.x / scale.x, point.y / scale.y};
}

bool same(Point left, Point right) {
  return left.x == right.x && left.y == right.y;
}

Point lerp(Point start, Point finish, float weight) {
  return {start.x + ((finish.x - start.x) * weight),
          start.y + ((finish.y - start.y) * weight)};
}

Point rounded(Point point) {
  return {std::round(point.x), std::round(point.y)};
}

Point floored(Point point) {
  return {std::floor(point.x), std::floor(point.y)};
}

float squared_distance(Point one, Point other) {
  const Point apart = one - other;
  return (apart.x * apart.x) + (apart.y * apart.y);
}

float length(Point point) {
  return std::sqrt((point.x * point.x) + (point.y * point.y));
}

Point normalized(Point point) {
  const float squared = (point.x * point.x) + (point.y * point.y);
  if (squared == 0.0F) {
    return point;
  }
  const float size = std::sqrt(squared);
  return {point.x / size, point.y / size};
}

Point from_angle(float angle) { return {std::cos(angle), std::sin(angle)}; }

bool has_point(const Box& box, Point point) {
  return point.x >= box.position.x && point.y >= box.position.y &&
         point.x < box.position.x + box.size.x &&
         point.y < box.position.y + box.size.y;
}

Box grown(const Box& box, float amount) {
  return {box.position - Point{amount, amount},
          box.size + Point{amount * TWICE_F, amount * TWICE_F}};
}

bool intersects(const Box& box, const Box& other) {
  return box.position.x < other.position.x + other.size.x &&
         box.position.x + box.size.x > other.position.x &&
         box.position.y < other.position.y + other.size.y &&
         box.position.y + box.size.y > other.position.y;
}

Point end_of(const Box& box) { return box.position + box.size; }

// Godot's own types hold their coordinates in unions, which the checks keep
// this code from reading; a Variant reads them by name.
Point point_of(const godot::Variant& vector) {
  const double across = vector.get("x");
  const double down = vector.get("y");
  return {single(across), single(down)};
}

Box box_of(const godot::Variant& rect) {
  return {point_of(rect.get("position")), point_of(rect.get("size"))};
}

godot::Vector2 vector_of(Point point) { return {point.x, point.y}; }

godot::Color colour_of(Rgba colour) {
  return {colour.red, colour.green, colour.blue, colour.alpha};
}

Rgba with_alpha(Rgba colour, double alpha) {
  colour.alpha = single(alpha);
  return colour;
}

template <typename Item>
std::vector<Item> items_of(const godot::PackedByteArray& bytes) {
  std::vector<Item> items(static_cast<std::size_t>(bytes.size()) /
                          sizeof(Item));
  std::memcpy(items.data(), bytes.ptr(), items.size() * sizeof(Item));
  return items;
}

std::vector<Point> points_of(const godot::Variant& vectors) {
  return items_of<Point>(godot::PackedVector2Array(vectors).to_byte_array());
}

template <typename Packed>
std::vector<std::int64_t> integers_of(const Packed& packed) {
  std::vector<std::int64_t> values;
  values.reserve(static_cast<std::size_t>(packed.size()));
  for (const auto value : packed) {
    values.push_back(value);
  }
  return values;
}

godot::PackedFloat32Array packed_of(const std::vector<float>& values) {
  godot::PackedFloat32Array packed;
  packed.resize(static_cast<std::int64_t>(values.size()));
  std::memcpy(packed.ptrw(), values.data(), values.size() * sizeof(float));
  return packed;
}

std::size_t index_of(std::int64_t value) {
  return static_cast<std::size_t>(value);
}

double lerpd(double start, double finish, double weight) {
  return start + ((finish - start) * weight);
}

double smoothstep(double low, double high, double value) {
  const double share = std::clamp((value - low) / (high - low), 0.0, 1.0);
  return share * share * (SMOOTH - (TWICE * share));
}

double hash01(std::int64_t across, std::int64_t down, std::int64_t salt) {
  std::uint64_t mixed =
      (((static_cast<std::uint64_t>(across) & LOW_32) * HASH_X) +
       ((static_cast<std::uint64_t>(down) & LOW_32) * HASH_Y) +
       ((static_cast<std::uint64_t>(salt) & LOW_32) * HASH_SALT)) &
      LOW_32;
  mixed = ((mixed ^ (mixed >> SHIFT_A)) * HASH_MIX) & LOW_32;
  mixed = mixed ^ (mixed >> SHIFT_B);
  return static_cast<double>(mixed & LOW_16) / HASH_RANGE;
}

double value_noise(Point point, std::int64_t salt) {
  const Point cell = floored(point);
  Point fraction = point - cell;
  fraction =
      fraction * fraction * (Point{SMOOTH_F, SMOOTH_F} - (fraction * TWICE_F));
  const auto column = static_cast<std::int64_t>(cell.x);
  const auto row = static_cast<std::int64_t>(cell.y);
  return lerpd(lerpd(hash01(column, row, salt), hash01(column + 1, row, salt),
                     fraction.x),
               lerpd(hash01(column, row + 1, salt),
                     hash01(column + 1, row + 1, salt), fraction.x),
               fraction.y);
}

double fbm(Point point, std::int64_t salt) {
  double sum = 0.0;
  double amplitude = HALF;
  for (int octave = 0; octave < FBM_OCTAVES; ++octave) {
    sum += amplitude * value_noise(point, salt + octave);
    point = point * single(FBM_LACUNARITY);
    amplitude *= HALF;
  }
  return sum / FBM_TOTAL;
}

Point warped(Point point) {
  const Point scaled = point / single(WARP_SCALE);
  const Point noise{
      single(fbm(scaled + Point{WARP_SALT_X, 0.0F}, WARP_NOISE_X)),
      single(fbm(scaled + Point{0.0F, WARP_SALT_Y}, WARP_NOISE_Y))};
  return point + ((noise - Point{HALF_F, HALF_F}) * TWICE_F * single(WARP));
}

std::int64_t floor_div(float value, double unit) {
  return static_cast<std::int64_t>(
      std::floor(static_cast<double>(value) / unit));
}

std::int64_t bucket_key(std::int64_t column, std::int64_t row) {
  constexpr std::int64_t SPAN = std::int64_t{1} << 32;
  return (column * SPAN) + row;
}

// An arc round the origin: the angle it starts at, how far round it sweeps,
// its radius, and how many points it is drawn through.
struct Arc {
  float start = 0.0F;
  float sweep = 0.0F;
  float radius = 0.0F;
  int points = 0;
};

// The points of `arc`, as CanvasItem.draw_arc lays them.
godot::PackedVector2Array ring(const Arc& arc) {
  godot::PackedVector2Array line;
  for (int step = 0; step < arc.points; ++step) {
    const float theta =
        ((static_cast<float>(step) / (static_cast<float>(arc.points) - 1.0F)) *
         arc.sweep) +
        arc.start;
    line.push_back(vector_of(from_angle(theta) * arc.radius));
  }
  return line;
}

}  // namespace

void BodyView::set_atlas(const godot::Vector2& size,
                         const godot::PackedInt64Array& frames,
                         const godot::PackedInt64Array& outlines,
                         const godot::Vector2i& slots,
                         const godot::PackedInt64Array& sprite_of_faction,
                         const godot::PackedColorArray& accents) {
  atlas_size_ = point_of(size);
  // A tag the atlas does not name takes the first frame.
  frames_ = integers_of(frames);
  outline_frames_ = integers_of(outlines);
  frames_.resize(TAGS, 0);
  outline_frames_.resize(TAGS, 0);
  const Point widths = point_of(slots);
  faction_width_ = static_cast<std::int64_t>(widths.x);
  shadow_x_ = static_cast<std::int64_t>(widths.y);
  sprite_of_faction_ = integers_of(sprite_of_faction);
  accents_ = items_of<Rgba>(accents.to_byte_array());
}

void BodyView::set_palette(const godot::PackedColorArray& colours) {
  palette_ = items_of<Rgba>(colours.to_byte_array());
}

BodyView::Rgba BodyView::paint(std::size_t which) const {
  return which < palette_.size() ? palette_.at(which) : Rgba{};
}

void BodyView::set_relief(const godot::Dictionary& cells) {
  const Point size = point_of(cells.get("size", godot::Variant()));
  relief_width_ = static_cast<std::int64_t>(size.x);
  relief_height_ = static_cast<std::int64_t>(size.y);
  levels_.clear();
  for (const std::int32_t level :
       godot::PackedInt32Array(cells.get("level", godot::Variant()))) {
    levels_.push_back(level);
  }
  slopes_.clear();
  for (const std::uint8_t slope :
       godot::PackedByteArray(cells.get("slope", godot::Variant()))) {
    slopes_.push_back(slope);
  }
  flat_.clear();
  for (const std::int32_t flat :
       godot::PackedInt32Array(cells.get("flat", godot::Variant()))) {
    flat_.push_back(flat);
  }
  highest_ = 0;
  for (const std::int32_t level : levels_) {
    highest_ = std::max<std::int64_t>(highest_, level);
  }
}

void BodyView::set_occluders(const godot::PackedFloat32Array& occluders) {
  occluders_.clear();
  const std::vector<float> fields = items_of<float>(occluders.to_byte_array());
  for (std::size_t first = 0; first + OCCLUDER_FIELDS <= fields.size();
       first += OCCLUDER_FIELDS) {
    const Occluder occluder{Box{{fields.at(first), fields.at(first + 1)},
                                {fields.at(first + 2), fields.at(first + 3)}},
                            fields.at(first + 4)};
    const Point end = end_of(occluder.rect);
    for (std::int64_t column = floor_div(occluder.rect.position.x, BUCKET);
         column <= floor_div(end.x, BUCKET); ++column) {
      for (std::int64_t row = floor_div(occluder.rect.position.y, BUCKET);
           row <= floor_div(end.y, BUCKET); ++row) {
        occluders_[bucket_key(column, row)].push_back(occluder);
      }
    }
  }
}

void BodyView::set_viewer(std::int64_t army,
                          const godot::PackedInt64Array& relation_of_faction) {
  viewer_ = army;
  relation_of_faction_ = integers_of(relation_of_faction);
}

void BodyView::set_steered(std::int64_t army) { steered_army_ = army; }

void BodyView::set_pixel_scale(double scale) {
  pixel_scale_ = scale > 0.0 ? scale : 1.0;
}

// `point` rounded to the nearest whole screen pixel, in world pixels.
Point BodyView::on_screen(Point point) const {
  return {single(std::round(static_cast<double>(point.x) * pixel_scale_) /
                 pixel_scale_),
          single(std::round(static_cast<double>(point.y) * pixel_scale_) /
                 pixel_scale_)};
}

void BodyView::set_damage_numbers(std::int64_t which) {
  damage_numbers_ = which;
}

void BodyView::set_blood(bool shown) { blood_ = shown; }

void BodyView::set_hit_stop(bool shown) { hit_stop_ = shown; }

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

BodyView::Read BodyView::read(const godot::Dictionary& snapshot) {
  const auto field = [&snapshot](const char* key) {
    return integers_of(
        godot::PackedInt64Array(snapshot.get(key, godot::Variant())));
  };
  return Read{field("first"), field("person"),
              points_of(snapshot.get("position", godot::Variant())),
              field("health"), field("faction")};
}

void BodyView::begin(const godot::Dictionary& snapshot) {
  clear_people();
  const Read now = read(snapshot);
  read_armies(now);
  read_bodies(now, false);
}

void BodyView::read_armies(const Read& snapshot) {
  for (std::size_t army = 0; army < snapshot.factions.size(); ++army) {
    const auto from = static_cast<std::ptrdiff_t>(snapshot.first.at(army));
    const auto upto = static_cast<std::ptrdiff_t>(snapshot.first.at(army + 1));
    std::vector<std::int64_t> roster(snapshot.persons.begin() + from,
                                     snapshot.persons.begin() + upto);
    std::vector<Point> now(snapshot.positions.begin() + from,
                           snapshot.positions.begin() + upto);
    if (army < armies_.size()) {
      Army& known = armies_.at(army);
      known.before = std::move(known.now);
      known.now = std::move(now);
      known.same = roster == known.roster;
      known.roster = std::move(roster);
      known.faction = snapshot.factions.at(army);
    } else {
      std::vector<Point> before = now;
      armies_.push_back(Army{std::move(roster), std::move(now),
                             std::move(before), true,
                             snapshot.factions.at(army)});
    }
  }
}

void BodyView::grow(std::size_t size) {
  const std::size_t total = std::max(size, seen_.size() * 2);
  seen_.resize(total, -2);
  health_.resize(total, 0);
  peak_.resize(total, 0);
  army_.resize(total, 0);
  body_.resize(total, 0);
  struck_tick_.resize(total, -1);
  struck_by_lead_.resize(total, false);
  nearest_.resize(total, 0.0);
  striker_.resize(total, 0);
  hit_ms_.resize(total, NEVER);
  away_.resize(total, Point{});
  struck_at_.resize(total, -BODY_STRIKE_MS);
}

bool BodyView::alive(std::int64_t person) const {
  return person >= 0 && index_of(person) < seen_.size() &&
         seen_.at(index_of(person)) == tick_;
}

void BodyView::read_bodies(const Read& snapshot, bool compare) {
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    const std::size_t start = index_of(snapshot.first.at(army));
    const std::vector<Point>& now = armies_.at(army).now;
    for (std::size_t body = 0; body < now.size(); ++body) {
      const std::int64_t person = snapshot.persons.at(start + body);
      const std::size_t index = index_of(person);
      if (index >= seen_.size()) {
        grow(index + 1);
      }
      const std::int64_t health = snapshot.healths.at(start + body);
      if (compare && seen_.at(index) == tick_ - 1 &&
          health < health_.at(index)) {
        hit(person, health_.at(index) - health, screen_point(now.at(body)));
      }
      seen_.at(index) = tick_;
      health_.at(index) = health;
      peak_.at(index) = std::max(peak_.at(index), health);
      army_.at(index) = static_cast<std::int64_t>(army);
      body_.at(index) = static_cast<std::int64_t>(body);
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
  std::vector<std::vector<Point>> stood;
  stood.reserve(armies_.size());
  for (const Army& army : armies_) {
    stood.push_back(army.now);
  }
  const std::pair<std::int64_t, std::int64_t> tick{steered_army, now};
  steered_army_ = tick.first;
  ms_ = tick.second;
  ++tick_;
  bleeds_ = 0;
  hit_stops_.clear();
  bleed_at_.clear();
  bleed_dir_.clear();
  bleed_seed_.clear();
  const Read read_now = read(snapshot);
  lead_ = -1;
  const std::size_t steered = index_of(steered_army_);
  if (steered_army_ >= 0 && steered + 1 < read_now.first.size() &&
      read_now.first.at(steered) < read_now.first.at(steered + 1)) {
    lead_ = read_now.persons.at(index_of(read_now.first.at(steered)));
  }
  read_strikes(snapshot, stood);
  note_deaths(died, stood);
  read_armies(read_now);
  read_bodies(read_now, true);
  if (selected_ >= 0 && alive(selected_)) {
    target_ = selected_;
  } else if (target_ >= 0 &&
             (!alive(target_) || ms_ - target_ms_ > TARGET_MS)) {
    target_ = -1;
  }
  start_holds();
  forget(ms_);
  return drips();
}

void BodyView::read_strikes(const godot::Dictionary& snapshot,
                            const std::vector<std::vector<Point>>& stood) {
  const auto field = [&snapshot](const char* key) {
    return integers_of(
        godot::PackedInt64Array(snapshot.get(key, godot::Variant())));
  };
  const std::vector<std::int64_t> persons = field("strike_person");
  const std::vector<std::int64_t> shapes = field("strike_shape");
  const std::vector<std::int64_t> ranges = field("strike_range");
  const std::vector<std::int64_t> radii = field("strike_radius");
  const std::vector<std::int64_t> damages = field("strike_body_damage");
  const std::vector<std::int64_t> first = field("strike_first");
  const std::vector<std::int64_t> struck = field("struck");
  const std::vector<Point> origins =
      points_of(snapshot.get("strike_origin", godot::Variant()));
  const std::vector<Point> towards =
      points_of(snapshot.get("strike_toward", godot::Variant()));
  const std::vector<Point> ends =
      points_of(snapshot.get("strike_end", godot::Variant()));
  // A use that strikes only structures shows in the structures' stages
  // alone.
  for (std::size_t use = 0; use < persons.size(); ++use) {
    if (damages.at(use) <= 0) {
      continue;
    }
    Strike strike{
        persons.at(use),
        shapes.at(use),
        ranges.at(use),
        radii.at(use),
        damages.at(use),
        origins.at(use),
        towards.at(use),
        ends.at(use),
        std::vector<std::int64_t>(
            struck.begin() + static_cast<std::ptrdiff_t>(first.at(use)),
            struck.begin() + static_cast<std::ptrdiff_t>(first.at(use + 1))),
        ms_,
        persons.at(use) == lead_};
    const std::size_t striker = index_of(strike.person);
    if (striker >= struck_at_.size()) {
      grow(striker + 1);
    }
    struck_at_.at(striker) = ms_;
    note_struck(strike, stood);
    strikes_.push_back(std::move(strike));
  }
}

void BodyView::note_struck(const Strike& strike,
                           const std::vector<std::vector<Point>>& stood) {
  double best = std::numeric_limits<double>::infinity();
  for (const std::int64_t person : strike.struck) {
    const std::size_t index = index_of(person);
    if (index >= seen_.size() || seen_.at(index) != tick_ - 1) {
      continue;
    }
    const Point where =
        stood.at(index_of(army_.at(index))).at(index_of(body_.at(index)));
    const double near = squared_distance(where, strike.origin);
    if (struck_tick_.at(index) != tick_) {
      struck_tick_.at(index) = tick_;
      struck_by_lead_.at(index) = false;
      nearest_.at(index) = std::numeric_limits<double>::infinity();
    }
    if (near < nearest_.at(index)) {
      nearest_.at(index) = near;
      away_.at(index) = normalized(where - strike.origin);
      striker_.at(index) = strike.person;
    }
    if (strike.steered) {
      struck_by_lead_.at(index) = true;
      const double toward = squared_distance(where, strike.toward);
      if (toward < best) {
        best = toward;
        target_ = person;
        target_ms_ = ms_;
      }
    }
  }
}

void BodyView::note_deaths(const godot::PackedInt64Array& died,
                           const std::vector<std::vector<Point>>& stood) {
  // The dead have left their rosters, so each is read as it stood on the
  // tick before; one no use struck died of hunger or age, and does not fall.
  for (const std::int64_t person : died) {
    const std::size_t index = index_of(person);
    if (person < 0 || index >= seen_.size() || seen_.at(index) != tick_ - 1 ||
        struck_tick_.at(index) != tick_) {
      continue;
    }
    const Point where =
        stood.at(index_of(army_.at(index))).at(index_of(body_.at(index)));
    const Point spot = screen_point(where);
    hit(person, health_.at(index), spot);
    deaths_.push_back(Death{person, army_.at(index), spot,
                            where * Point{single(CELL_X), single(CELL_Y)} /
                                static_cast<float>(murim::CELL_SIZE),
                            ms_});
  }
}

void BodyView::hit(std::int64_t person, std::int64_t lost, Point where) {
  const std::size_t index = index_of(person);
  const bool struck = struck_tick_.at(index) == tick_;
  hit_ms_.at(index) = ms_;
  if (!struck) {
    away_.at(index) = Point{};
  }
  const bool own = person == lead_;
  const bool mine = own || (struck && struck_by_lead_.at(index));
  if (struck) {
    bleed(person, where, mine);
    // A hit on or from the steered person holds the struck, and the one
    // whose use struck it nearest.
    if (hit_stop_ && mine) {
      stop_hit(Loss{person, lost});
    }
  }
  if (damage_numbers_ == NUMBERS_EVERYONE ||
      (damage_numbers_ == NUMBERS_MINE && mine)) {
    add_number(ms_, godot::String::num_int64(lost), vector_of(where), own,
               mine);
  }
}

void BodyView::bleed(std::int64_t person, Point where, bool mine) {
  if (!blood_ || (!mine && (bleeds_ >= MOST_BLEEDS ||
                            !has_point(grown(view_, single(CELL_X)), where)))) {
    return;
  }
  if (!mine) {
    ++bleeds_;
  }
  bleed_at_.push_back(where);
  bleed_dir_.push_back(away_.at(index_of(person)));
  bleed_seed_.push_back((person * BLEED_SEED) + tick_);
}

void BodyView::stop_hit(const Loss& loss) {
  const std::int64_t person = loss.person;
  const std::int64_t lost = loss.lost;
  std::int64_t frames = HIT_STOP_MOST;
  for (const auto& step : HIT_STOP) {
    if (lost < step.at(0)) {
      frames = step.at(1);
      break;
    }
  }
  hit_stops_.push_back(Stop{person, frames});
  hit_stops_.push_back(Stop{striker_.at(index_of(person)), frames});
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

void BodyView::start_holds() {
  // A hit stop starts holding its person afresh from the next fill.
  for (const Stop& stop : hit_stops_) {
    if (stop.person < 0) {
      continue;
    }
    const auto found = held_.find(stop.person);
    Hold hold;
    hold.left =
        std::max(stop.frames,
                 found == held_.end() ? std::int64_t{0} : found->second.left);
    held_[stop.person] = hold;
  }
}

godot::Dictionary BodyView::drips() const {
  godot::PackedVector2Array bleed_at;
  godot::PackedVector2Array bleed_dir;
  godot::PackedInt64Array bleed_seed;
  for (std::size_t bled = 0; bled < bleed_at_.size(); ++bled) {
    bleed_at.push_back(vector_of(bleed_at_.at(bled)));
    bleed_dir.push_back(vector_of(bleed_dir_.at(bled)));
    bleed_seed.push_back(bleed_seed_.at(bled));
  }
  godot::PackedVector2Array drip_at;
  godot::PackedInt64Array drip_shape;
  // Each maimed person on screen drips a drop at its feet every DRIP_TICKS
  // ticks, its own tick apart.
  for (const Army& army : armies_) {
    for (std::size_t body = 0; blood_ && body < army.now.size(); ++body) {
      const std::int64_t person = army.roster.at(body);
      if ((person + tick_) % DRIP_TICKS != 0 || wound(person) < 2) {
        continue;
      }
      const Point spot = screen_point(army.now.at(body));
      if (has_point(view_, spot)) {
        drip_at.push_back(
            vector_of(spot + Point{single((unit(person, tick_) * DRIP_SPREAD) -
                                          (DRIP_SPREAD / TWICE)),
                                   0.0F}));
        drip_shape.push_back(person);
      }
    }
  }
  godot::Dictionary laid;
  laid.set("bleed_at", bleed_at);
  laid.set("bleed_dir", bleed_dir);
  laid.set("bleed_seed", bleed_seed);
  laid.set("drip_at", drip_at);
  laid.set("drip_shape", drip_shape);
  return laid;
}

void BodyView::add_number(std::int64_t when, const godot::String& text,
                          const godot::Vector2& where, bool own, bool mine) {
  double width = 0.0;
  if (font_.is_valid()) {
    width = point_of(font_->get_string_size(
                         text, godot::HORIZONTAL_ALIGNMENT_LEFT, -1,
                         static_cast<std::int32_t>(font_size_)))
                .x;
  }
  numbers_.push_back(Number{when, text, point_of(where), own, mine, width});
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
  if (person < 0 || index_of(person) >= hit_ms_.size()) {
    return 0.0;
  }
  return std::clamp(
      1.0 - (static_cast<double>(now - hit_ms_.at(index_of(person))) /
             static_cast<double>(FLASH_MS)),
      0.0, 1.0);
}

BodyView::Point BodyView::knocked(std::int64_t person, std::int64_t now) const {
  const double left = flash(person, now);
  if (left == 0.0) {
    return {};
  }
  return away_.at(index_of(person)) * KNOCK_PIXELS * single(left);
}

godot::Vector2 BodyView::knock(std::int64_t person, std::int64_t now) const {
  return vector_of(knocked(person, now));
}

std::int64_t BodyView::wound(std::int64_t person) const {
  if (person < 0 || index_of(person) >= peak_.size() ||
      peak_.at(index_of(person)) <= 0) {
    return 0;
  }
  const std::int64_t share =
      health_.at(index_of(person)) * PER_MILLE / peak_.at(index_of(person));
  if (share < MAIMED) {
    return 2;
  }
  return share < WOUNDED ? 1 : 0;
}

godot::Vector2i BodyView::place_of(std::int64_t person) const {
  if (!alive(person)) {
    return {-1, -1};
  }
  return {static_cast<std::int32_t>(army_.at(index_of(person))),
          static_cast<std::int32_t>(body_.at(index_of(person)))};
}

std::int64_t BodyView::target() const { return target_; }

godot::Vector2i BodyView::shake(std::int64_t now) const {
  if (!hit_stop_ || now - shake_ms_ >= SHAKE_MS || now < shake_ms_) {
    return {};
  }
  const auto step =
      static_cast<std::size_t>(((now - shake_ms_) / SHAKE_STEP_MS) %
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

double BodyView::unit(std::int64_t seed, std::int64_t salt) {
  // GDScript's integers wrap on overflow, as unsigned arithmetic does here.
  const auto wrap = [](std::uint64_t value) {
    return static_cast<std::int64_t>(value);
  };
  std::int64_t hashed = wrap(static_cast<std::uint64_t>(seed) * UNIT_X) ^
                        wrap(static_cast<std::uint64_t>(salt) * UNIT_Y);
  hashed =
      wrap(static_cast<std::uint64_t>(hashed ^ (hashed >> SHIFT_A)) * UNIT_MIX);
  const std::int64_t mixed = hashed ^ (hashed >> SHIFT_B);
  const std::int64_t magnitude = mixed < 0 ? -mixed : mixed;
  return static_cast<double>(magnitude % UNIT_RANGE) /
         static_cast<double>(UNIT_RANGE);
}

godot::TypedArray<godot::Vector2i> BodyView::shake_pattern() {
  godot::TypedArray<godot::Vector2i> steps;
  for (const auto& step : SHAKE_PATTERN) {
    steps.push_back(godot::Vector2i(step.at(0), step.at(1)));
  }
  return steps;
}

std::size_t BodyView::cell_index(std::int64_t column, std::int64_t row) const {
  return index_of(
      (std::clamp<std::int64_t>(row, 0, relief_height_ - 1) * relief_width_) +
      std::clamp<std::int64_t>(column, 0, relief_width_ - 1));
}

double BodyView::height_at(Point point) const {
  const Point cell = floored(point / Point{single(CELL_X), single(CELL_Y)});
  const std::int32_t flat = flat_.at(cell_index(
      static_cast<std::int32_t>(cell.x), static_cast<std::int32_t>(cell.y)));
  if (flat > 0) {
    return static_cast<double>(flat - 1);
  }
  const Point bent = warped(point);
  const Point grid = Point{single(static_cast<double>(bent.x) / CELL_X),
                           single(static_cast<double>(bent.y) / CELL_Y)} -
                     Point{HALF_F, HALF_F};
  const Point corner = floored(grid);
  const Point fraction = grid - corner;
  const auto column = static_cast<std::int32_t>(corner.x);
  const auto row = static_cast<std::int32_t>(corner.y);
  const std::array<std::size_t, 4> corners{
      cell_index(column, row), cell_index(column + 1, row),
      cell_index(column, row + 1), cell_index(column + 1, row + 1)};
  std::array<double, 4> levels{};
  std::array<double, 4> slopes{};
  for (std::size_t spot = 0; spot < corners.size(); ++spot) {
    levels.at(spot) = static_cast<double>(levels_.at(corners.at(spot)));
    slopes.at(spot) = slopes_.at(corners.at(spot)) == 1 ? 1.0 : 0.0;
  }
  const double level =
      lerpd(lerpd(levels.at(0), levels.at(1), fraction.x),
            lerpd(levels.at(2), levels.at(3), fraction.x), fraction.y);
  const double slope =
      lerpd(lerpd(slopes.at(0), slopes.at(1), fraction.x),
            lerpd(slopes.at(2), slopes.at(3), fraction.x), fraction.y);
  double best = levels.at(0);
  for (std::size_t spot = 1; spot < levels.size(); ++spot) {
    const double here = std::abs(levels.at(spot) - level);
    const double there = std::abs(best - level);
    if (here < there || (here == there && levels.at(spot) > best)) {
      best = levels.at(spot);
    }
  }
  return lerpd(best, level, smoothstep(0.0, SLOPE_BLEND, slope));
}

double BodyView::lift_at(Point point) const {
  if (flat_.empty()) {
    return 0.0;
  }
  const std::int32_t flat = flat_.at(cell_index(
      static_cast<std::int64_t>(static_cast<double>(point.x) / CELL_X),
      static_cast<std::int64_t>(static_cast<double>(point.y) / CELL_Y)));
  if (flat > 0) {
    return static_cast<double>(flat - 1) * LEVEL_PX;
  }
  return height_at(point) * LEVEL_PX;
}

BodyView::Point BodyView::screen_point(Point sub) const {
  const Point on_ground = sub * Point{single(CELL_X), single(CELL_Y)} /
                          static_cast<float>(murim::CELL_SIZE);
  return on_ground - Point{0.0F, single(lift_at(on_ground))};
}

godot::Vector2 BodyView::to_screen(const godot::Vector2& sub) const {
  return vector_of(screen_point(point_of(sub)));
}

BodyView::Point BodyView::sub_at(const Army& army, std::size_t body,
                                 double between) {
  const Point now = army.now.at(body);
  if (army.same && body < army.before.size()) {
    return lerp(army.before.at(body), now,
                single(std::clamp(between, 0.0, 1.0)));
  }
  return now;
}

godot::Vector2 BodyView::body_at(std::int64_t army, std::int64_t body,
                                 double between) const {
  return vector_of(screen_point(
      sub_at(armies_.at(index_of(army)), index_of(body), between)));
}

std::int64_t BodyView::sprite_of(std::size_t army) const {
  return sprite_for(armies_.at(army).faction);
}

std::int64_t BodyView::sprite_for(std::int64_t faction) const {
  if (sprite_of_faction_.empty()) {
    return 0;
  }
  return sprite_of_faction_.at(
      index_of(faction % static_cast<std::int64_t>(sprite_of_faction_.size())));
}

Rgba BodyView::accent(std::int64_t person) const {
  if (!alive(person)) {
    return paint(PAPER);
  }
  const std::size_t sprite =
      index_of(sprite_of(index_of(army_.at(index_of(person)))));
  return sprite < accents_.size() ? accents_.at(sprite) : paint(PAPER);
}

bool BodyView::hidden(Box rect, double sort_y) const {
  const Point end = end_of(rect);
  const auto covers = [&rect, sort_y](const Occluder& occluder) {
    return occluder.sort_y > sort_y && intersects(occluder.rect, rect);
  };
  for (std::int64_t column = floor_div(rect.position.x, BUCKET);
       column <= floor_div(end.x, BUCKET); ++column) {
    for (std::int64_t row = floor_div(rect.position.y, BUCKET);
         row <= floor_div(end.y, BUCKET); ++row) {
      const auto found = occluders_.find(bucket_key(column, row));
      if (found != occluders_.end() &&
          std::any_of(found->second.begin(), found->second.end(), covers)) {
        return true;
      }
    }
  }
  return false;
}

void BodyView::pack(std::vector<float>& buffer, const Sprite& sprite) const {
  const double width =
      static_cast<double>(sprite.frame.size.x) * (sprite.flip ? -1.0 : 1.0);
  const auto height = static_cast<double>(sprite.frame.size.y);
  const double cos_turn = std::cos(sprite.turn);
  const double sin_turn = std::sin(sprite.turn);
  const Point local = on_screen(sprite.centre) +
                      Point{single(height / TWICE * sin_turn),
                            single(height / TWICE * (1.0 - cos_turn))};
  const bool body = sprite.outline_of.x < 0.0F;
  const Point custom = body ? Point{single(sprite.white), single(sprite.alpha)}
                            : sprite.outline_of / atlas_size_;
  const Rgba colour = body ? Rgba{1.0F, 1.0F, 1.0F, 1.0F}
                           : Rgba{single(sprite.white), 0.0F, 0.0F, 1.0F};
  const Point corner = sprite.frame.position / atlas_size_;
  const std::array<float, BODY_FLOATS> values{single(width * cos_turn),
                                              single(height * sin_turn),
                                              0.0F,
                                              local.x,
                                              single(width * sin_turn),
                                              single(-height * cos_turn),
                                              0.0F,
                                              local.y,
                                              colour.red,
                                              colour.green,
                                              colour.blue,
                                              colour.alpha,
                                              corner.x,
                                              corner.y,
                                              custom.x,
                                              custom.y};
  buffer.insert(buffer.end(), values.begin(), values.end());
}

BodyView::Look BodyView::look_of(const Frame& frame, const Army& army,
                                 std::size_t body) const {
  const std::int64_t now = frame.now;
  const std::int64_t sprite = sprite_for(army.faction);
  const std::int64_t person = army.roster.at(body);
  const Point heading = body < army.before.size()
                            ? army.now.at(body) - army.before.at(body)
                            : Point{};
  Look look;
  std::int64_t index = 0;
  const std::size_t who = index_of(person);
  const std::int64_t struck =
      who < struck_at_.size() ? struck_at_.at(who) : -BODY_STRIKE_MS;
  if (struck > 0 && now - struck < BODY_STRIKE_MS) {
    look.tag = TAG_STRIKE;
    index = now - struck < BODY_STRIKE_MS / 2 ? 0 : 1;
  } else if (!same(heading, Point{})) {
    look.tag = heading.y < 0.0F && -heading.y > std::abs(heading.x) ? TAG_BACK
                                                                    : TAG_WALK;
    index = (now / WALK_FRAME_MS) % WALK_FRAMES;
  }
  // Facing the viewer, standing or walking, a wounded body stoops and is
  // stained.
  if (look.tag == TAG_IDLE || look.tag == TAG_WALK) {
    look.tag = WOUND_TAGS.at(index_of(look.tag)).at(index_of(wound(person)));
  }
  look.frame =
      Box{{static_cast<float>(
               (sprite * faction_width_) +
               ((frames_.at(index_of(look.tag)) + index) * FRAME_WIDTH)),
           0.0F},
          {FRAME_W, FRAME_H}};
  look.white = flash(person, now);
  look.flip = heading.x < 0.0F;
  return look;
}

void BodyView::pack_body(Frame& frame, const Army& army, std::size_t body) {
  const std::int64_t person = army.roster.at(body);
  const bool moving = army.same && body < army.before.size();
  const Point sub =
      moving ? lerp(army.before.at(body), army.now.at(body), frame.share)
             : army.now.at(body);
  const Point on_ground = sub * Point{single(CELL_X), single(CELL_Y)} /
                          static_cast<float>(murim::CELL_SIZE);
  Point spot = on_ground - Point{0.0F, single(lift_at(on_ground))} +
               knocked(person, frame.now);
  const auto holding = held_.find(person);
  if (holding != held_.end() && holding->second.drawn) {
    spot = holding->second.at;
  }
  if (!has_point(frame.shown, spot) || frame.bodies.empty()) {
    return;
  }
  const std::int64_t row = std::clamp<std::int64_t>(
      static_cast<std::int64_t>(static_cast<double>(on_ground.y) / CELL_Y),
      frame.first_row, frame.last_row);
  Look look = look_of(frame, army, body);
  if (holding != held_.end()) {
    Hold& hold = holding->second;
    if (hold.drawn) {
      look = hold.look;
    } else {
      hold = Hold{hold.left, true, spot, look};
    }
  }
  const std::size_t band = index_of(row - frame.first_row);
  const Point origin{
      0.0F, single((static_cast<double>(row) * CELL_Y) + (CELL_Y / TWICE))};
  const Point size{FRAME_W, FRAME_H};
  // Rounded before the band's origin is taken off, so a body stands on the
  // same screen pixel in every band.
  pack(frame.shadows.at(band),
       Sprite{Box{{static_cast<float>(shadow_x_), 0.0F}, size},
              on_screen(spot) - origin});
  const Point centre{spot.x, single(static_cast<double>(spot.y) - FEET +
                                    (static_cast<double>(FRAME_H) / TWICE))};
  pack(frame.bodies.at(band),
       Sprite{look.frame, on_screen(centre) - origin, look.flip, look.white});
  const Box covers = grown(Box{centre - (size / TWICE_F), size}, -BODY_INSET);
  if (hidden(covers, static_cast<double>(origin.y))) {
    Box outline = look.frame;
    outline.position.x +=
        static_cast<float>((outline_frames_.at(index_of(look.tag)) -
                            frames_.at(index_of(look.tag))) *
                           FRAME_WIDTH);
    pack(frame.outlines, Sprite{outline, centre, look.flip, look.white, 1.0,
                                0.0, look.frame.position});
    ++frame.outlined;
  }
  ++frame.drawn;
}

void BodyView::pack_dead(Frame& frame) const {
  // The dead fall a quarter turn about their feet and fade where they last
  // stood, flashing as their killing blow lands.
  for (const Death& death : deaths_) {
    const double age = static_cast<double>(frame.now - death.ms) /
                       static_cast<double>(DEATH_MS);
    if (age < 0.0 || age >= 1.0 || !has_point(frame.shown, death.at) ||
        frame.bodies.empty()) {
      continue;
    }
    const std::int64_t row = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(static_cast<double>(death.ground.y) / CELL_Y),
        frame.first_row, frame.last_row);
    const Point centre{death.at.x,
                       single(static_cast<double>(death.at.y) - FEET +
                              (static_cast<double>(FRAME_H) / TWICE))};
    const Point origin{
        0.0F, single((static_cast<double>(row) * CELL_Y) + (CELL_Y / TWICE))};
    const Box frame_of{
        {static_cast<float>(sprite_of(index_of(death.army)) * faction_width_),
         0.0F},
        {FRAME_W, FRAME_H}};
    pack(frame.bodies.at(index_of(row - frame.first_row)),
         Sprite{frame_of, on_screen(centre) - origin, false,
                flash(death.person, frame.now), 1.0 - age,
                std::min(age * DEATH_FALL, 1.0) * godot::Math::PI / TWICE});
  }
}

void BodyView::count_holds() {
  // A hold counts down once a frame, its body drawn or not, and holds no one
  // dead.
  for (auto hold = held_.begin(); hold != held_.end();) {
    hold->second.left -= 1;
    if (hold->second.left <= 0 || !alive(hold->first)) {
      hold = held_.erase(hold);
    } else {
      ++hold;
    }
  }
}

std::vector<BodyView::Near> BodyView::near(double between) const {
  std::vector<Near> found;
  if (viewer_ < 0 || index_of(viewer_) >= armies_.size() ||
      armies_.at(index_of(viewer_)).now.empty()) {
    return found;
  }
  const Point from = sub_at(armies_.at(index_of(viewer_)), 0, between);
  const Box shown = grown(view_, single(MARGIN));
  for (std::size_t army = 0; army < armies_.size(); ++army) {
    const Army& known = armies_.at(army);
    const std::int64_t relation =
        known.faction >= 0 &&
                index_of(known.faction) < relation_of_faction_.size()
            ? relation_of_faction_.at(index_of(known.faction))
            : -1;
    for (std::size_t body = 0; body < known.now.size(); ++body) {
      const Point sub = sub_at(known, body, between);
      const double cells = static_cast<double>(length(sub - from)) /
                           static_cast<double>(murim::CELL_SIZE);
      if (cells > static_cast<double>(NAME_CELLS) ||
          (army == index_of(viewer_) && body == 0)) {
        continue;
      }
      const std::int64_t person = known.roster.at(body);
      const auto holding = held_.find(person);
      const Point spot = holding != held_.end() && holding->second.drawn
                             ? holding->second.at
                             : screen_point(sub) + knocked(person, now_);
      if (has_point(shown, spot)) {
        found.push_back(
            Near{person, spot - Point{0.0F, HEAD}, relation, cells});
      }
    }
  }
  std::stable_sort(found.begin(), found.end(),
                   [](const Near& one, const Near& other) {
                     return one.cells < other.cells;
                   });
  return found;
}

godot::Dictionary BodyView::fill(const godot::Rect2& view, double between,
                                 std::int64_t now) {
  const std::pair<double, std::int64_t> moment{between, now};
  show_at(view, moment.second, godot::Vector2());
  Frame frame;
  frame.shown = grown(view_, single(MARGIN));
  frame.now = moment.second;
  frame.share = single(std::clamp(moment.first, 0.0, 1.0));
  const std::int64_t rows = relief_height_ > 0
                                ? relief_height_
                                : std::numeric_limits<std::int32_t>::max();
  frame.first_row = std::max<std::int64_t>(
      static_cast<std::int64_t>(static_cast<double>(frame.shown.position.y) /
                                CELL_Y),
      0);
  frame.last_row = std::min<std::int64_t>(
      static_cast<std::int64_t>((static_cast<double>(end_of(frame.shown).y) +
                                 (static_cast<double>(highest_) * LEVEL_PX)) /
                                CELL_Y) +
          1,
      rows + 1);
  const std::size_t bands =
      index_of(std::max<std::int64_t>(frame.last_row - frame.first_row + 1, 0));
  frame.shadows.resize(bands);
  frame.bodies.resize(bands);
  for (const Army& army : armies_) {
    for (std::size_t body = 0; body < army.now.size(); ++body) {
      pack_body(frame, army, body);
    }
  }
  const std::vector<Near> named = near(moment.first);
  count_holds();
  pack_dead(frame);
  godot::Array packed;
  for (std::size_t band = 0; band < bands; ++band) {
    std::vector<float>& shadows = frame.shadows.at(band);
    shadows.insert(shadows.end(), frame.bodies.at(band).begin(),
                   frame.bodies.at(band).end());
    packed.push_back(packed_of(shadows));
  }
  godot::PackedInt64Array near_person;
  godot::PackedVector2Array near_head;
  godot::PackedInt64Array near_relation;
  godot::PackedFloat64Array near_cells;
  for (const Near& one : named) {
    near_person.push_back(one.person);
    near_head.push_back(vector_of(one.head));
    near_relation.push_back(one.relation);
    near_cells.push_back(one.cells);
  }
  const bool lead = steered_army_ >= 0 &&
                    index_of(steered_army_) < armies_.size() &&
                    !armies_.at(index_of(steered_army_)).now.empty();
  const bool targeted = alive(target_);
  const godot::Vector2 target_at =
      targeted ? body_at(army_.at(index_of(target_)),
                         body_.at(index_of(target_)), moment.first)
               : godot::Vector2();
  show_at(view, moment.second, target_at);
  godot::Dictionary filled;
  filled.set("bands", packed);
  filled.set("first_row", frame.first_row);
  filled.set("outlines", packed_of(frame.outlines));
  filled.set("drawn", frame.drawn);
  filled.set("outlined", frame.outlined);
  filled.set("near_person", near_person);
  filled.set("near_head", near_head);
  filled.set("near_relation", near_relation);
  filled.set("near_cells", near_cells);
  filled.set("lead", lead);
  filled.set("lead_at", lead ? vector_of(on_screen(point_of(
                                   body_at(steered_army_, 0, moment.first))))
                             : godot::Vector2());
  filled.set("target", targeted);
  filled.set("target_at", target_at);
  return filled;
}

void BodyView::show_at(const godot::Rect2& view, std::int64_t now,
                       const godot::Vector2& target_at) {
  view_ = box_of(view);
  now_ = now;
  target_at_ = point_of(target_at);
}

godot::Rect2 BodyView::view() const {
  return {view_.position.x, view_.position.y, view_.size.x, view_.size.y};
}

bool BodyView::shows(const Strike& strike, std::int64_t now) const {
  return now - strike.ms < STRIKE_MS &&
         (strike.steered || has_point(view_, screen_point(strike.origin)));
}

void BodyView::draw_effects(const godot::RID& canvas_item) const {
  for (const Strike& strike : strikes_) {
    if (shows(strike, now_)) {
      draw_strike(canvas_item, strike,
                  1.0 - (static_cast<double>(now_ - strike.ms) /
                         static_cast<double>(STRIKE_MS)));
    }
  }
  if (target_ >= 0) {
    draw_target(canvas_item, on_screen(target_at_));
  }
  if (!font_.is_valid()) {
    return;
  }
  for (const godot::Variant& entry : shown_numbers(now_)) {
    const godot::Variant spot = entry.get("at");
    const godot::String text = entry.get("text");
    const godot::Variant colour = entry.get("colour");
    const double alpha = colour.get("a");
    font_->draw_string(canvas_item,
                       vector_of(point_of(spot) + Point{1.0F, 1.0F}), text,
                       godot::HORIZONTAL_ALIGNMENT_LEFT, -1,
                       static_cast<std::int32_t>(font_size_),
                       colour_of(with_alpha(paint(INK), alpha)));
    font_->draw_string(canvas_item, spot, text,
                       godot::HORIZONTAL_ALIGNMENT_LEFT, -1,
                       static_cast<std::int32_t>(font_size_), colour);
  }
}

void BodyView::draw_strike(const godot::RID& canvas_item, const Strike& strike,
                           double left) const {
  godot::RenderingServer* server = godot::RenderingServer::get_singleton();
  const Rgba colour = strike.steered ? paint(JADE) : accent(strike.person);
  const godot::Color fill = colour_of(with_alpha(colour, FILL_ALPHA * left));
  const godot::Color edge = colour_of(with_alpha(colour, EDGE_ALPHA * left));
  const Point origin = screen_point(strike.origin);
  const double scale = CELL_X / static_cast<double>(murim::CELL_SIZE);
  const float reach = single(static_cast<double>(strike.range) * scale);
  const float radius = single(
      std::max(static_cast<double>(strike.radius) * scale, LEAST_RADIUS));
  // Distances on the ground are foreshortened down the screen: a disc on it
  // is drawn as an ellipse.
  const Point flat{1.0F, single(CELL_Y / CELL_X)};
  const auto shape = static_cast<murim::Shape>(strike.shape);
  if (shape == murim::Shape::ARC) {
    // An arc used toward its user's own place sweeps all around.
    const Point way = strike.toward - strike.origin;
    const bool aimed = !same(way, Point{});
    const double facing =
        aimed ? static_cast<double>(std::atan2(way.y, way.x)) : 0.0;
    const double sweep =
        aimed ? static_cast<double>(murim::ARC_HALF_WIDTH_DEGREES) *
                    (godot::Math::PI / HALF_TURN_DEGREES)
              : godot::Math::PI;
    godot::PackedVector2Array points{vector_of(origin)};
    for (int step = 0; step < ARC_POINTS; ++step) {
      const double angle =
          facing - sweep +
          (TWICE * sweep * step / (static_cast<double>(ARC_POINTS) - 1.0));
      points.push_back(
          vector_of(origin + (from_angle(single(angle)) * reach * flat)));
    }
    server->canvas_item_add_polygon(canvas_item, points,
                                    godot::PackedColorArray{fill});
    server->canvas_item_add_set_transform(
        canvas_item,
        godot::Transform2D(0.0F, vector_of(flat), 0.0F, vector_of(origin)));
    const float start = single(facing - sweep);
    const float stop = single(facing + sweep);
    server->canvas_item_add_polyline(
        canvas_item,
        ring(Arc{start,
                 std::clamp(stop - start, -single(godot::Math::TAU),
                            single(godot::Math::TAU)),
                 reach, ARC_POINTS}),
        godot::PackedColorArray{edge}, 1.0F);
    server->canvas_item_add_set_transform(canvas_item, godot::Transform2D());
  } else if (shape == murim::Shape::LINE || shape == murim::Shape::TARGETED) {
    const Point end = screen_point(strike.end);
    server->canvas_item_add_line(canvas_item, vector_of(origin), vector_of(end),
                                 fill,
                                 std::max(radius * TWICE_F * flat.y, 1.0F));
    server->canvas_item_add_circle(
        canvas_item, vector_of(lerp(origin, end, single(1.0 - left))),
        PROJECTILE, edge);
  } else {
    server->canvas_item_add_set_transform(
        canvas_item,
        godot::Transform2D(0.0F, vector_of(flat), 0.0F,
                           vector_of(screen_point(strike.toward))));
    server->canvas_item_add_circle(canvas_item, godot::Vector2(), radius, fill);
    server->canvas_item_add_polyline(
        canvas_item,
        ring(Arc{0.0F, single(godot::Math::TAU), radius, DISC_POINTS}),
        godot::PackedColorArray{edge}, 1.0F);
    server->canvas_item_add_set_transform(canvas_item, godot::Transform2D());
  }
}

void BodyView::draw_target(const godot::RID& canvas_item, Point where) const {
  godot::RenderingServer* server = godot::RenderingServer::get_singleton();
  const Box box{where + Point{BRACKET_LEFT, BRACKET_TOP - single(BODY_HEIGHT)},
                {BRACKET_W, BRACKET_H}};
  const Point end = end_of(box);
  const std::array<Point, 4> corners{box.position, Point{end.x, box.position.y},
                                     Point{box.position.x, end.y}, end};
  const godot::Color crimson = colour_of(paint(CRIMSON));
  for (const Point corner : corners) {
    const float across = corner.x == box.position.x ? 1.0F : -1.0F;
    const float down = corner.y == box.position.y ? 1.0F : -1.0F;
    server->canvas_item_add_line(
        canvas_item, vector_of(corner),
        vector_of(corner + Point{BRACKET_ARM * across, 0.0F}), crimson, 1.0F);
    server->canvas_item_add_line(
        canvas_item, vector_of(corner),
        vector_of(corner + Point{0.0F, BRACKET_ARM * down}), crimson, 1.0F);
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
    godot::PackedInt64Array struck;
    for (const std::int64_t person : strike.struck) {
      struck.push_back(person);
    }
    godot::Dictionary fields;
    fields.set("person", strike.person);
    fields.set("shape", strike.shape);
    fields.set("range", strike.range);
    fields.set("radius", strike.radius);
    fields.set("body_damage", strike.body_damage);
    fields.set("origin", vector_of(strike.origin));
    fields.set("toward", vector_of(strike.toward));
    fields.set("end", vector_of(strike.end));
    fields.set("struck", struck);
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
  std::int64_t index = 0;
  for (const Strike& strike : strikes_) {
    if (shows(strike, now)) {
      shown.push_back(kept.get(index));
    }
    ++index;
  }
  return shown;
}

godot::TypedArray<godot::Dictionary> BodyView::deaths() const {
  godot::TypedArray<godot::Dictionary> kept;
  for (const Death& death : deaths_) {
    godot::Dictionary fields;
    fields.set("person", death.person);
    fields.set("army", death.army);
    fields.set("at", vector_of(death.at));
    fields.set("ground", vector_of(death.ground));
    fields.set("ms", death.ms);
    kept.push_back(fields);
  }
  return kept;
}

godot::TypedArray<godot::Dictionary> BodyView::dying(std::int64_t now) const {
  godot::TypedArray<godot::Dictionary> shown;
  for (const Death& death : deaths_) {
    const double age =
        static_cast<double>(now - death.ms) / static_cast<double>(DEATH_MS);
    if (age >= 0.0 && age < 1.0) {
      godot::Dictionary fields;
      fields.set("army", death.army);
      fields.set("at", vector_of(death.at));
      fields.set("ground", vector_of(death.ground));
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
    fields.set("at", vector_of(number.at));
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
  if (damage_numbers_ == NUMBERS_OFF) {
    return shown;
  }
  for (const Number& number : numbers_) {
    const double age =
        static_cast<double>(now - number.ms) / static_cast<double>(NUMBER_MS);
    if (age < 0.0 || age >= 1.0 || !has_point(view_, number.at) ||
        (damage_numbers_ == NUMBERS_MINE && !number.mine)) {
      continue;
    }
    godot::Dictionary fields;
    fields.set("text", number.text);
    fields.set("colour",
               colour_of(with_alpha(paint(number.own ? CRIMSON : PAPER),
                                    1.0 - (age * age))));
    fields.set(
        "at",
        vector_of(rounded(number.at +
                          Point{single(-number.width / TWICE),
                                single(-BODY_HEIGHT - (NUMBER_RISE * age))})));
    shown.push_back(fields);
  }
  return shown;
}

void BodyView::_bind_methods() {
  using godot::ClassDB;
  using godot::D_METHOD;
  ClassDB::bind_method(D_METHOD("set_atlas", "size", "frames", "outlines",
                                "slots", "sprite_of_faction", "accents"),
                       &BodyView::set_atlas);
  ClassDB::bind_method(D_METHOD("set_palette", "colours"),
                       &BodyView::set_palette);
  ClassDB::bind_method(D_METHOD("set_relief", "cells"), &BodyView::set_relief);
  ClassDB::bind_method(D_METHOD("set_occluders", "occluders"),
                       &BodyView::set_occluders);
  ClassDB::bind_method(D_METHOD("set_viewer", "army", "relation_of_faction"),
                       &BodyView::set_viewer);
  ClassDB::bind_method(D_METHOD("set_steered", "army"), &BodyView::set_steered);
  ClassDB::bind_method(D_METHOD("set_pixel_scale", "scale"),
                       &BodyView::set_pixel_scale);
  ClassDB::bind_method(D_METHOD("set_damage_numbers", "which"),
                       &BodyView::set_damage_numbers);
  ClassDB::bind_method(D_METHOD("set_blood", "shown"), &BodyView::set_blood);
  ClassDB::bind_method(D_METHOD("set_hit_stop", "shown"),
                       &BodyView::set_hit_stop);
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
      D_METHOD("add_number", "when", "text", "where", "own", "mine"),
      &BodyView::add_number);
  ClassDB::bind_method(D_METHOD("show_at", "view", "now", "target_at"),
                       &BodyView::show_at);
  ClassDB::bind_method(D_METHOD("view"), &BodyView::view);
  ClassDB::bind_static_method("BodyView", D_METHOD("unit", "seed", "salt"),
                              &BodyView::unit);
  ClassDB::bind_static_method("BodyView", D_METHOD("shake_pattern"),
                              &BodyView::shake_pattern);
  const godot::StringName& name = get_class_static();
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
