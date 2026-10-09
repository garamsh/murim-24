-- Draws contact_sheet.aseprite: every frame of the production set, read back from the
-- .aseprite sources, a small scene built from them on the 16-pixel grid with shadows drawn
-- the way the view is to draw them, a mixed-terrain patch laid as the view is to lay it, a
-- farming village, a sect's seat and a grown village with its workshops assembled from them. It
-- is a review image; the game does not load it.
-- Run: aseprite --batch --script art/contact_sheet/contact_sheet.lua (after the asset scripts)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)
local c = px.c
local art = here .. "/.."

local W, H = 608, 2250
local sheet = px.canvas(W, H)
sheet:rect(0, 0, W, H, c("ink", 2))

local function load(path)
  local frames, tags = px.load_frames(art .. "/" .. path)
  return { frames = frames, tags = tags }
end
local terrain = load("terrain/terrain.aseprite")
local bodies = {
  orthodox = load("bodies/orthodox.aseprite"),
  demonic = load("bodies/demonic.aseprite"),
  unorthodox = load("bodies/unorthodox.aseprite"),
}
local house = load("structures/house.aseprite")
local hall = load("structures/hall.aseprite")
local well = load("structures/well.aseprite")
local barrels = load("structures/barrels.aseprite")
local fence = load("structures/fence.aseprite")
local ground_tiles = load("decorations/ground.aseprite")
local banner = load("decorations/banner.aseprite")
local icons = load("icons/skill_icons.aseprite")
local army = load("markers/army_marker.aseprite")
local player = load("markers/player_marker.aseprite")

local function frame(set, tag, n) return set.frames[set.tags[tag][1] + (n or 1) - 1] end
local function label(x, y, s, col) sheet:text(x, y, s, col or c("stone", 4)) end
local KINDS = { "plain", "forest", "hills", "water", "mountain" }

label(8, 6, "MURIM24  PRODUCTION SET", c("paper", 3))
label(120, 6, "ONE CELL = 16 PX, SHOWN AT NATIVE SIZE")

-- Terrain: the four variants of each kind; the tiled patches are further down.
label(8, 20, "TERRAIN  VARIANTS 1-4", c("paper", 3))
for k, kind in ipairs(KINDS) do
  local y = 30 + (k - 1) * 20
  label(8, y + 6, kind:upper(), c("paper", 2))
  for v = 1, 4 do sheet:stamp(frame(terrain, kind, v), 48 + (v - 1) * 17, y) end
end

-- Bodies: idle, then the four walk frames, each standing on a plain cell.
label(256, 20, "BODIES  IDLE", c("paper", 3))
label(320, 20, "WALK 1-4")
local rows = { { "orthodox", "ORTHODOX", "azure" }, { "demonic", "DEMONIC", "crimson" }, { "unorthodox", "UNORTHODOX", "amber" } }
local function cell_strip(x, y, n) for t = 0, n - 1 do sheet:stamp(frame(terrain, "plain", 1 + t % 4), x + t * 17, y) end end
for k, r in ipairs(rows) do
  local y = 30 + (k - 1) * 26
  label(256, y + 14, r[2], c(r[3], 4))
  cell_strip(300, y + 8, 5)
  for f = 0, 4 do
    local set = bodies[r[1]]
    local fr = f == 0 and frame(set, "idle") or frame(set, "walk", f)
    sheet:stamp(fr, 300 + f * 17, y)
  end
end
-- The player's markers alone, then round an Orthodox body.
local py = 30 + 3 * 26
label(256, py + 14, "PLAYER", c("jade", 3))
cell_strip(300, py + 8, 5)
sheet:stamp(frame(player, "ring"), 300, py)
sheet:stamp(frame(player, "chevron", 1), 317, py)
sheet:stamp(frame(player, "chevron", 2), 334, py)
sheet:stamp(frame(player, "ring"), 368, py)
sheet:stamp(frame(bodies.orthodox, "idle"), 368, py)
sheet:stamp(frame(player, "chevron", 1), 368, py)

-- The house in its four stages, each on its 3x3-cell footprint of plain ground.
label(8, 140, "HOUSE, 3 X 3 CELLS", c("paper", 3))
for k, stage in ipairs({ "intact", "damaged", "destroyed", "rubble" }) do
  local x, y = 8 + (k - 1) * 68, 150
  for cy = 0, 2 do for cx = 0, 2 do sheet:stamp(frame(terrain, "plain", 1 + (cx + cy) % 4), x + 8 + cx * 16, y + 8 + cy * 16) end end
  sheet:stamp(frame(house, stage), x, y)
  label(x + 8, y + 62, stage:upper(), c("paper", 2))
