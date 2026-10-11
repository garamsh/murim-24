-- Draws arts_panel.aseprite: the arts panel open over the target screen on PC, at the native
-- 480x270, with the data ADR 0035 gives a person. A column on the left holds the player's realm
-- on the ladder of eight, the qi left against the store, the heart method and its nature, and
-- what a greyed row means. The list on the right holds every art the player knows a skill of and
-- its faction's skill, each skill with its qi cost, the realm it needs and its cooldown, and the
-- key it is on. Along the foot, the five slots on Q W E R T, the Black Fang Dart just put on T.
-- Text is Korean first, in Galmuri9 (hud/fonts/Galmuri9.bdf).
-- Run: aseprite --batch --script art/mockup/arts_panel.lua (after target_screen.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local ui = dofile(here .. "/panel.lua")(px, bdf)
local arts = dofile(here .. "/arts.lua")(px, bdf, ui, here)
local c = px.c
local words = arts.words

local W, H = 480, 270
local font = arts.font()

local background = ui.backdrop(here .. "/target_screen.aseprite", W, H)
local panel = px.canvas(W, H)

local PX, PY, PW, PH = 8, 6, 464, 258
ui.dialog(panel, font, PX, PY, PW, PH, words.title[1], words.close[1], words.keys[1])

-- The left column: realm, qi, the heart method, and what the list's greys mean.
local CX, CW = PX + 10, 102
local y = arts.status(panel, CX, PY + 26, CW)
panel:hline(CX, y + 4, CW, c("ink", 3))
local heart = arts.ARTS[1]
arts.say(panel, "heart", CX, y + 8, c("gold", 3))
bdf.text(panel, font, CX, y + 22, heart.name[1], c("paper", 3))
local tx = CX + arts.tag(panel, CX, y + 37, heart.nature[1], heart.ramp) + 3
arts.tag(panel, tx, y + 37, heart.polarity[1], heart.ramp)
panel:hline(CX, y + 54, CW, c("ink", 3))
panel:rect(CX, y + 62, 6, 6, c("stone", 3))
arts.say(panel, "unlearned", CX + 10, y + 58, c("stone", 4))
panel:rect(CX, y + 76, 6, 6, c("crimson", 4))
arts.say(panel, "locked", CX + 10, y + 72, c("stone", 4))

local DIV = CX + CW + 6
panel:vline(DIV, PY + 24, 160, c("ink", 3))

-- The list: a header of its columns, then each art and its skills. Columns are right-aligned to
-- the edges below, each wide enough for the longer language.
local LX, LR = DIV + 6, PX + PW - 10
local KEY, COOL, REALM, COST = LR - 11, LR - 16, LR - 60, LR - 142
local NAME = LX + 10
local ROW = 14
local ly = PY + 24
arts.say(panel, "col_skill", NAME, ly, c("stone", 4))
arts.say_right(panel, "col_qi", COST, ly, c("stone", 4))
arts.say_right(panel, "col_realm", REALM, ly, c("stone", 4))
arts.say_right(panel, "col_cooldown", COOL, ly, c("stone", 4))
panel:hline(LX, ly + 13, LR - LX, c("ink", 3))

-- Which slot each skill is on.
local slot_of = {}
for n, ref in ipairs(arts.LOADOUT) do
  if ref then slot_of[ref[1] * 10 + ref[2]] = n end
end

for a, art in ipairs(arts.ARTS) do
  ly = ly + ROW
  -- The art's header: its name, its nature, and whether it is the heart method or weaker.
  local hx = LX + bdf.text(panel, font, LX, ly, art.name[1], art.faction and c("stone", 4) or c("gold", 3)) + 6
  if art.nature then
    hx = hx + bdf.text(panel, font, hx, ly, art.nature[1] .. " · " .. art.polarity[1], c(art.ramp, 4)) + 6
    if art.heart then arts.tag(panel, hx, ly + 1, words.heart_tag[1], "jade") end
    if art.weaker then arts.tag(panel, hx, ly + 1, words.weaker[1], "crimson") end
  end
  for s, skill in ipairs(art.skills) do
    ly = ly + ROW
    local selected = arts.SELECTED[1] == a and arts.SELECTED[2] == s
    if selected then
      panel:rect(LX, ly - 1, LR - LX + 2, ROW, c("ink", 3))
      panel:vline(LX, ly - 1, ROW, c("jade", 3))
      for k = 0, 3 do panel:vline(LX + 3 + k, ly + 3 + k, 7 - 2 * k, c("gold", 4)) end
    end
    local known = skill[6] == "known"
    local name_colour = selected and c("gold", 4) or (known and c("paper", 3) or c("stone", 3))
    local value_colour = known and c("paper", 3) or c("stone", 3)
    bdf.text(panel, font, NAME, ly, skill[1][1], name_colour)
    if art.faction then
      arts.say_right(panel, "none", COST, ly, value_colour)
      arts.say_right(panel, "none", REALM, ly, value_colour)
      arts.say_right(panel, "shared", COOL, ly, value_colour)
    else
      arts.text_right(panel, skill[2], COST, ly, value_colour)
      arts.text_right(panel, arts.REALMS[skill[3]][1], REALM, ly,
        skill[6] == "locked" and c("crimson", 4) or value_colour)
      arts.text_right(panel, skill[4] .. words.seconds[1], COOL, ly, value_colour)
    end
    local slot = slot_of[a * 10 + s]
    if slot then
      local just = slot == arts.JUST
      panel:rect(KEY, ly, 11, 12, just and c("jade", 2) or c("ink", 4))
      panel:rect(KEY, ly, 11, 11, just and c("jade", 3) or c("ink", 3))
      bdf.text(panel, font, KEY + (11 - bdf.width(font, arts.KEYS[slot])) // 2, ly, arts.KEYS[slot], c("paper", 3))
    end
  end
end

-- The loadout: a slot for each of Q W E R T, its key above it, the slot just filled in jade.
local FY = PY + 24 + 11 * ROW + 6
panel:hline(PX + 3, FY - 4, PW - 6, c("ink", 3))
arts.say(panel, "loadout", CX, FY + 14, c("gold", 3))
local TILE, STEP = 30, 52
local sx0 = LX + (LR - LX - (4 * STEP + TILE)) // 2
for n, ref in ipairs(arts.LOADOUT) do
  local sx = sx0 + (n - 1) * STEP
  local kw = bdf.width(font, arts.KEYS[n])
  ui.keycap(panel, font, sx + (TILE - math.max(kw + 6, 13)) // 2, FY, arts.KEYS[n])
  local skill = ref and arts.skill(ref)
  arts.tile(panel, sx, FY + 15, TILE, skill and skill[5], { selected = n == arts.JUST })
  if not ref then
    panel:hline(sx + 11, FY + 30, 9, c("ink", 4)); panel:vline(sx + 15, FY + 26, 9, c("ink", 4))
  end
end

px.save(W, H, { { "game, dimmed", background }, { "arts panel", panel } }, here .. "/arts_panel.aseprite")
