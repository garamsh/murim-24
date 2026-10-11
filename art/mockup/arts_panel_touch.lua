-- Draws arts_panel_touch.aseprite: the arts panel open on a phone, at the native 480x270, over
-- the dimmed world with the skill arc left clear to its right. Every place a touch lands is at
-- least the touch HUD's 42 pixels. The panel's left holds each art the player knows a skill of
-- and its faction's skill as tiles of their icons, greyed when not yet learned and underlined in
-- crimson when the realm is not reached; its right holds the realm, the qi, and the tapped
-- skill's nature, qi cost, realm, cooldown, range and area. The Black Fang Dart is tapped, so
-- the arc's buttons are ringed in jade, waiting for the one it goes on; the fourth and fifth are
-- empty. The pause button closes the panel while it is open.
-- Text is Korean first, in Galmuri9 (hud/fonts/Galmuri9.bdf).
-- Run: aseprite --batch --script art/mockup/arts_panel_touch.lua (after target_screen.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local ui = dofile(here .. "/panel.lua")(px, bdf)
local touch = dofile(here .. "/touch.lua")(px, bdf, ui, here)
local arts = dofile(here .. "/arts.lua")(px, bdf, ui, here)
local c = px.c
local words = arts.words

local W, H = touch.W, touch.H
local font = arts.font()

-- The world without the target screen's mouse-and-keyboard HUD, dimmed as ui.backdrop dims it.
local world = touch.world(here .. "/target_screen.aseprite")
local background = px.canvas(W, H)
for k, i in pairs(world.p) do
  local x, y = k % W, k // W
  background.p[k] = (x + y) % 2 == 0 and c("ink", 1) or px.shift(i, -3)
end
local panel = px.canvas(W, H)

local PX, PY, PW, PH = 4, 4, 302, 262
ui.dialog(panel, font, PX, PY, PW, PH, words.title[1], "", words.touch_keys[1])

-- The tiles: a group for each art, its name and whether it is the heart method or weaker over a
-- row of its skills.
local TILE, GAP = 42, 4
local GX = PX + 8
local gy = PY + 22
for a, art in ipairs(arts.ARTS) do
  local hx = GX + bdf.text(panel, font, GX, gy, art.name[1], art.faction and c("stone", 4) or c("gold", 3)) + 4
  if art.heart then arts.tag(panel, hx, gy + 1, words.heart_tag[1], "jade") end
  if art.weaker then arts.tag(panel, hx, gy + 1, words.weaker[1], "crimson") end
  for s, skill in ipairs(art.skills) do
    local tx, ty = GX + (s - 1) * (TILE + GAP), gy + 14
    local selected = arts.SELECTED[1] == a and arts.SELECTED[2] == s
    local on_arc = false
    for n = 1, 3 do
      local ref = arts.LOADOUT[n]
      if ref and ref[1] == a and ref[2] == s then on_arc = true end
    end
    arts.tile(panel, tx, ty, TILE, skill[5], { selected = selected, lit = on_arc, dim = skill[6] ~= "known" })
    if skill[6] == "locked" then panel:hline(tx + 8, ty + TILE - 6, TILE - 16, c("crimson", 4)) end
  end
  gy = gy + 14 + TILE + 6
end

-- The right column: the realm and qi, then the tapped skill.
local DIV = GX + 3 * TILE + 2 * GAP + 6
panel:vline(DIV, PY + 22, PH - 44, c("ink", 3))
local DX, DW = DIV + 6, PX + PW - 8 - (DIV + 6)
local y = arts.status(panel, DX, PY + 22, DW)
panel:hline(DX, y + 3, DW, c("ink", 3))

local art = arts.ARTS[arts.SELECTED[1]]
local skill = arts.skill(arts.SELECTED)
y = y + 7
bdf.text(panel, font, DX, y, skill[1][1], c("gold", 4))
bdf.text(panel, font, DX, y + 13, art.name[1], c("paper", 3))
local tx = DX + arts.tag(panel, DX, y + 28, art.nature[1], art.ramp) + 3
tx = tx + arts.tag(panel, tx, y + 28, art.polarity[1], art.ramp) + 3
arts.tag(panel, tx, y + 28, words.weaker[1], "crimson")
y = y + 44
local rows = {
  { words.col_qi[1], skill[2] },
  { words.col_realm[1], arts.REALMS[skill[3]][1] },
  { words.col_cooldown[1], skill[4] .. words.seconds[1] },
  { words.range[1], words.range_value[1] },
  { words.area[1], words.area_value[1] },
}
for n, r in ipairs(rows) do
  local ry = y + (n - 1) * 13
  bdf.text(panel, font, DX, ry, r[1], c("stone", 4))
  arts.text_right(panel, r[2], DX + DW, ry, c("paper", 3))
end

-- The arc: the loadout's first three skills on their buttons, the last two empty, every rim in
-- jade while a skill waits to be placed, under a slip saying so.
local loadout = { arts.LOADOUT[1], arts.LOADOUT[2], arts.LOADOUT[3], false, false }
for n, s in ipairs(touch.SKILLS) do
  touch.disc(panel, s.x, s.y, s.r, "jade")
  local ref = loadout[n]
  if ref then
    arts.icon(panel, arts.skill(ref)[5], s.x - 8, s.y - 8)
  else
    panel:hline(s.x - 4, s.y, 9, c("jade", 2)); panel:vline(s.x, s.y - 4, 9, c("jade", 2))
  end
end
local slip = words.tap_slot[1]
local sw = bdf.width(font, slip) + 8
local SX, SY = 404 - sw // 2, 136
panel:rect(SX, SY, sw, 14, c("ink", 1)); panel:hline(SX, SY, sw, c("jade", 3))
bdf.text(panel, font, SX + 4, SY + 1, slip, c("paper", 3))

-- The pause button's place closes the panel.
local p = touch.PAUSE
ui.frame(panel, p[1], p[2], p[3], p[4])
for k = -6, 6 do
  panel:rect(p[1] + 20 + k, p[2] + 20 + k, 2, 2, c("paper", 3))
  panel:rect(p[1] + 20 + k, p[2] + 20 - k, 2, 2, c("paper", 3))
end

px.save(W, H, { { "game, dimmed", background }, { "arts panel", panel } }, here .. "/arts_panel_touch.aseprite")
