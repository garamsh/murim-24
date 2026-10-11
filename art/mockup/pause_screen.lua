-- Draws pause_screen.aseprite: the pause panel open over the target screen, at the native
-- 480x270. A menu on the left resumes, opens the settings or leaves for the title or the desktop,
-- over the seed this world was opened from; the controls guide on the right shows each key beside
-- what it does. Text is Korean first, in Galmuri9 (hud/fonts/Galmuri9.bdf), with the HUD's frame
-- from panel.lua.
-- Run: aseprite --batch --script art/mockup/pause_screen.lua (after target_screen.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local ui = dofile(here .. "/panel.lua")(px, bdf)
local c = px.c

local W, H = 480, 270

local words = {
  title = "일시정지", close = "Esc 계속", resume = "계속하기", settings = "설정", to_title = "타이틀로",
  quit = "게임 종료", seed = "시드", value = "4817203365", controls = "조작법",
  keys = "↑↓ 고르기 · Enter 선택 · Esc 계속",
}
-- The controls guide: the keys of each action, then what it does.
local controls = {
  { { "W", "A", "S", "D" }, "이동" },
  { { "좌클릭" }, "그곳으로 이동" },
  { { "1-6" }, "기술" },
  { { "C" }, "수장에게 도전" },
  { { "G" }, "족보" },
  { { "M" }, "문파" },
  { { "F2" }, "언어" },
  { { "Esc" }, "일시정지" },
}
local texts = {}
for _, w in pairs(words) do texts[#texts + 1] = w end
for _, k in ipairs(controls) do
  for _, key in ipairs(k[1]) do texts[#texts + 1] = key end
  texts[#texts + 1] = k[2]
end
local font = bdf.load(here .. "/../../hud/fonts/Galmuri9.bdf", texts)

local background = ui.backdrop(here .. "/target_screen.aseprite", W, H)
local panel = px.canvas(W, H)

local PX, PY, PW, PH = 80, 34, 320, 202
ui.dialog(panel, font, PX, PY, PW, PH, words.title, words.close, words.keys)

local MW = 120
ui.menu(panel, font, PX + 10, PY + 32, MW, {
  { words.resume, chosen = true },
  { words.settings },
  { words.to_title },
  { words.quit },
})
-- The seed, to open this world again.
local seed_y = PY + PH - 40
panel:hline(PX + 10, seed_y - 5, MW, c("ink", 3))
bdf.text(panel, font, PX + 12, seed_y, words.seed, c("gold", 3))
bdf.text(panel, font, PX + 10 + MW - 4 - bdf.width(font, words.value), seed_y, words.value, c("stone", 4))

local SX = PX + 10 + MW + 10
panel:vline(SX, PY + 26, PH - 52, c("ink", 3))

-- The controls guide, a row a control: its keys' caps, then what it does.
local GX = SX + 10
bdf.text(panel, font, GX, PY + 26, words.controls, c("gold", 3))
panel:hline(GX, PY + 39, PX + PW - 10 - GX, c("ink", 3))
for n, k in ipairs(controls) do
  local y = PY + 44 + (n - 1) * 16
  local x = GX
  for _, key in ipairs(k[1]) do x = x + ui.keycap(panel, font, x, y, key) + 2 end
  bdf.text(panel, font, GX + 64, y, k[2], c("paper", 3))
end

px.save(W, H, { { "game, dimmed", background }, { "pause panel", panel } }, here .. "/pause_screen.aseprite")
