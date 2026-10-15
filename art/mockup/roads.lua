-- The mockup of #172's road looks, for the PM's approval before the build: bandits, wandering
-- martial artists and an escort agency with its caravan, at the production body's size on ADR
-- 0069's ramps with sel-out. It draws, from the production palette (art/pixel.lua) and body template
-- (art/sprites.lua):
--   roads/<kind>.aseprite, 16x24 frames with the feet on row 22 as art/bodies' sheets, tagged idle
--     (one), walk (four, 150 ms: step, pass, other step, pass, a passing frame a pixel higher) and
--     strike (#184's swing, three, 80 ms: raised behind the head, level, followed through low; a
--     spear thrusts instead), for the three bandits (dao, club, axe), the wanderer, and the escort's
--     spear guard and pennant-bearer in each candidate livery, water and stone;
--   roads/caravan.aseprite, 32x24 frames: the mule idle and walking (four), the pack mule idle and
--     walking (four), and the cart's two wheel frames;
--   roads/roads_sheet.aseprite, the contact sheet: every frame at native size, the two liveries
--     beside the azure and jade swatches and the stone escort beside the stone wanderer, and a road
--     at play zoom, 3x: the caravan and its guards waylaid by a bandit band, a wanderer beside a
--     villager;
--   roads/roads_frames.aseprite, every person's frame, for the 6x export.
-- Bandits carry no accent: they read by silhouette and value, bare arms under a fur-trimmed
-- leather vest and an ink face-cloth, and their nameplate's red carries the hostility (#164).
-- Wanderers carry no faction cue: a hooded cloak in the stone ramp, a pack, a sword slung across it.
-- Run: art/mockup/roads.sh [path to aseprite]

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)
local c = px.c

local W, H = 16, 24
-- The body canvas from sprites.lua puts the feet on its row 16; this lands them on row 22.
local TOP = 6
-- In the raw body's 16x18 pixels: the hand that holds a weapon, the other hand, the last row above
-- the hips, the feet's row, and the topknot head's third row, where a headband sits.
local HAND = { 11, 13 }
local HIPS = 13
local FEET = 16
local BROW = 6

local function copy(spec, over)
  local s = {}
  for k, v in pairs(spec) do s[k] = v end
  for k, v in pairs(over or {}) do s[k] = v end
  return s
end

-- Everything above the hips moved `dx` across and `dy` down: a lean, as art/bodies leans a pose.
local function lean(cv, dx, dy)
  local out = px.canvas(cv.w, cv.h)
  for k, i in pairs(cv.p) do
    local x, y = k % cv.w, k // cv.w
    if y > HIPS then out:set(x, y, i) end
  end
  for k, i in pairs(cv.p) do
    local x, y = k % cv.w, k // cv.w
    if y <= HIPS then out:set(x + dx, y + dy, i) end
  end
  return out
end

