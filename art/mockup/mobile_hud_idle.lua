-- Draws mobile_hud_idle.aseprite: the touch HUD at rest over the target screen's world, at the
-- native 480x270. The status, leader, rumour and map panels sit along the top, out of the thumbs'
-- corners, with the genealogy, sect and duel buttons under the status and the pause button in the
-- top-right corner. The joystick rests at the bottom left; the skill buttons stand in an arc round
-- the main one at the bottom right, the palm cooling down with three seconds left.
-- Run: aseprite --batch --script art/mockup/mobile_hud_idle.lua (after target_screen.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local ui = dofile(here .. "/panel.lua")(px, bdf)
local touch = dofile(here .. "/touch.lua")(px, bdf, ui, here)

touch.font()
local world, old = touch.world(here .. "/target_screen.aseprite")
local hud = px.canvas(touch.W, touch.H)
touch.hud(hud, world, old, { [2] = { cooldown = { 0.6, touch.words.cooldown[1] } } })
touch.save(world, hud, "mobile_hud_idle")
