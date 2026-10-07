-- Draws icon.aseprite: the game's 32x32 application icon, which the macOS and Windows exports
-- scale up by whole multiples. A jian stands planted before ridges of mountains under a low sun,
-- as on the title screen, inside the HUD's ink frame with its gold trim and lacquer corners.
-- Run: aseprite --batch --script art/icon/icon.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c

local S = 32
local icon = px.canvas(S, S)

-- The icon's face, rounded by two pixels at each corner; everything is drawn inside it.
local function inside(x, y)
  local dx = math.max(0, 2 - x, x - (S - 3))
  local dy = math.max(0, 2 - y, y - (S - 3))
  return dx + dy <= 2 and x >= 0 and y >= 0 and x < S and y < S
end

-- The sky darkens upward, its bands dithered into each other.
local sky = { { 0, c("ink", 1) }, { 8, c("ink", 2) }, { 15, c("ink", 3) }, { 20, c("stone", 1) } }
for y = 0, S - 1 do
  for x = 0, S - 1 do
    if inside(x, y) then
      local band = 1
      for n, b in ipairs(sky) do if y >= b[1] then band = n end end
      local i = sky[band][2]
      local next_band = sky[band + 1]
      if next_band and px.bayer(x, y) < ((y - sky[band][1]) / (next_band[1] - sky[band][1]) - 0.4) / 0.6 then
        i = next_band[2]
      end
      icon:set(x, y, i)
    end
  end
end

-- The low sun, haloed by dithered gold.
local SX, SY = 22, 11
icon:each_ellipse(SX, SY, 8, 8, function(x, y, d)
  if px.bayer(x, y) > d * 1.6 - 0.2 then icon:set(x, y, c("gold", 1)) end
end)
icon:ellipse(SX, SY, 4.5, 4.5, c("gold", 3))
icon:ellipse(SX - 1, SY - 1, 2.5, 2.5, c("gold", 4))

-- Ridges back to front, each lit a step at its crest.
local ridges = {
  { base = 23, amp = 9, scale = 8, seed = 11, colour = c("stone", 2) },
  { base = 27, amp = 6, scale = 6, seed = 12, colour = c("ink", 3) },
  { base = 29, amp = 3, scale = 6, seed = 14, colour = c("ink", 2) },
}
for _, r in ipairs(ridges) do
  for x = 0, S - 1 do
    local top = math.floor(r.base - px.noise(x, 0, r.scale, r.seed) * r.amp)
    for y = top, S - 1 do
      if inside(x, y) then icon:set(x, y, y == top and px.shift(r.colour, 1) or r.colour) end
    end
  end
end

-- The jian, point down in the near ridge: a blade lit on its left edge, a gold guard, a wrapped
-- grip and a pommel with a crimson tassel, outlined in ink on a canvas of its own.
local jian = px.canvas(S, S)
jian:vline(10, 9, 18, c("stone", 6)); jian:vline(11, 9, 18, c("stone", 4)); jian:set(10, 26, c("stone", 5))
jian:hline(7, 8, 8, c("gold", 3)); jian:hline(8, 7, 6, c("gold", 4))
jian:vline(10, 3, 4, c("wood", 4)); jian:vline(11, 3, 4, c("wood", 3))
jian:set(10, 4, c("wood", 2)); jian:set(11, 6, c("wood", 2))
jian:hline(10, 2, 2, c("gold", 4))
jian:set(12, 2, c("crimson", 4)); jian:set(13, 3, c("crimson", 4)); jian:set(13, 4, c("crimson", 3))
jian:set(14, 5, c("crimson", 3))
jian:outline(6, 1, 10, 27, c("ink", 1))
icon:stamp(jian, 0, 0)

-- The frame: an ink edge and a gold rim inside it, with lacquer studs at the corners.
local function edge(x, y, n)
  return not (inside(x - n, y) and inside(x + n, y) and inside(x, y - n) and inside(x, y + n))
end
for y = 0, S - 1 do
  for x = 0, S - 1 do
    if inside(x, y) and edge(x, y, 1) then
      icon:set(x, y, c("ink", 1))
    elseif inside(x, y) and edge(x, y, 2) then
      icon:set(x, y, c("gold", 2))
    end
  end
end
for _, p in ipairs({ { 2, 2 }, { S - 3, 2 }, { 2, S - 3 }, { S - 3, S - 3 } }) do
  icon:set(p[1], p[2], c("lacquer", 3))
end

px.save_frames(S, S, { icon }, { { "icon", 1, 1, 100 } }, here .. "/icon.aseprite")
