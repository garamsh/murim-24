-- Draws fields.aseprite: 16x16 field tiles in each state a field passes through, two variants
-- each, tagged sown, growing, ripe, fallow and burnt, and the same five under snow, tagged with
-- _winter. Fields never block or break; the view lays them as decoration. Every tile has the
-- earth border of a plot along its top and left edges, as the ground tiles' fields do.
-- Run: aseprite --batch --script art/decorations/fields.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c
local T = 16

-- Tilled soil in rows four pixels apart: furrow, ridge, ridge, furrow.
local function soil(cv, v, dark)
  for y = 0, T - 1 do
    for x = 0, T - 1 do
      local col = (x % 4 == 0) and c("earth", 2) or ((x % 4 == 3) and c("earth", 3) or c("earth", 4))
      if dark then col = px.shift(col, -1) end
      if px.hash(x, y, 70 + v) > 0.92 then col = px.shift(col, 1) end
      cv:set(x, y, col)
    end
  end
end

local states = {
  sown = function(cv, v)
    soil(cv, v, true)
    for y = 2, T - 1, 3 do
      for x = 2, T - 1, 4 do
        if px.hash(x, y, 80 + v) > 0.3 then cv:set(x, y, c("grass", 5)) end
      end
    end
  end,
  growing = function(cv, v)
    soil(cv, v, false)
    for y = 1, T - 1, 3 do
      for x = 1, T - 1, 4 do
        local h = 1 + math.floor(px.hash(x, y, 90 + v) * 2)
        for k = 0, h do cv:set(x + 1, y + 2 - k, k == h and c("grass", 6) or c("grass", 4)) end
        cv:set(x, y + 1, c("grass", 5)); cv:set(x + 2, y + 1, c("grass", 5))
      end
    end
  end,
  ripe = function(cv, v)
    for y = 0, T - 1 do
      for x = 0, T - 1 do
        local soilrow = x % 4 == 0
        local col = soilrow and c("earth", 2) or (((y + x // 4 + v) % 3 == 0) and c("amber", 5) or c("amber", 4))
        if not soilrow and px.hash(x, y, 100 + v) > 0.85 then col = c("amber", 3) end
        cv:set(x, y, col)
      end
    end
  end,
  fallow = function(cv, v)
    for y = 0, T - 1 do
      for x = 0, T - 1 do
        local n = px.tile_noise(x, y, 4, T, 110 + v)
        local col = n > 0.55 and c("grass", 4) or (n > 0.35 and c("grass", 3) or c("earth", 3))
        if x % 4 == 0 and n < 0.45 then col = c("earth", 2) end
        cv:set(x, y, col)
      end
    end
    for k = 1, 3 do
      local x, y = 2 + math.floor(px.hash(k, v, 111) * 11), 2 + math.floor(px.hash(v, k, 112) * 11)
      cv:set(x, y, c("grass", 6)); cv:set(x - 1, y + 1, c("grass", 5)); cv:set(x + 1, y + 1, c("grass", 5))
    end
  end,
  burnt = function(cv, v)
    for y = 0, T - 1 do
      for x = 0, T - 1 do
        local n = px.tile_noise(x, y, 4, T, 120 + v)
        local col = n > 0.6 and c("stone", 3) or (n > 0.3 and c("ink", 3) or c("ink", 2))
        if x % 4 == 1 and y % 3 ~= 0 then col = c("wood", 1) end
        cv:set(x, y, col)
      end
    end
    for k = 1, 3 do
      cv:set(1 + math.floor(px.hash(k, v, 121) * 14), 1 + math.floor(px.hash(v, k, 122) * 14), c("fire", 2))
    end
  end,
}

-- The plot's border, then a dusting of snow in winter, gathered in the furrows, the field's
-- state still showing through.
local function finish(cv, v, winter)
  for k = 0, T - 1 do cv:set(k, 0, c("earth", 3)); cv:set(0, k, c("earth", 3)) end
  if winter then
    for y = 0, T - 1 do
      for x = 0, T - 1 do
        local n = px.tile_noise(x, y, 4, T, 130 + v) + (x % 4 == 0 and 0.2 or 0)
        if n > 0.7 then cv:set(x, y, n > 0.85 and c("stone", 6) or c("stone", 5)) end
      end
    end
  end
end

local frames, tags = {}, {}
for _, winter in ipairs({ false, true }) do
  for _, state in ipairs({ "sown", "growing", "ripe", "fallow", "burnt" }) do
    local first = #frames + 1
    for v = 1, 2 do
      local cv = px.canvas(T, T)
      states[state](cv, v)
      finish(cv, v, winter)
      frames[#frames + 1] = cv
    end
    tags[#tags + 1] = { state .. (winter and "_winter" or ""), first, #frames, 100 }
  end
end
px.save_frames(T, T, frames, tags, here .. "/fields.aseprite")
