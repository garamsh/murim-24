-- Draws terrain.aseprite: four 16x16 variants of each terrain kind the region generator
-- defines (core/region.cpp, region_data()), one tag per kind. Every variant's edge is drawn
-- from noise that repeats every 16 pixels, so any variant meets any other of its kind
-- without a seam.
-- Run: aseprite --batch --script art/terrain/terrain.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c

local T = 16
local VARIANTS = 4

local function plain(cv, v)
  local r = px.rng(100 + v)
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local n = px.tile_noise(x, y, 8, T, 10 + v) + (px.tile_noise(x, y, 4, T, 20 + v) - 0.5) * 0.4
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
  -- Canopy seen from above: lumps lit from the upper left over a dark understorey.
  for y = 0, T - 1 do
    for x = 0, T - 1 do cv:set(x, y, px.tile_noise(x, y, 4, T, 30 + v) > 0.5 and c("leaf", 2) or c("leaf", 1)) end
  end
  local r = px.rng(200 + v)
  local lumps = { { 4, 4, 4 }, { 12, 5, 4 }, { 7, 11, 5 }, { 15, 13, 3 }, { 0, 12, 3 } }
  for _, l in ipairs(lumps) do
    local cx, cy, rad = l[1] + r.int(-1, 1), l[2] + r.int(-1, 1), l[3]
    for dy = -rad, rad do
      for dx = -rad, rad do
        local d = math.sqrt(dx * dx + dy * dy) / rad
        if d <= 1 then
          local x, y = (cx + dx) % T, (cy + dy) % T
          local light = (-dx - dy) / rad + (px.tile_noise(x, y, 2, T, 40 + v) - 0.5) * 0.8
          local col = c("leaf", 3)
          if light > 0.7 then col = c("leaf", 5) elseif light > 0.2 then col = c("leaf", 4)
          elseif light < -0.6 then col = c("leaf", 2) end
          cv:set(x, y, col)
        end
      end
    end
  end
end

local function hills(cv, v)
  -- Terraced slopes: wavy contour ridges two to a tile, each lit on its upper edge and
  -- shaded below. Every variant's wave is zero at the tile's edge, so ridges run on across
  -- tiles whichever variants meet; variants differ in the wave between the edges.
  local sign = v <= 2 and 1 or -1
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local wave = sign * 2.2 * math.sin(x / T * 2 * math.pi) + (v % 2) * 1.2 * math.sin(x / T * 4 * math.pi)
      local t = (y + wave) % 8
      local n = px.tile_noise(x, y, 4, T, 50 + v)
      local col = c("grass", 4)
      if t < 1 then col = c("grass", 6)
      elseif t < 3 then col = n > 0.4 and c("grass", 5) or c("grass", 4)
      elseif t >= 7 then col = c("grass", 2)
      elseif t >= 5.5 then col = c("grass", 3) end
      cv:set(x, y, col)
    end
  end
end

local function water(cv, v)
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local n = px.tile_noise(x, y, 4, T, 60 + v)
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
  -- Bare rock as a ridge crossing the tile: the lit slope above the crest, the shaded face
  -- below it creased by gullies, scree at the foot. The crest repeats every 16 pixels, so
  -- ridges run on across tiles and stacked tiles read as ridge behind ridge. The crest is the
  -- same in every variant; the gullies and scree differ.
  local peak = T // 2
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local d = math.abs(((x - peak + T / 2) % T) - T / 2)
      local crest = 3 + math.floor(d * 0.6 + 0.5)
      local n = px.tile_noise(x, y, 2, T, 70 + v)
      local col
      if y < crest then
        col = (y < crest - 2) and c("stone", 4) or c("stone", 5)
        if x < peak and y >= crest - 1 then col = c("stone", 6) end
      elseif y == crest then
        col = c("stone", 1)
      elseif y < 13 then
        col = ((x + v) % 5 == 0 or n > 0.8) and c("stone", 2) or c("stone", 3)
        if y > crest + 1 and n < 0.2 then col = c("stone", 4) end
      else
        col = (px.bayer(x, y) + n * 0.5 > 0.7) and c("stone", 3) or c("stone", 2)
      end
      cv:set(x, y, col)
    end
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
