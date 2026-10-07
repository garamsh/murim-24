-- Draws mobile_hud_self.aseprite: a long press on the medicine button, which casts it on the
-- player, at the native 480x270. The ring round the button has filled in jade as the press was
-- held, a slip over it says the skill goes to the player, and on the ground a jade ring lies
-- round the player with the medicine's motes rising in it.
-- Run: aseprite --batch --script art/mockup/mobile_hud_self.lua (after target_screen.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local ui = dofile(here .. "/panel.lua")(px, bdf)
local touch = dofile(here .. "/touch.lua")(px, bdf, ui, here)
local c = px.c

touch.font()
local world, old = touch.world(here .. "/target_screen.aseprite")
local hud = px.canvas(touch.W, touch.H)
local P, s = touch.PLAYER, touch.SKILLS[5]

-- The player's ring, and motes of medicine rising round them.
touch.ground_ring(hud, P.x, P.y, 16, c("jade", 3), false, c("jade", 2))
touch.ground_ring(hud, P.x, P.y, 14, c("jade", 4))
for _, m in ipairs({ { -9, -10 }, { 8, -16 }, { -4, -24 }, { 11, -4 }, { -12, -2 } }) do
  local x, y = P.x + m[1], P.y + m[2]
  hud:set(x, y, c("jade", 4)); hud:set(x - 1, y, c("jade", 3)); hud:set(x + 1, y, c("jade", 3))
  hud:set(x, y - 1, c("jade", 3)); hud:set(x, y + 1, c("jade", 3))
end

touch.hud(hud, world, old, nil, 5)
-- The hold's ring round the button, full: the press has become a cast on the player.
hud:each_ellipse(s.x, s.y, s.r + 4.5, s.r + 4.5, function(x, y, d)
  if d > (s.r + 2) / (s.r + 4.5) then hud:set(x, y, d > (s.r + 3.5) / (s.r + 4.5) and c("ink", 1) or c("jade", 4)) end
end)
touch.skill(hud, 5, { rim = "jade" })

-- The slip over the button.
touch.slip(hud, "self", s.x, s.y - s.r - 22, "jade")

touch.save(world, hud, "mobile_hud_self")
