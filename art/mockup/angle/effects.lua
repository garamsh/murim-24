-- Draws the effects of a high-realm skill that levels a village, at the 2.5D angle of #200:
--   qi.aseprite: 16x48 frames tagged fall (four), a blade of sword qi falling point first under a
--     short guard, a tail of azure light behind it, its point on (8, 44).
--   pillar.aseprite: 48x192 frames tagged rise (six), a column of white-hot qi rising from the
--     ground in azure light, its foot on (24, 186).
--   dust.aseprite: 48x32 frames tagged puff (five), grey dust lit from the upper left, swelling
--     and thinning by dithering, its foot on (24, 28).
--   debris.aseprite: 8x8 frames tagged shard (four), a tile shard turning, centred.
-- Nothing is blended: light and dust thin by dithering.
-- Run: aseprite --batch --script art/mockup/angle/effects.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c

local function save(name, w, h, frames, tag, ms)
  px.save_frames(w, h, frames, { { tag, 1, #frames, ms } }, here .. "/" .. name .. ".aseprite")
end

-- Sword qi: the tail lengthens and flickers from frame to frame; the point is a crescent.
local qi = {}
for k = 0, 3 do
  local cv = px.canvas(16, 48)
  local tail = 10 + k * 6
  for y = 28 - tail, 28 do
    local t = (y - (28 - tail)) / tail
    local half = 1
    for x = 8 - half, 8 + half - 1 do
      local col = t > 0.55 and c("azure", 4) or c("azure", 3)
      if x == 8 or (half == 2 and x == 7) then col = t > 0.7 and c("robe", 3) or c("azure", 5) end
      if t < 0.35 and px.bayer(x, y + k) > t * 2.4 then col = nil end
      if col then cv:set(x, y, col) end
    end
  end
  -- The blade: three pixels wide, tapering to its point, white along its edge.
  for y = 30, 44 do
    local half = y > 40 and (44 - y) // 2 or 1
    for x = 8 - half, 8 + half do cv:set(x, y, x == 8 and c("robe", 3) or c("azure", 5)) end
  end
  cv:hline(5, 30, 7, c("azure", 4)); cv:hline(6, 29, 5, c("azure", 5))
  qi[#qi + 1] = cv
end
save("qi", 16, 48, qi, "fall", 60)

-- The pillar: rises over six frames, its core white-hot, its edges azure and dithered away.
local pillar = {}
for k = 1, 6 do
  local cv = px.canvas(48, 192)
  local height = math.floor(186 * k / 6)
  local top = 186 - height
  for y = top, 186 do
    local fade = (y - top) / math.max(1, height)
    for x = 6, 41 do
      local d = math.abs(x - 23.5) / 18
      local col
      if d < 0.18 then col = c("fire", 5)
      elseif d < 0.36 then col = c("robe", 3)
      elseif d < 0.62 then col = c("azure", 5)
      elseif d < 0.85 then col = c("azure", 4)
      else col = c("azure", 3) end
      local thin = d > 0.6 and (d - 0.6) / 0.4 or 0
      if fade < 0.12 then thin = math.max(thin, 1 - fade / 0.12) end
      if px.bayer(x, y + k) >= thin then cv:set(x, y, col) end
    end
  end
  cv:each_ellipse(24, 186, 22, 5, function(x, y, d)
    if px.bayer(x, y) > d * 0.9 then cv:set(x, y, d < 0.5 and c("robe", 3) or c("azure", 4)) end
  end)
  pillar[k] = cv
end
save("pillar", 48, 192, pillar, "rise", 80)

-- Dust: puffs swell and rise, lit on the upper left, thinning toward the last frame.
local dust = {}
for k = 1, 5 do
  local cv = px.canvas(48, 32)
  local grow = 0.5 + k * 0.12
  local thin = (k - 1) / 5
  for _, p in ipairs({ { 14, 22, 9 }, { 26, 18, 11 }, { 36, 23, 8 }, { 22, 25, 7 } }) do
    local cx, cy, r = p[1], p[2] - k, p[3] * grow
    cv:each_ellipse(cx, cy, r, r * 0.75, function(x, y, d)
      if px.bayer(x, y) < thin + d * 0.4 then return end
      local lit = (x - cx) + (y - cy) < -r * 0.3
      cv:set(x, y, lit and c("stone", 5) or (d > 0.7 and c("stone", 3) or c("stone", 4)))
    end)
  end
  dust[k] = cv
end
save("dust", 48, 32, dust, "puff", 120)

-- Debris: a shard of roof tile turning a quarter at a time.
local shards = {
  { "..##....", ".####...", "######..", ".#####..", "..###...", "........", "........", "........" },
  { "........", "...##...", "..####..", "..####..", "..####..", "...##...", "........", "........" },
  { "........", "....##..", "...####.", "..######", "...#####", "....###.", "........", "........" },
  { "........", "........", ".######.", "########", ".######.", "........", "........", "........" },
}
local debris = {}
for k, rows in ipairs(shards) do
  local cv = px.canvas(8, 8)
  cv:sprite(0, 0, rows, { ["#"] = c("stone", 3) })
  for y = 0, 7 do for x = 0, 7 do
    if cv:get(x, y) ~= 0 and (cv:get(x, y - 1) == 0) then cv:set(x, y, c("stone", 5)) end
    if cv:get(x, y) ~= 0 and (cv:get(x, y + 1) == 0) then cv:set(x, y, c("stone", 2)) end
  end end
  cv:outline(0, 0, 8, 8, c("ink", 1))
  debris[k] = cv
end
save("debris", 8, 8, debris, "shard", 80)
