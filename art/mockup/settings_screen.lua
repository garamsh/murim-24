-- Draws settings_screen.aseprite: the settings panel open over the target screen, at the native
-- 480x270, as from the pause panel. Each setting is a row of its choices, the chosen one lit in
-- jade: the speed of the world's time, the window mode, the resolution in whole multiples of the
-- native size, the language, and the volume of all sound, of the effects and of the music in
-- quarters. The row being changed is lit, and a line under the rows says what it sets. Text is Korean first, in Galmuri9 (hud/fonts/Galmuri9.bdf), with the HUD's frame
-- from panel.lua.
-- Run: aseprite --batch --script art/mockup/settings_screen.lua (after target_screen.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local ui = dofile(here .. "/panel.lua")(px, bdf)
local c = px.c

local W, H = 480, 270

local words = {
  title = "설정", close = "Esc 뒤로", about = "세상의 시간이 흐르는 빠르기. 일시정지 중에는 멈춘다.",
  keys = "↑↓ 고르기 · ←→ 바꾸기 · Esc 뒤로",
}
-- Each setting: its label, its choices, which is chosen and, for a volume, its slips' width. The
-- first row is being changed.
local volumes = { "0%", "25%", "50%", "75%", "100%" }
local settings = {
  { "시간 속도", { "1배", "2배", "4배" }, 1 },
  { "화면 모드", { "창", "전체 화면" }, 1 },
  { "해상도", { "960x540", "1440x810", "1920x1080" }, 2 },
  { "언어", { "한국어", "English" }, 1 },
  { "전체 음량", volumes, 5, 40 },
  { "효과음", volumes, 5, 40 },
  { "음악", volumes, 4, 40 },
}
local texts = {}
for _, w in pairs(words) do texts[#texts + 1] = w end
for _, s in ipairs(settings) do
  texts[#texts + 1] = s[1]
  for _, choice in ipairs(s[2]) do texts[#texts + 1] = choice end
end
local font = bdf.load(here .. "/../../hud/fonts/Galmuri9.bdf", texts)

local background = ui.backdrop(here .. "/target_screen.aseprite", W, H)
local panel = px.canvas(W, H)

local PX, PY, PW, PH = 60, 10, 360, 250
ui.dialog(panel, font, PX, PY, PW, PH, words.title, words.close, words.keys)

-- The rows, ruled like the sect panel's figures; the choices sit right-aligned on slips.
local RX, RW, CHOICE_W = PX + 10, PW - 20, 64
for n, s in ipairs(settings) do
  local y = PY + 30 + (n - 1) * 26
  if n > 1 then panel:hline(RX, y - 5, RW, c("ink", 3)) end
  if n == 1 then
    panel:rect(RX, y - 4, RW, 22, c("ink", 1))
    panel:vline(RX, y - 4, 22, c("jade", 3))
  end
  bdf.text(panel, font, RX + 8, y + 3, s[1], n == 1 and c("gold", 4) or c("gold", 3))
  local slip = s[4] or CHOICE_W
  local x = RX + RW - 4 - #s[2] * (slip + 4) + 4
  for k, choice in ipairs(s[2]) do
    ui.slip(panel, font, x, y + 2, slip, choice, k == s[3])
    x = x + slip + 4
  end
end

-- What the row being changed sets.
local about_y = PY + 30 + #settings * 26
panel:hline(RX, about_y - 5, RW, c("ink", 3))
bdf.text(panel, font, RX + 8, about_y + 2, words.about, c("stone", 4))

px.save(W, H, { { "game, dimmed", background }, { "settings panel", panel } }, here .. "/settings_screen.aseprite")
