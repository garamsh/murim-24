// Draws the 45 x 45 cells round a village of a generated region, lived
// `years` years at 5 ticks a day, 8 pixels a cell, as a PPM: terrain by
// kind, each structure by its kind. Usage: render <packs> <seed> <years> <village> <out.ppm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "calendar.hpp"
#include "json_packs.hpp"
#include "region.hpp"
#include "world.hpp"
using namespace murim;
int main(int, char** argv) {
  const RegionData data = murim_packs::load_base(argv[1]);
  const unsigned long long seed = strtoull(argv[2], nullptr, 10);
  const int years = atoi(argv[3]); const size_t village = strtoul(argv[4], nullptr, 10);
  Region r = generate_region(Seed{seed}, data); r.rules.ticks_per_day = 5;
  World w{r.seed, Map{r.map_rows}, r.armies, r.structures, r.rules, village_specs(r), r.leadership, r.life, r.sects};
  for (unsigned long long t = 0; t < (unsigned long long)years * DAYS_PER_YEAR * 5; ++t) w.tick();
  const Village& v = r.villages.at(village);
  const int cx = v.corner.x + v.area.width / 2, cy = v.corner.y + v.area.height / 2, R = 22, PX = 8, N = 2 * R + 1;
  std::vector<unsigned char> img(N * PX * N * PX * 3);
  auto put = [&](int x, int y, int cr, int cg, int cb) { int gx = x - (cx - R), gy = y - (cy - R); if (gx < 0 || gy < 0 || gx >= N || gy >= N) return; for (int dy = 0; dy < PX; ++dy) for (int dx = 0; dx < PX; ++dx) { int i = ((gy * PX + dy) * N * PX + gx * PX + dx) * 3; img[i] = cr; img[i + 1] = cg; img[i + 2] = cb; } };
  for (int y = cy - R; y <= cy + R; ++y) for (int x = cx - R; x <= cx + R; ++x) {
    if (x < 0 || y < 0 || x >= 128 || y >= 128) { put(x, y, 20, 20, 20); continue; }
    const std::string& t = data.terrain.at(r.terrain.at(y * 128 + x)).name;
    if (t == "water") put(x, y, 70, 110, 170); else if (t == "hills") put(x, y, 150, 135, 95); else if (t == "mountain") put(x, y, 100, 100, 100); else if (t == "forest") put(x, y, 70, 120, 70); else put(x, y, 120, 160, 90);
  }
  for (size_t s = 0; s < w.structure_count(); ++s) {
    const Structure st = w.structure(s);
    std::string kind = s < r.structure_kinds.size() ? r.structure_kinds[s] : data.life.growth->plans.at(w.plan_of(s).value_or(0)).kind;
    int cr = 110, cg = 70, cb = 50;
    if (kind == "field") { cr = 220; cg = 200; cb = 60; } else if (kind == "hall" || kind == "well" || kind == "smithy" || kind == "apothecary") { cr = 150; cg = 40; cb = 40; } else if (kind == "fence") { cr = 60; cg = 40; cb = 20; }
    for (Cell c : cells_of(st.cell, st.footprint)) put(c.x, c.y, cr, cg, cb);
  }
  FILE* f = fopen(argv[5], "wb"); fprintf(f, "P6 %d %d 255\n", N * PX, N * PX); fwrite(img.data(), 1, img.size(), f); fclose(f);
  printf("%s: seed %llu year %d village %zu (%s) centre %d,%d, %zu structures in the world\n", argv[5], seed, years, village, v.kind.c_str(), cx, cy, w.structure_count());
}