end

-- The aggregate-army marker of each faction.
label(8, 226, "AGGREGATE ARMY MARKER", c("paper", 3))
for k, r in ipairs(rows) do
  local x, y = 8 + (k - 1) * 70, 236
  sheet:stamp(frame(army, r[1]), x, y)
  label(x + 4, y + 52, r[2], c(r[3], 4))
end

-- A scene: terrain from the tiles, then shadows, then everything standing sorted by the bottom
-- edge of its footprint.
label(232, 226, "ASSEMBLED ON THE GRID", c("paper", 3))
local SX, SY = 232, 236
local map = {
  "mmfffppppphhww", "mffppppppphhww", "fpppppppppppww", "pppppppppppppw",
  "pppppppppppppw", "hppppppppppppw", "hhpppppppppppw",
}
local code = { m = "mountain", f = "forest", h = "hills", p = "plain", w = "water" }
local ground = px.canvas(W, H)
for row, line in ipairs(map) do
  for col = 1, #line do
    local kind = code[line:sub(col, col)]
    ground:stamp(frame(terrain, kind, 1 + math.floor(px.hash(col, row, 7) * 4)), SX + (col - 1) * 16, SY + (row - 1) * 16)
  end
end
local function cell(col, row) return SX + col * 16, SY + row * 16 end
local standing = {}
local function stand(bottom, fn) standing[#standing + 1] = { bottom, #standing, fn } end
local scratch = px.canvas(W, H)
-- Houses: footprint at (col, row), 3x3 cells.
for _, h in ipairs({ { 1, 1, "intact", 101 }, { 5, 1, "destroyed", 103 } }) do
  local x, y = cell(h[1], h[2])
  S.house(scratch, ground, x, y, h[3], h[4])
  stand(y + 48, function() sheet:stamp(frame(house, h[3]), x - 8, y - 8) end)
end
-- The army marker: a 4x3-cell frame.
do
  local x, y = cell(8, 0)
  S.army(scratch, ground, x + 4, y + 20, 13, 4, "unorthodox", "amber", 41)
  stand(y + 48, function() sheet:stamp(frame(army, "unorthodox"), x, y) end)
end
-- Bodies: frame top is 8 px above the cell.
local folk = {
  { "orthodox", 2, 5, "idle" }, { "orthodox", 4, 5, "walk", 2, true }, { "demonic", 7, 5, "walk", 1 },
  { "demonic", 8, 4, "idle" }, { "unorthodox", 10, 5, "walk", 3 }, { "unorthodox", 11, 3, "idle" },
}
for _, b in ipairs(folk) do
  local x, y = cell(b[2], b[3])
  S.shadow(ground, x, y)
  stand(y + 16, function()
    if b[6] then sheet:stamp(frame(player, "ring"), x, y - 8) end
    sheet:stamp(frame(bodies[b[1]], b[4], b[5]), x, y - 8)
    if b[6] then sheet:stamp(frame(player, "chevron", 1), x, y - 8) end
  end)
end
sheet:stamp(ground, 0, 0)
table.sort(standing, function(a, b) if a[1] == b[1] then return a[2] < b[2] end return a[1] < b[1] end)
for _, s in ipairs(standing) do s[3]() end

-- Transition tiles: each kind's corner tiles over the plain, by corner mask 1 to 14 (mask 0
-- is empty).
local transitions = load("terrain/transitions.aseprite")
local landforms = load("terrain/landforms.aseprite")
local EDGED = { "forest", "hills", "water", "mountain" }
label(8, 352, "TRANSITIONS, CORNER MASK 1-14 (TL 1, TR 2, BL 4, BR 8)", c("paper", 3))
for k, kind in ipairs(EDGED) do
  local y = 362 + (k - 1) * 18
  label(8, y + 6, kind:upper(), c("paper", 2))
  for m = 1, 14 do
    local x = 48 + (m - 1) * 17
    sheet:stamp(frame(terrain, "plain", 1 + m % 4), x, y)
    sheet:stamp(frame(transitions, kind, m + 1), x, y)
  end
end

