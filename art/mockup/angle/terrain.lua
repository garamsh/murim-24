-- Draws the ground at the 2.5D angle of #200, where a cell is 16 pixels wide and 12 deep:
--   terrain.aseprite: 16x12 tiles tagged plain (four), hills, forest, road, paving (two each),
--     one per field state the core gives, sown, growing, ripe, fallow and burnt, and for the
--     revised valley's water, rock, paddy, lane, garden and steps (two each).
--   faces.aseprite: 16x8 tiles, two each, tagged hills_face, the step where hills, raised 8
--     pixels, drop to the plain along their front edge, and stone_face, rock_face, bank_face and
--     steps_face, tiled downward wherever the valley's ground drops to its south.
--   trees.aseprite: 32x40 frames tagged tree (three), pine, bamboo, willow, poplar and snow_pine (two each),
--     each foot at (16, 36), the point the tree is sorted by.
--   shade.json: each palette colour with the colours one and two steps darker on its ramp.
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
-- The revised valley's ground (PR #204's review), two variants each, after the first round's
-- tags so those keep their frames.
-- Water: clear blue with ripples.
local function waters(cv, v, ramp, seed)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local n = px.tile_noise(x, y * 4 // 3, 4, W, seed + v)
      local col = ramp[2]
      if n > 0.62 then col = ramp[3] elseif n < 0.3 then col = ramp[1] end
      if (y + v * 3) % 6 == 0 and (x + y * 3) % 8 < 3 then col = ramp[4] end
      cv:set(x, y, col)
    end
  end
end
add("water", 2, function(cv, v) waters(cv, v, { c("water", 2), c("water", 3), c("water", 4), c("water", 5) }, 100) end)
-- Rock: fractured stone.
add("rock", 2, function(cv, v)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local n = px.tile_noise(x, y * 4 // 3, 4, W, 150 + v)
      -- Broad patches of two greys, a darker crack here and there: calm at 3x.
      local col = n > 0.5 and c("stone", 4) or c("stone", 3)
      if px.hash(x, y, 151 + v) > 0.96 then col = c("stone", 2) end
      cv:set(x, y, col)
    end
  end
end)
-- Paddy: water between rows of young rice.
add("paddy", 2, function(cv, v)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local col = ((x + y % 2) % 4 == 0) and c("grass", 5) or c("water", 4)
      if (x + y % 2) % 4 == 1 and y % 3 == 0 then col = c("grass", 4) end
      if px.hash(x, y, 160 + v) > 0.93 then col = c("water", 6) end
      cv:set(x, y, col)
    end
  end
end)
-- A lane of packed earth, lighter where feet wear it; a kitchen garden of vegetable rows; and
-- stone steps, where a path climbs from one level to the next.
add("lane", 2, function(cv, v)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local n = px.tile_noise(x, y * 4 // 3, 4, W, 230 + v) + (px.hash(x, y, 231 + v) - 0.5) * 0.3
      cv:set(x, y, n > 0.68 and c("earth", 6) or (n > 0.3 and c("earth", 5) or c("earth", 4)))
    end
  end
end)
add("garden", 2, function(cv, v)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local row = y % 4
      local col = row < 2 and c("earth", 3) or (((x + v) % 3 == 0) and c("grass", 6) or c("leaf", 4))
      if row == 3 and px.hash(x, y, 240 + v) > 0.8 then col = c("crimson", 4) end
      cv:set(x, y, col)
    end
  end
end)
add("steps", 2, function(cv, v)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local tread = y % 4
      local col = tread == 0 and c("stone", 5) or (tread == 3 and c("stone", 2) or c("stone", 4))
      if x == 0 or x == W - 1 then col = c("stone", 2) end
      cv:set(x, y, col)
    end
  end
end)
px.save_frames(W, D, frames, tags, here .. "/terrain.aseprite")

-- The palette darkened along its ramps, one step and two, for shadows baked into the ground:
-- shade.json maps each colour, as rrggbb, to [one step darker, two steps darker].
do
  local f = io.open(here .. "/shade.json", "w")
  local parts = {}
  for i, hex in ipairs(px.colours) do
    parts[#parts + 1] = string.format('"%s": ["%s", "%s"]', hex, px.colours[px.shift(i, -1)], px.colours[px.shift(i, -2)])
  end
  f:write("{\n  " .. table.concat(parts, ",\n  ") .. "\n}\n")
  f:close()
end

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
-- Faces for the levels, each tiling downward: masonry for terraces' walls, rock for cliffs,
-- earth under a grass lip for banks, and stone steps where a stair climbs.
local face_tags = { { "hills_face", 1, 2, 100 } }
local function face(name, draw)
  local first = #faces + 1
  for v = 1, 2 do
    local cv = px.canvas(W, 8)
    for y = 0, 7 do for x = 0, W - 1 do cv:set(x, y, draw(x, y, v)) end end
    faces[#faces + 1] = cv
  end
  face_tags[#face_tags + 1] = { name, first, #faces, 100 }
end
face("stone_face", function(x, y, v)
  local course = y // 4
  local joint = (x + course * 4 + v) % 8 == 0 or y % 4 == 3
  return joint and c("stone", 2) or (y % 4 == 0 and c("stone", 4) or c("stone", 3))
end)
face("rock_face", function(x, y, v)
  local n = px.tile_noise(x, y, 4, W, 190 + v)
  return n > 0.6 and c("stone", 3) or (n > 0.3 and c("stone", 2) or c("ink", 3))
end)
face("bank_face", function(x, y, v)
  if y == 0 then return c("grass", 4) end
  return (px.hash(x, y, 210 + v) > 0.8) and c("earth", 2) or c("earth", 3)
end)
face("steps_face", function(x, y, v)
  if x == 0 or x == W - 1 then return c("stone", 2) end
  local tread = y % 4
  return tread == 0 and c("stone", 5) or (tread == 1 and c("stone", 4) or c("stone", 3))
end)
px.save_frames(W, 8, faces, face_tags, here .. "/faces.aseprite")

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

-- Bamboo: a clump of jointed jade stems with sprays of leaf.
local function bamboo(seed)
  local cv = px.canvas(32, 40)
  local r = px.rng(seed)
  for k = 0, 4 do
    local sx = 6 + k * 5 + r.int(0, 1)
    local top = r.int(1, 10)
    cv:vline(sx, top, 37 - top, c("jade", 2))
    for y = top + 3, 34, 5 do cv:set(sx, y, c("leaf", 2)) end
    for j = 1, 3 do
      local ly = top + r.int(0, 16)
      local dir = (j % 2 == 0) and -1 or 1
      for t = 1, 4 do cv:set(sx + dir * t, ly + (t + 1) // 2, (t < 3) and c("grass", 5) or c("leaf", 4)) end
    end
  end
  cv:outline(0, 0, 32, 40, c("leaf", 1))
  return cv
end
-- Willow: a short trunk and long hanging strands of pale green.
local function willow(seed)
  local cv = px.canvas(32, 40)
  cv:rect(15, 22, 3, 15, c("wood", 2)); cv:vline(15, 22, 15, c("wood", 3))
  cv:each_ellipse(16, 10, 8, 5, function(x, y)
    local n = px.noise(x, y, 3, seed)
    cv:set(x, y, n > 0.55 and c("grass", 6) or (n > 0.3 and c("grass", 5) or c("grass", 4)))
  end)
  for x = 4, 28, 3 do
    local start = 10 + math.floor(math.abs(x - 16) / 3)
    local len = 12 + math.floor(px.hash(x, 0, seed) * 12)
    for y = start, math.min(start + len, 34) do
      cv:set(x + ((y // 4) % 2), y, (y % 3 == 0) and c("grass", 6) or c("grass", 4))
    end
  end
  cv:outline(0, 0, 32, 40, c("leaf", 1))
  return cv
end
-- Poplar: the oases' tree, a tall narrow crown of yellow-green on a pale trunk.
local function poplar(seed)
  local cv = px.canvas(32, 40)
  cv:rect(15, 28, 2, 9, c("paper", 1)); cv:vline(15, 28, 9, c("paper", 2))
  cv:each_ellipse(16, 16, 5, 14, function(x, y)
    local n = px.noise(x, y, 2, seed)
    local light = -(x - 16) / 5 * 0.6 - (y - 16) / 14 * 0.4 + (n - 0.5) * 0.8
    cv:set(x, y, light > 0.4 and c("grass", 6) or (light > -0.1 and c("grass", 5) or (light > -0.5 and c("grass", 4) or c("leaf", 3))))
  end)
  cv:outline(0, 0, 32, 40, c("leaf", 1))
  return cv
end
-- A pine high on a snowy range, snow lying on the upper side of each tier.
local function snow_pine(seed)
  local cv = pine(seed)
  for y = 0, 39 do
    for x = 0, 31 do
      local i = cv:get(x, y)
      if i ~= 0 and i ~= c("leaf", 1) and cv:get(x, y - 1) == c("leaf", 1) and px.hash(x, y, seed) > 0.25 then
        cv:set(x, y, x < 16 and c("robe", 3) or c("robe", 2))
      end
    end
  end
  return cv
end
trees[6] = bamboo(85)
trees[7] = bamboo(86)
trees[8] = willow(87)
trees[9] = willow(88)
trees[10] = poplar(89)
trees[11] = poplar(90)
trees[12] = snow_pine(91)
trees[13] = snow_pine(92)
px.save_frames(32, 40, trees, { { "tree", 1, 3, 100 }, { "pine", 4, 5, 100 }, { "bamboo", 6, 7, 100 },
  { "willow", 8, 9, 100 }, { "poplar", 10, 11, 100 }, { "snow_pine", 12, 13, 100 } }, here .. "/trees.aseprite")
