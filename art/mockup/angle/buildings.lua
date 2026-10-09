-- Draws the revised village set at the 2.5D angle of #200 (PM review on PR #204): taller fronts,
-- steep roofs with hips and eaves, buildings of several kinds and sizes, walls, and the things
-- a village is lived in with. One frame each, tagged intact, with its footprint's bottom-left
-- corner at (8, height - 8) and what rises above in the rows above, as structures.lua's.
-- Shadows are not in the frames; the mockup bakes them into the ground from each silhouette.
--   tiled_house 3x2, tiled_house_big 4x3, ancestral_hall 5x3: plaster and timber under slate.
--   thatched_house 3x2, thatched_small 2x2: mud walls under rounded straw.
--   inn 4x3: two storeys, an open front hung with lanterns and a balcony above.
--   shrine 2x2, pavilion 2x2, guardian 1x1, village_gate 3x1, watchtower 2x2.
--   stall_red, stall_blue, stall_amber 2x1: an awning over a counter of goods.
--   stilt_house 3x2 on piles over water, pier 1x1, boat 2x1.
--   wall_h 1x1, a run of town wall facing south; wall_v 1x1, its cap where it runs north;
--   town_gate 4x2, the market town's gate under a tower; stone_wall 1x1, brush_fence 1x1.
--   well_square 2x2, crocks 1x1, laundry 2x1, cart 2x1, haystack 1x1, firewood 1x1, lantern 1x1,
--   bush 1x1, boulder 1x1.
-- Run: aseprite --batch --script art/mockup/angle/buildings.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../../pixel.lua")
local c = px.c
local INK = c("ink", 1)
local CW, CD = 16, 12

local function frame(w, d, rise)
  local W, H = w * CW + 16, d * CD + rise + 16
  return px.canvas(W, H), 8, H - 8
end

local function save(name, cv)
  cv:outline(0, 0, cv.w, cv.h, INK)
  px.save_frames(cv.w, cv.h, { cv }, { { "intact", 1, 1, 1000 } }, here .. "/" .. name .. ".aseprite")
end

-- A block standing on `base`: its top face `d` deep and its front `h` high, lit from the left.
local function block(cv, x, base, w, d, h, ramp, n)
  cv:rect(x, base - h - d, w, d, c(ramp, n + 1))
  cv:hline(x, base - h - d, w, c(ramp, n + 2))
  cv:rect(x, base - h, w, h, c(ramp, n))
  cv:hline(x, base - h, w, c(ramp, n + 1))
  cv:vline(x, base - h - d, d + h, c(ramp, n + 1))
  cv:vline(x + w - 1, base - h - d, d + h, c(ramp, n - 1))
end

