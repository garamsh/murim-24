-- Draws sect_panel.aseprite: the sect panel open over the target screen, at the native 480x270,
-- with the data ADR 0026 derives. The steered person's sect fills the left: its seat village's
-- gate and whether the seat is held, its faction, master, elders and members against its cap, and
-- the cards of its master and elders. A column on the right holds a faction: its leader, how the
-- leader is chosen, and its sects. Text is Korean first, in Galmuri9 (hud/fonts/Galmuri9.bdf).
-- Run: aseprite --batch --script art/mockup/sect_panel.lua (after target_screen.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local ui = dofile(here .. "/panel.lua")(px, bdf)
local c = px.c

local W, H = 480, 270

-- The sect shown: the steered person's (백서진, of the genealogy mockup). Its master leads the
-- Orthodox, as the master of the sect with the most elders.
local master = { name = "백운천", age = "61세" }
local elders = { { name = "청허", age = "58세" }, { name = "남궁현", age = "47세" }, { name = "서연화", age = "49세" } }
local members, cap = 18, 24
-- The faction's sects: members and cap, or that the seat has fallen or the sect itself.
local sects = {
  { name = "화산파", count = "18/24", chosen = true },
  { name = "무당파", count = "12/20" },
  { name = "아미파", count = "9/16" },
  { name = "점창파", state = "seat" },
  { name = "종남파", state = "fallen" },
}
local words = {
  title = "문파", sect = "화산파", faction = "정파", own = "본인 소속", close = "M 닫기",
  seat = "본산", held = "보유", lost = "함락", fallen = "멸문", village = "화음촌",
  faction_label = "세력", master = "장문인", elders = "장로", members = "문도", count = "3명",
  ranks = "장문인 · 장로", leader = "수장", election = "선출", sects = "문파",
  tabs = { "정파", "사파", "마교" }, members_count = members .. " / " .. cap,
  keys = "방향키 문파 고르기 · Tab 세력 · M 닫기",
}
local texts = { master.name, master.age }
for _, e in ipairs(elders) do texts[#texts + 1] = e.name; texts[#texts + 1] = e.age end
for _, s in ipairs(sects) do texts[#texts + 1] = s.name; texts[#texts + 1] = s.count or "" end
for _, w in pairs(words) do
  if type(w) == "table" then for _, t in ipairs(w) do texts[#texts + 1] = t end else texts[#texts + 1] = w end
end
local font = bdf.load(here .. "/../../hud/fonts/Galmuri9.bdf", texts)

local background = ui.backdrop(here .. "/target_screen.aseprite", W, H)
local panel = px.canvas(W, H)
local body = px.load_frames(here .. "/../bodies/orthodox.aseprite")[1]

-- A word on a filled slip, its text in paper; returns the slip's width.
local function tag(x, y, text, ramp)
  local w = bdf.width(font, text) + 6
  panel:rect(x, y, w, 11, c(ramp, 2))
  bdf.text(panel, font, x + 3, y, text, c("paper", 3))
  return w
end

local PX, PY, PW, PH = 20, 12, 440, 246
ui.frame(panel, PX, PY, PW, PH)
-- Title bar: the panel's name, the sect's, its faction, that the steered person belongs to it.
panel:rect(PX + 3, PY + 3, PW - 6, 16, c("ink", 1))
panel:hline(PX + 3, PY + 19, PW - 6, c("gold", 2))
local tx = PX + 8
tx = tx + bdf.text(panel, font, tx, PY + 4, words.title, c("paper", 3), c("ink", 1)) + 10
tx = tx + bdf.text(panel, font, tx, PY + 4, words.sect, c("gold", 4), c("ink", 1)) + 8
tx = tx + bdf.text(panel, font, tx, PY + 4, words.faction, c("azure", 4)) + 10
tag(tx, PY + 5, words.own, "jade")
local close_w = bdf.width(font, words.close)
bdf.text(panel, font, PX + PW - 8 - close_w, PY + 4, words.close, c("stone", 4))

-- The seat: its village's gate on its ground, the village's name and whether the seat is held.
local LX, TOP = PX + 10, PY + 28
local SW, SH = 112, 100
ui.frame(panel, LX, TOP, SW, SH, c("ink", 1))
panel:rect(LX + 6, TOP + 6, SW - 12, 70, c("grass", 3))
panel:hline(LX + 6, TOP + 6, SW - 12, c("grass", 4))
for x = LX + 6, LX + SW - 7 do
  for y = TOP + 7, TOP + 75 do
    if px.hash(x, y, 26) < 0.08 then panel:set(x, y, c("grass", 4)) end
  end
end
local gate = px.load_frames(here .. "/../structures/seat_gate.aseprite")[1]
for k, i in pairs(gate.p) do
  local gx, gy = k % gate.w, k // gate.w
  panel:set(LX + (SW - gate.w) // 2 + gx, TOP + 6 + gy, i)
end
bdf.text(panel, font, LX + 8, TOP + 80, words.seat, c("gold", 3))
bdf.text(panel, font, LX + 34, TOP + 80, words.village, c("paper", 3))
local held_w = bdf.width(font, words.held) + 6
tag(LX + SW - 8 - held_w, TOP + 81, words.held, "jade")

-- The sect's figures, in rows ruled like the genealogy panel's details.
local RX, RW = LX + SW + 12, 176
local rows = {
  { words.faction_label, words.faction, c("azure", 4) },
  { words.master, master.name, c("paper", 3) },
  { words.elders, words.count, c("paper", 3) },
  { words.members, words.members_count, c("paper", 3) },
}
for n, r in ipairs(rows) do
  local y = TOP + 4 + (n - 1) * 20
  if n > 1 then panel:hline(RX, y - 4, RW, c("ink", 3)) end
  bdf.text(panel, font, RX + 2, y, r[1], c("gold", 3))
  local w = bdf.width(font, r[2])
  bdf.text(panel, font, RX + RW - 2 - w, y, r[2], r[3])
end
-- Members against the cap, one pip a place: jade for a member, ink for a place still open.
local PIP = 7
local pips_x = RX + (RW - cap * PIP + 2) // 2
for n = 0, cap - 1 do
  local x, y = pips_x + n * PIP, TOP + 86
  if n < members then
    panel:rect(x, y, 5, 8, c("jade", 2)); panel:hline(x, y, 5, c("jade", 4)); panel:vline(x, y + 1, 7, c("jade", 3))
  else
    panel:rect(x, y, 5, 8, c("ink", 3)); panel:hline(x, y + 7, 5, c("ink", 4))
  end
end

-- The master's and elders' cards, the master's framed in gold with its tag clear of its name.
local CY = TOP + SH + 10
bdf.text(panel, font, LX, CY, words.ranks, c("gold", 3))
panel:hline(LX, CY + 13, RX + RW - LX, c("ink", 3))
local GAP = (RX + RW - LX - 4 * ui.CARD_W) // 3
local cards = { { p = master, ramp = "gold" } }
for _, e in ipairs(elders) do cards[#cards + 1] = { p = e } end
for n, k in ipairs(cards) do
  ui.card(panel, font, LX + (n - 1) * (ui.CARD_W + GAP), CY + 26, {
    body = body, name = k.p.name, line = k.p.age, ramp = k.ramp,
  })
end
local master_w = bdf.width(font, words.master) + 6
tag(LX + ui.CARD_W - master_w - 3, CY + 17, words.master, "gold")

-- The faction: a tab for each, its leader and how the leader is chosen, and its sects.
local FX, FY, FW, FH = PX + PW - 116, PY + 26, 108, 196
ui.frame(panel, FX, FY, FW, FH, c("ink", 1))
local tab_x = FX + 6
for n, t in ipairs(words.tabs) do
  local w = bdf.width(font, t) + 8
  if n == 1 then
    panel:rect(tab_x, FY + 6, w, 14, c("jade", 2)); panel:hline(tab_x, FY + 6, w, c("jade", 3))
  else
    panel:rect(tab_x, FY + 6, w, 14, c("ink", 3))
  end
  bdf.text(panel, font, tab_x + 4, FY + 7, t, n == 1 and c("paper", 3) or c("stone", 4))
  tab_x = tab_x + w + 4
end
panel:hline(FX + 6, FY + 20, FW - 12, c("jade", 2))

bdf.text(panel, font, FX + 8, FY + 28, words.leader, c("gold", 3))
local elect_w = bdf.width(font, words.election) + 6
tag(FX + FW - 8 - elect_w, FY + 29, words.election, "azure")
panel:rect(FX + 8, FY + 44, 16, 16, c("grass", 3))
panel:hline(FX + 8, FY + 44, 16, c("grass", 4))
for k, i in pairs(body.p) do
  local bx, by = k % body.w, k // body.w
  panel:set(FX + 8 + bx, FY + 36 + by, i)
end
bdf.text(panel, font, FX + 30, FY + 46, master.name, c("paper", 3))
bdf.text(panel, font, FX + 30, FY + 58, words.sect, c("stone", 4))

local LY = FY + 80
panel:hline(FX + 6, LY - 4, FW - 12, c("ink", 3))
bdf.text(panel, font, FX + 8, LY, words.sects, c("gold", 3))
for n, s in ipairs(sects) do
  local y = LY + 4 + n * 18
  panel:hline(FX + 6, y - 4, FW - 12, c("ink", 3))
  if s.chosen then
    panel:rect(FX + 4, y - 3, FW - 8, 16, c("ink", 2))
    panel:vline(FX + 4, y - 3, 16, c("jade", 3))
  end
  local name_colour = s.state == "fallen" and c("stone", 3) or (s.chosen and c("gold", 4) or c("paper", 3))
  bdf.text(panel, font, FX + 10, y, s.name, name_colour)
  local right, colour = s.count, c("stone", 4)
  if s.state == "seat" then right, colour = words.lost, c("crimson", 4)
  elseif s.state == "fallen" then right, colour = words.fallen, c("stone", 3) end
  local w = bdf.width(font, right)
  bdf.text(panel, font, FX + FW - 8 - w, y, right, colour)
end

-- The keys, along the panel's foot.
local keys_w = bdf.width(font, words.keys)
panel:hline(PX + 3, PY + PH - 20, PW - 6, c("gold", 2))
bdf.text(panel, font, PX + (PW - keys_w) // 2, PY + PH - 17, words.keys, c("stone", 4))

px.save(W, H, { { "game, dimmed", background }, { "sect panel", panel } }, here .. "/sect_panel.aseprite")
