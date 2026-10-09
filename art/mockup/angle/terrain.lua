-- Draws the ground at the 2.5D angle of #200, where a cell is 16 pixels wide and 12 deep:
--   terrain.aseprite: 16x12 tiles tagged plain (four), hills, forest, road, paving (two each),
--     and one per field state the core gives, sown, growing, ripe, fallow and burnt.
--   faces.aseprite: 16x8 tiles tagged hills_face (two), the step where hills, raised 8 pixels,
--     drop to the plain along their front edge.
--   trees.aseprite: 32x40 frames tagged tree (three) and pine (two), each trunk's foot at
--     (16, 36), the point the tree is sorted by.
-- Ground tiles carry no outline, so tiles of one kind join into one surface.
-- Run: aseprite --batch --script art/mockup/angle/terrain.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../../pixel.lua")
local c = px.c

local W, D = 16, 12

-- Grain that repeats every tile, so tiles of one kind meet without a seam.
local function grain(x, y, seed, v)
  return 0.5 + (px.hash(x, y, seed + 7 * v) - 0.5) * 0.35
    + (px.tile_noise(x, y * 4 // 3, 8, W, seed + 100 * v) - 0.5) * 0.6
end

local function grass(cv, v, base, seed)
  local r = px.rng(seed + v)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local n = grain(x, y, seed, v)
      local col = c("grass", base)
      if n > 0.66 + px.bayer(x, y) * 0.1 then col = px.shift(c("grass", base), 1)
      elseif n < 0.3 + px.bayer(x, y) * 0.1 then col = px.shift(c("grass", base), -1) end
      cv:set(x, y, col)
    end
  end
  -- Tufts stand up from the ground: a light blade over a dark root.
  for _ = 1, 1 + v % 2 do
    local x, y = r.int(2, 13), r.int(2, 9)
    cv:set(x, y, px.shift(c("grass", base), 2)); cv:set(x - 1, y + 1, px.shift(c("grass", base), 1))
    cv:set(x + 1, y + 1, px.shift(c("grass", base), 1)); cv:set(x, y + 1, px.shift(c("grass", base), -1))
  end
  if v == 4 then cv:set(5, 8, c("paper", 3)); cv:set(11, 3, c("amber", 5)) end
end

local function forest_floor(cv, v)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local n = grain(x, y, 30, v)
      cv:set(x, y, n > 0.62 and c("grass", 2) or (n > 0.36 and c("leaf", 2) or c("leaf", 1)))
    end
  end
  cv:set(4 + v * 5, 7, c("grass", 3)); cv:set(11 - v * 3, 3, c("leaf", 3))
end

-- Road: packed earth with two wheel ruts running along it, east to west.
local function road(cv, v)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local n = px.tile_noise(x, y, 4, W, 50 + v) + (px.hash(x, y, 51 + v) - 0.5) * 0.3
      local col = c("earth", 4)
      if n > 0.72 then col = c("earth", 5) elseif n < 0.28 then col = c("earth", 3) end
      if (y == 3 or y == 8) and px.hash(x, y, 52 + v) > 0.2 then col = c("earth", 3) end
      cv:set(x, y, col)
    end
  end
end

-- Paving: slabs 8 wide and 6 deep in offset courses, a light slab here and there.
local function paving(cv, v)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local course = y // 6
      local sx = (x + (course % 2) * 4) % 8
      local col = c("stone", 4)
      if sx == 0 or y % 6 == 0 then col = c("stone", 3) end
      local slab = px.hash((x + (course % 2) * 4) // 8, course, 40 + v)
      if sx ~= 0 and y % 6 ~= 0 and slab > 0.7 then col = c("stone", 5) end
      cv:set(x, y, col)
    end
  end
end

-- A field: rows of tilled soil three pixels apart, foreshortened from the cells' four.
local function field(cv, state)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local row = y % 3
      local col
      if state == "sown" then
        col = row == 0 and c("earth", 2) or c("earth", 3)
        if row == 1 and px.hash(x, y, 60) > 0.7 then col = c("earth", 5) end
      elseif state == "growing" then
        col = row == 0 and c("earth", 2) or (row == 1 and c("grass", 4) or c("grass", 5))
        if row == 2 and px.hash(x, y, 61) > 0.6 then col = c("grass", 6) end
      elseif state == "ripe" then
        col = row == 0 and c("amber", 2) or (row == 1 and c("amber", 3) or c("amber", 4))
        if row == 2 and px.hash(x, y, 62) > 0.55 then col = c("amber", 5) end
      elseif state == "fallow" then
        col = row == 0 and c("earth", 3) or c("grass", 3)
        if px.hash(x, y, 63) > 0.75 then col = c("grass", 4) end
      else
        col = row == 0 and c("ink", 2) or c("ink", 3)
        if px.hash(x, y, 64) > 0.9 then col = c("fire", 2) end
      end
      cv:set(x, y, col)
    end
  end
end

local frames, tags = {}, {}
local function add(name, count, draw)
  local first = #frames + 1
  for v = 1, count do
    local cv = px.canvas(W, D)
    draw(cv, v)
    frames[#frames + 1] = cv
  end
  tags[#tags + 1] = { name, first, #frames, 100 }
end
add("plain", 4, function(cv, v) grass(cv, v, 4, 10) end)
add("hills", 2, function(cv, v) grass(cv, v, 5, 20) end)
add("forest", 2, forest_floor)
add("road", 2, road)
add("paving", 2, paving)
for _, state in ipairs({ "sown", "growing", "ripe", "fallow", "burnt" }) do
  add(state, 1, function(cv) field(cv, state) end)
end
px.save_frames(W, D, frames, tags, here .. "/terrain.aseprite")

-- The hills' front step: a grass lip, then earth and stone in strata, darker toward the foot.
local faces = {}
for v = 1, 2 do
  local cv = px.canvas(W, 8)
  for y = 0, 7 do
    for x = 0, W - 1 do
      local col = y < 2 and c("grass", 3) or c("earth", 4 - (y - 2) // 3)
      if y >= 2 and px.hash(x, y, 70 + v) > 0.8 then col = c("stone", 3) end
      if y == 1 and px.hash(x, 0, 72 + v) > 0.5 then col = c("grass", 2) end
      cv:set(x, y, col)
    end
  end
  faces[v] = cv
end
px.save_frames(W, 8, faces, { { "hills_face", 1, 2, 100 } }, here .. "/faces.aseprite")

-- Trees standing at the angle: a trunk on its foot, a crown of leaf lit from the upper left
-- with a dark rim; pines a stack of tiers.
local trees = {}
local function broadleaf(seed, rad)
  local cv = px.canvas(32, 40)
  cv:rect(15, 26, 3, 11, c("wood", 2)); cv:vline(15, 26, 11, c("wood", 3))
  cv:set(14, 36, c("wood", 2)); cv:set(18, 36, c("wood", 2))
  local cx, cy = 16, 26 - rad * 0.75
  for _, blob in ipairs({ { -rad * 0.45, 2, 0.7 }, { rad * 0.45, 3, 0.7 }, { 0, 0, 1 } }) do
    local bx, by, s = cx + blob[1], cy + blob[2], blob[3]
    cv:each_ellipse(bx, by, rad * s, rad * s * 0.85, function(x, y, d)
      local n = px.noise(x, y, 3, seed)
      local light = -(x - cx) / rad * 0.5 - (y - cy) / rad * 0.7 + (n - 0.5) * 0.9
      local col = c("leaf", 3)
      if light > 0.55 then col = c("leaf", 5) elseif light > 0.15 then col = c("leaf", 4)
      elseif light < -0.45 then col = c("leaf", 2) end
      if d > 0.9 and n < 0.4 then return end
      cv:set(x, y, col)
    end)
  end
  cv:outline(0, 0, 32, 40, c("leaf", 1))
  return cv
end
local function pine(seed)
  local cv = px.canvas(32, 40)
  cv:rect(15, 30, 2, 7, c("wood", 2))
  for tier = 0, 3 do
    local top, h = 6 + tier * 6, 9
    for y = 0, h - 1 do
      local half = math.floor((y + 2) * (5 + tier) / h + 0.5)
      for x = -half, half do
        local col = x < -half / 3 and c("leaf", 4) or (x > half / 2 and c("leaf", 2) or c("leaf", 3))
        if y == h - 1 and px.hash(x, tier, seed) > 0.5 then col = c("leaf", 2) end
        cv:set(16 + x, top + y, col)
      end
    end
  end
  cv:set(16, 5, c("leaf", 4))
  cv:outline(0, 0, 32, 40, c("leaf", 1))
  return cv
end
trees[1] = broadleaf(80, 11)
trees[2] = broadleaf(81, 9)
trees[3] = broadleaf(82, 12.5)
trees[4] = pine(83)
trees[5] = pine(84)
px.save_frames(32, 40, trees, { { "tree", 1, 3, 100 }, { "pine", 4, 5, 100 } }, here .. "/trees.aseprite")
