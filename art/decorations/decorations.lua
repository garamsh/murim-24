-- Draws the village decorations, which never block or break and are laid by the view from
-- presentation data: ground.aseprite, 16x16 tiles of built ground tagged paving, road, millet
-- and rice; and banner.aseprite, a faction's banner on a pole in a 16x24 frame whose bottom 16
-- rows are its cell, tagged orthodox, demonic and unorthodox.
-- Run: aseprite --batch --script art/decorations/decorations.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)
local c = px.c
local T = 16

-- Paving: slabs 8 pixels square in courses offset by half a slab, a light slab here and there.
local function paving(cv, v)
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local course = y // 8
      local sx = (x + (course % 2) * 4) % 8
      local col = c("stone", 4)
      if sx == 0 or y % 8 == 0 then col = c("stone", 3) end
      local slab = px.hash((x + (course % 2) * 4) // 8, course, 40 + v)
      if sx ~= 0 and y % 8 ~= 0 and slab > 0.75 then col = c("stone", 5) end
      if sx ~= 0 and y % 8 ~= 0 and px.hash(x, y, 41 + v) > 0.96 then col = c("stone", 3) end
      cv:set(x, y, col)
    end
  end
end

-- Road: packed earth with two wheel ruts and pebbles.
local function road(cv, v)
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local n = px.tile_noise(x, y, 4, T, 50 + v) + (px.hash(x, y, 51 + v) - 0.5) * 0.3
      local col = c("earth", 4)
      if n > 0.72 then col = c("earth", 5) elseif n < 0.28 then col = c("earth", 3) end
      if (y == 4 or y == 11) and px.hash(x, y, 52 + v) > 0.2 then col = c("earth", 3) end
      cv:set(x, y, col)
    end
  end
end

-- A field: crop rows over soil, a border of earth along the plot's edge.
local function field(cv, v, crop)
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local soil = x % 4 == 0
      local col
      if crop == "millet" then
        col = soil and c("earth", 2) or (((y + x // 4 + v) % 3 == 0) and c("amber", 5) or c("amber", 4))
        if not soil and px.hash(x, y, 60 + v) > 0.85 then col = c("amber", 3) end
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
    local cv = px.canvas(T, T)
    draw(cv, v)
    frames[#frames + 1] = cv
  end
  tags[#tags + 1] = { tag, first, #frames, 100 }
end
add("paving", 4, paving)
add("road", 4, road)
add("millet", 2, function(cv, v) field(cv, v, "millet") end)
add("rice", 2, function(cv, v) field(cv, v, "rice") end)
px.save_frames(T, T, frames, tags, here .. "/ground.aseprite")

local banners, banner_tags = {}, {}
for n, f in ipairs({ { "orthodox", "azure" }, { "demonic", "crimson" }, { "unorthodox", "amber" } }) do
  local cv = px.canvas(T, 24)
  S.banner(cv, 3, 23, f[2])
  banners[n] = cv
  banner_tags[n] = { f[1], n, n, 100 }
end
px.save_frames(T, 24, banners, banner_tags, here .. "/banner.aseprite")