-- Landforms: each frame on the full tiles of its kind, its footprint outlined in the frame.
label(312, 352, "LANDFORMS, FOOTPRINT AT BOTTOM LEFT", c("paper", 3))
local forms = {}
for name, range in pairs(landforms.tags) do forms[#forms + 1] = { range[1], name } end
table.sort(forms, function(a, b) return a[1] < b[1] end)
for n, f in ipairs(forms) do
  local x, y = 312 + ((n - 1) % 4) * 72, 362 + ((n - 1) // 4) * 84
  local kind, w, h = f[2]:match("^(%a+)_(%d+)x(%d+)$")
  w, h = tonumber(w), tonumber(h)
  for row = 0, h - 1 do
    for col = 0, w - 1 do sheet:stamp(frame(terrain, kind, 1 + (row + col) % 4), x + col * 16, y + 64 - (h - row) * 16) end
  end
  sheet:stamp(landforms.frames[f[1]], x, y)
  label(x, y + 68, f[2]:upper(), c("paper", 2))
end

-- A mixed patch of 24 x 12 cells. Terrain is partitioned by nearest site, as the region
-- generator does, with kinds drawn by its weights. The plain is laid on every cell. Each other
-- kind is laid on the dual grid, half a cell up and left: the tile whose corners sit on four
-- cells' centres takes its mask from which of those cells are the kind, mask 15 being a full
-- tile. Landforms then stand on stretches of the kind, placed greedily in a seeded order.
local PW, PH = 24, 12
local PX, PY = 8, 556
label(8, PY - 12, "MIXED TERRAIN, 24 X 12 CELLS, SEEDED", c("paper", 3))
local weights = { plain = 4, forest = 3, hills = 2, water = 1, mountain = 1 }
local rng = px.rng(4601)
local sites = {}
for n = 1, 11 do
  local kind
  if n <= #KINDS then
    kind = KINDS[n]
  else
    local roll, acc = rng.int(1, 11), 0
    for _, kk in ipairs(KINDS) do
      acc = acc + weights[kk]
      if roll <= acc then kind = kk; break end
    end
  end
  sites[n] = { rng.int(0, PW - 1), rng.int(0, PH - 1), kind }
end
local cells = {}
local function kind_at(col, row)
  col = math.max(0, math.min(PW - 1, col)); row = math.max(0, math.min(PH - 1, row))
  return cells[row * PW + col]
end
for row = 0, PH - 1 do
  for col = 0, PW - 1 do
    local best, bd = nil, math.huge
    for _, s in ipairs(sites) do
      local d = (s[1] - col) ^ 2 + (s[2] - row) ^ 2
      if d < bd then bd, best = d, s[3] end
    end
    cells[row * PW + col] = best
  end
end
local patch = px.canvas(W, H)
for row = 0, PH - 1 do
  for col = 0, PW - 1 do
    patch:stamp(frame(terrain, "plain", 1 + math.floor(px.hash(col, row, 46) * 4)), PX + col * 16, PY + row * 16)
  end
end
-- Dual-grid tile (dx, dy) has its corners on cells (dx - 1, dy - 1), (dx, dy - 1),
-- (dx - 1, dy) and (dx, dy), and is drawn half a cell up and left of cell (dx, dy).
for _, kind in ipairs(EDGED) do
  for dy = 0, PH do
    for dx = 0, PW do
      local function is(col, row) return kind_at(col, row) == kind and 1 or 0 end
      local mask = is(dx - 1, dy - 1) + 2 * is(dx, dy - 1) + 4 * is(dx - 1, dy) + 8 * is(dx, dy)
      local x, y = PX + dx * 16 - 8, PY + dy * 16 - 8
      if mask == 15 then
        patch:stamp(frame(terrain, kind, 1 + math.floor(px.hash(dx, dy, 47) * 4)), x, y)
      elseif mask > 0 then
        patch:stamp(frame(transitions, kind, mask + 1), x, y)
      end
    end
  end
end
local sizes = { mountain = { { 4, 3 }, { 3, 3 }, { 3, 2 }, { 2, 2 } }, hills = { { 3, 3 }, { 3, 2 }, { 2, 2 }, { 2, 1 } } }
-- How often a free cell is left bare: mountains mostly covered, hills left open between.
local skip = { mountain = 0.05, hills = 0.3 }
local taken, placed = {}, {}
local order = {}
for i = 0, PW * PH - 1 do order[#order + 1] = i end
table.sort(order, function(a, b) return px.hash(a, 0, 461) < px.hash(b, 0, 461) end)
for _, i in ipairs(order) do
  local col, row = i % PW, i // PW
  local kind = cells[i]
  if sizes[kind] and not taken[i] and px.hash(col, row, 462) > skip[kind] then
    local start = 1 + math.floor(px.hash(col, row, 463) * #sizes[kind])
    for t = 0, #sizes[kind] - 1 do
      local s = sizes[kind][(start + t - 1) % #sizes[kind] + 1]
      local fits = col + s[1] <= PW and row + s[2] <= PH
      for r = row, row + s[2] - 1 do
        for q = col, col + s[1] - 1 do
          if not fits then break end
          local j = r * PW + q
          if cells[j] ~= kind or taken[j] then fits = false end
        end
      end
      if fits then
        for r = row, row + s[2] - 1 do for q = col, col + s[1] - 1 do taken[r * PW + q] = true end end
        placed[#placed + 1] = { col, row, s[1], s[2], kind }
        break
      end
    end
  end
end
table.sort(placed, function(a, b) return a[2] + a[4] < b[2] + b[4] end)
for _, p in ipairs(placed) do
  local name = p[5] .. "_" .. p[3] .. "x" .. p[4]
  patch:stamp(landforms.frames[landforms.tags[name][1]], PX + p[1] * 16, PY + (p[2] + p[4]) * 16 - 64)
end
-- The patch is clipped to its 24 x 12 cells: the dual grid's outer half cells and the
-- landforms' rise past the top are cut.
for y = PY, PY + PH * 16 - 1 do
  for x = PX, PX + PW * 16 - 1 do sheet:set(x, y, patch:get(x, y)) end
end

-- The hall in its four stages, each on its 5x3-cell footprint of plain ground.
label(8, 760, "HALL, 5 X 3 CELLS", c("paper", 3))
for k, stage in ipairs({ "intact", "damaged", "destroyed", "rubble" }) do
  local x, y = 8 + (k - 1) * 104, 770
  for cy = 0, 2 do for cx = 0, 4 do sheet:stamp(frame(terrain, "plain", 1 + (cx + cy) % 4), x + 8 + cx * 16, y + 8 + cy * 16) end end
  sheet:stamp(frame(hall, stage), x, y)
  label(x + 8, y + 62, stage:upper(), c("paper", 2))
end

-- The village props that are structures, each stage on its footprint of plain ground; then
-- the decorations, ground tiles and banners, and the technique icons.
local function stages(set, name, fw, fh, x0, y0, step)
  label(x0, y0 - 10, name, c("paper", 3))
  for k, stage in ipairs({ "intact", "damaged", "destroyed", "rubble" }) do
    local x = x0 + (k - 1) * step
    for cy = 0, fh - 1 do
      for cx = 0, fw - 1 do sheet:stamp(frame(terrain, "plain", 1 + (cx + cy) % 4), x + 8 + cx * 16, y0 + 8 + cy * 16) end
    end
    sheet:stamp(frame(set, stage), x, y0)
  end
end
stages(well, "WELL, 2 X 2", 2, 2, 8, 868, 56)
stages(barrels, "BARRELS, 1 X 1", 1, 1, 240, 868, 40)
stages(fence, "FENCE, 5 X 1", 5, 1, 8, 932, 104)
label(8, 982, "DECORATION: PAVING, ROAD, MILLET, RICE; BANNERS; TECHNIQUE ICONS", c("paper", 3))
local gx = 8
for _, tag in ipairs({ "paving", "road", "millet", "rice" }) do
  local first, last = ground_tiles.tags[tag][1], ground_tiles.tags[tag][2]
  for f = first, last do
    sheet:stamp(ground_tiles.frames[f], gx, 996)
    gx = gx + 17
  end
end
for k, tag in ipairs({ "orthodox", "demonic", "unorthodox" }) do
  sheet:stamp(frame(terrain, "plain", k), gx + 8 + (k - 1) * 18, 996)
  sheet:stamp(frame(banner, tag), gx + 8 + (k - 1) * 18, 988)
end
for k, tag in ipairs({ "strike", "burst" }) do
  sheet:rect(gx + 70 + (k - 1) * 22, 994, 20, 20, c("ink", 1))
  sheet:stamp(frame(icons, tag), gx + 72 + (k - 1) * 22, 996)
end

-- The economy: the granary and the storehouse in their stages; fields in each state, in summer
-- and under snow; the economy icons and the ware icons; then all of it assembled in a village.
local granary = load("structures/granary.aseprite")
local storehouse = load("structures/storehouse.aseprite")
local fields = load("decorations/fields.aseprite")
local economy_icons = load("icons/economy_icons.aseprite")
label(8, 1030, "ECONOMY", c("paper", 3))
stages(granary, "GRANARY, 2 X 2", 2, 2, 8, 1050, 56)
stages(storehouse, "STOREHOUSE, 3 X 2", 3, 2, 240, 1050, 72)
local states = { "sown", "growing", "ripe", "fallow", "burnt" }
-- A plot of 3 x 2 tiles of `tag`, its variants alternating, at (x, y).
local function plot(canvas, tag, x, y, cols, rows)
  for r = 0, rows - 1 do
    for q = 0, cols - 1 do
      canvas:stamp(frame(fields, tag, 1 + (q + r) % 2), x + q * 16, y + r * 16)
    end
  end
end
for k, state in ipairs(states) do
  local x = 8 + (k - 1) * 56
  plot(sheet, state, x, 1120, 3, 2)
  label(x, 1154, state:upper(), c("paper", 2))
  plot(sheet, state .. "_winter", x, 1166, 3, 2)
end
label(8, 1110, "FIELDS, SUMMER", c("paper", 3))
label(8, 1200, "UNDER SNOW", c("paper", 2))
label(300, 1110, "ECONOMY ICONS", c("paper", 3))
for k, tag in ipairs({ "food", "stock", "housing" }) do
  local x = 300 + (k - 1) * 48
  sheet:rect(x, 1120, 20, 20, c("ink", 1))
  sheet:stamp(frame(economy_icons, tag), x + 2, 1122)
  label(x, 1144, tag:upper(), c("paper", 2))
end
local ware_icons = load("icons/ware_icons.aseprite")
label(300, 1158, "WARE ICONS", c("paper", 3))
for k, tag in ipairs({ "herb", "elixir", "silver" }) do
  local x = 300 + (k - 1) * 48
  sheet:rect(x, 1168, 20, 20, c("ink", 1))
  sheet:stamp(frame(ware_icons, tag), x + 2, 1170)
  label(x, 1192, tag:upper(), c("paper", 2))
end

-- A village of 30 x 10 cells: houses, the granary and the storehouse north of the road, a well
-- and barrels by it, and fields in each state south of it. Everything standing is drawn in order
-- of the bottom edge of its footprint.
local VX, VY = 8, 1224
label(8, VY - 12, "ASSEMBLED IN A VILLAGE", c("paper", 3))
local village = px.canvas(W, H)
for row = 0, 9 do
  for col = 0, 29 do
    village:stamp(frame(terrain, "plain", 1 + math.floor(px.hash(col, row, 77) * 4)), VX + col * 16, VY + row * 16)
  end
end
local ground_tiles = load("decorations/ground.aseprite")
for col = 0, 29 do
  village:stamp(ground_tiles.frames[ground_tiles.tags.road[1] + col % 4], VX + col * 16, VY + 5 * 16)
end
for k, state in ipairs(states) do plot(village, state, VX + (2 + (k - 1) * 4) * 16, VY + 6 * 16, 3, 3) end
local standing_village = {}
local function stand_at(set, tag, col, row, rows)
  standing_village[#standing_village + 1] = { row + rows, #standing_village, function()
    village:stamp(frame(set, tag), VX + col * 16 - 8, VY + row * 16 - 8)
  end }
end
stand_at(house, "intact", 1, 1, 3)
stand_at(granary, "intact", 5, 2, 2)
stand_at(storehouse, "intact", 8, 2, 2)
stand_at(granary, "damaged", 12, 2, 2)
stand_at(house, "intact", 15, 1, 3)
stand_at(storehouse, "destroyed", 19, 2, 2)
stand_at(well, "intact", 23, 3, 2)
stand_at(barrels, "intact", 26, 4, 1)
stand_at(house, "rubble", 26, 6, 3)
table.sort(standing_village, function(a, b) if a[1] == b[1] then return a[2] < b[2] end return a[1] < b[1] end)
for _, s in ipairs(standing_village) do s[3]() end
for y = VY, VY + 159 do
  for x = VX, VX + 479 do sheet:set(x, y, village:get(x, y)) end
end

-- A sect's seat (ADR 0026): the training ground, the manual hall and the seat gate in their
-- stages, the sect's banners beside the faction's, then a seat village assembled from them.
local training_ground = load("structures/training_ground.aseprite")
local manual_hall = load("structures/manual_hall.aseprite")
local seat_gate = load("structures/seat_gate.aseprite")
local sect_banner = load("decorations/sect_banner.aseprite")
label(8, 1396, "SECT SEAT", c("paper", 3))
stages(training_ground, "TRAINING GROUND, 6 X 4", 6, 4, 8, 1420, 120)
stages(manual_hall, "MANUAL HALL, 4 X 3", 4, 3, 8, 1520, 88)
label(372, 1510, "SECT AND FACTION BANNERS, 1 X 1", c("paper", 3))
for k, tag in ipairs({ "orthodox", "demonic", "unorthodox" }) do
  local x = 372 + (k - 1) * 44
  sheet:stamp(frame(terrain, "plain", k), x, 1552); sheet:stamp(frame(terrain, "plain", k + 1), x + 18, 1552)
  sheet:stamp(frame(sect_banner, tag), x, 1536)
  sheet:stamp(frame(banner, tag), x + 18, 1544)
end
-- The gate's footprint is the 48 x 16 at (8, 40) in its frame; what rises stands above it.
label(8, 1594, "SEAT GATE, 3 X 1", c("paper", 3))
for k, stage in ipairs({ "intact", "damaged", "destroyed", "rubble" }) do
  local x = 8 + (k - 1) * 72
  for cx = 0, 2 do sheet:stamp(frame(terrain, "plain", 1 + cx % 4), x + 8 + cx * 16, 1604 + 40) end
  sheet:stamp(frame(seat_gate, stage), x, 1604)
end

-- A seat of 30 x 13 cells: the manual hall at the head of a paved square, the training ground
-- in it, the gate at its foot where the road comes in; around them the houses, stores and fields
-- every village has. The training ground is laid with the ground, so pupils stand on it; the
-- rest stands and is drawn in order of the bottom edge of its footprint.
local SVX, SVY = 8, 1690
label(8, SVY - 12, "ASSEMBLED IN A SECT'S SEAT", c("paper", 3))
local seat = px.canvas(W, H)
for row = 0, 12 do
  for col = 0, 29 do
    seat:stamp(frame(terrain, "plain", 1 + math.floor(px.hash(col, row, 91) * 4)), SVX + col * 16, SVY + row * 16)
  end
end
for row = 4, 9 do
  for col = 10, 20 do seat:stamp(ground_tiles.frames[ground_tiles.tags.paving[1] + (col + row) % 4], SVX + col * 16, SVY + row * 16) end
end
for row = 10, 12 do seat:stamp(ground_tiles.frames[ground_tiles.tags.road[1] + row % 4], SVX + 15 * 16, SVY + row * 16) end
for _, p in ipairs({ { "ripe", 1 }, { "growing", 5 }, { "ripe", 22 }, { "sown", 26 } }) do plot(seat, p[1], SVX + p[2] * 16, SVY + 11 * 16, 3, 2) end
seat:stamp(frame(training_ground, "intact"), SVX + 12 * 16 - 8, SVY + 5 * 16 - 8)
local standing_seat = {}
-- Stands a frame whose footprint's top-left sits at (ox, oy) in it, footprint at (col, row).
local function stand_seat(set, tag, col, row, rows, ox, oy)
  standing_seat[#standing_seat + 1] = { row + rows, #standing_seat, function()
    seat:stamp(frame(set, tag), SVX + col * 16 - ox, SVY + row * 16 - oy)
  end }
end
stand_seat(manual_hall, "intact", 13, 1, 3, 8, 8)
stand_seat(seat_gate, "intact", 14, 12, 1, 8, 40)
for _, b in ipairs({ { 12, 3 }, { 17, 3 }, { 13, 12 }, { 17, 12 } }) do stand_seat(sect_banner, "orthodox", b[1], b[2], 1, 0, 16) end
stand_seat(house, "intact", 1, 2, 3, 8, 8)
stand_seat(house, "intact", 5, 1, 3, 8, 8)
stand_seat(granary, "intact", 1, 6, 2, 8, 8)
stand_seat(storehouse, "intact", 4, 6, 2, 8, 8)
stand_seat(well, "intact", 8, 7, 2, 8, 8)
stand_seat(hall, "intact", 21, 1, 3, 8, 8)
stand_seat(house, "intact", 26, 2, 3, 8, 8)
stand_seat(granary, "intact", 22, 6, 2, 8, 8)
stand_seat(house, "intact", 25, 6, 3, 8, 8)
stand_seat(barrels, "intact", 20, 8, 1, 8, 8)
for _, b in ipairs({
  { 13, 7, "idle" }, { 14, 7, "walk", 2 }, { 16, 6, "walk", 1 }, { 17, 7, "idle" },
  { 15, 8, "walk", 3 }, { 11, 9, "idle" }, { 15, 11, "walk", 4 },
}) do
  standing_seat[#standing_seat + 1] = { b[2] + 1, #standing_seat, function()
    seat:stamp(frame(bodies.orthodox, b[3], b[4]), SVX + b[1] * 16, SVY + b[2] * 16 - 8)
  end }
end
table.sort(standing_seat, function(a, b) if a[1] == b[1] then return a[2] < b[2] end return a[1] < b[1] end)
for _, s in ipairs(standing_seat) do s[3]() end
for y = SVY, SVY + 207 do
  for x = SVX, SVX + 479 do sheet:set(x, y, seat:get(x, y)) end
end

-- The workshops a growing village builds: the smithy and the apothecary in their stages, then a
-- grown village of 30 x 10 cells, its workshops among the houses and stores by the road and
-- fields south of it. Everything standing is drawn in order of the bottom edge of its footprint.
local smithy = load("structures/smithy.aseprite")
local apothecary = load("structures/apothecary.aseprite")
label(8, 1912, "WORKSHOPS", c("paper", 3))
stages(smithy, "SMITHY, 3 X 2", 3, 2, 8, 1936, 72)
stages(apothecary, "APOTHECARY, 4 X 2", 4, 2, 8, 2000, 88)
local GVX, GVY = 8, 2076
label(8, GVY - 12, "ASSEMBLED IN A GROWN VILLAGE", c("paper", 3))
local grown = px.canvas(W, H)
for row = 0, 9 do
  for col = 0, 29 do
    grown:stamp(frame(terrain, "plain", 1 + math.floor(px.hash(col, row, 108) * 4)), GVX + col * 16, GVY + row * 16)
  end
end
for col = 0, 29 do
  grown:stamp(ground_tiles.frames[ground_tiles.tags.road[1] + col % 4], GVX + col * 16, GVY + 5 * 16)
end
for k, p in ipairs({ { "ripe", 9 }, { "growing", 13 }, { "ripe", 17 }, { "sown", 21 } }) do plot(grown, p[1], GVX + p[2] * 16, GVY + 7 * 16, 3, 3) end
local standing_grown = {}
local function stand_grown(set, tag, col, row, rows)
  standing_grown[#standing_grown + 1] = { row + rows, #standing_grown, function()
    grown:stamp(frame(set, tag), GVX + col * 16 - 8, GVY + row * 16 - 8)
  end }
end
stand_grown(house, "intact", 1, 1, 3)
stand_grown(smithy, "intact", 5, 3, 2)
stand_grown(granary, "intact", 9, 2, 2)
stand_grown(house, "intact", 12, 1, 3)
stand_grown(apothecary, "intact", 16, 3, 2)
stand_grown(storehouse, "intact", 21, 2, 2)
stand_grown(well, "intact", 25, 3, 2)
stand_grown(barrels, "intact", 28, 4, 1)
stand_grown(house, "intact", 1, 6, 3)
stand_grown(smithy, "damaged", 5, 7, 2)
stand_grown(apothecary, "destroyed", 25, 7, 2)
for _, b in ipairs({ { "unorthodox", 8, 4, "idle" }, { "unorthodox", 15, 5, "walk", 2 }, { "orthodox", 20, 4, "idle" } }) do
  standing_grown[#standing_grown + 1] = { b[3] + 1, #standing_grown, function()
    grown:stamp(frame(bodies[b[1]], b[4], b[5]), GVX + b[2] * 16, GVY + b[3] * 16 - 8)
  end }
end
table.sort(standing_grown, function(a, b) if a[1] == b[1] then return a[2] < b[2] end return a[1] < b[1] end)
for _, s in ipairs(standing_grown) do s[3]() end
for y = GVY, GVY + 159 do
  for x = GVX, GVX + 479 do sheet:set(x, y, grown:get(x, y)) end
end

px.save(W, H, { { "sheet", sheet } }, here .. "/contact_sheet.aseprite")
