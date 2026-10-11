-- Draws mobile_hud_targets.aseprite: mobile_hud_idle with every place a touch lands outlined
-- and its size in native pixels beside it, over the world dimmed, at the native 480x270. The
-- skill buttons, the cancel zone and the menu and pause buttons take a touch inside their dashed
-- outlines, the joystick anywhere in its dashed zone; a touch anywhere else on the world taps
-- it. A legend gives the size of a native pixel on a phone at the 4x export's scale. It is for
-- review, not a screen of the game.
-- Run: aseprite --batch --script art/mockup/mobile_hud_targets.lua (after target_screen.lua)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/pixel.lua")
local bdf = dofile(here .. "/bdf.lua")
local ui = dofile(here .. "/panel.lua")(px, bdf)
local touch = dofile(here .. "/touch.lua")(px, bdf, ui, here)
local c = px.c

-- A native pixel at 4x on a 460 ppi screen, the densest of the common phones.
local MM = 4 * 25.4 / 460
local function mm(n) return string.format("%.1fmm", n * MM) end
local legend = {
  "터치 영역 · 4배, 460ppi",
  string.format("1px = %.2fmm · 최소 9mm", MM),
  string.format("42px = %s · 48px = %s", mm(42), mm(48)),
}
local labels = { "42", "48", "166×42", "조이스틱 200×130", "취소 48" }
local texts = {}
for _, t in ipairs(legend) do texts[#texts + 1] = t end
for _, t in ipairs(labels) do texts[#texts + 1] = t end
local font = touch.font(texts)

local world, old = touch.world(here .. "/target_screen.aseprite")
local dim = px.canvas(touch.W, touch.H)
for k, i in pairs(world.p) do dim.p[k] = px.shift(i, -2) end
local hud = px.canvas(touch.W, touch.H)
touch.hud(hud, dim, old, { [2] = { cooldown = { 0.6, touch.words.cooldown[1] } } })
touch.cancel(hud, false, true)

local marks = px.canvas(touch.W, touch.H)
local amber = c("amber", 4)
local function tag(text, x, y)
  local w = bdf.width(font, text) + 6
  marks:rect(x, y, w, 13, c("ink", 1))
  bdf.text(marks, font, x + 3, y, text, c("amber", 5))
end
local function dashed_rect(x, y, w, h)
  for k = 0, w - 1 do
    if k % 4 < 2 then marks:set(x + k, y, amber); marks:set(x + k, y + h - 1, amber) end
  end
  for k = 0, h - 1 do
    if k % 4 < 2 then marks:set(x, y + k, amber); marks:set(x + w - 1, y + k, amber) end
  end
end
local function dashed_circle(x, y, r)
  for t = 0, 359, 3 do
    if t % 12 < 6 then
      local a = math.rad(t)
      marks:set(math.floor(x + math.cos(a) * r + 0.5), math.floor(y + math.sin(a) * r + 0.5), amber)
    end
  end
end

for n, s in ipairs(touch.SKILLS) do
  dashed_circle(s.x, s.y, s.hit)
  tag(tostring(s.hit * 2), s.x - s.hit, s.y - s.hit - 6)
end
local z = touch.CANCEL
dashed_circle(z.x, z.y, z.hit)
tag("취소 48", z.x - z.hit - 10, z.y - z.hit - 8)
for _, b in ipairs(touch.MENU) do
  dashed_rect(b[1] - 1, b[2] - 1, b[3] + 2, b[4] + 2)
  tag("42", b[1] + 2, b[2] + b[4] - 9)
end
local p = touch.PAUSE
dashed_rect(p[1] - 1, p[2] - 1, p[3] + 2, p[4] + 2)
tag("42", p[1] + 2, p[2] + p[4] - 9)
local r = touch.RUMOUR
dashed_rect(r[1] - 1, r[2] - 1, r[3] + 2, r[4] + 2)
tag("166×42", r[1] + r[3] - 46, r[2] + r[4] - 6)
local zone = touch.ZONE
dashed_rect(zone.x + 1, zone.y, zone.w - 1, zone.h - 1)
tag("조이스틱 200×130", zone.x + 104, zone.y + 3)

-- The legend, in the HUD's frame under the rumour.
local LX, LY, LW, LH = 176, 54, 166, 44
ui.frame(marks, LX, LY, LW, LH)
for n, t in ipairs(legend) do
  assert(bdf.width(font, t) <= LW - 10, t .. " overruns the legend")
  bdf.text(marks, font, LX + 5, LY + 1 + (n - 1) * 13, t, n == 1 and c("amber", 5) or c("paper", 3))
end

px.save(touch.W, touch.H, { { "game, dimmed", dim }, { "touch hud", hud }, { "touch targets", marks } },
  here .. "/mobile_hud_targets.aseprite")
