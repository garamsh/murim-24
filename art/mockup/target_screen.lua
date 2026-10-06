-- Draws target_screen.aseprite: one finished-game screen at the native 480x270, on the 16-pixel
-- grid. A village on a river is raided by the Demonic Cult; Orthodox disciples and the player
-- hold the road while a cult elder's area skill lands among them, Unorthodox looters work the
-- ruins, and an Unorthodox army arrives from the north as one aggregate marker.
-- Run: aseprite --batch --script art/mockup/target_screen.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)
local c = px.c

local W, H = 480, 270
local COLS, ROWS = 30, 17
local terrain = px.canvas(W, H)
local objects = px.canvas(W, H)
local effects = px.canvas(W, H)
local hud = px.canvas(W, H)

-- The cell map: one terrain kind per 16-pixel cell.
-- g grass, d dirt road, w water, b bridge, p paving, f millet field, F rice field.
local map = {}
for row = 0, ROWS - 1 do
  map[row] = {}
  for col = 0, COLS - 1 do map[row][col] = "g" end
end
local function fill(c0, r0, c1, r1, kind)
  for row = r0, r1 do for col = c0, c1 do if map[row] then map[row][col] = kind end end end
end
for row = 0, ROWS - 1 do
  local centre = 2 + math.floor(math.sin(row / 2.6) * 1.2 + 0.5)
  fill(centre - 1, row, centre + 1, row, "w")
end
for col = 0, COLS - 1 do map[9][col] = map[9][col] == "w" and "b" or "d" end
fill(21, 0, 21, 8, "d")
fill(8, 8, 12, 10, "p")
fill(9, 11, 10, 14, "f")
fill(11, 11, 12, 14, "F")

local function kind_at(x, y)
  local col, row = math.floor(x / 16), math.floor(y / 16)
  col = math.max(0, math.min(COLS - 1, col)); row = math.max(0, math.min(ROWS - 1, row))
  return map[row][col]
end

-- Natural edges (grass, road, water) are ragged; built ones (paving, fields, bridge) are straight.
local built = { p = true, f = true, F = true, b = true }
local function natural_at(x, y)
  local jx = (px.noise(x, y, 4, 11) - 0.5) * 7
  local jy = (px.noise(x, y, 4, 12) - 0.5) * 7
  local k = kind_at(x + jx, y + jy)
  if built[k] then k = (k == "b") and "w" or "g" end
  return k
end

