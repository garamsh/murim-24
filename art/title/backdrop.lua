-- Draws backdrop.aseprite: the title screen's backdrop at the native 480x270. Ridges of mountains
-- fade into mist under a low sun, a sect's gate stands on a far peak, and the working title sits
-- at 4x of the font over its romanised name, in Galmuri9 (hud/fonts/Galmuri9.bdf). The menu and
-- the words that change with the language are drawn over it by the game.
-- Run: aseprite --batch --script art/title/backdrop.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local bdf = dofile(here .. "/../mockup/bdf.lua")
local c = px.c

local W, H = 480, 270

local words = { title = "무림24", latin = "MURIM24" }
local font = bdf.load(here .. "/../../hud/fonts/Galmuri9.bdf", { words.title, words.latin })

local scene = px.canvas(W, H)

-- The sky darkens upward from the horizon, its bands dithered into each other.
local sky = { { 0, c("ink", 1) }, { 46, c("ink", 2) }, { 96, c("ink", 4) }, { 138, c("stone", 1) } }
for y = 0, H - 1 do
  for x = 0, W - 1 do
    local band = 1
    for n, b in ipairs(sky) do if y >= b[1] then band = n end end
    local next_band = sky[band + 1]
    local i = sky[band][2]
    if next_band and px.bayer(x, y) < ((y - sky[band][1]) / (next_band[1] - sky[band][1]) - 0.4) / 0.6 then
      i = next_band[2]
    end
    scene:set(x, y, i)
  end
end

-- The low sun, haloed by dithered gold.
local SX, SY = 352, 112
scene:each_ellipse(SX, SY, 44, 44, function(x, y, d)
  if px.bayer(x, y) > d * 1.6 - 0.2 then scene:set(x, y, c("gold", 1)) end
end)
scene:ellipse(SX, SY, 20, 20, c("gold", 4))
scene:ellipse(SX - 3, SY - 3, 13, 13, c("gold", 6))

-- Birds crossing the sky.
for _, b in ipairs({ { 300, 62 }, { 312, 70 }, { 292, 74 }, { 404, 54 } }) do
  scene:set(b[1] - 2, b[2] - 1, c("ink", 1)); scene:set(b[1] - 1, b[2], c("ink", 1))
  scene:set(b[1], b[2] - 1, c("ink", 1))
  scene:set(b[1] + 1, b[2], c("ink", 1)); scene:set(b[1] + 2, b[2] - 1, c("ink", 1))
end

-- Ridges back to front. Each is lit a step at its foot, where mist lies in the valley before
-- the next ridge.
local ridges = {
  { base = 150, amp = 46, scale = 70, seed = 11, colour = c("stone", 2) },
  { base = 180, amp = 40, scale = 52, seed = 12, colour = c("stone", 1) },
  { base = 210, amp = 30, scale = 40, seed = 13, colour = c("ink", 4) },
  { base = 238, amp = 18, scale = 30, seed = 14, colour = c("ink", 2) },
}
local function top(r, x)
  local n = px.noise(x, 0, r.scale, r.seed) * 0.7 + px.noise(x, 0, r.scale / 3, r.seed + 50) * 0.3
  return math.floor(r.base - n * r.amp)
end
for k, r in ipairs(ridges) do
  local nearer = ridges[k + 1]
  for x = 0, W - 1 do
    local t = top(r, x)
    local foot = nearer and top(nearer, x) or H
    for y = t, H - 1 do
      local i = r.colour
      local mist = foot - y
      if nearer and mist < 14 and px.bayer(x, y) > mist / 14 then i = px.shift(i, 1) end
      scene:set(x, y, i)
    end
  end
end

-- A sect's gate, in silhouette, on the second ridge's highest peak.
local peak_x, peak_t = 0, H
for x = 260, 440 do
  local t = top(ridges[2], x)
  if t < peak_t then peak_x, peak_t = x, t end
end
local gate = px.load_frames(here .. "/../structures/seat_gate.aseprite")[1]
for k in pairs(gate.p) do
  local gx, gy = k % gate.w, k // gate.w
  if gy < 46 then scene:set(peak_x - 16 + gx // 2, peak_t - 22 + gy // 2, c("stone", 1)) end
end

-- Pines on the nearest ridges.
local function pine(x, base, height, colour)
  for row = 0, height - 1 do
    local half = (row * 4) // height + (row % 3 == 2 and 1 or 0)
    scene:hline(x - half, base - height + row, half * 2 + 1, colour)
  end
  scene:vline(x, base, 2, colour)
end
for x = 6, W - 6, 23 do
  local r = ridges[3]
  if px.hash(x, 3, 30) < 0.55 then pine(x, top(r, x) + 3, 9 + math.floor(px.hash(x, 4, 30) * 6), c("ink", 2)) end
end
for x = 12, W - 6, 17 do
  local r = ridges[4]
  pine(x, top(r, x) + 4, 12 + math.floor(px.hash(x, 5, 30) * 7), c("ink", 1))
end

local wordmark = px.canvas(W, H)

-- The working title at 4x, its shadow a pixel of the font down and right in lacquer, outlined in
-- ink. The 4x is whole pixels of the font, so it stays on the grid.
local glyphs = px.canvas(W, 24)
local title_w = bdf.text(glyphs, font, 0, 0, words.title, c("paper", 6))
local TX, TY, S = 40, 40, 4
for k in pairs(glyphs.p) do
  local gx, gy = k % glyphs.w, k // glyphs.w
  wordmark:rect(TX + gx * S + S, TY + gy * S + S, S, S, c("lacquer", 4))
end
for k in pairs(glyphs.p) do
  local gx, gy = k % glyphs.w, k // glyphs.w
  wordmark:rect(TX + gx * S, TY + gy * S, S, S, gy >= 7 and c("paper", 4) or c("paper", 6))
end
wordmark:outline(TX - 1, TY - 1, title_w * S + S + 2, 12 * S + S + 2, c("ink", 1))
local rule_y = TY + 12 * S + 6
wordmark:hline(TX, rule_y, title_w * S, c("gold", 3))
wordmark:hline(TX, rule_y + 1, title_w * S, c("gold", 1))
bdf.text(wordmark, font, TX, rule_y + 4, words.latin, c("gold", 4), c("ink", 1))

px.save(W, H, { { "scene", scene }, { "wordmark", wordmark } }, here .. "/backdrop.aseprite")
