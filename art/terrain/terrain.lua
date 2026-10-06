-- Draws terrain.aseprite: four 16x16 variants of each terrain kind the region generator
-- defines (core/region.cpp, region_data()), one tag per kind; and transitions.aseprite, the
-- edge and corner tiles where a kind meets the plain (see the end of this file). Every
-- variant's ground is drawn from noise that repeats every 16 pixels and is the same in every
-- variant near the tile's edge, so any variant meets any other of its kind without a seam.
-- Features larger than a cell (massifs, hill clusters) are landforms.lua's, not a tile's.
-- Run: aseprite --batch --script art/terrain/terrain.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c

local T = 16
local VARIANTS = 4

-- Ground noise: per-pixel grain, plus the variant's own coarse noise faded out over the four
-- pixels nearest the tile's edge. Near the edge every variant is grain alone, which has no
-- shape to break at a seam, and the variants differ in the middle.
local function ground(x, y, scale, seed, v)
  local edge = math.min(x, y, T - 1 - x, T - 1 - y)
  local window = math.min(1, (edge + 0.5) / 4)
  return 0.5 + (px.hash(x, y, seed + 7 * v) - 0.5) * 0.3
    + (px.tile_noise(x, y, scale, T, seed + 100 * v) - 0.5) * 0.6 * window
end

local function plain(cv, v)
  local r = px.rng(100 + v)
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local n = ground(x, y, 8, 10, v)
      local col = c("grass", 4)
      if n > 0.66 + px.bayer(x, y) * 0.1 then col = c("grass", 5)
      elseif n < 0.28 + px.bayer(x, y) * 0.1 then col = c("grass", 3) end
      cv:set(x, y, col)
    end
  end
  -- Tufts, kept off the edge so they never break across a seam.
  for k = 1, 2 + v % 2 do
    local x, y = r.int(2, 13), r.int(2, 12)
    cv:set(x, y, c("grass", 6)); cv:set(x - 1, y + 1, c("grass", 5)); cv:set(x + 1, y + 1, c("grass", 5))
    cv:set(x, y + 1, c("grass", 3))
  end
  if v == 4 then
    cv:set(5, 10, c("paper", 3)); cv:set(11, 4, c("amber", 5)); cv:set(4, 9, c("grass", 6))
  end
end

local function forest(cv, v)
  -- Separate crowns seen from above, each lit from the upper left with a dark rim, over a
  -- dark understorey that shows between them; a trunk shows below a crown where there is
  -- room. Every crown stays inside its tile, so tiles meet on understorey alone.
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local n = ground(x, y, 4, 30, v)
      cv:set(x, y, n > 0.6 and c("grass", 2) or (n > 0.3 and c("leaf", 2) or c("leaf", 1)))
    end
  end
  local crowns = ({
    { { 5, 5, 4.5 }, { 11, 10, 4.5 } },
    { { 8, 6, 5.5 }, { 3, 13, 2.5 } },
    { { 4, 9, 4 }, { 11, 4, 4 }, { 12, 12, 3 } },
    { { 6, 6, 5 }, { 12, 12, 3.5 } },
  })[v]
  for _, k in ipairs(crowns) do
    local cx, cy, rad = k[1], k[2], k[3]
    -- Shadow on the floor to the lower right, then the trunk where the crown leaves room.
    cv:each_ellipse(cx + 1.5, cy + 1.5, rad, rad * 0.9, function(x, y)
      if x >= 0 and x < T and y >= 0 and y < T then cv:set(x, y, c("leaf", 1)) end
    end)
    if cy + rad + 2 <= T - 1 then
      cv:vline(cx, math.floor(cy + rad) - 1, 3, c("wood", 2)); cv:set(cx - 1, math.floor(cy + rad) + 1, c("wood", 3))
    end
  end
  for _, k in ipairs(crowns) do
    local cx, cy, rad = k[1], k[2], k[3]
    cv:each_ellipse(cx, cy, rad, rad * 0.9, function(x, y, d)
      if x < 0 or x >= T or y < 0 or y >= T then return end
      local dx, dy = (x - cx) / rad, (y - cy) / rad
      local n = px.tile_noise(x, y, 2, T, 40 + v)
      local light = -dx * 0.7 - dy * 0.9 + (n - 0.5) * 0.7
      local col = c("leaf", 3)
      if light > 0.75 then col = c("leaf", 5) elseif light > 0.2 then col = c("leaf", 4)
      elseif light < -0.55 then col = c("leaf", 2) end
      if d > 0.82 and dx + dy > 0.2 then col = c("leaf", 1) end
      cv:set(x, y, col)
    end)
  end
end

local function hills(cv, v)
  -- Upland grass, a step lighter than the plain, with a faint contour arc in the middle of
  -- the tile. The mounds themselves are the multi-cell landforms (landforms.lua), so no
  -- tile carries a feature of its own and a stretch of hills shows no per-cell rhythm.
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local n = ground(x, y, 4, 50, v)
      local col = c("grass", 5)
      if n < 0.36 + px.bayer(x, y) * 0.1 then col = c("grass", 4)
      elseif n > 0.72 then col = c("grass", 6) end
      cv:set(x, y, col)
    end
  end
  -- One faint contour arc in the middle of the tile.
  local r = px.rng(250 + v)
  local x, y = r.int(4, 8), r.int(5, 10)
  for t = 0, 4 do cv:set(x + t, y + ((t == 0 or t == 4) and 0 or 1), c("grass", 4)) end
