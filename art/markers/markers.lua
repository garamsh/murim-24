-- Draws the two markers that stand outside a body's own sprite:
-- army_marker.aseprite, the aggregate-army marker, one 64x48 frame per faction tagged by the
-- faction's name. The block covers the bottom 4x2 cells and the banner rises above it; the
-- army's name and count are text the HUD draws, not part of the frame.
-- player_marker.aseprite, the player's markers on the body's 16x24 frame: tagged ring (drawn
-- under the body) and chevron (drawn over it, two frames bobbing by a pixel).
-- shadow.aseprite, a 16x8 frame tagged shadow: the dithered ink ellipse on the ground under every
-- body's feet, centred on (8, 4).
-- Run: aseprite --batch --script art/markers/markers.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)
local c = px.c

-- The aggregate army: 13 columns by 4 rows of heads, the banner on the faction's accent ramp.
local armies = { { "orthodox", "azure" }, { "demonic", "crimson" }, { "unorthodox", "amber" } }
local frames, tags = {}, {}
for n, a in ipairs(armies) do
  local cv = px.canvas(64, 48)
  S.army(cv, px.canvas(64, 48), 4, 20, 13, 4, a[1], a[2], 41)
  frames[n] = cv
  tags[n] = { a[1], n, n, 100 }
end
px.save_frames(64, 48, frames, tags, here .. "/army_marker.aseprite")

-- The player: a jade ring round the feet and a gold chevron over the head.
local ring = px.canvas(16, 24)
ring:each_ellipse(8, 21, 7, 2.6, function(x, y, d) if d > 0.68 then ring:set(x, y, c("jade", 3)) end end)
local function chevron(dy)
  local cv = px.canvas(16, 24)
  cv:sprite(6, 2 + dy, { "#####", ".###.", "..#.." }, { ["#"] = c("gold", 4) })
  cv:outline(5, 1 + dy, 7, 5, c("ink", 1))
  return cv
end
px.save_frames(16, 24, { ring, chevron(0), chevron(1) }, { { "ring", 1, 1, 100 }, { "chevron", 2, 3, 400 } },
  here .. "/player_marker.aseprite")

-- Every body's shadow: every other pixel of a flat ellipse, so the ground shows through.
local shadow = px.canvas(16, 8)
shadow:each_ellipse(8, 4, 5.5, 2.2, function(x, y) if (x + y) % 2 == 0 then shadow:set(x, y, c("ink", 1)) end end)
px.save_frames(16, 8, { shadow }, { { "shadow", 1, 1, 100 } }, here .. "/shadow.aseprite")
