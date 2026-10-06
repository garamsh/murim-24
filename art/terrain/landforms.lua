-- Draws landforms.aseprite: features larger than one cell that stand on a stretch of one
-- terrain kind, so that a large area reads as a landform and not as one feature per cell.
-- Mountain massifs and hill clusters, one 64x64 frame each, tagged <kind>_<width>x<height>
-- by their footprint in cells. The footprint sits at the frame's bottom-left corner; the
-- frame above and right of it holds what rises over it in the three-quarter view.
-- Run: aseprite --batch --script art/terrain/landforms.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c

local F = 64

-- A massif over a w x h footprint: one silhouette whose ridge is the highest of its peaks at
-- each column, each peak {x, summit row, slope} falling away at slope rows per column. Faces
-- left of the peak that rules a column are lit, faces right of it shaded; summits are pale,
-- gullies crease the shaded faces, and the foot darkens into the ground.
local function massif(w, h, peaks, seed)
  local cv = px.canvas(F, F)
  local fw = w * 16
  local foot = F - 1
  local function ridge(x)
    local best, by = nil, math.huge
    for _, p in ipairs(peaks) do
      local y = p[2] + math.abs(x - p[1]) * p[3]
      if y < by then by, best = y, p end
    end
    by = math.floor(by + (px.noise(x, 0, 2, seed) - 0.5) * 3)
    return by, best
  end
  for x = 1, fw - 2 do
    local top, p = ridge(x)
    for y = top, foot do
      local depth = y - top
      local n = px.noise(x, y, 2, seed + 1)
      local col
      if x < p[1] then
        col = depth < 6 and c("stone", 5) or c("stone", 4)
        if n > 0.8 then col = c("stone", 3) end
      elseif x == p[1] then
        col = c("stone", 4)
      else
        col = depth < 4 and c("stone", 3) or c("stone", 2)
        if n > 0.82 then col = c("stone", 1) end
      end
      if depth < 2 and top - p[2] < 3 then col = c("stone", 6) end
      if foot - y < 3 and px.bayer(x, y) < (3 - (foot - y)) / 4 then col = c("stone", 2) end
      cv:set(x, y, col)
    end
  end
  -- Gullies down the shaded faces.
  for n, p in ipairs(peaks) do
    local r = px.rng(seed + n)
    local gx = p[1] + 2 + r.int(0, 2)
    local gy = p[2] + 4
    while gy < foot - 3 and cv:get(gx, gy) ~= 0 do
      cv:set(gx, gy, c("stone", 1))
      gy = gy + 1
      if r.next() < 0.4 then gx = gx + 1 end
    end
  end
  cv:outline(0, 0, F, F, c("stone", 1))
  return cv
end

-- A cluster of low mounds over a w x h footprint, drawn back to front: each casts a shadow
-- to the lower right, is lit on its upper-left slope and shaded on its lower-right one, and
-- carries a contour along its foot.
local function hill_cluster(w, h, mounds, seed)
  local cv = px.canvas(F, F)
  table.sort(mounds, function(a, b) return a[2] + a[4] < b[2] + b[4] end)
  for n, m in ipairs(mounds) do
    local cx, cy, rx, ry = m[1], m[2], m[3], m[4]
    cv:each_ellipse(cx + 2, cy + 2, rx, ry, function(x, y, d)
      if cv:get(x, y) == 0 or d > 0.6 then cv:set(x, y, c("grass", 3)) end
    end)
    cv:each_ellipse(cx, cy, rx, ry, function(x, y, d)
      local dx, dy = (x - cx) / rx, (y - cy) / ry
      local nn = px.noise(x, y, 3, seed + n)
      local light = -dx * 0.6 - dy * 0.8 + (nn - 0.5) * 0.3
      local col = c("grass", 5)
      if light > 0.5 then col = c("grass", 6) elseif light < -0.55 then col = c("grass", 3)
      elseif light < -0.1 then col = c("grass", 4) end
      if d > 0.86 and dx + dy > 0.1 then col = c("grass", 2) end
      cv:set(x, y, col)
    end)
  end
  return cv
end

local forms = {
  { "mountain_2x2", function()
    return massif(2, 2, { { 12, 22, 3.0 }, { 22, 30, 2.6 } }, 11)
  end },
  { "mountain_3x2", function()
    return massif(3, 2, { { 11, 26, 2.8 }, { 25, 14, 2.6 }, { 38, 26, 2.8 } }, 23)
  end },
  { "mountain_3x3", function()
    return massif(3, 3, { { 10, 16, 2.6 }, { 24, 4, 2.4 }, { 37, 14, 2.6 } }, 37)
  end },
  { "mountain_4x3", function()
    return massif(4, 3, { { 10, 18, 2.6 }, { 23, 8, 2.4 }, { 39, 2, 2.4 }, { 53, 16, 2.6 } }, 53)
  end },
  { "hills_2x1", function()
    return hill_cluster(2, 1, { { 11, 55, 9, 6 }, { 22, 57, 8, 5 } }, 61)
  end },
  { "hills_2x2", function()
    return hill_cluster(2, 2, { { 12, 42, 10, 8 }, { 21, 49, 9, 7 }, { 10, 56, 8, 5 } }, 67)
  end },
  { "hills_3x2", function()
    return hill_cluster(3, 2, { { 13, 41, 11, 8 }, { 31, 44, 13, 9 }, { 21, 54, 10, 6 }, { 40, 56, 6, 5 } }, 71)
  end },
  { "hills_3x3", function()
    return hill_cluster(3, 3, { { 22, 28, 14, 10 }, { 10, 42, 9, 7 }, { 36, 44, 10, 8 }, { 20, 54, 12, 7 } }, 79)
  end },
}

local frames, tags = {}, {}
for n, f in ipairs(forms) do
  frames[n] = f[2]()
  tags[n] = { f[1], n, n, 100 }
end
px.save_frames(F, F, frames, tags, here .. "/landforms.aseprite")
