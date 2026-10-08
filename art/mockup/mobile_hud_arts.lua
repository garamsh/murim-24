-- Draws mobile_hud_arts.aseprite: mobile_hud_idle with the arts wired in, at the native
-- 480x270. The status shows the qi left against the store and the realm by name, a fourth
-- button beside the genealogy, sect and duel buttons opens the arts panel, and the arc holds the
-- loadout arts_panel.png shows on Q W E R T, its fourth button empty.
-- Run: aseprite --batch --script art/mockup/mobile_hud_arts.lua (after target_screen.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local ui = dofile(here .. "/panel.lua")(px, bdf)
local touch = dofile(here .. "/touch.lua")(px, bdf, ui, here)
local arts = dofile(here .. "/arts.lua")(px, bdf, ui, here)
local c = px.c

-- The status's qi and realm, each with the room the status gives a value. The bar's share,
-- 0.55, is the qi left over the store.
touch.words.qi_value = { "3,520 / 6,400", "3,520 / 6,400", 90 }
touch.words.art = { "경지", "Realm", 40 }
touch.words.art_value = { arts.REALMS[arts.RANK][1], arts.REALMS[arts.RANK][2], 90 }
arts.font()
local font = touch.font({ arts.words.menu[1], arts.words.menu[2] })

local world, old = touch.world(here .. "/target_screen.aseprite")
local hud = px.canvas(touch.W, touch.H)
touch.panels(hud, old)
touch.stick(hud, world, 0, 0)

-- The arts button: a jian upright over the word.
local ARTS = { 142, 92, 42, 42 }
ui.frame(hud, ARTS[1], ARTS[2], ARTS[3], ARTS[4])
hud:sprite(ARTS[1] + 16, ARTS[2] + 6, {
  "...#...", "...#...", "...#...", "...#...", "...#...", "...#...", ".#####.", "...l...", "...l...",
}, { ["#"] = c("paper", 3), l = c("gold", 3) })
local t = arts.words.menu[1]
assert(bdf.width(font, arts.words.menu[2]) <= ARTS[3] - 6, "the arts button's word overruns it")
bdf.text(hud, font, ARTS[1] + (ARTS[3] - bdf.width(font, t)) // 2, ARTS[2] + 24, t, c("paper", 3))

-- The arc, holding the loadout.
for n, s in ipairs(touch.SKILLS) do
  touch.disc(hud, s.x, s.y, s.r)
  local ref = arts.LOADOUT[n]
  if ref then
    arts.icon(hud, arts.skill(ref)[5], s.x - 8, s.y - 8)
  else
    hud:hline(s.x - 4, s.y, 9, c("ink", 4)); hud:vline(s.x, s.y - 4, 9, c("ink", 4))
  end
end

touch.save(world, hud, "mobile_hud_arts")
