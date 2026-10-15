-- The mockup of #171's animals, for the PM's approval before ADR 0067's step 6 (the view) is built:
-- the base pack's wolf, tiger, boar, deer and spirit tiger at the 2.5D angle (ADR 0059) on ADR 0069's
-- ramps, outlined as a body is, the whole ring in the darkest step of the coat's ramp. It draws, from
-- the production palette (art/pixel.lua) and body template (art/sprites.lua):
--   animals/<kind>.aseprite, one sheet a kind, its frames tagged idle, walk (4, 150 ms), run (4,
--     100 ms), graze (deer and boar: 2, 300 ms), strike (80 ms), hurt (the struck recoil, 2, 80 ms),
--     wounded (standing, blood on its flank below half health) and dead (on its side, eyes shut);
--     the spirit tiger in two sheets, spirit_tiger_fire and spirit_tiger_water, one for each
--     candidate colour of its qi marks;
--   animals/animals_sheet.aseprite, the contact sheet: every frame on grass at native size, a herd
--     of deer and a wolf pack beside a person for scale, the spirit tiger's two candidates beside
--     the jade it must not read as, and the same scene at play zoom, 3x;
--   animals/animals_frames.aseprite, every frame on grass, for the 6x export.
-- Sizes are real proportions against the production body, 15 pixels tall with its outline for
-- 1.7 m (art-direction §Scale): a deer 18 long and 15 tall to its antlers, a wolf 16 by 10, a boar
-- 15 by 9, a tiger 25 by 11 with its tail, a spirit tiger half again a tiger. Frames are 32x24, the
-- tigers' 40x24 and the spirit tiger's 48x32, the feet on the frame's second row from the bottom and
-- the animal's point at the middle of its width. An animal faces right, mirrored by the view to face
-- left, in profile only.
-- Run: aseprite --batch --script art/mockup/animals.lua, then export as the header of
-- art/mockup/animals.sh says.

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)
local c = px.c

-- The kinds. Each names its frame, its coat's ramp and the steps its marks take, its torso, legs,
-- head and tail, and which states it has.
local kinds = {}

-- Gait: per frame, each leg's foot offset across and lift, in the order near fore, far fore, near
-- hind, far hind, and the torso's rise. Diagonal pairs move together at a walk; a run gallops,
-- gathered, then stretched in the air, then landing.
local WALK = {
  { feet = { { 1, 0 }, { -1, 0 }, { -1, 0 }, { 1, 0 } }, rise = 0 },
  { feet = { { 0, 1 }, { 0, 0 }, { 0, 0 }, { 0, 1 } }, rise = 1 },
  { feet = { { -1, 0 }, { 1, 0 }, { 1, 0 }, { -1, 0 } }, rise = 0 },
  { feet = { { 0, 0 }, { 0, 1 }, { 0, 1 }, { 0, 0 } }, rise = 1 },
}
local RUN = {
  { feet = { { -1, 0 }, { -2, 1 }, { 2, 0 }, { 1, 1 } }, rise = 0 },
  { feet = { { 2, 1 }, { 1, 2 }, { -2, 1 }, { -3, 2 } }, rise = 2 },
  { feet = { { 2, 0 }, { 3, 1 }, { -1, 1 }, { -2, 1 } }, rise = 1 },
  { feet = { { 0, 1 }, { 0, 0 }, { 1, 0 }, { 0, 1 } }, rise = 0 },
}
local STAND = { feet = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } }, rise = 0 }

-- A head is rows of letters from its top-left; `at` is where its top-left sits relative to the
-- front-top corner of the torso. Letters: C coat, L coat's lighter step, D coat's darker step,
-- B belly or muzzle, E eye, e shut eye, N nose and mouth, H horn or antler, h antler's dark step,
-- T tusk or tooth, S stripe, Q qi mark, q qi mark's dark step, R inside the ear or the mouth.

