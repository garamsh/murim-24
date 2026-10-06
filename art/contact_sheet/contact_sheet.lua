-- Draws contact_sheet.aseprite: every frame of the production set, read back from the
-- .aseprite sources, and a small scene built from them on the 16-pixel grid with shadows
-- drawn the way the view is to draw them. It is a review image; the game does not load it.
-- Run: aseprite --batch --script art/contact_sheet/contact_sheet.lua (after the asset scripts)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)
local c = px.c
local art = here .. "/.."

local W, H = 608, 584
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

-- Each terrain kind tiled over 12 x 6 cells, every cell's variant picked by a seeded hash,
-- to show how a stretch of one kind reads.
label(8, 352, "TERRAIN TILED 12 X 6 CELLS, VARIANT PER CELL BY SEEDED HASH", c("paper", 3))
for k, kind in ipairs(KINDS) do
  local x, y = 8 + ((k - 1) % 3) * 200, 368 + ((k - 1) // 3) * 112
  label(x, y - 8, kind:upper(), c("paper", 2))
  for row = 0, 5 do
    for col = 0, 11 do
      sheet:stamp(frame(terrain, kind, 1 + math.floor(px.hash(col, row, 100 + k) * 4)), x + col * 16, y + row * 16)
    end
  end
end

px.save(W, H, { { "sheet", sheet } }, here .. "/contact_sheet.aseprite")
