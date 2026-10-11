-- Draws title_screen.aseprite: the title screen at the native 480x270. Over the backdrop of
-- art/title/backdrop.aseprite, its mountains and working title, a line and a menu sit on the left:
-- a new game, a continue that waits for saving, settings and quit. Text is Korean first, in
-- Galmuri9 (hud/fonts/Galmuri9.bdf), with the HUD's frame from panel.lua.
-- Run: aseprite --batch --script art/mockup/title_screen.lua (after art/title/backdrop.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local ui = dofile(here .. "/panel.lua")(px, bdf)
local c = px.c

local W, H = 480, 270

local words = {
  line = "세상은 당신 없이도 흘러간다",
  new = "새 게임", continue = "이어하기", later = "준비 중", settings = "설정", quit = "종료",
  keys = "↑↓ 고르기 · Enter 선택",
}
local texts = {}
for _, w in pairs(words) do texts[#texts + 1] = w end
local font = bdf.load(here .. "/../../hud/fonts/Galmuri9.bdf", texts)

-- The mountains and the working title, from the production backdrop the game draws too.
local backdrop = px.load_frames(here .. "/frozen/title/backdrop.aseprite")[1]

local menu = px.canvas(W, H)

-- The line under the title's romanised name, where art/title/backdrop.lua sets it.
local TX = 40
bdf.text(menu, font, TX, 110, words.line, c("paper", 2), c("ink", 1))

-- The menu, in the HUD's frame.
local MX, MY, MW, MH = TX, 150, 136, 86
ui.frame(menu, MX, MY, MW, MH, c("ink", 1))
ui.menu(menu, font, MX + 6, MY + 10, MW - 12, {
  { words.new, chosen = true },
  { words.continue, note = words.later },
  { words.settings },
  { words.quit },
})

-- The keys, along the foot of the screen.
menu:rect(0, H - 16, W, 16, c("ink", 1))
menu:hline(0, H - 17, W, c("gold", 2))
local keys_w = bdf.width(font, words.keys)
bdf.text(menu, font, (W - keys_w) // 2, H - 15, words.keys, c("stone", 4))

px.save(W, H, { { "backdrop", backdrop }, { "line and menu", menu } }, here .. "/title_screen.aseprite")
