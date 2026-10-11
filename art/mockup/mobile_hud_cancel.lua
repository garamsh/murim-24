-- Draws mobile_hud_cancel.aseprite: the drag of mobile_hud_aim carried onto the cancel zone, at
-- the native 480x270. The zone is lit crimson under the thumb, the palm's pad has turned crimson
-- with no knob in it, the thumb having left it, and the aim on the ground has turned grey:
-- releasing here casts nothing.
-- Run: aseprite --batch --script art/mockup/mobile_hud_cancel.lua (after target_screen.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local ui = dofile(here .. "/panel.lua")(px, bdf)
local touch = dofile(here .. "/touch.lua")(px, bdf, ui, here)
local c = px.c

touch.font()
local world, old = touch.world(here .. "/target_screen.aseprite")
local hud = px.canvas(touch.W, touch.H)
local P = touch.PLAYER

-- The aim is left where the drag last aimed, greyed.
local ax, ay = touch.aim(26, -4)

touch.ground_ring(hud, P.x, P.y, touch.RANGE, c("stone", 3), true)
touch.ground_ring(hud, ax, ay, 24, c("stone", 3), true)
touch.hud(hud, world, old, nil, 2)
touch.pad(hud, world, 2, nil, nil, "crimson")
touch.cancel(hud, true)

touch.save(world, hud, "mobile_hud_cancel")