-- Terrain, pixel by pixel.
for y = 0, H - 1 do
  for x = 0, W - 1 do
    local exact = kind_at(x, y)
    local k = built[exact] and exact or natural_at(x, y)
    local col
    if k == "g" then
      local n = px.noise(x, y, 22, 1) + (px.noise(x, y, 4, 2) - 0.5) * 0.3
      col = c("grass", 4)
      if n > 0.64 + px.bayer(x, y) * 0.1 then col = c("grass", 5)
      elseif n < 0.26 + px.bayer(x, y) * 0.1 then col = c("grass", 3) end
    elseif k == "d" then
      local n = px.noise(x, y, 6, 3) + (px.hash(x, y, 4) - 0.5) * 0.3
      col = c("earth", 4)
      if n > 0.72 then col = c("earth", 5) elseif n < 0.3 then col = c("earth", 3) end
      -- Wheel ruts along the road.
      local ry = y % 16
      if exact == "d" and map[math.floor(y / 16)][math.floor(x / 16)] == "d" and map[9][math.floor(x / 16)] == "d"
          and math.floor(y / 16) == 9 and (ry == 5 or ry == 11) and px.hash(x, y, 5) > 0.25 then
        col = c("earth", 3)
      end
    elseif k == "w" then
      local deep = true
      for _, o in ipairs({ { -5, 0 }, { 5, 0 }, { 0, -4 }, { 0, 4 } }) do
        if natural_at(x + o[1], y + o[2]) ~= "w" then deep = false end
      end
      col = deep and c("water", 3) or c("water", 4)
      if deep and px.noise(x, y, 9, 6) < 0.35 then col = c("water", 2) end
      local ripple = px.hash(math.floor(x / 5), y, 7)
      if ripple > 0.93 and (x + y * 3) % 5 < 3 then col = c("water", 5) end
      -- Foam and a bank of wet earth at the shore.
      local shore = false
      for _, o in ipairs({ { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } }) do
        if natural_at(x + o[1], y + o[2]) ~= "w" then shore = true end
      end
      if shore then col = c("water", 6) end
    elseif k == "p" then
      local sx, sy = (x + (math.floor(y / 8) % 2) * 4) % 8, y % 8
      col = c("stone", 4)
      if sx == 0 or sy == 0 then col = c("stone", 3) end
      if px.hash(math.floor((x + (math.floor(y / 8) % 2) * 4) / 8), math.floor(y / 8), 8) > 0.8 and sx ~= 0 and sy ~= 0 then col = c("stone", 5) end
    elseif k == "f" or k == "F" then
      local soil = (x % 4 == 0)
      if k == "f" then
        col = soil and c("earth", 2) or (((y + x // 4) % 3 == 0) and c("amber", 5) or c("amber", 4))
        if not soil and px.hash(x, y, 9) > 0.85 then col = c("amber", 3) end
      else
        col = soil and c("water", 3) or (((y + x // 4) % 3 == 0) and c("grass", 6) or c("grass", 5))
      end
      if y % 16 == 0 or x % 16 == 0 then col = c("earth", 3) end
    elseif k == "b" then
      col = (x % 4 == 0) and c("wood", 2) or (x % 4 == 1 and c("wood", 5) or c("wood", 4))
      local ry = y % 16
      if ry <= 1 or ry >= 14 then col = c("wood", 3) end
      if ry == 0 or ry == 15 then col = c("wood", 1) end
    end
    terrain:set(x, y, col)
  end
end

-- The bank darkens the grass next to the water.
for y = 0, H - 1 do
  for x = 0, W - 1 do
    local k = natural_at(x, y)
    if k ~= "w" and not built[kind_at(x, y)] then
      for d = 1, 2 do
        if natural_at(x - d, y) == "w" or natural_at(x + d, y) == "w" or natural_at(x, y - d) == "w" then
          terrain:set(x, y, d == 1 and c("earth", 5) or c("earth", 4))
          break
        end
      end
    end
  end
end

-- Bridge rails and posts, and its shadow on the water.
for col = 0, COLS - 1 do
  if map[9][col] == "b" then
    for x = col * 16, col * 16 + 15 do
      terrain:shade_at(x, 160, -2); terrain:shade_at(x, 161, -1)
    end
    for _, x in ipairs({ col * 16, col * 16 + 8 }) do
      objects:rect(x, 141, 2, 4, c("wood", 2)); objects:rect(x, 157, 2, 4, c("wood", 2))
    end
    objects:hline(col * 16, 142, 16, c("wood", 4)); objects:hline(col * 16, 158, 16, c("wood", 4))
  end
end

-- Grass tufts and flowers.
local rng = px.rng(20261006)
for k = 1, 420 do
  local x, y = rng.int(0, W - 1), rng.int(0, H - 1)
  if natural_at(x, y) == "g" and not built[kind_at(x, y)] then
    if rng.next() < 0.88 then
      terrain:set(x, y, c("grass", 6)); terrain:set(x - 1, y + 1, c("grass", 5)); terrain:set(x + 1, y + 1, c("grass", 5))
      terrain:set(x, y + 1, c("grass", 3))
    else
      terrain:set(x, y, rng.pick({ c("paper", 3), c("amber", 5), c("crimson", 5) }))
    end
  end
end

-- Draw order: everything standing is drawn by the y of its base.
local drawables = {}
local function later(base_y, fn) drawables[#drawables + 1] = { base_y, #drawables, fn } end

-- Trees and bamboo.
local trees = {
  { 72, 30, 10 }, { 100, 22, 12 }, { 128, 34, 9 }, { 160, 20, 11 }, { 190, 36, 9 }, { 218, 18, 10 },
  { 244, 40, 8 }, { 88, 56, 8 }, { 456, 92, 11 }, { 470, 136, 9 }, { 440, 232, 10 }, { 470, 210, 8 },
  { 408, 80, 9 }, { 24, 212, 9 }, { 60, 250, 10 },
}
for n, t in ipairs(trees) do later(t[2], function() S.tree(objects, terrain, t[1], t[2], t[3], 300 + n) end) end
for n, b in ipairs({ { 76, 118 }, { 66, 196 }, { 84, 236 } }) do
  later(b[2], function() S.bamboo(objects, terrain, b[1], b[2], 400 + n) end)
end

-- Rocks.
local function rock(x, y, r)
  terrain:shade_ellipse(x + 2, y + 2, r + 1, r * 0.6, -2, true)
  objects:each_ellipse(x, y, r, r * 0.75, function(xx, yy)
    local l = -(xx - x) - (yy - y)
    objects:set(xx, yy, l > r * 0.6 and c("stone", 5) or (l > -r * 0.3 and c("stone", 4) or c("stone", 3)))
  end)
  objects:outline(x - r - 1, y - r - 1, r * 2 + 3, r * 2 + 3, c("ink", 2))
end
later(214, function() rock(396, 214, 4) end)
later(68, function() rock(380, 68, 3) end)
later(100, function() rock(270, 98, 3) end)

-- Buildings. North of the road: intact, intact hall, damaged, rubble. South: intact, destroyed.
local houses = {
  { 64, 80, "intact", false }, { 128, 80, "intact", true }, { 224, 80, "damaged", false },
  { 288, 80, "rubble", false }, { 80, 176, "intact", false }, { 240, 176, "destroyed", false },
}
for n, h in ipairs(houses) do
  later(h[2] + 46, function() S.house(objects, terrain, h[1], h[2], h[3], 500 + n, h[4]) end)
end

-- Village props: the well, a cart, barrels, a haystack, a lantern post.
later(170, function()
  local x, y = 168, 160
  terrain:shade_ellipse(x + 9, y + 9, 9, 4, -2, true)
  objects:each_ellipse(x + 7, y + 6, 7, 5, function(xx, yy, d)
    objects:set(xx, yy, d > 0.62 and ((xx + yy) % 3 == 0 and c("stone", 3) or c("stone", 4)) or c("water", 2))
  end)
  objects:set(x + 5, y + 5, c("water", 5)); objects:set(x + 6, y + 5, c("water", 4))
  objects:rect(x, y - 4, 1, 9, c("wood", 3)); objects:rect(x + 14, y - 4, 1, 9, c("wood", 3))
  objects:hline(x, y - 4, 15, c("wood", 4))
  objects:outline(x - 1, y - 5, 17, 14, c("ink", 1))
end)
local function barrel(x, y)
  terrain:shade_ellipse(x + 4, y + 7, 4, 1.5, -2)
  objects:rect(x, y, 6, 7, c("wood", 4)); objects:vline(x, y, 7, c("wood", 5)); objects:vline(x + 5, y, 7, c("wood", 2))
  objects:hline(x, y + 1, 6, c("ink", 3)); objects:hline(x, y + 5, 6, c("ink", 3))
  objects:hline(x + 1, y, 4, c("wood", 5))
  objects:outline(x, y, 6, 7, c("ink", 1))
end
later(135, function() barrel(118, 128) end)
later(137, function() barrel(124, 130) end)
later(232, function()
  local x, y = 60, 220
  terrain:shade_ellipse(x + 10, y + 11, 11, 3, -2, true)
  objects:each_ellipse(x + 8, y + 6, 8, 6, function(xx, yy, d)
    local l = -(xx - x - 8) - (yy - y - 6) + px.hash(xx, yy, 3) * 4
    objects:set(xx, yy, l > 5 and c("amber", 5) or (l > -2 and c("amber", 4) or c("amber", 3)))
  end)
  objects:outline(x - 1, y - 1, 18, 14, c("ink", 2))
end)
local function lantern(x, y)
  terrain:shade_ellipse(x + 3, y + 1, 3, 1, -2)
  objects:vline(x, y - 14, 15, c("wood", 2)); objects:hline(x, y - 14, 4, c("wood", 2))
  objects:rect(x + 2, y - 13, 3, 4, c("crimson", 4)); objects:set(x + 3, y - 12, c("fire", 4))
  objects:outline(x - 1, y - 15, 7, 17, c("ink", 1))
end
later(142, function() lantern(140, 141) end)
later(142, function() lantern(204, 141) end)
-- A broken cart on the road, east of the square.
later(166, function()
  local x, y = 214, 158
  terrain:shade_ellipse(x + 10, y + 8, 11, 2.5, -2, true)
  objects:rect(x, y, 16, 5, c("wood", 3)); objects:hline(x, y, 16, c("wood", 4))
  for k = 0, 15, 4 do objects:vline(x + k, y, 5, c("wood", 2)) end
  objects:rect(x + 2, y + 4, 4, 4, c("wood", 2)); objects:set(x + 3, y + 5, c("wood", 4))
  objects:line(x + 16, y + 2, x + 22, y + 5, c("wood", 3))
  objects:rect(x + 4, y - 3, 5, 3, c("amber", 4)); objects:rect(x + 9, y - 2, 4, 2, c("cloth", 3))
  objects:outline(x - 1, y - 4, 25, 13, c("ink", 1))
end)

-- Burnt ground around the destroyed house and the rubble.
terrain:shade_ellipse(264, 222, 38, 12, -1, true)
terrain:shade_ellipse(312, 122, 30, 9, -1, true)

-- Bodies.
local function body(spec, x, y, flip, down)
  local b = S.body(spec)
  later(y + 16, function()
    if not down then S.shadow(terrain, x, y) end
    S.place(objects, b, x, y, flip, down)
  end)
end
local O = "orthodox"
local D = "demonic"
local U = "unorthodox"
local V = "villager"

-- The player, holding the road, striking the raider in front of them.
local player_x, player_y = 296, 140
terrain:each_ellipse(player_x + 8, player_y + 15, 7, 2.6, function(xx, yy, d)
  if d > 0.68 then terrain:set(xx, yy, c("jade", 3)) end
end)
body({ faction = O, head = "topknot", body = "run", weapon = "sword_strike", over = { A = c("jade", 3) } }, player_x, player_y)

-- Orthodox disciples on the road and south of it.
body({ faction = O, head = "topknot", weapon = "sword" }, 274, 128)
body({ faction = O, head = "bun", body = "run", weapon = "sword_strike", over = { A = c("azure", 4) } }, 276, 156)
body({ faction = O, head = "topknot", body = "tunic", weapon = "spear" }, 256, 144)
-- An Orthodox elder looses sword qi at the raiders.
body({ faction = O, head = "white", body = "run", weapon = "sword_strike", over = { R = c("azure", 3), M = c("azure", 2), D = c("azure", 1), A = c("robe", 3) } }, 344, 112)
-- Disciples caught in the cult elder's palm.
body({ faction = O, head = "topknot", weapon = "sword" }, 310, 178, false, true)
body({ faction = O, head = "bun", weapon = "sword" }, 334, 194, true, true)
body({ faction = O, head = "topknot", body = "run", weapon = "sword" }, 296, 196, true)
body({ faction = O, head = "topknot", body = "tunic", weapon = "spear" }, 348, 170, true)

-- Demonic Cult raiders.
body({ faction = D, head = "spiky", body = "run", weapon = "saber_strike" }, 318, 138, true)
body({ faction = D, head = "spiky", body = "tunic", weapon = "saber" }, 340, 148, true)
body({ faction = D, head = "hood", weapon = "torch" }, 360, 136, true)
body({ faction = D, head = "spiky", body = "run", weapon = "saber" }, 382, 120, true)
body({ faction = D, head = "spiky", body = "tunic", weapon = "torch" }, 398, 152, true)
body({ faction = D, head = "bald", body = "big", over = { R = c("skin", 3), M = c("skin", 2), D = c("skin", 1) } }, 414, 134, true)
body({ faction = D, head = "hood", weapon = "knife" }, 428, 164, true)
body({ faction = D, head = "spiky", body = "tunic", weapon = "saber" }, 444, 142, true)
body({ faction = D, head = "spiky", body = "tunic", weapon = "saber" }, 404, 116, true, true)
-- The cult elder whose palm lands among the disciples.
body({ faction = D, head = "white", body = "run", weapon = "palm", over = { R = c("crimson", 3), M = c("crimson", 2), D = c("crimson", 1), A = c("ink", 1), W = c("stone", 6) } }, 376, 186, true)

-- Unorthodox looters at the ruins, one with a sack of loot.
body({ faction = U, head = "topknot", hat = "straw", body = "tunic", weapon = "knife" }, 300, 92)
body({ faction = U, head = "mask", body = "tunic", weapon = "knife", over = { X = c("ink", 2), x = c("ink", 1) } }, 226, 212, true)
later(108, function()
  objects:each_ellipse(296, 103, 4, 4, function(xx, yy) objects:set(xx, yy, (xx + yy) % 4 == 0 and c("cloth", 2) or c("cloth", 3)) end)
  objects:set(295, 100, c("gold", 4)); objects:set(297, 102, c("gold", 4))
  objects:outline(291, 98, 10, 10, c("ink", 1))
end)
body({ faction = U, head = "topknot", hat = "straw", weapon = "bow" }, 252, 50)

-- Villagers fleeing west along the road.
body({ faction = V, head = "topknot", body = "run" }, 196, 138, true)
body({ faction = V, head = "bun", body = "run", over = { R = c("cloth", 4), A = c("lacquer", 3) } }, 150, 146, true)
body({ faction = V, head = "white", body = "tunic", weapon = "staff" }, 120, 142, true)
body({ faction = V, head = "topknot", hat = "straw", body = "run", over = { X = c("earth", 6), x = c("earth", 5), Y = c("earth", 4) } }, 36, 140, true)
body({ faction = V, head = "topknot", body = "tunic" }, 172, 182)

-- Draw everything standing, back to front.
table.sort(drawables, function(a, b) if a[1] == b[1] then return a[2] < b[2] end return a[1] < b[1] end)
for _, d in ipairs(drawables) do d[3]() end

-- The player's chevron, above every body.
objects:sprite(player_x + 6, player_y - 5, { "#####", ".###.", "..#.." }, { ["#"] = c("gold", 4) })
objects:outline(player_x + 5, player_y - 6, 7, 5, c("ink", 1))

-- Effects: fire and smoke on the destroyed house, the area skill, sword qi, sparks.
S.fire(effects, 246, 182, 38, 77)
S.smoke(effects, 258, 178, 74, -0.55, 5)
S.smoke(effects, 312, 116, 26, -0.4, 6)
S.area_skill(effects, terrain, 322, 196, 30, "crimson", 9)
S.slash(effects, 372, 122, 14, 1, "azure")
S.slash(effects, 304, 148, 9, 1, "azure")
S.spark(effects, 316, 146, c("fire", 4))
S.spark(effects, 392, 124, c("azure", 5))
S.spark(effects, 288, 168, c("fire", 4))
-- Damage numbers rise from the struck.
effects:text(318, 126, "-128", c("azure", 5), c("ink", 1))
effects:text(388, 104, "-96", c("azure", 5), c("ink", 1))
effects:text(314, 168, "-412", c("crimson", 5), c("ink", 1))

-- The Unorthodox army arriving down the north road, as one aggregate marker.
S.army(objects, terrain, 302, 24, 18, 4, U, "amber", 41)
for y = 2, 20 do
  for x = 316, 372 do if px.noise(x, y, 3, 77) > 0.55 and px.bayer(x, y) > 0.5 then effects:set(x, y, c("earth", 5)) end end
end

-- HUD.
local function panel(x, y, w, h)
  hud:rect(x, y, w, h, c("ink", 2))
  hud:rect(x + 1, y + 1, w - 2, h - 2, c("ink", 1))
  hud:rect(x + 2, y + 2, w - 4, h - 4, c("ink", 2))
  for xx = x + 2, x + w - 3 do hud:set(xx, y, c("gold", 2)) end
  hud:hline(x + 2, y, w - 4, c("gold", 3)); hud:hline(x + 2, y + h - 1, w - 4, c("gold", 2))
  hud:vline(x, y + 2, h - 4, c("gold", 2)); hud:vline(x + w - 1, y + 2, h - 4, c("gold", 2))
  for _, p in ipairs({ { x, y }, { x + w - 3, y }, { x, y + h - 3 }, { x + w - 3, y + h - 3 } }) do
    hud:rect(p[1], p[2], 3, 3, c("gold", 3)); hud:set(p[1] + 1, p[2] + 1, c("lacquer", 3))
  end
end
local function bar(x, y, w, frac, ramp, label, value)
  hud:rect(x, y, w, 5, c("ink", 1))
  local fill = math.floor((w - 2) * frac)
  hud:rect(x + 1, y + 1, fill, 3, c(ramp, 3))
  hud:hline(x + 1, y + 1, fill, c(ramp, 4))
  hud:rect(x + 1 + fill, y + 1, w - 2 - fill, 3, c("ink", 3))
  hud:text(x - 13, y, label, c(ramp, 4))
  hud:text(x + w + 3, y, value, c("paper", 3))
end

-- Top left: the player.
panel(4, 4, 152, 40)
hud:rect(8, 8, 32, 32, c("jade", 1))
for yy = 8, 39 do for xx = 8, 39 do if (xx + yy) % 2 == 0 and yy > 28 then hud:set(xx, yy, c("jade", 2)) end end end
local portrait = S.body({ faction = O, head = "topknot", over = { A = c("jade", 3) } })
for k, i in pairs(portrait.p) do
  local bx, by = k % portrait.w, k // portrait.w
  local hx, hy = 8 + (bx - 3) * 3, 8 + (by - 3) * 3 + 2
  if hy < 39 and hx >= 8 and hx < 38 then hud:rect(hx, hy, 3, math.min(3, 40 - hy), i) end
end
hud:rect(8, 8, 32, 1, c("gold", 3)); hud:rect(8, 39, 32, 1, c("gold", 2))
hud:vline(7, 8, 32, c("gold", 2)); hud:vline(40, 8, 32, c("gold", 2))
hud:text(46, 8, "BAEK SEO-JIN", c("paper", 3), c("ink", 1))
hud:text(46, 15, "AZURE CLOUD SECT  AGE 19", c("azure", 4))
bar(59, 24, 62, 0.72, "crimson", "HP", "184")
bar(59, 32, 62, 0.55, "azure", "QI", "92")

-- Top right: the map and the date.
panel(392, 4, 84, 62)
local mx, my = 396, 8
for row = 0, ROWS - 1 do
  for col = 0, COLS - 1 do
    local k = map[row][col]
    local col_i = ({ g = c("grass", 3), d = c("earth", 4), w = c("water", 3), b = c("wood", 4), p = c("stone", 4), f = c("amber", 4), F = c("grass", 5) })[k]
    hud:rect(mx + 3 + col * 2 + 8, my + 4 + row * 2 + 6, 2, 2, col_i)
  end
end
-- Beyond the screen on the map: more grass and the next village to the north-west.
hud:rect(mx + 2, my + 2, 76, 6, c("grass", 2)); hud:rect(mx + 2, my + 2, 6, 46, c("grass", 2))
hud:rect(mx + 71, my + 2, 7, 46, c("grass", 2)); hud:rect(mx + 2, my + 44, 76, 4, c("grass", 2))
hud:rect(mx + 4, my + 3, 3, 2, c("stone", 4))
-- The screen's frame, the village, the raid, the army and the player.
local function mark(x, y, i) hud:rect(mx + 11 + x, my + 10 + y, 2, 2, i) end
for _, h in ipairs(houses) do mark(h[1] // 8, h[2] // 8, h[3] == "intact" and c("stone", 5) or c("ink", 3)) end
mark(50, 22, c("crimson", 4)); mark(48, 20, c("crimson", 4)); mark(54, 18, c("crimson", 4))
hud:rect(mx + 11 + 38, my + 4, 8, 3, c("amber", 4))
mark(player_x // 8, player_y // 8, c("jade", 4))
hud:set(mx + 11 + player_x // 8 + 2, my + 10 + player_y // 8, c("jade", 4))
hud:text(397, 58, "SPRING  YEAR 3", c("paper", 3))

-- Bottom left: the rumour that reached the player.
panel(4, 226, 164, 40)
hud:rect(9, 231, 22, 9, c("lacquer", 2)); hud:text(11, 233, "RUMOR", c("paper", 3))
hud:text(36, 233, "CULT RAIDERS PUT WILLOW FORD", c("paper", 3))
hud:text(36, 241, "TO THE TORCH.", c("paper", 3))
hud:text(9, 251, "THE AMBER HATS MARCH SOUTH,", c("stone", 4))
hud:text(9, 258, "PAID IN SILVER BY NO SECT.", c("stone", 4))

-- Bottom centre: the technique bar; the palm is cooling down.
local slots = {
  { "1", function(x, y) -- sword technique: a crescent
    S.slash(hud, x + 5, y + 10, 8, 1, "azure") end },
  { "2", function(x, y) -- palm
    hud:sprite(x + 5, y + 4, { "..#.#.#...", "..#.#.#...", "#.#.#.#...", "#.#.#.#...", "#######...",
      "#######.#.", "#######.#.", "########..", ".######...", ".#####....", "..####....", "..####...." },
      { ["#"] = c("skin", 4) })
    hud:outline(x + 4, y + 3, 12, 14, c("jade", 3)) end },
  { "3", function(x, y) -- lightness skill: a feather
    hud:line(x + 5, y + 15, x + 14, y + 4, c("paper", 3)); hud:line(x + 6, y + 13, x + 13, y + 4, c("jade", 3))
    hud:line(x + 7, y + 14, x + 15, y + 6, c("jade", 2)) end },
  { "4", function(x, y) -- meditation: inner energy, a swirl of qi
    hud:each_ellipse(x + 10, y + 10, 6, 6, function(xx, yy, d) if d > 0.7 then hud:set(xx, yy, c("azure", 4)) end end)
    hud:rect(x + 9, y + 9, 3, 3, c("azure", 5)); hud:set(x + 13, y + 5, c("azure", 5)) end },
  { "5", function(x, y) -- medicine gourd
    hud:each_ellipse(x + 10, y + 13, 4, 4, function(xx, yy) hud:set(xx, yy, c("amber", 4)) end)
    hud:each_ellipse(x + 10, y + 7, 3, 2.5, function(xx, yy) hud:set(xx, yy, c("amber", 4)) end)
    hud:hline(x + 9, y + 9, 3, c("lacquer", 3)); hud:rect(x + 9, y + 3, 3, 2, c("wood", 3)) end },
  { "6", function(x, y) -- an empty, locked slot
    hud:text(x + 8, y + 8, "+", c("ink", 4)) end },
}
local sx0 = 240 - (#slots * 24) // 2
panel(sx0 - 4, 238, #slots * 24 + 6, 28)
for n, s in ipairs(slots) do
  local x, y = sx0 + (n - 1) * 24, 242
  hud:rect(x, y, 21, 21, c("ink", 3)); hud:rect(x + 1, y + 1, 19, 19, c("ink", 1))
  s[2](x, y)
  if n == 2 then
    for yy = y + 1, y + 12 do for xx = x + 1, x + 19 do hud:shade_at(xx, yy, -2) end end
    hud:hline(x + 1, y + 12, 19, c("jade", 3))
    hud:text(x + 14, y + 13, "3", c("paper", 3), c("ink", 1))
  end
  if n == 1 then
    for _, r in ipairs({ { x, y, 21, 1 }, { x, y + 20, 21, 1 }, { x, y, 1, 21 }, { x + 20, y, 1, 21 } }) do
      hud:rect(r[1], r[2], r[3], r[4], c("jade", 3))
    end
  end
  hud:rect(x, y, 5, 7, c("ink", 2)); hud:text(x + 1, y + 1, s[1], c("gold", 4))
end

-- Bottom right: the target the player is fighting.
panel(354, 238, 122, 28)
hud:text(360, 243, "BLOOD SABER JO", c("crimson", 5), c("ink", 1))
hud:text(360, 250, "DEMONIC CULT", c("stone", 4))
bar(380, 257, 56, 0.34, "crimson", "HP", "61")

-- The aggregate army's plate.
hud:rect(310, 55, 68, 9, c("ink", 1)); hud:hline(310, 55, 68, c("amber", 3))
hud:text(314, 57, "AMBER HATS x1,240", c("amber", 5))

px.save(W, H, { { "terrain", terrain }, { "bodies and structures", objects }, { "effects", effects }, { "hud", hud } },
  here .. "/target_screen.aseprite")
