-- Draws trees.aseprite: the decorative trees the view scatters over the ground at the 2.5D angle
-- (ADR 0059), standing at their full height: 32x40 frames tagged tree (three broadleaf), pine,
-- bamboo, willow, poplar and snow_pine (two each), each foot at (16, 36), the point the view sorts
-- it by. A crown is lit from the upper left with a dark rim; a pine is a stack of tiers.
-- Run: aseprite --batch --script art/terrain/trees.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c

-- A broadleaf: a trunk on its foot under a crown of leaf lit from the upper left
-- with a dark rim; a pine: tiers stacked on a short trunk.
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
  cv:selout(0, 0, 32, 40)
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
  cv:selout(0, 0, 32, 40)
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
  cv:selout(0, 0, 32, 40)
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
  cv:selout(0, 0, 32, 40)
  return cv
end
-- Poplar: the oases' tree, a tall narrow crown of yellow-green on a pale trunk.
local function poplar(seed)
  local cv = px.canvas(32, 40)
  cv:rect(15, 28, 2, 9, c("paper", 3)); cv:vline(15, 28, 9, c("paper", 4))
  cv:each_ellipse(16, 16, 5, 14, function(x, y)
    local n = px.noise(x, y, 2, seed)
    local light = -(x - 16) / 5 * 0.6 - (y - 16) / 14 * 0.4 + (n - 0.5) * 0.8
    cv:set(x, y, light > 0.4 and c("grass", 6) or (light > -0.1 and c("grass", 5) or (light > -0.5 and c("grass", 4) or c("leaf", 3))))
  end)
  cv:selout(0, 0, 32, 40)
  return cv
end
-- A pine high on a snowy range, snow lying on the upper side of each tier.
local function snow_pine(seed)
  local cv = pine(seed)
  for y = 0, 39 do
    for x = 0, 31 do
      local i = cv:get(x, y)
      if i ~= 0 and i ~= c("leaf", 1) and cv:get(x, y - 1) == c("leaf", 1) and px.hash(x, y, seed) > 0.25 then
        cv:set(x, y, x < 16 and c("robe", 7) or c("robe", 4))
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