-- Weapons, each drawn from the hand (x, y) by pose: 0 carried, 1 raised behind the head, 2 swung
-- level, 3 followed through low.
local function blade_from(cv, x, y, pose, len, edge, flat)
  local ends = { [0] = { 0, -len }, [1] = { -len + 1, -len + 1 }, [2] = { len, -1 }, [3] = { len - 1, len // 2 } }
  local e = ends[pose]
  cv:line(x, y + 1, x + e[1], y + e[2] + 1, flat)
  cv:line(x, y, x + e[1], y + e[2], edge)
end
local weapons = {
  -- A heavy dao: a broad single-edged blade.
  dao = function(cv, x, y, pose)
    blade_from(cv, x, y, pose, 6, c("stone", 6), c("stone", 4))
    cv:set(x, y, c("wood", 3))
  end,
  -- A knotted club.
  club = function(cv, x, y, pose)
    local ends = { [0] = { 0, -6 }, [1] = { -5, -5 }, [2] = { 6, -1 }, [3] = { 5, 3 } }
    local e = ends[pose]
    cv:line(x, y, x + e[1], y + e[2], c("wood", 4))
    cv:rect(x + e[1] - 1, y + e[2] - 1, 2, 2, c("wood", 3)); cv:set(x + e[1], y + e[2] - 1, c("wood", 5))
  end,
  -- An axe: a haft and an iron head at its end.
  axe = function(cv, x, y, pose)
    local ends = { [0] = { 0, -7 }, [1] = { -5, -5 }, [2] = { 6, -1 }, [3] = { 5, 3 } }
    local e = ends[pose]
    cv:line(x, y, x + e[1], y + e[2], c("wood", 4))
    cv:rect(x + e[1], y + e[2], 2, 3, c("stone", 4)); cv:set(x + e[1] + 1, y + e[2], c("stone", 6))
  end,
  -- The wanderer's straight sword, drawn from its back.
  sword = function(cv, x, y, pose)
    blade_from(cv, x, y, pose, 7, c("stone", 7), c("stone", 4))
    cv:hline(x - 1, y, 3, c("wood", 3))
  end,
  -- The escort's spear, its tassel paper: carried upright; at a strike, levelled, thrust, recovered.
  spear = function(cv, x, y, pose)
    if pose == 0 then
      cv:vline(x, y - 9, 13, c("wood", 4)); cv:set(x, y - 10, c("stone", 7)); cv:set(x, y - 11, c("stone", 6))
      cv:set(x - 1, y - 9, c("paper", 6))
      return
    end
    local reach = ({ 0, 3, 1 })[pose]
    cv:hline(x - 6 + reach, y, 10, c("wood", 4))
    cv:hline(x + 4 + reach, y, 2, c("stone", 7)); cv:set(x + 6 + reach, y, c("stone", 6))
    cv:set(x + 3 + reach, y + 1, c("paper", 6))
  end,
}

-- The escort's pennant on its pole: a paper triangle bearing the agency's mark in ink.
local function pennant(cv, x, y, pose)
  local tip = pose == 0 and 0 or 1
  cv:vline(x, y - 11, 15, c("wood", 4))
  for row = 0, 4 do cv:hline(x + 1, y - 11 + row, 5 - row + tip, c("paper", 6)) end
  cv:hline(x + 1, y - 7, 1, c("paper", 4))
  cv:set(x + 2, y - 10, c("ink", 2)); cv:set(x + 3, y - 10, c("ink", 2)); cv:set(x + 2, y - 9, c("ink", 2))
  cv:set(x, y - 12, c("stone", 6))
end

-- The kinds of person: a body spec over the villager's scheme, garment pieces drawn over the raw
-- body, and a weapon.
local function livery(ramp, mid, dark)
  return { R = c(ramp, mid), M = c(ramp, mid - 1), D = c(ramp, dark), A = c("paper", 6), P = c(ramp, dark),
    F = c("ink", 2), X = c("paper", 6), x = c("paper", 5), Y = c("paper", 4) }
end
local BANDIT = { R = c("wood", 4), M = c("wood", 3), D = c("wood", 2), A = c("earth", 3), P = c("cloth", 3),
  F = c("ink", 2), X = c("ink", 3), x = c("ink", 2), Y = c("earth", 3) }
local WANDERER = { R = c("stone", 4), M = c("stone", 3), D = c("stone", 2), A = c("wood", 3), P = c("stone", 3),
  F = c("ink", 2), X = c("stone", 4), x = c("stone", 3), Y = c("stone", 2) }

local pieces = {}

-- The bandit's bare arms and chest under its open vest, the vest's fur trim, and its hair tied up
-- rough above the mask.
function pieces.bandit(cv)
  for y = 11, 13 do cv:set(4, y, c("skin", 5)); cv:set(11, y, c("skin", 4)) end
  for y = 10, 11 do cv:set(7, y, c("skin", 5)); cv:set(8, y, c("skin", 4)) end
  cv:set(7, 12, c("skin", 4))
  cv:set(5, 10, c("earth", 6)); cv:set(9, 10, c("earth", 6)); cv:set(5, 11, c("earth", 5)); cv:set(10, 11, c("earth", 5))
  cv:set(7, 3, c("ink", 2)); cv:set(8, 2, c("ink", 2)); cv:set(8, 3, c("ink", 1)); cv:set(6, 3, c("ink", 1))
end

-- The wanderer's pack on its back, a bedroll strapped on top of it.
function pieces.wanderer(cv)
  cv:rect(1, 10, 3, 4, c("earth", 4)); cv:vline(1, 10, 4, c("earth", 3)); cv:hline(1, 13, 3, c("earth", 3))
  cv:hline(1, 9, 4, c("cloth", 5)); cv:set(4, 9, c("cloth", 4))
  cv:set(5, 11, c("wood", 3)); cv:set(6, 12, c("wood", 3))
end

-- The escort's paper headband across its brow.
function pieces.escort(cv)
  for x = 5, 10 do
    if cv:get(x, BROW) ~= 0 then cv:set(x, BROW, c("paper", 6)) end
  end
  cv:set(4, BROW, c("paper", 6)); cv:set(3, BROW + 1, c("paper", 5)); cv:set(4, BROW + 2, c("paper", 5))
end

-- The wanderer's sword slung across its back, its hilt over the shoulder, behind the body.
local function slung(cv)
  cv:line(2, 15, 6, 6, c("wood", 3)); cv:set(6, 5, c("stone", 6)); cv:set(7, 4, c("stone", 6))
end

local KINDS = {
  { name = "bandit_dao", label = "BANDIT, DAO", spec = { faction = "villager", head = "mask", body = "tunic", over = BANDIT },
    ramp = "wood", piece = "bandit", weapon = "dao" },
  { name = "bandit_club", label = "BANDIT, CLUB", spec = { faction = "villager", head = "mask", body = "tunic", over = BANDIT },
    ramp = "wood", piece = "bandit", weapon = "club" },
  { name = "bandit_axe", label = "BANDIT, AXE", spec = { faction = "villager", head = "mask", body = "tunic", over = BANDIT },
    ramp = "wood", piece = "bandit", weapon = "axe" },
  { name = "wanderer", label = "WANDERER", spec = { faction = "villager", head = "hood", body = "robe", over = WANDERER },
    ramp = "stone", piece = "wanderer", weapon = "sword", carried = "slung" },
  { name = "escort_water_spear", label = "ESCORT A WATER, SPEAR",
    spec = { faction = "villager", head = "topknot", body = "tunic", over = livery("water", 4, 2) }, ramp = "water",
    piece = "escort", weapon = "spear" },
  { name = "escort_water_pennant", label = "ESCORT A WATER, PENNANT",
    spec = { faction = "villager", head = "topknot", body = "tunic", over = livery("water", 4, 2) }, ramp = "water",
    piece = "escort", weapon = "spear", carried = "pennant" },
  { name = "escort_stone_spear", label = "ESCORT B STONE, SPEAR",
    spec = { faction = "villager", head = "topknot", body = "tunic", over = livery("stone", 4, 2) }, ramp = "stone",
    piece = "escort", weapon = "spear" },
  { name = "escort_stone_pennant", label = "ESCORT B STONE, PENNANT",
    spec = { faction = "villager", head = "topknot", body = "tunic", over = livery("stone", 4, 2) }, ramp = "stone",
    piece = "escort", weapon = "spear", carried = "pennant" },
}

-- One frame of kind `k`: pose 0 carrying, 1 to 3 the strike's; step and bob for the walk.
local LEANS = { [1] = { -1, 0 }, [2] = { 1, 0 }, [3] = { 1, 1 } }
local function frame(k, pose, step, bob)
  local cv = S.body(copy(k.spec, { raw = true, step = step }))
  if pieces[k.piece] then pieces[k.piece](cv) end
  local dx, dy = 0, 0
  if pose > 0 then dx, dy = LEANS[pose][1], LEANS[pose][2] end
  cv = lean(cv, dx, dy)
  if k.carried == "slung" and pose == 0 then
    local back = px.canvas(cv.w, cv.h)
    slung(back)
    back:stamp(cv, 0, 0)
    cv = back
  end
  if k.carried == "pennant" and pose == 0 then
    pennant(cv, HAND[1] + dx, HAND[2] + dy, pose)
  else
    weapons[k.weapon](cv, HAND[1] + dx, HAND[2] + dy, pose)
  end
  cv:selout(0, 0, 16, 18, k.ramp)
  local out = px.canvas(W, H)
  out:stamp(cv, 0, TOP)
  if bob then
    local feet = TOP + FEET
    local lifted = px.canvas(W, H)
    for key, i in pairs(out.p) do
      local x, y = key % W, key // W
      if y < feet - 1 then lifted:set(x, y - 1, i) else lifted:set(x, y, i) end
    end
    for x = 0, W - 1 do
      if lifted:get(x, feet - 2) == 0 then lifted:set(x, feet - 2, out:get(x, feet - 2)) end
    end
    out = lifted
  end
  return out
end

local sheets = {}
for _, k in ipairs(KINDS) do
  local sets = {
    { "idle", { frame(k, 0) }, 500 },
    { "walk", { frame(k, 0, "left"), frame(k, 0, nil, true), frame(k, 0, "right"), frame(k, 0, nil, true) }, 150 },
    { "strike", { frame(k, 1), frame(k, 2), frame(k, 3) }, 80 },
  }
  sheets[k.name] = sets
  local frames, tags = {}, {}
  for _, s in ipairs(sets) do
    local first = #frames + 1
    for _, cv in ipairs(s[2]) do frames[#frames + 1] = cv end
    tags[#tags + 1] = { s[1], first, #frames, s[3] }
  end
  px.save_frames(W, H, frames, tags, here .. "/roads/" .. k.name .. ".aseprite")
end

-- The villager, the production template's, for comparison.
local function villager(step, bob)
  local cv = px.canvas(W, H)
  cv:stamp(S.body({ faction = "villager", head = "bun", body = "tunic", step = step }), 0, TOP)
  return cv
end

-- The caravan --------------------------------------------------------------------------------------

local CW, CH = 32, 24
-- The mule in profile facing right: torso, neck, a long head with tall ears, a short tail; its legs
-- drawn by gait as the animals mockup draws them. Earth ramp, a pale muzzle and belly, ink hooves.
local MULE_BODY = {
  ".............RR..",
  ".............CR..",
  "............CCCC.",
  "...........CCECCC",
  "...........CC.BBN",
  ".LLLLLLLLLCCC....",
  "TCCCCCCCCCCCC....",
  "TCCCCCCCCCCC.....",
  ".CCCCCCCCCCC.....",
  "..BBBBBBBBB......",
}
local MULE_GAIT = {
  { { 1, 0 }, { -1, 0 }, { -1, 0 }, { 1, 0 } },
  { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } },
  { { -1, 0 }, { 1, 0 }, { 1, 0 }, { -1, 0 } },
  { { 0, 0 }, { 0, 1 }, { 0, 1 }, { 0, 0 } },
}
local function mule(gait, packed)
  local cv = px.canvas(CW, CH)
  local map = { C = c("earth", 4), L = c("earth", 5), D = c("earth", 3), B = c("earth", 6), E = c("ink", 1),
    N = c("ink", 2), R = c("earth", 6), T = c("ink", 3) }
  local ox, oy = 7, 22 - 7 - #MULE_BODY + 1
  local legs_top = oy + #MULE_BODY - 1
  local feet = gait or { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } }
  local roots = { { ox + 10, false }, { ox + 8, true }, { ox + 2, false }, { ox + 4, true } }
  -- Far legs first, a step darker.
  for _, n in ipairs({ 2, 4, 1, 3 }) do
    local r, f = roots[n], feet[n]
    local colour = r[2] and c("earth", 3) or c("earth", 4)
    cv:line(r[1], legs_top, r[1] + f[1], 22 - f[2], colour)
    cv:line(r[1] + 1, legs_top, r[1] + 1 + f[1], 22 - f[2], colour)
    cv:hline(r[1] + f[1], 22 - f[2], 2, c("ink", 2))
    if n == 4 then cv:sprite(ox, oy, MULE_BODY, map) end
  end
  if packed then
    -- Panniers over its back, a sack each side, roped.
    cv:rect(ox + 2, oy + 3, 8, 3, c("cloth", 5)); cv:hline(ox + 2, oy + 3, 8, c("cloth", 6))
    cv:rect(ox + 3, oy + 6, 3, 3, c("cloth", 4)); cv:rect(ox + 7, oy + 6, 3, 3, c("cloth", 4))
    cv:vline(ox + 6, oy + 3, 6, c("wood", 3))
  end
  cv:selout(0, 0, CW, CH, "earth")
  return cv
end

-- The cart: a box bed of planks with crates and sacks under a cloth, shafts reaching forward, a
-- spoked wheel at its middle that turns between two frames.
local function cart(turn)
  local cv = px.canvas(CW, CH)
  -- The load under its cloth, then the bed.
  cv:rect(5, 9, 18, 4, c("cloth", 5)); cv:hline(5, 9, 18, c("cloth", 6)); cv:rect(8, 7, 6, 2, c("cloth", 5))
  cv:hline(8, 7, 6, c("cloth", 6)); cv:vline(13, 9, 4, c("wood", 3)); cv:vline(19, 9, 4, c("wood", 3))
  cv:rect(16, 7, 4, 2, c("wood", 5)); cv:hline(16, 7, 4, c("wood", 6))
  cv:rect(4, 13, 20, 3, c("wood", 4)); cv:hline(4, 13, 20, c("wood", 5)); cv:hline(4, 15, 20, c("wood", 3))
  -- The shafts, forward to the mule.
  cv:line(24, 14, 31, 13, c("wood", 4))
  -- The wheel.
  cv:each_ellipse(13, 18, 4.5, 4.5, function(x, y, d) if d > 0.7 then cv:set(x, y, c("wood", 3)) end end)
  cv:set(13, 18, c("stone", 4))
  if turn == 1 then
    cv:line(10, 18, 16, 18, c("wood", 4)); cv:line(13, 15, 13, 21, c("wood", 4))
  else
    cv:line(11, 16, 15, 20, c("wood", 4)); cv:line(15, 16, 11, 20, c("wood", 4))
  end
  cv:set(13, 18, c("stone", 4))
  cv:selout(0, 0, CW, CH, "wood")
  return cv
end

local caravan = {
  { "mule_idle", { mule() }, 500 },
  { "mule_walk", { mule(MULE_GAIT[1]), mule(MULE_GAIT[2]), mule(MULE_GAIT[3]), mule(MULE_GAIT[4]) }, 150 },
  { "pack_idle", { mule(nil, true) }, 500 },
  { "pack_walk", { mule(MULE_GAIT[1], true), mule(MULE_GAIT[2], true), mule(MULE_GAIT[3], true), mule(MULE_GAIT[4], true) }, 150 },
  { "cart", { cart(1), cart(2) }, 150 },
}
do
  local frames, tags = {}, {}
  for _, s in ipairs(caravan) do
    local first = #frames + 1
    for _, cv in ipairs(s[2]) do frames[#frames + 1] = cv end
    tags[#tags + 1] = { s[1], first, #frames, s[3] }
  end
  px.save_frames(CW, CH, frames, tags, here .. "/roads/caravan.aseprite")
end

-- The contact sheet -------------------------------------------------------------------------------

local CELL, GAP = 17, 6
local ROW_W = 8 * CELL + 3 * GAP
local ZOOM, RW, RH = 3, 220, 104
local SW = math.max(8 + 2 * (130 + ROW_W), RW * ZOOM + 16)
local SH = 30 + 4 * 40 + 46 + 82 + RH * ZOOM + 20
local bg = px.canvas(SW, SH)
local ground = px.canvas(SW, SH)
local shadows = px.canvas(SW, SH)
local things = px.canvas(SW, SH)
local labels = px.canvas(SW, SH)
bg:rect(0, 0, SW, SH, c("ink", 2))

local function grass(cv, x, y, w, h)
  for yy = y, y + h - 1 do
    for xx = x, x + w - 1 do
      local n = px.noise(xx, yy, 9, 3) + (px.hash(xx, yy, 9) - 0.5) * 0.15
      local col = c("grass", 4)
      if n > 0.72 then col = c("grass", 5) elseif n < 0.25 then col = c("grass", 3) end
      cv:set(xx, yy, col)
    end
  end
end
-- A road of packed earth, its edges ragged into the grass.
local function road(cv, x, y, w, h)
  for yy = y, y + h - 1 do
    for xx = x, x + w - 1 do
      local edge = math.min(yy - y, y + h - 1 - yy)
      local n = px.hash(xx, yy, 21)
      if edge > 1 or n > 0.45 then
        local col = c("earth", 5)
        if px.noise(xx, yy, 6, 7) < 0.3 then col = c("earth", 4) elseif px.hash(xx, yy, 4) > 0.93 then col = c("earth", 6) end
        cv:set(xx, yy, col)
      end
    end
  end
end
local function shadow(cv, x, y, rx)
  cv:each_ellipse(x, y, rx or 6, 2.5, function(sx, sy, d)
    if px.bayer(sx, sy) < 0.8 - 0.7 * d then cv:set(sx, sy, c("ink", 1)) end
  end)
end
local function mirror(cv)
  local out = px.canvas(cv.w, cv.h)
  for key, i in pairs(cv.p) do out:set(cv.w - 1 - key % cv.w, key // cv.w, i) end
  return out
end

labels:text(8, 5, "MURIM24  ROAD LOOKS MOCKUP (#172)", c("paper", 6))
labels:text(8, 12, "NATIVE PIXELS. THE PRODUCTION BODY (16X24, SEL-OUT, ADR 0069 RAMPS). BANDITS AND WANDERERS CARRY NO FACTION CUE;",
  c("stone", 5))
labels:text(8, 19, "THE BANDITS' HOSTILITY IS THEIR RED NAMEPLATE'S. TWO LIVERIES FOR THE ESCORT AGENCY: A WATER, B STONE.", c("stone", 5))

-- People, two kinds to a row.
local y = 30
for n, k in ipairs(KINDS) do
  local col = (n - 1) % 2
  local x0 = 8 + col * (130 + ROW_W)
  if col == 0 and n > 1 then y = y + 40 end
  labels:text(x0, y + 12, k.label, c("paper", 6))
  grass(ground, x0 + 122, y + 2, ROW_W + 4, 28)
  local x = x0 + 124
  for _, s in ipairs(sheets[k.name]) do
    if n <= 2 then labels:text(x, y + 31, s[1]:upper(), c("stone", 5)) end
    for _, cv in ipairs(s[2]) do things:stamp(cv, x, y + 4); x = x + CELL end
    x = x + GAP
  end
end
y = y + 46

-- The caravan.
labels:text(8, y, "CARAVAN  A MULE AND ITS CART (WHEEL ON TWO FRAMES), A PACK MULE, AND THE VILLAGER FOR COMPARISON", c("paper", 5))
grass(ground, 8, y + 7, SW - 16, 30)
do
  local x = 12
  for _, s in ipairs(caravan) do
    local name = s[1]:upper():gsub("_", " ")
    labels:text(x, y + 39, name, c("stone", 5))
    local start = x
    for _, cv in ipairs(s[2]) do things:stamp(cv, x, y + 9); x = x + 26 end
    x = math.max(x, start + px.text_width(name) + 4) + 8
  end
  for n = 1, 2 do things:stamp(villager(), x, y + 9); x = x + 18 end
  labels:text(x - 36, y + 39, "VILLAGER", c("stone", 5))
end
y = y + 50

-- The two liveries beside azure and jade, and the stone escort beside the stone wanderer.
labels:text(8, y, "LIVERY  A WATER AND B STONE BESIDE AZURE (ORTHODOX) AND JADE (PLAYER); B BESIDE THE WANDERER, TOLD APART BY",
  c("paper", 5))
labels:text(8, y + 7, "ITS PAPER HEADBAND AND PENNANT", c("paper", 5))
grass(ground, 8, y + 14, SW - 16, 58)
do
  local orth = px.canvas(W, H)
  orth:stamp(S.body({ faction = "orthodox", head = "topknot", weapon = "sword" }), 0, TOP)
  local x = 16
  local function show(cv, label)
    shadow(shadows, x + 8, y + 58)
    things:stamp(cv, x, y + 36)
    labels:text(x - 2, y + 62, label, c("stone", 5))
    x = x + 34
  end
  local function swatch(colour, label)
    things:rect(x, y + 34, 12, 12, colour)
    labels:text(x - 2, y + 62, label, c("stone", 5))
    x = x + 34
  end
  show(sheets.escort_water_spear[1][2][1], "A"); show(sheets.escort_water_pennant[1][2][1], "A")
  swatch(c("water", 4), "WATER"); show(orth, "ORTHO"); swatch(c("azure", 4), "AZURE"); swatch(c("jade", 4), "JADE")
  x = x + 20
  show(sheets.escort_stone_spear[1][2][1], "B"); show(sheets.escort_stone_pennant[1][2][1], "B")
  show(sheets.wanderer[1][2][1], "WANDER"); swatch(c("stone", 4), "STONE")
end
y = y + 82

-- The road at play zoom: the caravan heading right with its guards, four bandits closing in from
-- the grass, and a wanderer beside a villager at the roadside.
local scene = { ground = px.canvas(RW, RH), shadows = px.canvas(RW, RH), things = px.canvas(RW, RH) }
grass(scene.ground, 0, 0, RW, RH)
road(scene.ground, 0, 40, RW, 32)
local function person(cv, x, fy, flip)
  shadow(scene.shadows, x, fy)
  scene.things:stamp(flip and mirror(cv) or cv, x - 8, fy - 22)
end
local function beast(cv, x, fy, flip, rx)
  shadow(scene.shadows, x, fy, rx or 9)
  scene.things:stamp(flip and mirror(cv) or cv, x - 16, fy - 22)
end
local function f(name, set, n) for _, s in ipairs(sheets[name]) do if s[1] == set then return s[2][n] end end end
local cast = {
  { 26, function(fy) person(f("bandit_axe", "walk", 2), 150, fy, true) end },
  { 34, function(fy) person(f("bandit_club", "strike", 1), 176, fy, true) end },
  { 46, function(fy) person(f("escort_water_spear", "strike", 2), 128, fy) end },
  { 50, function(fy) beast(mule(MULE_GAIT[1], true), 40, fy) end },
  { 54, function(fy) person(f("escort_water_pennant", "walk", 1), 112, fy) end },
  { 58, function(fy)
    shadow(scene.shadows, 58, fy, 10)
    scene.things:stamp(cart(1), 60 - 16, fy - 22)
    scene.things:stamp(mule(MULE_GAIT[3]), 84 - 16, fy - 22)
    shadow(scene.shadows, 84, fy, 9)
  end },
  { 66, function(fy) person(f("escort_water_spear", "idle", 1), 64, fy) end },
  { 70, function(fy) person(f("bandit_dao", "strike", 2), 150, fy, true) end },
  { 84, function(fy) person(f("bandit_dao", "walk", 1), 172, fy, true) end },
  { 92, function(fy) person(f("wanderer", "idle", 1), 24, fy) end },
  { 93, function(fy) person(villager(), 44, fy, true) end },
}
for _, actor in ipairs(cast) do actor[2](actor[1]) end
labels:text(8, y, "PLAY ZOOM 3X  A ROAD: THE CARAVAN WITH ITS GUARDS (LIVERY A) WAYLAID BY A BANDIT BAND; A WANDERER BESIDE A VILLAGER",
  c("paper", 5))
for _, layer in ipairs({ scene.ground, scene.shadows, scene.things }) do
  for key, i in pairs(layer.p) do
    local x, yy = key % RW, key // RW
    ground:rect(8 + x * ZOOM, y + 8 + yy * ZOOM, ZOOM, ZOOM, i)
  end
end

px.save(SW, SH, { { "bg", bg }, { "ground", ground }, { "shadows", shadows }, { "things", things }, { "labels", labels } },
  here .. "/roads/roads_sheet.aseprite")

-- Every person's frame on grass, a kind to a row, then the caravan's, for the 6x export.
do
  local FW, FH = 4 + 8 * 18 + 3 * 6, 2 + #KINDS * 26 + 26
  FW = math.max(FW, 4 + 15 * 34)
  local fb = px.canvas(FW, FH)
  grass(fb, 0, 0, FW, FH)
  for n, k in ipairs(KINDS) do
    local x = 4
    for _, s in ipairs(sheets[k.name]) do
      for _, cv in ipairs(s[2]) do fb:stamp(cv, x, 2 + (n - 1) * 26); x = x + 18 end
      x = x + 6
    end
  end
  local x = 4
  for _, s in ipairs(caravan) do
    for _, cv in ipairs(s[2]) do fb:stamp(cv, x, 2 + #KINDS * 26); x = x + 34 end
  end
  px.save(FW, FH, { { "frames", fb } }, here .. "/roads/roads_frames.aseprite")
end
