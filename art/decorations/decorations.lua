-- Draws the village decorations, which never block or break and are laid by the view from
-- presentation data: ground.aseprite, tiles of built ground a cell wide and deep at the 2.5D angle
-- (ADR 0059), 16x12, tagged paving, road, millet and rice; and banner.aseprite, a faction's banner on a pole in a 16x24 frame whose bottom 16
-- rows are its cell, tagged orthodox, demonic and unorthodox; and sect_banner.aseprite, a sect's
-- tall streamer hung from a crossbar, in a 16x32 frame whose bottom 16 rows are its cell, tagged
-- the same.
-- Run: aseprite --batch --script art/decorations/decorations.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)
local c = px.c
local T = 16
-- A ground tile is a cell, 16 pixels wide and 12 deep.
local W, D = 16, 12

-- Paving: slabs 8 pixels wide and 6 deep in courses offset by half a slab, a light slab here and
-- there.
local function paving(cv, v)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local course = y // 6
      local sx = (x + (course % 2) * 4) % 8
      local col = c("stone", 4)
      if sx == 0 or y % 6 == 0 then col = c("stone", 3) end
      local slab = px.hash((x + (course % 2) * 4) // 8, course, 40 + v)
      if sx ~= 0 and y % 6 ~= 0 and slab > 0.75 then col = c("stone", 6) end
      if sx ~= 0 and y % 6 ~= 0 and px.hash(x, y, 41 + v) > 0.96 then col = c("stone", 3) end
      cv:set(x, y, col)
    end
  end
end

-- Road: packed earth with two wheel ruts and pebbles.
local function road(cv, v)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local n = px.tile_noise(x, y * 4 // 3, 4, T, 50 + v) + (px.hash(x, y, 51 + v) - 0.5) * 0.3
      local col = c("earth", 4)
      if n > 0.72 then col = c("earth", 5) elseif n < 0.28 then col = c("earth", 3) end
      if (y == 3 or y == 8) and px.hash(x, y, 52 + v) > 0.2 then col = c("earth", 3) end
      cv:set(x, y, col)
    end
  end
end

-- A field: crop rows over soil, a border of earth along the plot's edge.
local function field(cv, v, crop)
  for y = 0, D - 1 do
    for x = 0, W - 1 do
      local soil = x % 4 == 0
      local col
      if crop == "millet" then
        col = soil and c("earth", 2) or (((y + x // 4 + v) % 3 == 0) and c("amber", 7) or c("amber", 5))
        if not soil and px.hash(x, y, 60 + v) > 0.85 then col = c("amber", 4) end
      else
        col = soil and c("water", 3) or (((y + x // 4 + v) % 3 == 0) and c("grass", 6) or c("grass", 5))
      end
      if y == 0 or x == 0 then col = c("earth", 3) end
      cv:set(x, y, col)
    end
  end
end

local frames, tags = {}, {}
local function add(tag, count, draw)
  local first = #frames + 1
  for v = 1, count do
    local cv = px.canvas(W, D)
    draw(cv, v)
    frames[#frames + 1] = cv
  end
  tags[#tags + 1] = { tag, first, #frames, 100 }
end
add("paving", 4, paving)
add("road", 4, road)
add("millet", 2, function(cv, v) field(cv, v, "millet") end)
add("rice", 2, function(cv, v) field(cv, v, "rice") end)
px.save_frames(W, D, frames, tags, here .. "/ground.aseprite")

local banners, banner_tags = {}, {}
for n, f in ipairs({ { "orthodox", "azure" }, { "demonic", "crimson" }, { "unorthodox", "amber" } }) do
  local cv = px.canvas(T, 24)
  S.banner(cv, 3, 23, f[2])
  banners[n] = cv
  banner_tags[n] = { f[1], n, n, 100 }
end
px.save_frames(T, 24, banners, banner_tags, here .. "/banner.aseprite")

-- A sect's banner: a tall pole on a stone foot, a crossbar near its top, and a long streamer of
-- paper-white cloth edged in the faction's accent, two ink characters down it and a swallowtail
-- with tassels at its foot. The faction's banner is a short pennant of the accent itself, so the
-- two read apart at a glance.
local function sect_banner(cv, ramp)
  local x = 3
  cv:rect(x - 1, 29, 4, 3, c("stone", 4)); cv:hline(x - 1, 29, 4, c("stone", 6))
  cv:vline(x, 2, 27, c("wood", 4)); cv:vline(x + 1, 2, 27, c("wood", 2))
  cv:set(x, 0, c("stone", 7)); cv:set(x, 1, c("stone", 6)); cv:set(x + 1, 1, c("stone", 4))
  cv:hline(x, 3, 10, c("wood", 4)); cv:hline(x + 2, 4, 8, c("wood", 2))
  cv:set(x + 9, 2, c("gold", 6))
  local top, bottom = 5, 21
  for yy = top, bottom do
    for xx = x + 3, x + 9 do
      local col = c("paper", 6)
      if xx == x + 3 or yy == top then col = c(ramp, 5) end
      if xx == x + 9 or yy == bottom then col = c(ramp, 2) end
      if xx == x + 8 and yy > top and yy < bottom then col = c("paper", 4) end
      cv:set(xx, yy, col)
    end
  end
  -- The swallowtail: the cloth's foot cut in two points.
  for xx = x + 3, x + 9 do
    if xx ~= x + 6 then cv:set(xx, bottom + 1, xx < x + 6 and c(ramp, 4) or c(ramp, 2)) end
  end
  cv:set(x + 3, bottom + 2, c(ramp, 4)); cv:set(x + 9, bottom + 2, c(ramp, 2))
  cv:set(x + 3, bottom + 3, c("gold", 4)); cv:set(x + 9, bottom + 3, c("gold", 4))
  -- Two characters, brushed in ink.
  cv:hline(x + 5, 8, 3, c("ink", 1)); cv:vline(x + 6, 7, 5, c("ink", 1)); cv:set(x + 5, 11, c("ink", 1))
  cv:hline(x + 5, 14, 3, c("ink", 1)); cv:vline(x + 5, 15, 3, c("ink", 1)); cv:vline(x + 7, 15, 3, c("ink", 1))
  cv:hline(x + 5, 17, 3, c("ink", 1))
  cv:outline(0, 0, T, 32, c("ink", 1))
end
local sects, sect_tags = {}, {}
for n, f in ipairs({ { "orthodox", "azure" }, { "demonic", "crimson" }, { "unorthodox", "amber" } }) do
  local cv = px.canvas(T, 32)
  sect_banner(cv, f[2])
  sects[n] = cv
  sect_tags[n] = { f[1], n, n, 100 }
end
px.save_frames(T, 32, sects, sect_tags, here .. "/sect_banner.aseprite")
