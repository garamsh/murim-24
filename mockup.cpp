// Lays villages of three tiers by ADR 0076's roles with core/layout.cpp on
// three grounds, and writes each as a PPM: flat, a river, and hills.
#include <cstdio>
#include <string>
#include <vector>
#include "json_packs.hpp"
#include "layout.hpp"
#include "region.hpp"
using namespace murim;

constexpr int SIDE = 56, PX = 8;
struct Laid { Cell at; Footprint fp; std::string kind; Role role; };

Lot ground(int kind) {
  Lot lot{{SIDE, SIDE}, std::vector<Plot>(SIDE * SIDE, Plot::FREE), std::vector<bool>(SIDE * SIDE, true)};
  for (int y = 0; y < SIDE; ++y) for (int x = 0; x < SIDE; ++x) {
    int i = y * SIDE + x;
    if (kind == 1 && (x == SIDE / 2 + 5 + (y / 9) % 2 || x == SIDE / 2 + 6 + (y / 9) % 2)) { lot.plots[i] = Plot::BLOCKED; lot.passable[i] = false; }
    if (kind == 2 && x > SIDE / 2 + 4) lot.plots[i] = Plot::ROUGH;
  }
  return lot;
}

int main(int argc, char** argv) {
  const RegionData data = murim_packs::load_base(argv[1]);
  const Growth& g = *data.life.growth;
  // What each tier's village holds: its start list, then builds in turn.
  const VillageTemplate& start = data.villages.at(1);
  const int extra[3] = {2, 22, 62};
  const char* tiers[3] = {"hamlet", "village", "town"};
  const char* grounds[3] = {"flat", "river", "hills"};
  for (int t = 0; t < 3; ++t) for (int k = 0; k < 3; ++k) {
    Lot lot = ground(k);
    const Tier& tier = g.tiers.at(t);
    const Cell centre{SIDE / 2, SIDE / 2};
    const Townsite site{centre, tier.square, tier.vicinity};
    build_on(lot, square_corner(centre, tier.square), tier.square, false);
    std::vector<Laid> laid; std::vector<Structure> homes;
    std::vector<std::pair<Footprint, std::string>> todo;
    std::vector<Building> list = (t == 0) ? data.villages.at(0).buildings : start.buildings;
    std::vector<Building> sorted = list;
    std::stable_sort(sorted.begin(), sorted.end(), [&](const Building& a, const Building& b) {
      auto r = [&](const Building& x) { for (auto& kd : data.kinds) if (kd.name == x.kind) return kd.role; return Role::HOME; };
      return r(a) < r(b); });
    for (auto& b : sorted) todo.emplace_back(b.structure.footprint, b.kind);
    const char* cycle[] = {"house", "house", "field", "house", "granary", "field", "house", "smithy", "field", "apothecary"};
    for (int e = 0; e < extra[t]; ++e) { std::string kd = cycle[e % 10];
      for (auto& p : g.plans) if (p.kind == kd) { todo.emplace_back(p.footprint, kd); break; } }
    for (auto& [fp, kd] : todo) {
      Role role = Role::HOME; bool blocks = true;
      for (auto& x : data.kinds) if (x.name == kd) { role = x.role; blocks = x.blocks; }
      auto at = place(lot, site, fp, role, homes, placement_draw(7, t * 3 + k, laid.size()));
      if (!at) continue;
      build_on(lot, *at, fp, blocks);
      if (role == Role::HOME) homes.push_back(Structure{*at, 0, fp, blocks, role});
      laid.push_back({*at, fp, kd, role});
    }
    // Draw.
    std::vector<unsigned char> img(SIDE * PX * SIDE * PX * 3);
    auto put = [&](int x, int y, int r, int gg, int b) { for (int dy = 0; dy < PX; ++dy) for (int dx = 0; dx < PX; ++dx) { int i = ((y * PX + dy) * SIDE * PX + x * PX + dx) * 3; img[i] = r; img[i+1] = gg; img[i+2] = b; } };
    Lot base = ground(k);
    for (int y = 0; y < SIDE; ++y) for (int x = 0; x < SIDE; ++x) {
      Plot p = base.plots[y * SIDE + x];
      if (p == Plot::BLOCKED) put(x, y, 70, 110, 170); else if (p == Plot::ROUGH) put(x, y, 150, 135, 95); else put(x, y, 120, 160, 90);
    }
    Cell sq = square_corner(centre, tier.square);
    for (int y = 0; y < tier.square.height; ++y) for (int x = 0; x < tier.square.width; ++x) put(sq.x + x, sq.y + y, 205, 195, 170);
    for (auto& l : laid) for (int y = 0; y < l.fp.height; ++y) for (int x = 0; x < l.fp.width; ++x) {
      bool rough = base.plots[(l.at.y + y) * SIDE + l.at.x + x] == Plot::ROUGH;
      if (l.kind == "field") put(l.at.x + x, l.at.y + y, rough ? 200 : 220, rough ? 170 : 200, 60);
      else if (l.role == Role::SQUARE) put(l.at.x + x, l.at.y + y, 150, 40, 40);
      else if (l.kind == "fence") put(l.at.x + x, l.at.y + y, 90, 60, 30);
      else put(l.at.x + x, l.at.y + y, 110, 70, 50);
    }
    // Outline each structure a shade darker on its edge.
    for (auto& l : laid) for (int y = 0; y < l.fp.height * PX; ++y) for (int x = 0; x < l.fp.width * PX; ++x)
      if (x == 0 || y == 0 || x == l.fp.width * PX - 1 || y == l.fp.height * PX - 1) { int i = (((l.at.y * PX) + y) * SIDE * PX + l.at.x * PX + x) * 3; img[i] /= 2; img[i+1] /= 2; img[i+2] /= 2; }
    std::string name = std::string(tiers[t]) + "_" + grounds[k] + ".ppm";
    FILE* f = fopen(name.c_str(), "wb"); fprintf(f, "P6 %d %d 255\n", SIDE * PX, SIDE * PX); fwrite(img.data(), 1, img.size(), f); fclose(f);
    printf("%s: %zu of %zu structures laid\n", name.c_str(), laid.size(), todo.size());
  }
}