end

local function water(cv, v)
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local n = ground(x, y, 4, 60, v)
      cv:set(x, y, n < 0.3 + px.bayer(x, y) * 0.12 and c("water", 2) or c("water", 3))
    end
  end
  local r = px.rng(300 + v)
  for k = 1, 3 do
    local x, y = r.int(1, 11), r.int(1, 14)
    local len = r.int(2, 4)
    cv:hline(x, y, len, c("water", 5)); cv:set(x + 1, y + 1, c("water", 4))
  end
end

local function mountain(cv, v)
  -- Bare rock underfoot: fractured stone and scree. The peaks are the multi-cell massifs
  -- (landforms.lua), so no tile carries a feature of its own.
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local n = ground(x, y, 4, 70, v)
      local col = n > 0.6 and c("stone", 4) or (n > 0.35 and c("stone", 3) or c("stone", 2))
      if ground(x, y, 2, 75, v) > 0.84 then col = c("stone", 1) end
      cv:set(x, y, col)
    end
  end
end

local kinds = {
  { "plain", plain }, { "forest", forest }, { "hills", hills }, { "water", water }, { "mountain", mountain },
}
local frames, tags = {}, {}
for _, k in ipairs(kinds) do
  local first = #frames + 1
  for v = 1, VARIANTS do
    local cv = px.canvas(T, T)
    k[2](cv, v)
    frames[#frames + 1] = cv
  end
  tags[#tags + 1] = { k[1], first, #frames, 100 }
end

px.save_frames(T, T, frames, tags, here .. "/terrain.aseprite")

-- Transition tiles, a Godot Match Corners set for each kind but the plain: 15 frames indexed
-- by corner mask (top-left 1, top-right 2, bottom-left 4, bottom-right 8), mask 0 to 14, the
-- kind's four full tiles above standing for mask 15. They are laid on a dual grid, half a
-- cell off the cells, so each tile's corners sit on four cells' centres; mask 0 has none of
-- the kind and is empty. Within a tile the kind's area is the corners blended across it plus
-- noise that repeats every 16 pixels, so two tiles agree all along the edge they share; the
-- rest is transparent, so the plain beneath shows. The rim is drawn per kind: foam and a bank
-- for water, a dark edge for forest, a contour for hills, a lit lip and a cliff foot for
-- mountains.
local function area(mask, x, y, seed)
  local u, w = (x + 0.5) / T, (y + 0.5) / T
  local function bit(b) return (mask & b) ~= 0 and 1 or 0 end
  local top = bit(1) * (1 - u) + bit(2) * u
  local bottom = bit(4) * (1 - u) + bit(8) * u
  local f = top * (1 - w) + bottom * w
  return f + (px.tile_noise(x, y, 4, T, seed) - 0.5) * 0.32
end

local rims = {
  forest = function(cv, inside, x, y, f, at)
    if inside then
      if f < 0.56 then cv:set(x, y, c("leaf", 1)) end
    elseif at(x - 1, y - 1) >= 0.5 or at(x, y - 1) >= 0.5 then
      cv:set(x, y, c("grass", 2))
    end
  end,
  hills = function(cv, inside, x, y, f, at)
    if inside and f < 0.58 then
      cv:set(x, y, at(x + 1, y + 1) < at(x - 1, y - 1) and c("grass", 3) or c("grass", 6))
    end
  end,
  water = function(cv, inside, x, y, f, at)
    if inside then
      if f < 0.55 then cv:set(x, y, c("water", 6)) elseif f < 0.6 then cv:set(x, y, c("water", 4)) end
    elseif f >= 0.4 then
      cv:set(x, y, c("earth", 5))
    elseif f >= 0.33 then
      cv:set(x, y, c("earth", 4))
    end
  end,
  mountain = function(cv, inside, x, y, f, at)
    if inside then
      if f < 0.58 then cv:set(x, y, at(x + 1, y + 1) < at(x - 1, y - 1) and c("ink", 3) or c("stone", 5)) end
    elseif at(x, y - 1) >= 0.5 or at(x - 1, y - 1) >= 0.5 then
      cv:set(x, y, c("grass", 2))
    end
  end,
}

local seeds = { forest = 31, hills = 51, water = 61, mountain = 71 }
local tframes, ttags = {}, {}
for _, k in ipairs(kinds) do
  if rims[k[1]] then
    local full = px.canvas(T, T)
    k[2](full, 1)
    local first = #tframes + 1
    for mask = 0, 14 do
      -- Mask 0 holds none of the kind: an empty frame, kept so the frame index is the mask.
      local cv = px.canvas(T, T)
      local at = function(x, y) return area(mask, x, y, seeds[k[1]]) end
      for y = 0, mask > 0 and T - 1 or -1 do
        for x = 0, T - 1 do
          local f = at(x, y)
          local inside = f >= 0.5
          if inside then cv:set(x, y, full:get(x, y)) end
          rims[k[1]](cv, inside, x, y, f, at)
        end
      end
      tframes[#tframes + 1] = cv
    end
    ttags[#ttags + 1] = { k[1], first, #tframes, 100 }
  end
end
px.save_frames(T, T, tframes, ttags, here .. "/transitions.aseprite")
