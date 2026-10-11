-- Draws genealogy_panel.aseprite: the genealogy panel open over the target screen, at the native
-- 480x270. The steered person sits at the centre of a three-generation tree: parents above,
-- the person and their spouse, their children below. A column on the right holds the centred
-- person's details. Text is Korean first, in Galmuri9 (hud/fonts/Galmuri9.bdf); the panel takes
-- the HUD's tone from docs/art-direction.md §Effects and HUD.
-- Run: aseprite --batch --script art/mockup/genealogy_panel.lua (after target_screen.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local panel_ui = dofile(here .. "/panel.lua")(px, bdf)
local c = px.c

local W, H = 480, 270

-- The family shown: names are Korean, as the game generates them; `faction` picks the body
-- sprite; a person with `dead` is drawn greyed.
local people = {
  father = { name = "백운학", age = "52세", faction = "orthodox", dead = true },
  mother = { name = "서연화", age = "49세", faction = "orthodox" },
  self = { name = "백서진", age = "26세", faction = "orthodox" },
  spouse = { name = "하소윤", age = "24세", faction = "unorthodox" },
  first = { name = "백도윤", age = "6세", faction = "orthodox" },
  second = { name = "백하린", age = "4세", faction = "orthodox" },
  third = { name = "백시우", age = "1세", faction = "orthodox" },
}
local words = {
  title = "족보", family = "백씨 가문", close = "G 닫기", parents = "부모", own = "본인",
  children = "자녀", dead = "고인", spouse = "배우자", sect = "정파", age = "나이",
  talent = "천부", lifespan = "수명", unknown = "알 수 없음", count = "3명",
  keys = "방향키 이동 · Enter 중심으로 · G 닫기", generation = "3대",
}
local texts = {}
for _, p in pairs(people) do texts[#texts + 1] = p.name; texts[#texts + 1] = p.age end
for _, w in pairs(words) do texts[#texts + 1] = w end
local font = bdf.load(here .. "/../../hud/fonts/Galmuri9.bdf", texts)

local background = panel_ui.backdrop(here .. "/target_screen.aseprite", W, H)
local panel = px.canvas(W, H)
local frame = panel_ui.frame

local PX, PY, PW, PH = 20, 12, 440, 246
frame(panel, PX, PY, PW, PH)
-- Title bar: the panel's name and the family's, the key that closes it.
panel:rect(PX + 3, PY + 3, PW - 6, 16, c("ink", 1))
panel:hline(PX + 3, PY + 19, PW - 6, c("gold", 2))
bdf.text(panel, font, PX + 8, PY + 4, words.title, c("paper", 3), c("ink", 1))
bdf.text(panel, font, PX + 36, PY + 4, words.family, c("gold", 4), c("ink", 1))
bdf.text(panel, font, PX + 100, PY + 4, words.generation, c("stone", 4))
local close_w = bdf.width(font, words.close)
bdf.text(panel, font, PX + PW - 8 - close_w, PY + 4, words.close, c("stone", 4))

-- Bodies: each faction's idle frame, read back from the production sources.
local bodies = {}
for _, f in ipairs({ "orthodox", "demonic", "unorthodox" }) do
  bodies[f] = px.load_frames(here .. "/../bodies/" .. f .. ".aseprite")[1]
end

local CARD_W, CARD_H = panel_ui.CARD_W, panel_ui.CARD_H
-- A person's card at (x, y); the steered person's card carries a jade frame and a tag over it.
local function card(p, x, y, chosen)
  panel_ui.card(panel, font, x, y, {
    body = bodies[p.faction], name = p.name, line = p.dead and words.dead or p.age, dead = p.dead,
    ramp = chosen and "jade" or nil, tag = chosen and words.own or nil,
  })
end

-- Lines of kin in gold: a marriage is a double line with a knot, descent a single line.
local KIN = c("gold", 2)
local function marriage(x0, x1, y)
  panel:hline(x0, y - 1, x1 - x0, KIN); panel:hline(x0, y + 1, x1 - x0, KIN)
  local mx = (x0 + x1) // 2
  panel:rect(mx - 1, y - 2, 3, 5, c("gold", 4)); panel:set(mx, y, c("lacquer", 3))
  return mx
end
local function descent(x, y0, y1) panel:vline(x, y0, y1 - y0, KIN) end

-- Generations, left labels and cards.
local TX = PX + 52
local ROW = { PY + 34, PY + 100, PY + 170 }
local GAP = 34
for n, label in ipairs({ words.parents, words.own, words.children }) do
  bdf.text(panel, font, PX + 10, ROW[n] + 14, label, c("gold", 3))
  panel:hline(PX + 10, ROW[n] + 28, 26, c("ink", 3))
end
local left, right = TX + 16, TX + 16 + CARD_W + GAP
card(people.father, left, ROW[1])
card(people.mother, right, ROW[1])
local parents_knot = marriage(left + CARD_W, right, ROW[1] + CARD_H // 2)
card(people.self, left, ROW[2], true)
card(people.spouse, right, ROW[2])
local couple_knot = marriage(left + CARD_W, right, ROW[2] + CARD_H // 2)
-- From the parents' knot down to the person's card.
descent(parents_knot, ROW[1] + CARD_H // 2 + 3, ROW[1] + CARD_H + 10)
panel:hline(left + CARD_W // 2, ROW[1] + CARD_H + 10, parents_knot - left - CARD_W // 2 + 1, KIN)
descent(left + CARD_W // 2, ROW[1] + CARD_H + 10, ROW[2] - 5)
-- From the couple's knot down to the children.
local kids = { people.first, people.second, people.third }
local kid_x = {}
local span = 3 * CARD_W + 2 * 12
local first_x = couple_knot - span // 2
local bar = ROW[3] - 10
descent(couple_knot, ROW[2] + CARD_H // 2 + 3, bar)
for n, kid in ipairs(kids) do
  kid_x[n] = first_x + (n - 1) * (CARD_W + 12)
  card(kid, kid_x[n], ROW[3])
  descent(kid_x[n] + CARD_W // 2, bar, ROW[3])
end
panel:hline(kid_x[1] + CARD_W // 2, bar, kid_x[3] - kid_x[1] + 1, KIN)

-- The details of the centred person, in a column of its own.
local CX, CY, CW, CH = PX + PW - 116, PY + 26, 108, 196
frame(panel, CX, CY, CW, CH, c("ink", 1))
-- Their body at twice the size, on its ground.
panel:rect(CX + 8, CY + 8, 36, 44, c("grass", 3))
panel:hline(CX + 8, CY + 8, 36, c("grass", 4))
local self_body = bodies.orthodox
for k, i in pairs(self_body.p) do
  local bx, by = k % self_body.w, k // self_body.w
  panel:rect(CX + 10 + bx * 2, CY + 4 + by * 2, 2, 2, i)
end
bdf.text(panel, font, CX + 50, CY + 10, people.self.name, c("paper", 3), c("ink", 1))
bdf.text(panel, font, CX + 50, CY + 24, words.family, c("gold", 4))
bdf.text(panel, font, CX + 50, CY + 38, words.sect, c("azure", 4))
local rows = {
  { words.age, "26" }, { words.talent, words.unknown }, { words.lifespan, words.unknown },
  { words.spouse, people.spouse.name }, { words.children, words.count },
}
for n, r in ipairs(rows) do
  local y = CY + 62 + (n - 1) * 20
  panel:hline(CX + 6, y - 4, CW - 12, c("ink", 3))
  bdf.text(panel, font, CX + 8, y, r[1], c("gold", 3))
  local w = bdf.width(font, r[2])
  bdf.text(panel, font, CX + CW - 8 - w, y, r[2], r[2] == words.unknown and c("stone", 3) or c("paper", 3))
end

-- The keys, along the panel's foot.
local keys_w = bdf.width(font, words.keys)
panel:hline(PX + 3, PY + PH - 20, PW - 6, c("gold", 2))
bdf.text(panel, font, PX + (PW - keys_w) // 2, PY + PH - 17, words.keys, c("stone", 4))

px.save(W, H, { { "game, dimmed", background }, { "genealogy panel", panel } },
  here .. "/genealogy_panel.aseprite")
