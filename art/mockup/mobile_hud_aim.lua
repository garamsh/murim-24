-- Draws mobile_hud_aim.aseprite: the touch HUD while the palm is dragged to aim, at the native
-- 480x270. The thumb has dragged the palm's button right of its middle, and its knob sits where
-- the thumb is within the button's dashed pad. On the ground the skill's range is a dashed ring
-- round the player, a dotted line runs out to where the drag aims, and the palm's area lies
-- there in jade, among the raiders on the road. Releasing casts it there; the cancel zone has
-- appeared on the arc's upper arm.
-- Run: aseprite --batch --script art/mockup/mobile_hud_aim.lua (after target_screen.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local ui = dofile(here .. "/panel.lua")(px, bdf)
local touch = dofile(here .. "/touch.lua")(px, bdf, ui, here)
local c = px.c

touch.font()
local world, old = touch.world(here .. "/target_screen.aseprite")
local hud = px.canvas(touch.W, touch.H)
local P = touch.PLAYER

-- The drag, and where it aims.
local ax, ay, dx, dy = touch.aim(26, -4)

touch.ground_ring(hud, P.x, P.y, touch.RANGE, c("paper", 3), true)
touch.dots(hud, P.x, P.y, ax, ay, 5, c("jade", 3))
touch.ground_ring(hud, ax, ay, 24, c("jade", 3), false, c("jade", 2))
touch.ground_ring(hud, ax, ay, 22, c("jade", 4))
touch.hud(hud, world, old, nil, 2)
touch.pad(hud, world, 2, dx, dy, "jade")
touch.cancel(hud, false)

touch.save(world, hud, "mobile_hud_aim")