kinds.deer = {
  W = 32, H = 24, coat = "earth", belly = c("paper", 6), mark = c("paper", 5), dark = c("ink", 2),
  horn = c("wood", 6), horn_dark = c("wood", 5), inner = c("earth", 6),
  torso = { len = 12, top = 14, depth = 4 }, legs = { len = 5, w = 1, fore = 10, hind = 2, hoof = true },
  tail = { { 0, -1, "B" }, { -1, 0, "B" } },
  heads = {
    up = { at = { -2, -7 }, rows = {
      "...H.H.", "....Hh.", "..RLLH.", "..CCECN", "...CBB.", "..CC...", ".CC....", "CC.....",
    } },
    hind = { at = { -2, -7 }, rows = {
      ".......", ".......", "..RLL..", "..CCECN", "...CBB.", "..CC...", ".CC....", "CC.....",
    } },
    down = { at = { -1, 1 }, rows = {
      "CC......", ".CCCH...", "..RCCH..", "...CCCC.", "...CECCB", "....CCBN",
    } },
    hind_down = { at = { -1, 1 }, rows = {
      "CC......", ".CCC....", "..RCC...", "...CCCC.", "...CECCB", "....CCBN",
    } },
    shut = { at = { -2, -6 }, rows = {
      "...H.H.", "....Hh.", "..RLLH.", "..CCeCN", "...CBB.", "..CC...", ".CC....", "CC.....",
    } },
    dead = { rows = { "H....", "hHRC.", ".CCCC", ".CeCN", "..BB." } },
  },
  spots = { { 3, 1 }, { 6, 1 }, { 9, 1 }, { 4, 2 }, { 8, 2 } },
  graze = true, strike = "kick",
}

kinds.wolf = {
  W = 32, H = 24, coat = "stone", belly = c("stone", 6), dark = c("ink", 2), inner = c("stone", 6),
  saddle = c("stone", 3),
  torso = { len = 10, top = 15, depth = 4 }, legs = { len = 4, w = 1, fore = 8, hind = 1 },
  chest = { 8, 2, 2.5, 2.5 },
  tail = { { 0, 0, "C" }, { -1, 1, "C" }, { -2, 1, "D" }, { -2, 2, "C" }, { -3, 2, "B" }, { -3, 3, "B" } },
  heads = {
    up = { at = { -1, -4 }, rows = {
      ".C.....", ".RC....", "CCCC...", "CCECCC.", "BBCCCCN", ".BBBB..",
    } },
    shut = { at = { -1, -3 }, rows = {
      ".C.....", ".RC....", "CCCC...", "CCeCCC.", "BBCCCCN", ".BBBB..",
    } },
    bite = { at = { 0, -3 }, rows = {
      ".C......", ".RC.....", "CCCC....", "CCECCCCN", "BBCCRRT.", ".BBBBBTN",
    } },
    low = { at = { 0, -2 }, rows = {
      ".C.....", ".RC....", "CCCC...", "CCECCC.", "BBCCCCN", ".BBBB..",
    } },
    dead = { rows = { ".RC...", "CCCC..", "CCeCCN", ".BBBB." } },
  },
  graze = false, strike = "bite",
}

kinds.boar = {
  W = 32, H = 24, coat = "wood", belly = c("wood", 5), dark = c("ink", 1), inner = c("wood", 6),
  bristle = c("ink", 3), tusk = c("paper", 7), snout = c("skin", 3),
  torso = { len = 11, top = 16, depth = 4 }, legs = { len = 3, w = 1, fore = 8, hind = 2 },
  chest = { 7, 0, 3.5, 3 },
  tail = { { 0, 0, "D" }, { -1, 1, "D" } },
  heads = {
    up = { at = { -2, -1 }, rows = {
      ".R.....", "CCL....", "CLEC...", "CCCCCC.", "DCCCCCP", ".DTCN..",
    } },
    shut = { at = { -2, 0 }, rows = {
      ".R.....", "CCL....", "CLeC...", "CCCCCC.", "DCCCCCP", ".DTCN..",
    } },
    down = { at = { -2, 1 }, rows = {
      ".R.....", "CCL....", "CLEC...", "CCCCC..", "DCCCCC.", ".DTCCP.", "....NN.",
    } },
    charge = { at = { -1, 1 }, rows = {
      ".R......", "CCLL....", "CLECCC..", "CCCCCCCP", ".DC.TN..", "....T...",
    } },
    dead = { rows = { ".R...", "CCCC.", "CeCCP", ".CTN." } },
  },
  graze = true, strike = "charge",
}

