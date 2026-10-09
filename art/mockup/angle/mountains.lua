-- Draws the peaks that cap the tiered mountains of the revised village (PR #204's review), at
-- the 2.5D angle of #200: mountains.aseprite, 128x128 frames tagged peak (three) and snow (three,
-- for the Western Regions' Tianshan, snow far down their flanks and no green), a crag of
-- stone in strata rising from a ragged foot, lit from the upper left, scree spilling from its
-- foot and pines along it, the foot's centre on (64, 120).
-- Run: aseprite --batch --script art/mockup/angle/mountains.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../../pixel.lua")
local c = px.c

local W, H = 128, 128
local FOOT = 120

local function peak(seed, height, width, lean, snow)
  local cv = px.canvas(W, H)
  local cx = 64
  -- A ridge line from the summit down both flanks, roughened by noise.
  for x = cx - width, cx + width do
    local t = math.abs(x - cx - lean * (1 - math.abs(x - cx) / width)) / width
    -- The foot is ragged, rising and falling a few pixels, never a straight cut.
    local bottom = FOOT - math.floor(px.noise(x, 0, 5, seed + 4) * 6)
    local top = bottom - math.floor(height * (1 - t ^ 1.3) * (0.85 + px.noise(x, 0, 6, seed) * 0.3))
    for y = top, bottom do
      local depth = (FOOT - y) / height
      -- The lit flank is left of a ridge that wanders down from the summit and spreads.
      local ridge = cx + lean + (FOOT - height - y) * 0 + (px.noise(0, y, 5, seed + 2) - 0.5) * 14 + (1 - depth) * 6
      local side = x < ridge
      local strata = px.noise(x * 0.35, y * 1.4, 3, seed + 1)
      local col
      if side then
        col = strata > 0.62 and c("stone", 5) or (strata > 0.3 and c("stone", 4) or c("stone", 3))
      else
        col = strata > 0.62 and c("stone", 3) or (strata > 0.3 and c("stone", 2) or c("ink", 3))
      end
      if math.abs(x - ridge) < 1 then col = side and c("stone", 6) or c("stone", 2) end
      -- Green clings to the lower slopes.
      local green = px.noise(x, y, 4, seed + 3)
      if type(snow) == "number" then green = 0 end
      if depth < 0.45 and green > 0.45 + depth then col = side and c("leaf", 4) or c("leaf", 2) end
      if snow == true and y - top < 6 + px.noise(x, 3, 3, seed) * 6 then col = side and c("robe", 3) or c("stone", 5) end
      -- A range under snow: the snow reaches down its flanks in tongues along the gullies.
      if type(snow) == "number" and depth > snow - px.noise(x * 0.5, 0, 3, seed + 5) * 0.3 then
        col = side and (strata > 0.4 and c("robe", 3) or c("robe", 2)) or (strata > 0.5 and c("stone", 5) or c("stone", 4))
      end
      cv:set(x, y, col)
    end
  end
  -- Crevices running down from the ridge.
  for k = 1, 6 do
    local x = cx - width + math.floor(px.hash(k, 1, seed) * width * 2)
    local y = FOOT - math.floor(height * 0.8 * px.hash(k, 2, seed))
    for t = 0, 14 do
      if cv:get(x, y + t) ~= 0 then cv:set(x + (t // 5), y + t, c("ink", 2)) end
    end
  end
  -- Scree spilling from the foot, thinning by dithering as it spreads.
  for x = cx - width - 4, cx + width + 4 do
    for y = FOOT - 8, FOOT + 5 do
      if cv:get(x, y) == 0 and (cv:get(x, y - 1) ~= 0 or cv:get(x, y - 2) ~= 0 or cv:get(x, y - 3) ~= 0)
          and px.bayer(x, y) > 0.35 then
        cv:set(x, y, px.hash(x, y, seed) > 0.5 and c("stone", 3) or c("stone", 2))
      end
    end
  end
  -- Pines along the foot, straddling its edge.
  for k = 1, 16 do
    local x = cx - width + 2 + math.floor(px.hash(k, 3, seed) * (width * 2 - 4))
    local y = FOOT + 3 - math.floor(px.hash(k, 4, seed) * 12)
    for row = 0, 7 do
      local half = (row + 1) // 2
      cv:hline(x - half, y - 8 + row, half * 2 + 1, row < 3 and c("leaf", 4) or c("leaf", 3))
    end
    cv:set(x, y, c("wood", 2))
  end
  cv:outline(0, 0, W, H, c("ink", 1))
  return cv
end

px.save_frames(W, H, { peak(301, 104, 54, -6, true), peak(302, 82, 60, 8, false), peak(303, 64, 46, 0, false),
  peak(304, 112, 58, -4, 0.42), peak(305, 92, 62, 6, 0.38), peak(306, 76, 48, 0, 0.45) },
  { { "peak", 1, 3, 100 }, { "snow", 4, 6, 100 } }, here .. "/mountains.aseprite")
