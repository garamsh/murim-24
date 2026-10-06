-- Draws terrain.aseprite: four 16x16 variants of each terrain kind the region generator
-- defines (core/region.cpp, region_data()), one tag per kind. Every variant's ground is drawn
-- from noise that repeats every 16 pixels and is the same in every variant near the tile's
-- edge, and the features on it (crowns, mounds, crags) stay inside the tile, so any variant
-- meets any other of its kind without a seam or a band.
-- Run: aseprite --batch --script art/terrain/terrain.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c

local T = 16
local VARIANTS = 4

-- Ground noise: a fine texture shared by every variant of a kind, plus the variant's own
-- coarse noise faded out over the four pixels nearest the tile's edge. Two variants then
-- meet on the same fine texture, without a seam, and differ in the middle.
local function ground(x, y, scale, seed, v)
  local edge = math.min(x, y, T - 1 - x, T - 1 - y)
  local window = math.min(1, (edge + 0.5) / 4)
  return 0.5 + (px.tile_noise(x, y, 2, T, seed) - 0.5) * 0.5
    + (px.tile_noise(x, y, scale, T, seed + 100 * v) - 0.5) * 1.1 * window
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
  -- Rough grass with one or two low mounds inside the tile, each lit from the upper left,
  -- rimmed with a contour on its lower side and casting a short shadow. Variants differ in
  -- how many mounds, where and how large; nothing crosses the tile's edge, so tiled hills
  -- read as hummocky ground rather than lines.
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local n = ground(x, y, 4, 50, v)
      cv:set(x, y, n < 0.42 + px.bayer(x, y) * 0.1 and c("grass", 3) or c("grass", 4))
    end
  end
  local mounds = ({
    { { 8, 8, 6, 4.5 } },
    { { 5, 5, 4, 3 }, { 11, 11, 4, 3 } },
    { { 9, 6, 5.5, 4 }, { 4, 12, 3, 2.2 } },
    { { 7, 9, 6.5, 4.5 } },
  })[v]
  for _, m in ipairs(mounds) do
    local cx, cy, rx, ry = m[1], m[2], m[3], m[4]
    cv:each_ellipse(cx + 1, cy + 1.2, rx, ry, function(x, y)
      if x >= 0 and x < T and y >= 0 and y < T then cv:set(x, y, c("grass", 2)) end
    end)
    cv:each_ellipse(cx, cy, rx, ry, function(x, y, d)
      if x < 0 or x >= T or y < 0 or y >= T then return end
      local dx, dy = (x - cx) / rx, (y - cy) / ry
      local n = px.tile_noise(x, y, 2, T, 55 + v)
      local light = -dx * 0.6 - dy * 0.8 + (n - 0.5) * 0.4
      local col = c("grass", 4)
      if light > 0.55 then col = c("grass", 6) elseif light > 0.1 then col = c("grass", 5)
      elseif light < -0.5 then col = c("grass", 3) end
      if d > 0.85 and dy > 0.1 then col = c("grass", 2) end
      cv:set(x, y, col)
    end)
  end
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
  -- Bare rock: one or two crags standing inside the tile on dark scree, each with a lit
  -- west face, a shaded east face, a pale summit and a shadowed foot. Variants differ in
  -- the number, place and height of the crags; no crag crosses the tile's edge, so tiled
  -- mountains read as a field of peaks rather than a band.
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local n = ground(x, y, 4, 70, v)
      local col = n > 0.55 and c("stone", 3) or c("stone", 2)
      if ground(x, y, 2, 75, v) > 0.82 then col = c("stone", 1) end
      cv:set(x, y, col)
    end
  end
  local crags = ({
    { { 8, 2, 14, 6.5 } },
    { { 5, 3, 12, 4.5 }, { 11, 6, 15, 4 } },
    { { 10, 2, 13, 5 }, { 4, 8, 15, 3.5 } },
    { { 7, 5, 15, 6.5 } },
  })[v]
  for _, k in ipairs(crags) do
    local peak, top, base, hw = k[1], k[2], k[3], k[4]
    for y = top, base do
      local t = (y - top) / (base - top)
      local half = t * hw + (px.tile_noise(peak, y, 2, T, 80 + v) - 0.5) * 1.5 * t
      for x = math.ceil(peak - half), math.floor(peak + half) do
        if x >= 0 and x < T then
          local col
          if x < peak then
            col = (x < peak - half * 0.55) and c("stone", 4) or c("stone", 5)
          elseif x == peak then
            col = c("stone", 4)
          else
            col = (x > peak + half * 0.5) and c("stone", 2) or c("stone", 3)
          end
          if y - top < 2 then col = c("stone", 6) end
          if y == base then col = c("ink", 3) end
          cv:set(x, y, col)
        end
      end
    end
    -- A gully down the shaded face.
    cv:line(peak + 1, top + 3, peak + math.floor(hw / 2), base - 1, c("stone", 1))
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