kinds.tiger = {
  W = 40, H = 24, coat = "amber", belly = c("paper", 7), dark = c("ink", 1), inner = c("paper", 6),
  stripe = c("ink", 2),
  torso = { len = 17, top = 14, depth = 5 }, legs = { len = 4, w = 2, fore = 13, hind = 2, paw = true },
  chest = { 14, 2, 3, 3 },
  tail = { { 0, 0, "C" }, { -1, 1, "C" }, { -2, 1, "S" }, { -3, 2, "C" }, { -4, 2, "C" }, { -5, 2, "S" },
    { -6, 2, "C" }, { -7, 1, "C" }, { -8, 0, "S" } },
  stripes = { { 2, 3, 1 }, { 4, 2, 0 }, { 6, 4, 1 }, { 9, 3, 1 }, { 11, 4, 0 }, { 13, 3, 1 }, { 15, 2, 0 } },
  heads = {
    up = { at = { -2, -3 }, rows = {
      "..DD....", ".DRCL...", ".CSCLL..", "CSCCELLB", "CCSCCBBN", "BBCCBBN.", ".BBBBB..",
    } },
    shut = { at = { -2, -2 }, rows = {
      "..DD....", ".DRCL...", ".CSCLL..", "CSCCeLLB", "CCSCCBBN", "BBCCBBN.", ".BBBBB..",
    } },
    roar = { at = { -1, -3 }, rows = {
      "..DD.....", ".DRCL....", ".CSCLLL..", "CSCCELLBN", "CCSCCRRR.", "BBCCRRTT.", ".BBBBTN..", "..BBBB...",
    } },
    low = { at = { -1, -2 }, rows = {
      "..DD....", ".DRCL...", ".CSCLL..", "CSCCELLB", "CCSCCBBN", "BBCCBBN.", ".BBBBB..",
    } },
    dead = { rows = { ".DRCL..", "CCSCLLL", "CSCCeBN", "BBCBBB.", ".BBBB.." } },
  },
  graze = false, strike = "swipe",
}

-- The spirit tiger: half again a tiger, a white coat with ink stripes, qi marks along them and its
-- eye glowing; two candidates for the marks' colour, neither a faction's accent, the player's jade
-- nor the hostile crimson.
local function spirit(mark, mark_dark)
  return {
    W = 48, H = 32, coat = "robe", belly = c("robe", 7), dark = c("ink", 1), inner = c("robe", 6),
    stripe = c("ink", 2), qi = mark, qi_dark = mark_dark,
    torso = { len = 26, top = 19, depth = 7 }, legs = { len = 6, w = 3, fore = 20, hind = 3, paw = true },
    chest = { 21, 3, 4.5, 4 },
    tail = { { 0, 0, "C" }, { -1, 1, "C" }, { -2, 1, "S" }, { -3, 2, "C" }, { -4, 2, "C" }, { -5, 3, "Q" },
      { -6, 3, "C" }, { -7, 3, "C" }, { -8, 3, "S" }, { -9, 2, "C" }, { -10, 1, "C" }, { -11, 0, "Q" },
      { -11, -1, "C" } },
    stripes = { { 3, 4, 1 }, { 6, 3, 0 }, { 9, 5, 1 }, { 12, 4, 1 }, { 15, 5, 0 }, { 18, 4, 1 }, { 21, 3, 1 },
      { 23, 2, 0 } },
    heads = {
      up = { at = { -3, -5 }, rows = {
        "...DD......", "..DRCL.....", "..CCCLLL...", ".CSCCCLLL..", "CCCSCQELLB.", "CSCCSCCCBBB",
        "BCSCCCCBBBN", "BBCCCBBBBN.", ".BBBBBBBB..", "..BBBB.....",
      } },
      shut = { at = { -3, -4 }, rows = {
        "...DD......", "..DRCL.....", "..CCCLLL...", ".CSCCCLLL..", "CCCSCCeLLB.", "CSCCSCCCBBB",
        "BCSCCCCBBBN", "BBCCCBBBBN.", ".BBBBBBBB..", "..BBBB.....",
      } },
      roar = { at = { -2, -5 }, rows = {
        "...DD.......", "..DRCL......", "..CCCLLL....", ".CSCCCLLL...", "CCCSCQELLBBN", "CSCCSCCBBBN.",
        "BCSCCCRRRR..", "BBCCCRRTTT..", ".BBBBBTN....", "..BBBBB.....",
      } },
      low = { at = { -2, -4 }, rows = {
        "...DD......", "..DRCL.....", "..CCCLLL...", ".CSCCCLLL..", "CCCSCQELLB.", "CSCCSCCCBBB",
        "BCSCCCCBBBN", "BBCCCBBBBN.", ".BBBBBBBB..", "..BBBB.....",
      } },
      dead = { rows = { "..DRCL....", ".CCCCLLL..", "CSSCCCLLLB", "CCSCCeCBBN", "BBCCCBBBB.", ".BBBBBBB.." } },
    },
    graze = false, strike = "swipe",
  }
end
-- Candidate A: the fire ramp's yellow and its darker orange, an emitter's, so the marks keep their
-- colour at night and bloom beside a hearth's light. Candidate B: the water ramp's pale steps, cool.
kinds.spirit_tiger_fire = spirit(c("fire", 4), c("fire", 3))
kinds.spirit_tiger_water = spirit(c("water", 6), c("water", 5))

local ORDER = { "deer", "wolf", "boar", "tiger", "spirit_tiger_fire", "spirit_tiger_water" }