-- A front wall from x for w, its foot on `foot`, h high: plaster (or mud) between a timber
-- sill, lintel and posts every `bay` pixels.
local function wall(cv, x, foot, w, h, opts)
  opts = opts or {}
  local fill = opts.mud and c("earth", 5) or c("paper", 2)
  local shade = opts.mud and c("earth", 4) or c("paper", 1)
  local timber = opts.red and c("lacquer", 2) or c("wood", 2)
  local top = foot - h
  cv:rect(x, top, w, h, fill)
  for y = top, foot - 1 do
    for xx = x, x + w - 1 do
      if px.hash(xx, y, 7) > 0.9 then cv:set(xx, y, shade) end
    end
  end
  cv:hline(x, foot - 1, w, timber); cv:hline(x, foot - 2, w, timber)
  cv:hline(x, top, w, timber); cv:hline(x, top + 1, w, c("wood", 1))
  local bay = opts.bay or 16
  for xx = x, x + w - 1, bay do cv:rect(xx, top, 2, h, timber); cv:vline(xx, top, h, opts.red and c("lacquer", 3) or c("wood", 3)) end
  cv:rect(x + w - 2, top, 2, h, timber)
  if opts.mid then cv:hline(x, top + h // 2, w, timber) end
end

local function window(cv, x, y, w, h)
  cv:rect(x - 1, y - 1, w + 2, h + 2, c("wood", 2))
  cv:rect(x, y, w, h, c("ink", 2))
  for k = 1, w - 1, 2 do cv:vline(x + k, y, h, c("wood", 4)) end
  for k = 2, h - 1, 3 do cv:hline(x, y + k, w, c("wood", 4)) end
end

local function door(cv, x, foot, w, h, dark)
  cv:rect(x - 1, foot - h - 1, w + 2, h + 1, c("wood", 2))
  cv:rect(x, foot - h, w, h, dark and c("ink", 1) or c("wood", 3))
  if not dark then
    cv:vline(x + w // 2, foot - h, h, c("wood", 1))
    cv:set(x + w // 2 - 2, foot - h // 2, c("gold", 3)); cv:set(x + w // 2 + 1, foot - h // 2, c("gold", 3))
  end
end

-- A tiled hip roof: the front slope from the eave up to the ridge, the back slope above it,
-- hips at both ends lit and shaded as they face, round tile ends and upturned corners.
local function tiled_roof(cv, x0, x1, eave, ridge, back, hip, tone)
  tone = tone or "stone"
  for yy = ridge, eave do
    local t = (yy - ridge) / math.max(1, eave - ridge)
    local inset = math.floor(hip * (1 - t) + 0.5)
    local flare = (eave - yy < 2) and (2 - (eave - yy)) or 0
    for xx = x0 - flare, x1 + flare do
      local ch = (xx - x0) % 3
      local col = ch == 0 and c(tone, 2) or (ch == 1 and c(tone, 4) or c(tone, 3))
      -- Courses every four rows; the slope lighter near the ridge, darker toward the eave.
      if (yy - ridge) % 4 == 3 then col = c(tone, 2) end
      if t < 0.3 and ch ~= 0 then col = px.shift(col, 1) elseif t > 0.75 then col = px.shift(col, -1) end
      if eave - yy < 2 then col = c(tone, 1) end
      if xx < x0 + inset then col = px.shift(col, 1) elseif xx > x1 - inset then col = px.shift(col, -2) end
      cv:set(xx, yy, col)
    end
  end
  for yy = back, ridge - 1 do
    local t = (ridge - yy) / math.max(1, ridge - back)
    local inset = math.floor(hip * (1 - t) + 0.5)
    for xx = x0 + 1 + inset, x1 - 1 - inset do
      cv:set(xx, yy, (xx - x0) % 3 == 0 and c(tone, 4) or c(tone, 5))
    end
  end
  local r0, r1 = x0 + hip, x1 - hip
  cv:hline(r0, ridge - 1, r1 - r0 + 1, c(tone, 2)); cv:hline(r0, ridge, r1 - r0 + 1, c("ink", 2))
  for _, s in ipairs({ { r0, -1 }, { r1, 1 } }) do
    cv:set(s[1] + s[2], ridge - 1, c("ink", 2)); cv:set(s[1] + s[2] * 2, ridge - 2, c("ink", 2))
    cv:set(s[1] + s[2] * 2, ridge - 3, c("ink", 2)); cv:set(s[1] + s[2] * 3, ridge - 4, c("gold", 3))
  end
  for xx = x0 - 2, x1 + 2 do cv:set(xx, eave, ((xx - x0) % 3 == 1) and c(tone, 5) or c(tone, 1)) end
  cv:set(x0 - 3, eave - 1, c(tone, 2)); cv:set(x0 - 4, eave - 2, c(tone, 2)); cv:set(x0 - 5, eave - 3, c(tone, 3))
  cv:set(x1 + 3, eave - 1, c(tone, 2)); cv:set(x1 + 4, eave - 2, c(tone, 2)); cv:set(x1 + 5, eave - 3, c(tone, 3))
end

-- A rounded straw roof, thick at the eave, lit on its upper left, with a dark ridge rope.
local function thatch(cv, x0, x1, eave, ridge, hip)
  for yy = ridge - 3, eave + 2 do
    local t = (yy - (ridge - 3)) / math.max(1, eave + 2 - (ridge - 3))
    local inset = math.floor(hip * (1 - math.sqrt(t)) + 0.5)
    for xx = x0 - 2 + inset, x1 + 2 - inset do
      local n = px.noise(xx, yy, 2, 31)
      local col = n > 0.62 and c("earth", 6) or (n > 0.32 and c("earth", 5) or c("earth", 4))
      if t < 0.35 then col = px.shift(col, 1) end
      if xx > x1 - inset - 4 then col = px.shift(col, -1) end
      if yy > eave then col = c("earth", 3) end
      cv:set(xx, yy, col)
    end
  end
  for xx = x0 + hip - 2, x1 - hip + 2 do cv:set(xx, ridge - 3, c("wood", 1)); cv:set(xx, ridge - 2, c("wood", 2)) end
  for xx = x0 - 2, x1 + 2 do if px.hash(xx, 1, 33) > 0.4 then cv:set(xx, eave + 3, c("earth", 3)) end end
end

-- A house: a plinth, a front `wall` high, a roof whose ridge stands `ridge` above the wall.
local function house(cv, x, base, w, d, wallh, ridge, o)
  o = o or {}
  local W, D = w * CW, d * CD
  block(cv, x, base, W, D, 4, "stone", 3)
  local foot = base - 4
  wall(cv, x + 3, foot, W - 6, wallh, { mud = o.thatch, bay = o.bay or 16, mid = o.mid })
  for _, wx in ipairs(o.windows or {}) do window(cv, x + wx, foot - wallh + 6, 7, math.min(10, wallh - 12)) end
  door(cv, x + (o.door or (W // 2 - 4)), foot, 8, math.min(16, wallh - 5))
  local eave = foot - wallh + 3
  local back = foot - D - wallh + 4
  local top = foot - D // 2 - wallh - ridge
  if o.thatch then
    thatch(cv, x + 1, x + W - 2, eave, top, o.hip or 8)
  else
    tiled_roof(cv, x, x + W - 1, eave, top, back, o.hip or 10, o.tone)
  end
  for xx = x + 3, x + W - 4 do cv:shade_at(xx, eave + 1, -1); cv:shade_at(xx, eave + 2, -1) end
  return eave
end

do local cv, x, b = frame(3, 2, 58); house(cv, x, b, 3, 2, 24, 18, { windows = { 6, 34 } }); save("tiled_house", cv) end
do local cv, x, b = frame(4, 3, 66); house(cv, x, b, 4, 3, 28, 22, { windows = { 7, 18, 39, 50 }, mid = true }); save("tiled_house_big", cv) end
do
  local cv, x, b = frame(5, 3, 76)
  local eave = house(cv, x, b, 5, 3, 32, 26, { windows = { 8, 20, 53, 65 }, mid = true, hip = 14 })
  cv:rect(x + 33, eave - 30, 14, 7, c("gold", 3)); cv:rect(x + 34, eave - 29, 12, 5, c("lacquer", 2))
  save("ancestral_hall", cv)
end
do local cv, x, b = frame(3, 2, 52); house(cv, x, b, 3, 2, 22, 16, { thatch = true, windows = { 6 }, door = 28, hip = 9 }); save("thatched_house", cv) end
do local cv, x, b = frame(2, 2, 48); house(cv, x, b, 2, 2, 20, 14, { thatch = true, door = 12, hip = 7 }); save("thatched_small", cv) end

-- The inn: an open ground floor with lanterns and tables, a balcony and an upper storey.
do
  local cv, x, base = frame(4, 3, 86)
  local W, D = 64, 36
  block(cv, x, base, W, D, 4, "stone", 3)
  local foot = base - 4
  cv:rect(x + 3, foot - 24, W - 6, 24, c("ink", 2))
  for _, p in ipairs({ 3, 20, 41, W - 5 }) do cv:rect(x + p, foot - 24, 2, 24, c("lacquer", 2)) end
  for k = 0, 2 do cv:rect(x + 8 + k * 18, foot - 7, 8, 3, c("wood", 4)); cv:rect(x + 9 + k * 18, foot - 4, 1, 4, c("wood", 2)) end
  for _, lx in ipairs({ 10, 30, 50 }) do cv:rect(x + lx, foot - 22, 3, 4, c("crimson", 4)); cv:set(x + lx + 1, foot - 21, c("fire", 4)) end
  local deck = foot - 24
  cv:rect(x, deck - 3, W, 3, c("wood", 3)); cv:hline(x, deck - 3, W, c("wood", 4))
  wall(cv, x + 6, deck - 3, W - 12, 18, { red = true, bay = 13 })
  for k = 0, 3 do window(cv, x + 12 + k * 12, deck - 17, 6, 8) end
  for xx = x + 1, x + W - 2, 3 do cv:vline(xx, deck - 9, 6, c("wood", 4)) end
  cv:hline(x, deck - 9, W, c("wood", 3))
  cv:rect(x + 22, foot - 34, 20, 7, c("ink", 2)); cv:rect(x + 23, foot - 33, 18, 5, c("gold", 3))
  tiled_roof(cv, x - 2, x + W + 1, deck - 3 - 18 + 3, foot - D // 2 - 24 - 3 - 18 - 18, foot - D - 45, 12)
  save("inn", cv)
end

-- The shrine: red pillars on a stepped base under a steep hip roof, an offering table.
do
  local cv, x, base = frame(2, 2, 52)
  block(cv, x, base, 32, 24, 6, "stone", 3)
  for k = 0, 1 do cv:hline(x + 10, base - 2 - k * 2, 12, c("stone", 5)) end
  local foot = base - 6
  wall(cv, x + 3, foot, 26, 18, { red = true, bay = 12 })
  door(cv, x + 12, foot, 8, 12, true)
  tiled_roof(cv, x - 1, x + 32, foot - 18 + 3, foot - 12 - 18 - 14, foot - 24 - 18 + 2, 9)
  save("shrine", cv)
end

-- The guardian pavilion at the village's entrance: four posts, benches, a hip roof.
do
  local cv, x, base = frame(2, 2, 46)
  block(cv, x, base, 32, 24, 3, "stone", 3)
  local foot = base - 3
  for _, p in ipairs({ 2, 28 }) do
    cv:rect(x + p, foot - 24, 2, 24, c("lacquer", 2)); cv:rect(x + p, foot - 24 - 20, 2, 20, c("lacquer", 1))
  end
  cv:rect(x + 5, foot - 6, 22, 2, c("wood", 4))
  tiled_roof(cv, x - 2, x + 33, foot - 24 + 2, foot - 12 - 24 - 14, foot - 24 - 24 + 2, 9)
  save("pavilion", cv)
end

-- A guardian post: a tall carved pole with a painted face and a cap.
do
  local cv, x, base = frame(1, 1, 40)
  cv:rect(x + 5, base - 46, 6, 46, c("wood", 3)); cv:vline(x + 5, base - 46, 46, c("wood", 4)); cv:vline(x + 10, base - 46, 46, c("wood", 2))
  cv:rect(x + 4, base - 50, 8, 4, c("ink", 2))
  cv:set(x + 6, base - 42, INK); cv:set(x + 9, base - 42, INK); cv:hline(x + 6, base - 38, 4, c("crimson", 3))
  cv:hline(x + 5, base - 34, 6, c("crimson", 4))
  save("guardian", cv)
end

-- The village gate: two posts and a beam under a small tiled roof, its doors open.
do
  local cv, x, base = frame(3, 1, 44)
  for _, p in ipairs({ 2, 42 }) do
    block(cv, x + p - 1, base, 6, 4, 3, "stone", 3)
    cv:rect(x + p, base - 40, 4, 37, c("wood", 2)); cv:vline(x + p, base - 40, 37, c("wood", 3))
  end
  cv:rect(x, base - 42, 48, 4, c("wood", 3)); cv:hline(x, base - 42, 48, c("wood", 4))
  cv:rect(x + 6, base - 30, 6, 26, c("wood", 3)); cv:rect(x + 36, base - 30, 6, 26, c("wood", 3))
  tiled_roof(cv, x - 2, x + 49, base - 42, base - 52, base - 55, 4)
  save("village_gate", cv)
end

-- Market stalls: an awning of cloth in stripes over a counter of baskets and goods.
for _, s in ipairs({ { "stall_red", "crimson" }, { "stall_blue", "azure" }, { "stall_amber", "amber" } }) do
  local cv, x, base = frame(2, 1, 30)
  for _, p in ipairs({ 1, 29 }) do cv:rect(x + p, base - 26, 2, 26, c("wood", 2)) end
  cv:rect(x + 1, base - 10, 30, 6, c("wood", 3)); cv:hline(x + 1, base - 10, 30, c("wood", 4))
  for k = 0, 4 do
    local gx = x + 3 + k * 6
    local goods = ({ c("grass", 5), c("amber", 4), c("crimson", 4), c("paper", 3), c("earth", 5) })[k + 1]
    cv:rect(gx, base - 13, 5, 3, c("wood", 4)); cv:rect(gx + 1, base - 14, 3, 1, goods)
  end
  for yy = 0, 7 do
    for xx = -2, 33 do
      local stripe = ((xx + 2) // 4) % 2 == 0
      cv:set(x + xx, base - 34 + yy, stripe and c(s[2], yy < 2 and 4 or 3) or c("paper", yy < 2 and 3 or 2))
    end
  end
  for xx = -2, 33, 4 do cv:set(x + xx, base - 26, c(s[2], 2)) end
  save(s[1], cv)
end

-- The watchtower: four legs braced across, a railed platform and a small roof.
do
  local cv, x, base = frame(2, 2, 74)
  for _, p in ipairs({ 3, 27 }) do cv:rect(x + p, base - 50, 3, 50, c("wood", 2)); cv:vline(x + p, base - 50, 50, c("wood", 3)) end
  for k = 0, 2 do cv:line(x + 4, base - 6 - k * 16, x + 28, base - 20 - k * 16, c("wood", 3)) end
  cv:rect(x, base - 56, 32, 6, c("wood", 3)); cv:hline(x, base - 56, 32, c("wood", 4))
  for xx = x, x + 31, 3 do cv:vline(xx, base - 62, 6, c("wood", 4)) end
  cv:hline(x, base - 62, 32, c("wood", 3))
  tiled_roof(cv, x - 2, x + 33, base - 64, base - 76, base - 80, 7)
  save("watchtower", cv)
end

-- The stilt house: a plank house raised on piles over the water, a ladder and a deck.
do
  local cv, x, base = frame(3, 2, 64)
  local W, D = 48, 24
  for xx = x + 2, x + W - 4, 9 do
    cv:rect(xx, base - 18, 3, 18, c("wood", 2)); cv:vline(xx, base - 18, 18, c("wood", 3))
    cv:rect(xx, base - 12 - D // 2, 3, 6, c("wood", 1))
  end
  local floor = base - 16
  cv:rect(x - 2, floor - 3, W + 4, 4, c("wood", 3)); cv:hline(x - 2, floor - 3, W + 4, c("wood", 4))
  wall(cv, x + 3, floor - 3, W - 6, 20, { mud = true, bay = 11 })
  for xx = x + 4, x + W - 5, 2 do cv:vline(xx, floor - 22, 19, (xx % 4 == 0) and c("wood", 3) or c("wood", 4)) end
  window(cv, x + 8, floor - 17, 7, 7); door(cv, x + 30, floor - 3, 8, 14, true)
  for k = 0, 4 do cv:hline(x + 32, floor + k * 3, 6, c("wood", 4)) end
  thatch(cv, x + 1, x + W - 2, floor - 23 + 3, floor - 23 - D // 2 - 12, 8)
  save("stilt_house", cv)
end

-- A pier: planks on piles over the water, a cell of it.
do
  local cv, x, base = frame(1, 1, 8)
  for _, p in ipairs({ 1, 13 }) do cv:rect(x + p, base - 10, 2, 10, c("wood", 1)) end
  for yy = base - 12 - 6, base - 8 do cv:hline(x, yy, 16, (yy % 3 == 0) and c("wood", 2) or c("wood", 4)) end
  save("pier", cv)
end

-- A boat: a sampan with a rounded mat canopy, an oar trailing.
do
  local cv, x, base = frame(2, 1, 14)
  cv:each_ellipse(x + 16, base - 5, 16, 4, function(xx, yy, d) cv:set(xx, yy, yy < base - 5 and c("wood", 4) or c("wood", 2)) end)
  cv:each_ellipse(x + 16, base - 7, 12, 2, function(xx, yy) cv:set(xx, yy, c("wood", 1)) end)
  cv:each_ellipse(x + 18, base - 12, 7, 6, function(xx, yy) if yy < base - 8 then cv:set(xx, yy, c("earth", 4)) end end)
  cv:line(x + 2, base - 4, x - 4, base + 2, c("wood", 4))
  save("boat", cv)
end

-- The town wall: rammed earth faced with brick, a tiled cap with crenels.
local WALL = 34
do
  local cv, x, base = frame(1, 1, WALL + 4)
  cv:rect(x, base - WALL, 16, WALL, c("stone", 3))
  for yy = base - WALL, base - 1 do
    for xx = x, x + 15 do
      local course = (base - yy) // 4
      if (base - yy) % 4 == 0 or (xx + course * 4) % 8 == 0 then cv:set(xx, yy, c("stone", 2)) end
    end
  end
  cv:rect(x, base - WALL - 12, 16, 12, c("stone", 4))
  for xx = x, x + 15, 6 do cv:rect(xx, base - WALL - 16, 4, 4, c("stone", 4)); cv:hline(xx, base - WALL - 16, 4, c("stone", 5)) end
  cv:hline(x, base - WALL, 16, c("stone", 5))
  save("wall_h", cv)
end
do
  local cv, x, base = frame(1, 1, WALL + 4)
  cv:rect(x + 3, base - WALL - 12, 10, 12, c("stone", 4))
  cv:vline(x + 3, base - WALL - 12, 12, c("stone", 5)); cv:vline(x + 12, base - WALL - 12, 12, c("stone", 2))
  for yy = base - WALL - 12, base - WALL, 6 do cv:rect(x + 2, yy, 2, 3, c("stone", 5)) end
  save("wall_v", cv)
end

-- The town gate: the wall's height pierced by an arch, a two-storey tower over it.
do
  local cv, x, base = frame(4, 2, 92)
  local W = 64
  cv:rect(x, base - WALL, W, WALL, c("stone", 3))
  for yy = base - WALL, base - 1 do
    for xx = x, x + W - 1 do
      local course = (base - yy) // 4
      if (base - yy) % 4 == 0 or (xx + course * 4) % 8 == 0 then cv:set(xx, yy, c("stone", 2)) end
    end
  end
  cv:each_ellipse(x + 32, base - 18, 10, 10, function(xx, yy) if yy <= base then cv:set(xx, yy, c("ink", 1)) end end)
  cv:rect(x + 22, base - 18, 21, 18, c("ink", 1))
  cv:rect(x - 1, base - WALL - 24, W + 2, 24, c("stone", 4))
  local foot = base - WALL - 20
  wall(cv, x + 6, foot, W - 12, 22, { red = true, bay = 13 })
  for k = 0, 3 do window(cv, x + 12 + k * 12, foot - 16, 6, 9) end
  tiled_roof(cv, x + 1, x + W - 2, foot - 22 + 3, foot - 22 - 12 - 22, foot - 22 - 24, 14)
  save("town_gate", cv)
end

-- A low field-stone wall and a woven brushwood fence, a cell of each.
do
  local cv, x, base = frame(1, 1, 8)
  for yy = base - 10, base - 1 do
    for xx = x, x + 15 do
      local n = px.hash((xx - x) // 4 + ((base - yy) // 3) * 7, 0, 222)
      cv:set(xx, yy, n > 0.6 and c("stone", 4) or (n > 0.3 and c("stone", 3) or c("stone", 2)))
    end
  end
  for xx = x, x + 15 do cv:set(xx, base - 11, c("stone", 5)) end
  save("stone_wall", cv)
end
do
  local cv, x, base = frame(1, 1, 8)
  for _, p in ipairs({ 1, 9 }) do cv:rect(x + p, base - 14, 2, 14, c("wood", 2)) end
  for yy = base - 12, base - 3, 2 do
    for xx = x, x + 15 do cv:set(xx, yy, ((xx + yy) % 4 < 2) and c("wood", 4) or c("wood", 3)) end
  end
  save("brush_fence", cv)
end

-- The well: a stone ring round dark water under a roof on two posts.
do
  local cv, x, base = frame(2, 2, 40)
  local cx, cy = x + 16, base - 12
  for xx = cx - 11, cx + 11 do
    local dx = (xx - cx) / 11.5
    local lip = cy + math.floor(5 * math.sqrt(math.max(0, 1 - dx * dx)) + 0.5)
    for yy = lip, lip + 8 do
      local col = ((yy - lip) % 3 == 2) and c("stone", 2) or c("stone", 3)
      if xx < cx - 6 then col = px.shift(col, 1) elseif xx > cx + 6 then col = px.shift(col, -1) end
      cv:set(xx, yy, col)
    end
  end
  cv:ellipse(cx, cy, 11.5, 5.2, c("stone", 5))
  cv:ellipse(cx, cy + 0.5, 8.5, 3.4, c("water", 1))
  for _, p in ipairs({ cx - 12, cx + 11 }) do cv:rect(p, cy - 30, 2, 34, c("wood", 3)) end
  cv:hline(cx - 11, cy - 22, 23, c("wood", 2)); cv:vline(cx + 3, cy - 21, 14, c("paper", 1))
  cv:rect(cx + 1, cy - 8, 5, 4, c("wood", 3))
  tiled_roof(cv, cx - 15, cx + 15, cy - 29, cy - 39, cy - 42, 4)
  save("well_square", cv)
end

-- Crocks: three glazed jars of different sizes, lit on their upper left.
do
  local cv, x, base = frame(1, 1, 10)
  for _, j in ipairs({ { 4, 6, 5 }, { 11, 5, 4 }, { 8, 3, 3 } }) do
    cv:each_ellipse(x + j[1], base - j[2] - 2, j[3], j[3] + 1, function(xx, yy, d)
      local lit = (xx - (x + j[1])) + (yy - (base - j[2] - 2)) < -1
      cv:set(xx, yy, lit and c("wood", 3) or (d > 0.75 and c("wood", 1) or c("wood", 2)))
    end)
    cv:hline(x + j[1] - 1, base - j[2] * 2 - 3, 3, c("wood", 1))
  end
  save("crocks", cv)
end

-- Laundry: a line between two poles hung with cloths.
do
  local cv, x, base = frame(2, 1, 22)
  for _, p in ipairs({ 1, 30 }) do cv:rect(x + p, base - 24, 2, 24, c("wood", 2)) end
  cv:hline(x + 2, base - 22, 28, c("paper", 1))
  for k, col in ipairs({ c("azure", 4), c("paper", 3), c("crimson", 3), c("cloth", 3) }) do
    cv:rect(x + k * 6, base - 21, 5, 9 - (k % 2) * 2, col)
  end
  save("laundry", cv)
end

-- A cart: a plank bed on two spoked wheels, shafts on the ground, sacks aboard.
do
  local cv, x, base = frame(2, 1, 14)
  cv:rect(x + 4, base - 13, 22, 4, c("wood", 3)); cv:hline(x + 4, base - 13, 22, c("wood", 4))
  for _, wx in ipairs({ 8, 22 }) do
    cv:each_ellipse(x + wx, base - 6, 5, 5, function(xx, yy, d) if d > 0.65 then cv:set(xx, yy, c("wood", 2)) end end)
    cv:set(x + wx, base - 6, c("wood", 1))
  end
  cv:line(x + 26, base - 11, x + 32, base - 2, c("wood", 3))
  cv:rect(x + 8, base - 18, 6, 5, c("cloth", 3)); cv:rect(x + 15, base - 17, 6, 4, c("cloth", 2))
  save("cart", cv)
end

-- A haystack, a stack of firewood and a stone lantern, a cell each.
do
  local cv, x, base = frame(1, 1, 18)
  cv:each_ellipse(x + 8, base - 9, 7, 10, function(xx, yy, d)
    if yy > base - 1 then return end
    local n = px.noise(xx, yy, 2, 41)
    cv:set(xx, yy, (xx - x - 8) + (yy - base + 9) < -3 and c("amber", 5) or (n > 0.5 and c("amber", 4) or c("earth", 5)))
  end)
  cv:vline(x + 8, base - 22, 4, c("wood", 2))
  save("haystack", cv)
end
do
  local cv, x, base = frame(1, 1, 6)
  for row = 0, 2 do
    for k = 0, 3 - (row % 2) do
      local lx, ly = x + 1 + k * 4 + (row % 2) * 2, base - 3 - row * 3
      cv:ellipse(lx + 1, ly, 1.6, 1.4, c("wood", 4)); cv:set(lx + 1, ly, c("earth", 5))
    end
  end
  save("firewood", cv)
end
do
  local cv, x, base = frame(1, 1, 14)
  block(cv, x + 4, base, 8, 3, 3, "stone", 3)
  cv:rect(x + 7, base - 14, 2, 11, c("stone", 3))
  cv:rect(x + 4, base - 20, 8, 6, c("stone", 4)); cv:rect(x + 6, base - 18, 4, 3, c("fire", 4))
  cv:rect(x + 3, base - 22, 10, 2, c("stone", 5))
  save("lantern", cv)
end

-- A bush and a boulder, to break the ground up.
do
  local cv, x, base = frame(1, 1, 6)
  cv:each_ellipse(x + 8, base - 6, 8, 6, function(xx, yy, d)
    local n = px.noise(xx, yy, 2, 51)
    cv:set(xx, yy, (xx - x - 8) + (yy - base + 6) < -3 and c("grass", 5) or (n > 0.5 and c("leaf", 4) or c("leaf", 3)))
  end)
  save("bush", cv)
end
do
  local cv, x, base = frame(1, 1, 6)
  cv:each_ellipse(x + 8, base - 5, 7, 5, function(xx, yy, d)
    cv:set(xx, yy, (xx - x - 8) + (yy - base + 5) < -2 and c("stone", 5) or (d > 0.7 and c("stone", 2) or c("stone", 3)))
  end)
  save("boulder", cv)
end
