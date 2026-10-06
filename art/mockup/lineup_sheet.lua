-- Draws lineup_sheet.aseprite: the bodies of each faction, villagers and the player on 16-pixel
-- grass cells, the village house in its four damage stages, and the aggregate-army marker.
-- Run: aseprite --batch --script art/mockup/lineup_sheet.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)
local c = px.c

local W, H = 320, 232
local bg = px.canvas(W, H)
local ground = px.canvas(W, H)
local things = px.canvas(W, H)
local labels = px.canvas(W, H)

bg:rect(0, 0, W, H, c("ink", 2))
for y = 0, H - 1, 2 do bg:hline(0, y, W, c("ink", 2)) end

local function grass_strip(x, y, cells, rows)
  for yy = y, y + rows * 16 - 1 do
    for xx = x, x + cells * 16 - 1 do
      local n = px.noise(xx, yy, 5, 3) + (px.hash(xx, yy, 9) - 0.5) * 0.25
      local col = c("grass", 4)
      if n > 0.62 then col = c("grass", 5) elseif n < 0.35 then col = c("grass", 3) end
      ground:set(xx, yy, col)
    end
  end
  -- The 16-pixel grid, faint.
  for k = 0, cells do
    for yy = y, y + rows * 16 - 1, 2 do ground:shade_at(x + k * 16, yy, -1) end
  end
end

labels:text(8, 6, "MURIM24  FACTION LINE-UP AND BUILDING STAGES", c("paper", 3))
labels:text(8, 13, "NATIVE PIXELS, ONE CELL = 16 PX", c("stone", 4))

local rows = {
  { "ORTHODOX", "azure", {
    { faction = "orthodox", head = "topknot", weapon = "sword" },
    { faction = "orthodox", head = "bun", weapon = "sword", over = { A = c("azure", 4) } },
    { faction = "orthodox", head = "white", weapon = "staff", over = { R = c("azure", 3), M = c("azure", 2), D = c("azure", 1), A = c("robe", 3) } },
    { faction = "orthodox", head = "topknot", body = "tunic", weapon = "spear" },
    { faction = "orthodox", head = "topknot", body = "run", weapon = "sword_strike" },
  } },
  { "DEMONIC CULT", "crimson", {
    { faction = "demonic", head = "spiky", body = "tunic", weapon = "saber" },
    { faction = "demonic", head = "hood", weapon = "knife" },
    { faction = "demonic", head = "white", over = { R = c("crimson", 3), M = c("crimson", 2), D = c("crimson", 1), A = c("ink", 1), W = c("stone", 6) } },
    { faction = "demonic", head = "bald", body = "big", over = { R = c("skin", 3), M = c("skin", 2), D = c("skin", 1) } },
    { faction = "demonic", head = "spiky", body = "run", weapon = "saber_strike" },
  } },
  { "UNORTHODOX", "amber", {
    { faction = "unorthodox", head = "topknot", hat = "straw", body = "tunic", weapon = "knife" },
    { faction = "unorthodox", head = "mask", body = "tunic", weapon = "knife", over = { X = c("ink", 2), x = c("ink", 1) } },
    { faction = "unorthodox", head = "topknot", body = "fat", weapon = "fan", over = { R = c("amber", 4), M = c("amber", 3), D = c("amber", 2), A = c("crimson", 3) } },
    { faction = "unorthodox", head = "topknot", hat = "straw", weapon = "bow" },
    { faction = "unorthodox", head = "topknot", hat = "straw", body = "run", weapon = "saber_strike" },
  } },
}

local y0 = 24
for n, row in ipairs(rows) do
  local y = y0 + (n - 1) * 30
  labels:text(8, y + 7, row[1], c(row[2], 4))
  -- Accent swatch.
  labels:rect(8, y + 14, 12, 3, c(row[2], 3)); labels:rect(20, y + 14, 6, 3, c(row[2], 4)); labels:rect(26, y + 14, 4, 3, c(row[2], 2))
  grass_strip(64, y, 6, 1)
  for k, spec in ipairs(row[3]) do
    local x = 64 + (k - 1) * 16
    S.shadow(ground, x, y)
    S.place(things, S.body(spec), x, y)
  end
  -- The same body knocked down.
  S.place(things, S.body(row[3][1]), 64 + 5 * 16, y, false, true)
end

-- Villagers and the player.
local y = y0 + 3 * 30
labels:text(8, y + 7, "VILLAGERS", c("cloth", 4))
labels:text(8, y + 14, "PLAYER", c("jade", 3))
grass_strip(64, y, 6, 1)
local folk = {
  { faction = "villager", head = "topknot", body = "tunic" },
  { faction = "villager", head = "bun", over = { R = c("cloth", 4), A = c("lacquer", 3) } },
  { faction = "villager", head = "white", body = "tunic", weapon = "staff" },
  { faction = "villager", head = "topknot", hat = "straw", body = "tunic", over = { X = c("earth", 6), x = c("earth", 5), Y = c("earth", 4) } },
}
for k, spec in ipairs(folk) do
  local x = 64 + (k - 1) * 16
  S.shadow(ground, x, y)
  S.place(things, S.body(spec), x, y)
end
-- The player: the same rules as an NPC, marked by a jade ring and a chevron.
local pxx = 64 + 5 * 16
ground:each_ellipse(pxx + 8, y + 15, 7, 2.6, function(xx, yy, d) if d > 0.7 then ground:set(xx, yy, c("jade", 3)) end end)
S.place(things, S.body({ faction = "orthodox", head = "topknot", weapon = "sword", over = { A = c("jade", 3) } }), pxx, y)
things:sprite(pxx + 6, y - 4, { "#####", ".###.", "..#.." }, { ["#"] = c("gold", 4) })

-- Grid cells to the right of the line-up: the per-faction accent at 16 px, side by side.
labels:text(176, 24, "ACCENTS AT ONE CELL", c("stone", 4))
for k, ramp in ipairs({ "azure", "crimson", "amber", "jade" }) do
  local x = 176 + (k - 1) * 20
  labels:rect(x, 32, 16, 16, c(ramp, 3))
  labels:rect(x, 32, 16, 3, c(ramp, 4))
  labels:rect(x, 45, 16, 3, c(ramp, 2))
end

-- The aggregate army.
labels:text(176, 56, "AGGREGATE ARMY MARKER", c("stone", 4))
grass_strip(176, 64, 8, 4)
S.army(things, ground, 186, 86, 14, 4, "unorthodox", "amber", 41)
labels:rect(206, 114, 46, 9, c("ink", 1))
labels:text(208, 116, "x 1,240", c("amber", 5))

-- The house in four stages.
local sy = 148
labels:text(8, sy - 8, "VILLAGE HOUSE, 3 X 3 CELLS", c("stone", 4))
local stages = { "intact", "damaged", "destroyed", "rubble" }
for k, stage in ipairs(stages) do
  local x = 8 + (k - 1) * 78
  grass_strip(x, sy, 4, 4)
  S.house(things, ground, x + 6, sy + 8, stage, 100 + k)
  labels:text(x, sy + 66, stage:upper(), c("paper", 3))
end
local fx = px.canvas(W, H)
S.fire(fx, 8 + 2 * 78 + 10, sy + 14, 36, 77)

px.save(W, H, { { "background", bg }, { "ground", ground }, { "bodies and structures", things }, { "effects", fx }, { "labels", labels } },
  here .. "/lineup_sheet.aseprite")
