-- Draws new_game_screen.aseprite: the new-game panel open over the title screen, at the native
-- 480x270. The seed is typed into its field or drawn at random, under a line that says the same
-- seed opens the same world, above the button that starts it. Text is Korean first, in Galmuri9
-- (hud/fonts/Galmuri9.bdf), with the HUD's frame from panel.lua.
-- Run: aseprite --batch --script art/mockup/new_game_screen.lua (after title_screen.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local ui = dofile(here .. "/panel.lua")(px, bdf)
local c = px.c

local W, H = 480, 270

local words = {
  title = "새 게임", close = "Esc 뒤로", seed = "시드", value = "4817203365", random = "R 무작위",
  same = "같은 시드에서는 같은 세상이 열린다.", empty = "비워 두면 무작위로 정한다.",
  start = "Enter 시작", keys = "숫자 입력 · R 무작위 · Enter 시작 · Esc 뒤로",
}
local texts = {}
for _, w in pairs(words) do texts[#texts + 1] = w end
local font = bdf.load(here .. "/../../hud/fonts/Galmuri9.bdf", texts)

local background = ui.backdrop(here .. "/title_screen.aseprite", W, H)
local panel = px.canvas(W, H)

local PX, PY, PW, PH = 100, 66, 280, 138
ui.dialog(panel, font, PX, PY, PW, PH, words.title, words.close, words.keys)

-- The seed: its label, its field with the caret after what was typed, and the random draw.
local y = PY + 32
bdf.text(panel, font, PX + 12, y + 2, words.seed, c("gold", 3))
local FX, FW = PX + 44, 138
panel:rect(FX, y, FW, 16, c("jade", 3))
panel:rect(FX + 1, y + 1, FW - 2, 14, c("ink", 1))
local typed = bdf.text(panel, font, FX + 5, y + 2, words.value, c("paper", 3))
panel:vline(FX + 6 + typed, y + 3, 10, c("paper", 3))
local RW = PX + PW - 12 - (FX + FW + 8)
ui.slip(panel, font, FX + FW + 8, y + 1, RW, words.random, false)

bdf.text(panel, font, PX + 12, y + 26, words.same, c("stone", 4))
bdf.text(panel, font, PX + 12, y + 40, words.empty, c("stone", 3))

-- The button that starts the world.
panel:hline(PX + 6, y + 58, PW - 12, c("ink", 3))
local SW = 96
ui.slip(panel, font, PX + (PW - SW) // 2, y + 66, SW, words.start, true)

px.save(W, H, { { "title, dimmed", background }, { "new game panel", panel } }, here .. "/new_game_screen.aseprite")