-- Drawing ----------------------------------------------------------------------------------------

-- The point of a kind's frame: the middle of its width, on the feet's row.
local function anchor(k) return k.W // 2, k.H - 2 end

local function letters(k)
  return {
    C = c(k.coat, 4), L = c(k.coat, 5), D = c(k.coat, 3), B = k.belly, E = k.qi or k.dark, e = k.dark,
    N = k.dark, H = k.horn, h = k.horn_dark, T = k.tusk or c("paper", 7), S = k.stripe,
    Q = k.qi, q = k.qi_dark, R = k.inner, P = k.snout,
  }
end

-- Shades the coat's base step: lit where nothing of the animal lies above it, toward the light, and
-- a step darker along its underside and on the far legs.
local function shade(cv, k, far)
  local base, out = c(k.coat, 4), {}
  for key, i in pairs(cv.p) do
    local x, y = key % cv.w, key // cv.w
    if far[key] and (i == base or i == c(k.coat, 5)) then
      out[key] = c(k.coat, 3)
    elseif i == base then
      if cv:get(x, y - 1) == 0 then
        out[key] = c(k.coat, 5)
      elseif cv:get(x, y + 1) == 0 then
        out[key] = c(k.coat, 3)
      end
    end
  end
  for key, i in pairs(out) do cv.p[key] = i end
end

