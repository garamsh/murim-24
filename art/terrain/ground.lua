-- Draws the materials world_art/ground.gd draws the ground from at the 2.5D angle (ADR 0059):
--   ramps.aseprite: a 4 x 1 frame for each kind of ground, in the order world_art/relief.gd's KINDS
--     gives them and tagged by its name, its four colours dark to light, then bank, the earth of a
--     bank's face. Each ramp is stepped from a hue, saturation and lightness by shifting the hue
--     toward blue in the dark steps and toward yellow in the light ones (#212).
--   detail.aseprite: 5 x 5 frames of the small detail scattered over the ground, its root at (2, 3):
--     tuft, flower (red, yellow, white), litter and reed. A pixel in the
--     ink ramp's colours stands for that step, 0 to 3, of the ramp of the ground the view draws it
--     on; any other colour is drawn as it is.
-- Run: aseprite --batch --script art/terrain/ground.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c

local KINDS = {
  { "water", { 205, 0.45, 0.42 } },
  { "plain", { 100, 0.42, 0.42 } },
  { "hills", { 80, 0.36, 0.5 } },
  { "forest", { 130, 0.38, 0.27 } },
  { "rock", { 220, 0.12, 0.46 } },
  { "bank", { 28, 0.35, 0.36 } },
}

local function clamp(v) return math.max(0, math.min(1, v)) end

-- Step `step`, 0 to 3, of a ramp given as hue, saturation and lightness, as RGB from 0 to 1. A
-- ramp of saturation under 0.1, grey, shifts its hue half as far.
local function step_of(spec, step)
  local hue = spec[1] + ({ -22, -8, 6, 14 })[step + 1] * (spec[2] < 0.1 and 0.5 or 1)
  local s = clamp(spec[2] + ({ 0.04, 0, 0.02, -0.04 })[step + 1])
  local l = clamp(spec[3] + ({ -0.13, -0.05, 0.04, 0.12 })[step + 1])
  local q = l < 0.5 and l * (1 + s) or l + s - l * s
  local p = 2 * l - q
  local h = (hue % 360) / 360
  local function channel(t)
    t = t % 1
    if t < 1 / 6 then return p + (q - p) * 6 * t end
    if t < 0.5 then return q end
    if t < 2 / 3 then return p + (q - p) * (2 / 3 - t) * 6 end
    return p
  end
  return channel(h + 1 / 3), channel(h), channel(h - 1 / 3)
end

local function byte(v) return math.floor(v * 255 + 0.5) end

-- Ramps are drawn in RGB: they are hue-shifted, outside the master palette.
local ramps = Sprite(4, 1, ColorMode.RGB)
for n, kind in ipairs(KINDS) do
  if n > 1 then ramps:newEmptyFrame() end
  local image = Image(4, 1, ColorMode.RGB)
  for step = 0, 3 do
    local r, g, b = step_of(kind[2], step)
    image:drawPixel(step, 0, Color { r = byte(r), g = byte(g), b = byte(b), a = 255 })
  end
  if n == 1 then ramps:deleteCel(ramps.layers[1], 1) end
  ramps:newCel(ramps.layers[1], n, image, Point(0, 0))
  local tag = ramps:newTag(n, n)
  tag.name = kind[1]
end
ramps:saveAs(here .. "/ramps.aseprite")
ramps:close()

-- Detail, each mark as { dx, dy, colour } from its root at (2, 3); a step of the ground's ramp is
-- the ink ramp's colour of that step.
local function step(n) return c("ink", n + 1) end
local marks = {
  { "tuft", { { 0, 0, step(0) }, { -1, -1, step(0) }, { 1, -1, step(0) }, { 0, -1, step(3) } } },
  { "flower", { { 0, 0, c("crimson", 4) }, { 0, 1, step(0) } },
    { { 0, 0, c("amber", 5) }, { 0, 1, step(0) } },
    { { 0, 0, c("robe", 3) }, { 0, 1, step(0) } } },
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
