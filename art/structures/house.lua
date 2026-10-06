-- Draws house.aseprite: the village house over a 3x3-cell footprint, one frame per damage
-- stage, tagged intact, damaged, destroyed and rubble. A frame is 64x60: the footprint is the
-- 48x48 at (8, 8), and the margin holds the upturned eaves and the debris that spill past it.
-- The cast shadow is not in the frame; the view draws it on the ground.
-- Run: aseprite --batch --script art/structures/house.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)

local W, H = 64, 60
local frames, tags = {}, {}
for n, stage in ipairs({ "intact", "damaged", "destroyed", "rubble" }) do
  local cv = px.canvas(W, H)
  -- The shadow and scorch S.house puts on the ground go to a canvas that is not saved.
  S.house(cv, px.canvas(W, H), 8, 8, stage, 100 + n)
  frames[n] = cv
  tags[n] = { stage, n, n, 100 }
end
px.save_frames(W, H, frames, tags, here .. "/house.aseprite")
