-- Draws the materials world_art/ground.gd draws the ground from at the 2.5D angle (ADR 0059):
--   ramps.aseprite: a 7 x 1 frame for each kind of ground, in the order world_art/relief.gd's KINDS
--     gives them and tagged by its name, its seven colours dark to light, then bank, the earth of a
--     bank's face. Each ramp is built by pixel.lua's build_ramp (ADR 0069) from the colour of its
--     hue, saturation and lightness, over a narrower span of lightness than a material's, 0.13
--     down and 0.12 up, and a third of its cool and warm mix, so the ground stays calm under what
--     stands on it.
--   detail.aseprite: 5 x 5 frames of the small detail scattered over the ground, its root at (2, 3):
--     tuft, flower (violet, yellow, white: no red, which on the ground means blood), litter and
--     reed. A pixel in INK_STEPS's colours stands for that step, 0, 2, 4 or 6, of the ramp of the
--     ground the view draws it on; any other colour is drawn as it is.
-- Run: aseprite --batch --script art/terrain/ground.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c

local KINDS = {
  { "water", { 192, 0.42, 0.42 } },
  { "plain", { 100, 0.42, 0.42 } },
  { "hills", { 72, 0.36, 0.5 } },
  { "forest", { 130, 0.38, 0.27 } },
  { "rock", { 220, 0.12, 0.46 } },
  { "bank", { 28, 0.35, 0.36 } },
}

-- How far a ground ramp's darkest step lies below its colour in lightness, and its lightest above;
-- and how far its shadows and lights are mixed toward the cool and warm tints, a third of a
-- material's, so the ground keeps its own hue.
local DOWN, UP = 0.13, 0.12
local COOL, WARM = 0.18, 0.12
local REACH = 6 / 7

local function hsl_hex(h, s, l)
  local q = l < 0.5 and l * (1 + s) or l + s - l * s
  local p = 2 * l - q
  local function channel(t)
    t = t % 1
    if t < 1 / 6 then return p + (q - p) * 6 * t end
    if t < 0.5 then return q end
    if t < 2 / 3 then return p + (q - p) * (2 / 3 - t) * 6 end
    return p
  end
  local hue = (h % 360) / 360
  local function byte(v) return math.floor(v * 255 + 0.5) end
  return string.format("%02x%02x%02x", byte(channel(hue + 1 / 3)), byte(channel(hue)), byte(channel(hue - 1 / 3)))
end

-- Ramps are drawn in RGB: they are outside the master palette.
local ramps = Sprite(px.STEPS, 1, ColorMode.RGB)
for n, kind in ipairs(KINDS) do
  if n > 1 then ramps:newEmptyFrame() end
  local h, s, l = kind[2][1], kind[2][2], kind[2][3]
  local ramp = px.build_ramp(hsl_hex(h, s, l), DOWN / (l * REACH), UP / ((1 - l) * REACH), COOL, WARM)
  local image = Image(px.STEPS, 1, ColorMode.RGB)
  for step, hex in ipairs(ramp) do
    image:drawPixel(step - 1, 0, Color { r = tonumber(hex:sub(1, 2), 16), g = tonumber(hex:sub(3, 4), 16),
      b = tonumber(hex:sub(5, 6), 16), a = 255 })
  end
  if n == 1 then ramps:deleteCel(ramps.layers[1], 1) end
  ramps:newCel(ramps.layers[1], n, image, Point(0, 0))
  local tag = ramps:newTag(n, n)
  tag.name = kind[1]
end
ramps:saveAs(here .. "/ramps.aseprite")
ramps:close()

-- Detail, each mark as { dx, dy, colour } from its root at (2, 3); step 2n of the ground's ramp is
-- the ink ramp's colour INK_STEPS[n + 1], which world_art/ground.gd reads back.
local INK_STEPS = { 1, 2, 4, 5 }
local function step(n) return c("ink", INK_STEPS[n + 1]) end
local marks = {
  { "tuft", { { 0, 0, step(0) }, { -1, -1, step(0) }, { 1, -1, step(0) }, { 0, -1, step(3) } } },
  { "flower", { { 0, 0, c("violet", 5) }, { 0, 1, step(0) } },
    { { 0, 0, c("amber", 7) }, { 0, 1, step(0) } },
    { { 0, 0, c("robe", 7) }, { 0, 1, step(0) } } },
  { "litter", { { 0, 0, step(0) }, { 1, -1, step(0) }, { -1, -1, step(2) } } },
  { "reed", { { 0, 0, step(1) }, { 0, -1, step(1) }, { 0, -2, step(1) }, { 0, -3, step(3) } } },
}
local frames, tags = {}, {}
for _, detail in ipairs(marks) do
  local first = #frames + 1
  for v = 2, #detail do
    local cv = px.canvas(5, 5)
    for _, mark in ipairs(detail[v]) do cv:set(2 + mark[1], 3 + mark[2], mark[3]) end
    frames[#frames + 1] = cv
  end
  tags[#tags + 1] = { detail[1], first, #frames, 100 }
end
px.save_frames(5, 5, frames, tags, here .. "/detail.aseprite")