-- A leg from (x, top) to the ground at g: a fore leg straight, a hind leg back at the hock; a paw
-- or a hoof at its foot. Returns the pixels it set.
local function leg(cv, k, x, top, g, foot_dx, lift, hind, colour)
  local legs, set = k.legs, {}
  local function put(px_, py_, i)
    if cv:get(px_, py_) == 0 then set[#set + 1] = py_ * cv.w + px_ end
    cv:set(px_, py_, i)
  end
  local foot_x, foot_y = x + foot_dx, g - lift
  local hock_y = top + (foot_y - top) // 2 + 1
  local hock_x = hind and (x - 1 + foot_dx // 2) or (x + foot_dx // 2)
  for w = 0, legs.w - 1 do
    local seg = {}
    local function line(x0, y0, x1, y1)
      local dx, dy = math.abs(x1 - x0), -math.abs(y1 - y0)
      local sx, sy = x0 < x1 and 1 or -1, y0 < y1 and 1 or -1
      local err = dx + dy
      while true do
        put(x0, y0, colour)
        if x0 == x1 and y0 == y1 then break end
        local e2 = 2 * err
        if e2 >= dy then err = err + dy; x0 = x0 + sx end
        if e2 <= dx then err = err + dx; y0 = y0 + sy end
      end
    end
    line(x + w, top, hock_x + w, hock_y)
    line(hock_x + w, hock_y, foot_x + w, foot_y)
  end
  if legs.hoof then put(foot_x, foot_y, k.dark) end
  if legs.paw then put(foot_x + legs.w, foot_y, colour) end
  return set
end

-- One frame. pose: gait (feet and rise), head (a key of k.heads), dx and dy (the whole body's
-- shift), lean (the fore end raised, or lowered when negative), wound (blood on the flank), lying.
local function frame(k, pose)
  local cv = px.canvas(k.W, k.H)
  local L = letters(k)
  local ax, g = anchor(k)
  local t = k.torso
  local gait = pose.gait or STAND
  local x0 = ax - t.len // 2 + (pose.dx or 0)
  local top = t.top - gait.rise + (pose.dy or 0)
  local far = {}

  if pose.lying then
    -- On its side, its back toward the top of the screen: the torso a low oval just off the ground,
    -- the legs stretched out stiff toward the viewer down to it, the head flat, the eyes shut, no
    -- shadow.
    local depth = math.max(3, t.depth - 1)
    local cx = ax - 1
    local bottom = g - math.max(2, k.legs.len // 2)
    local cy = bottom - (depth - 1) / 2
    for n, fx in ipairs({ t.len // 2 - 3, t.len // 2 - 1, -t.len // 2 + 2, -t.len // 2 + 4 }) do
      local lx = cx + fx
      local colour = (n % 2 == 0) and L.D or L.C
      for s = 0, g - bottom do
        for w = 0, math.max(0, k.legs.w - 2) do cv:set(lx + s + w, bottom + s, colour) end
      end
      if k.legs.hoof then cv:set(lx + g - bottom, g, k.dark) end
    end
    cv:each_ellipse(cx, cy, t.len / 2 + 0.5, depth / 2 + 0.3, function(x, y)
      cv:set(x, y, (y >= cy + depth / 2 - 0.6) and k.belly or L.C)
    end)
    if k.stripes then
      for _, st in ipairs(k.stripes) do
        for d = 0, math.min(st[2], depth - 1) - 1 do
          cv:set(cx - t.len // 2 + st[1] + d * st[3] // 2, math.floor(cy - depth / 2 + 0.5) + d, k.stripe)
        end
      end
    end
    if k.spots then
      for _, sp in ipairs(k.spots) do cv:set(cx - t.len // 2 + sp[1], math.floor(cy - depth / 2 + 0.5) + 1, k.mark) end
    end
    local h = k.heads.dead.rows
    cv:sprite(cx + t.len // 2 - 1, bottom - #h + 2, h, L)
    for n, p in ipairs(k.tail) do cv:set(cx - t.len // 2 - n, bottom - (n > 2 and 0 or 1), L[p[3]]) end
    shade(cv, k, far)
    cv:selout(0, 0, k.W, k.H, k.coat)
    return cv
  end

  local legs = k.legs
  local lean = pose.lean or 0
  -- Where the torso's bottom lies at column x, leaning about its middle.
  local cx = x0 + t.len / 2 - 0.5
  local function lift_at(x) return math.floor(lean * (x - cx) / t.len + 0.5) end
  local hips = top + t.depth - 1
  local fore_x, hind_x = x0 + legs.fore, x0 + legs.hind
  -- The far legs, two pixels back of the near ones toward the middle, drawn first and darker.
  for _, p in ipairs(leg(cv, k, fore_x - 2, hips - lift_at(fore_x), g, gait.feet[2][1], gait.feet[2][2], false, L.C)) do far[p] = true end
  for _, p in ipairs(leg(cv, k, hind_x + 2, hips - lift_at(hind_x), g, gait.feet[4][1], gait.feet[4][2], true, L.C)) do far[p] = true end

  -- The torso, with its chest or hump, leaning.
  local cy = top + (t.depth - 1) / 2
  local function fill(ex, ey, rx, ry)
    cv:each_ellipse(ex, ey, rx, ry, function(x, y)
      local yy = y - lift_at(x)
      local belly = y >= cy + t.depth / 2 - 0.6
      if cv:get(x, yy) == 0 or far[yy * cv.w + x] then cv:set(x, yy, belly and k.belly or L.C) end
      far[yy * cv.w + x] = nil
    end)
  end
  fill(cx, cy, t.len / 2 + 0.5, t.depth / 2 + 0.2)
  if k.chest then fill(x0 + k.chest[1], top + k.chest[2], k.chest[3], k.chest[4]) end

  -- The near legs over it.
  leg(cv, k, fore_x, hips - lift_at(fore_x), g, gait.feet[1][1], gait.feet[1][2], false, L.C)
  leg(cv, k, hind_x, hips - lift_at(hind_x), g, gait.feet[3][1], gait.feet[3][2], true, L.C)

  -- Marks.
  if k.spots then
    for _, s in ipairs(k.spots) do cv:set(x0 + s[1], top + s[2] - lift_at(x0 + s[1]), k.mark) end
  end
  if k.saddle then
    for x = x0 + 2, x0 + t.len - 3 do
      cv:set(x, top - lift_at(x), k.saddle)
      if x % 3 ~= 0 then cv:set(x, top + 1 - lift_at(x), k.saddle) end
    end
  end
  if k.bristle then
    for x = x0 + 4, x0 + t.len do cv:set(x, top - 1 - lift_at(x) - (x >= x0 + 6 and 1 or 0), k.bristle) end
    for x = x0 + 7, x0 + t.len - 1, 2 do cv:set(x, top - 3 - lift_at(x), k.bristle) end
  end
  if k.stripes then
    for _, s in ipairs(k.stripes) do
      local x = x0 + s[1]
      for d = 0, s[2] - 1 do
        local sx = x + (d * s[3]) // 2
        local sy = top + d - lift_at(sx)
        local i = cv:get(sx, sy)
        if i ~= 0 and i ~= k.belly then cv:set(sx, sy, k.stripe) end
        if s[2] >= 4 and d == 0 then cv:set(sx + 1, sy, k.stripe) end
      end
      if k.qi then
        local qx = x - 1
        cv:set(qx, top + 1 - lift_at(qx), k.qi)
        if s[2] >= 4 then cv:set(qx, top + 2 - lift_at(qx), k.qi_dark) end
      end
    end
  end

  -- The tail from the rump, raised for a deer's flight.
  for _, p in ipairs(k.tail) do
    local tx = x0 - 1 + p[1]
    local ty = top + 1 + (pose.tail_up and -p[2] or p[2]) - lift_at(x0)
    cv:set(tx, ty, L[p[3]])
  end

  -- The head at the front-top corner of the torso.
  local h = k.heads[pose.head or "up"]
  local front = x0 + t.len - 1
  local hx = front + h.at[1] + (pose.head_dx or 0)
  local hy = top + h.at[2] + (pose.head_dy or 0) - lift_at(front)
  cv:sprite(hx, hy, h.rows, L)

  -- Wounded: a stain on the flank, the crimson ramp's darker steps on solid pixels only.
  if pose.wound then
    local wx = x0 + t.len // 2 + 1
    local wy = top + 1 - lift_at(wx)
    for _, s in ipairs({ { 0, 0, 2 }, { 1, 0, 4 }, { 0, 1, 4 }, { 1, 1, 2 }, { -1, 1, 4 }, { 0, 2, 4 }, { 1, 3, 4 } }) do
      local i = cv:get(wx + s[1], wy + s[2])
      if i ~= 0 then cv:set(wx + s[1], wy + s[2], c("crimson", s[3])) end
    end
  end

  shade(cv, k, far)
  cv:selout(0, 0, k.W, k.H, k.coat)
  return cv
end

-- Each kind's frames as { tag, { canvases }, ms }.
local function frames_of(name)
  local k = kinds[name]
  local sets = {}
  local function add(tag, list, ms) sets[#sets + 1] = { tag, list, ms } end
  add("idle", { frame(k, {}) }, 500)
  local walk, run = {}, {}
  for _, g in ipairs(WALK) do walk[#walk + 1] = frame(k, { gait = g }) end
  for n, g in ipairs(RUN) do
    run[#run + 1] = frame(k, { gait = g, lean = (n == 2) and 1 or 0, head = k.heads.low and "low" or "up",
      tail_up = name == "deer" })
  end
  add("walk", walk, 150)
  add("run", run, 100)
  if k.graze then
    add("graze", { frame(k, { head = "down", lean = -1 }), frame(k, { head = "down", lean = -1, head_dy = 1 }) }, 300)
  end
  if k.strike == "bite" then
    add("strike", { frame(k, { head = "low", dx = -1, gait = RUN[1] }), frame(k, { head = "bite", dx = 2, lean = 1, gait = RUN[2] }),
      frame(k, { head = "up", dx = 1 }) }, 80)
  elseif k.strike == "swipe" then
    -- A swipe: crouched, then reared with a forepaw raised and flung forward, then down.
    local swipe = { feet = { { 4, 4 }, { 2, 1 }, { -1, 0 }, { 0, 0 } }, rise = 1 }
    add("strike", { frame(k, { head = "low", dx = -1, lean = -1, dy = 1 }), frame(k, { head = "roar", dx = 2, lean = 3, gait = swipe }),
      frame(k, { head = "up", dx = 1, lean = 1 }) }, 80)
  elseif k.strike == "charge" then
    add("strike", { frame(k, { head = "charge", dx = -1, lean = -1, gait = RUN[1] }),
      frame(k, { head = "charge", dx = 2, lean = -1, gait = RUN[3] }), frame(k, { head = "up", dx = 1 }) }, 80)
  elseif k.strike == "kick" then
    add("strike", { frame(k, { head = "up", lean = 1, gait = { feet = { { 0, 0 }, { 0, 0 }, { -3, 2 }, { -2, 1 } }, rise = 0 } }),
      frame(k, { head = "up", gait = STAND }) }, 80)
  end
  add("hurt", { frame(k, { head = "shut", dx = -2, head_dy = 1 }), frame(k, { head = "shut", dx = -1 }) }, 80)
  add("wounded", { frame(k, { wound = true, head_dy = 1 }) }, 500)
  add("dead", { frame(k, { lying = true }) }, 500)
  return sets
end

-- The hind: a deer without antlers, for the herd.
local function hind_frame(gait, grazing)
  return frame(kinds.deer, { gait = gait, head = grazing and "hind_down" or "hind", lean = grazing and -1 or 0 })
end

local sheets = {}
for _, name in ipairs(ORDER) do
  local k = kinds[name]
  local sets = frames_of(name)
  sheets[name] = sets
  local frames, tags = {}, {}
  for _, s in ipairs(sets) do
    local first = #frames + 1
    for _, cv in ipairs(s[2]) do frames[#frames + 1] = cv end
    tags[#tags + 1] = { s[1], first, #frames, s[3] }
  end
  px.save_frames(k.W, k.H, frames, tags, here .. "/animals/" .. name .. ".aseprite")
end

-- The contact sheet -------------------------------------------------------------------------------

-- How far apart a kind's frames stand in a row: its frame less the empty margin round the animal.
local STEP = { [32] = 24, [40] = 33, [48] = 41 }
local function row_width(name)
  local k, w = kinds[name], 10
  for _, s in ipairs(sheets[name]) do
    w = w + math.max(#s[2] * STEP[k.W], px.text_width(s[1]:upper()) + 2) + 8
  end
  return w
end
local SW = 0
for _, name in ipairs(ORDER) do SW = math.max(SW, row_width(name) + 16) end
local ZOOM, SCENE_W, SCENE_H, ZOOM_W = 3, 300, 84, 204
SW = math.max(SW, ZOOM_W * ZOOM + 16)
local rows_h = 0
for _, name in ipairs(ORDER) do rows_h = rows_h + kinds[name].H + 24 end
local SH = 22 + rows_h + (SCENE_H + 16) + (SCENE_H * ZOOM + 16)
local bg = px.canvas(SW, SH)
local ground = px.canvas(SW, SH)
local shadows = px.canvas(SW, SH)
local things = px.canvas(SW, SH)
local labels = px.canvas(SW, SH)
bg:rect(0, 0, SW, SH, c("ink", 2))

-- Calm grass, as the ground lies in play: its base step with sparse lighter and darker patches.
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

-- A contact shadow under an animal standing at (x, y), as art/markers' body shadow, as wide as it.
local function shadow(cv, x, y, rx)
  cv:each_ellipse(x, y, rx, math.max(1.5, rx / 3), function(sx, sy, d)
    if px.bayer(sx, sy) < 0.8 - 0.7 * d then cv:set(sx, sy, c("ink", 1)) end
  end)
end

local function mirror(cv)
  local out = px.canvas(cv.w, cv.h)
  for key, i in pairs(cv.p) do out:set(cv.w - 1 - key % cv.w, key // cv.w, i) end
  return out
end

labels:text(8, 5, "MURIM24  ANIMALS MOCKUP (#171, ADR 0067 STEP 6)", c("paper", 6))
labels:text(8, 12, "NATIVE PIXELS. REAL PROPORTIONS AGAINST A PERSON (15 PX), SEL-OUT IN THE COAT'S DARKEST STEP, ADR 0069 RAMPS.",
  c("stone", 5))

local NAMES = { deer = "DEER", wolf = "WOLF", boar = "BOAR", tiger = "TIGER",
  spirit_tiger_fire = "SPIRIT TIGER  CANDIDATE A: FIRE, AN EMITTER (KEEPS ITS COLOUR AT NIGHT)",
  spirit_tiger_water = "SPIRIT TIGER  CANDIDATE B: WATER, PALE AND COOL" }
local y = 22
for _, name in ipairs(ORDER) do
  local k = kinds[name]
  labels:text(8, y, NAMES[name], c("paper", 5))
  grass(ground, 8, y + 7, SW - 16, k.H + 4)
  local x = 10
  for _, s in ipairs(sheets[name]) do
    labels:text(x, y + k.H + 13, s[1]:upper(), c("stone", 5))
    local start = x
    for _, cv in ipairs(s[2]) do
      things:stamp(cv, x - (k.W - STEP[k.W]) // 2, y + 9)
      x = x + STEP[k.W]
    end
    x = math.max(x, start + px.text_width(s[1]:upper()) + 2) + 8
  end
  y = y + k.H + 24
end

-- Scale: a herd of deer, a stag and four hinds, and a pack of four wolves, beside a person and a
-- tiger, drawn on a scene of its own so the play-zoom view below shows the same pixels.
local scene = { ground = px.canvas(SCENE_W, SCENE_H), shadows = px.canvas(SCENE_W, SCENE_H), things = px.canvas(SCENE_W, SCENE_H) }
grass(scene.ground, 0, 0, SCENE_W, SCENE_H)
local function put(cv, k, x, py, flip)
  local ax, g = anchor(k)
  shadow(scene.shadows, x, py, k.torso.len / 2 + 1)
  scene.things:stamp(flip and mirror(cv) or cv, x - ax, py - g)
end
local person = px.canvas(16, 24)
person:stamp(S.body({ faction = "unorthodox", head = "topknot", hat = "straw", body = "tunic" }), 0, 6)
local function put_person(x, py)
  shadow(scene.shadows, x, py, 6)
  scene.things:stamp(person, x - 8, py - 22)
end
local deer, wolf, tg = kinds.deer, kinds.wolf, kinds.tiger
-- Back to front, so the nearer stands over the farther.
local cast = {
  { 32, function(py) put(hind_frame(nil, true), deer, 62, py) end },
  { 38, function(py) put(frame(deer, {}), deer, 40, py) end },
  { 40, function(py) put(frame(wolf, {}), wolf, 150, py, true) end },
  { 46, function(py) put(hind_frame(WALK[3]), deer, 84, py) end },
  { 48, function(py) put_person(116, py) end },
  { 50, function(py) put(hind_frame(WALK[2]), deer, 24, py) end },
  { 52, function(py) put(frame(wolf, { gait = WALK[1] }), wolf, 172, py, true) end },
  { 56, function(py) put(hind_frame(nil, true), deer, 56, py) end },
  { 60, function(py) put(frame(wolf, { gait = WALK[3] }), wolf, 142, py, true) end },
  { 64, function(py) put(frame(tg, {}), tg, 248, py, true) end },
  { 68, function(py) put(frame(wolf, { head = "low" }), wolf, 168, py, true) end },
}
for _, actor in ipairs(cast) do actor[2](actor[1]) end
local sy = y
labels:text(8, sy, "SCALE  A HERD (A STAG, FOUR HINDS), A PACK, A PERSON, A TIGER", c("paper", 5))
ground:stamp(scene.ground, 8, sy + 7)
shadows:stamp(scene.shadows, 8, sy + 7)
things:stamp(scene.things, 8, sy + 7)

-- The spirit tiger's two candidates, side by side, beside a person, and swatches of the jade and
-- the accents their marks must not read as.
local qx = 8 + SCENE_W + 12
labels:text(qx, sy, "SPIRIT TIGER  A / B  AGAINST JADE AND THE ACCENTS", c("paper", 5))
grass(ground, qx, sy + 7, 170, SCENE_H)
local spa, spb = kinds.spirit_tiger_fire, kinds.spirit_tiger_water
for _, s in ipairs({ { spa, qx + 30, "A" }, { spb, qx + 84, "B" } }) do
  local ax, g = anchor(s[1])
  shadow(shadows, s[2], sy + 60, s[1].torso.len / 2 + 1)
  things:stamp(frame(s[1], {}), s[2] - ax, sy + 60 - g)
  labels:text(s[2] - 2, sy + 70, s[3], c("paper", 6))
end
shadow(shadows, qx + 130, sy + 60, 6)
things:stamp(person, qx + 122, sy + 38)
local swatches = { { "A", c("fire", 4) }, { "B", c("water", 6) }, { "JADE", c("jade", 4) }, { "AZURE", c("azure", 4) },
  { "VIOLET", c("violet", 5) }, { "AMBER", c("amber", 5) }, { "CRIMSON", c("crimson", 5) } }
for n, s in ipairs(swatches) do
  local sx = qx + 182 + ((n - 1) % 2) * 60
  local sw_y = sy + 12 + ((n - 1) // 2) * 14
  things:rect(sx, sw_y, 10, 8, s[2])
  labels:text(sx + 13, sw_y + 2, s[1], c("paper", 5))
end

-- The herd, the person and the pack at play zoom, 3x, as a 1920x1080 screen shows them.
local zy = sy + SCENE_H + 16
labels:text(8, zy, "PLAY ZOOM 3X  THE HERD, THE PERSON AND THE PACK AS A PC SHOWS THEM", c("paper", 5))
for _, layer in ipairs({ scene.ground, scene.shadows, scene.things }) do
  for key, i in pairs(layer.p) do
    local x, yy = key % SCENE_W, key // SCENE_W
    if x < ZOOM_W then ground:rect(8 + x * ZOOM, zy + 7 + yy * ZOOM, ZOOM, ZOOM, i) end
  end
end

px.save(SW, SH, { { "bg", bg }, { "ground", ground }, { "shadows", shadows }, { "things", things }, { "labels", labels } },
  here .. "/animals/animals_sheet.aseprite")

-- Every frame on grass, a kind to a row, for the 6x export.
do
  local FW, rows = 0, {}
  for _, name in ipairs(ORDER) do
    local k, w = kinds[name], 4
    for _, s in ipairs(sheets[name]) do w = w + #s[2] * (k.W + 2) + 4 end
    FW = math.max(FW, w)
    rows[#rows + 1] = k.H + 2
  end
  local FH = 2
  for _, h in ipairs(rows) do FH = FH + h end
  local fb = px.canvas(FW, FH)
  fb:rect(0, 0, FW, FH, c("grass", 4))
  local fy = 2
  for _, name in ipairs(ORDER) do
    local k, x = kinds[name], 4
    for _, s in ipairs(sheets[name]) do
      for _, cv in ipairs(s[2]) do fb:stamp(cv, x, fy); x = x + k.W + 2 end
      x = x + 4
    end
    fy = fy + k.H + 2
  end
  px.save(FW, FH, { { "frames", fb } }, here .. "/animals/animals_frames.aseprite")
end
