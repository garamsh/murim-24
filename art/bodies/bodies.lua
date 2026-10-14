-- Draws one body sprite per faction at the 2.5D angle (ADR 0059), <faction>.aseprite: 16x24
-- frames, the feet on row 22, tagged
--   idle (one frame), walk (four: step, pass, other step, pass; a passing frame rides a pixel
--   higher), facing the viewer turned to the right,
--   back (four) walking away from the viewer, the head seen from behind,
--   swing (three, #184): the weapon raised behind the head, swung level, then followed through low,
--   on a melee arc,
--   idle_hurt and walk_hurt (one and four), the body below half its health: a blood stain on its
--   chest, stooping forward,
--   idle_maimed and walk_maimed (one and four), below a quarter: the stain spread down its side
--   and leg, stooping, and limping, the last step dragged,
--   lunge (two): a thrust two pixels forward, then the recovery, on a use at one person,
--   cast (two): the weapon raised overhead, then its tip alight in the faction's accent, on an area,
--   throw (two): the arm drawn back, then flung forward, on a use along a line (the darts),
--   mend (two): a mallet raised, then struck down, on a use that repairs structures,
--   torch (two): a torch held up, then thrust forward, on a use that burns them,
--   recoil (two): struck, two pixels back with the head dropped a pixel and the eyes shut, the
--   weapon fallen low, then a pixel back, recovering,
-- and each again with _xray after it: only the silhouette's outline, which the view draws over
-- whatever hides the body, in the Orthodox's and the Unorthodox's light accent and in violet for
-- the Demonic Cult, whose crimson stays for hostility. The view mirrors a body to face left.
-- Run: aseprite --batch --script art/bodies/bodies.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)
local c = px.c

local W, H = 16, 24
-- The body canvas from sprites.lua puts the feet on its row 16; this lands them on row 22.
local TOP = 6
-- The last frame row of the head; the back view paints the face over with hair above it.
local HEAD_BOTTOM = 15

-- The body that stands for each faction, the approved line-up's first column, and the colour of
-- its outline seen through what hides it.
-- `accent` lights a cast's tip: the faction's light accent, violet for the Demonic Cult, as its outline.
local factions = {
  orthodox = { spec = { faction = "orthodox", head = "topknot", weapon = "sword" }, xray = c("azure", 5), accent = "azure" },
  demonic = { spec = { faction = "demonic", head = "spiky", body = "tunic", weapon = "saber" }, xray = c("violet", 5),
    accent = "violet" },
  unorthodox = { spec = { faction = "unorthodox", head = "topknot", hat = "straw", body = "tunic", weapon = "knife" },
    xray = c("amber", 5), accent = "amber" },
}

-- The raw body's hand, where art/sprites.lua holds a weapon, the last row above the hips, and the
-- template head's eyes, in the raw body's 16x18 pixels; and how far each weapon reaches.
local HAND = { 11, 13 }
local HIPS = 13
local EYES = { { 6, 8 }, { 9, 8 } }
local REACH = { sword = 6, saber = 6, knife = 3 }
-- #184's motions show 80 ms a frame.
local MOTION_MS = { swing = 80, lunge = 80, cast = 80, throw = 80, mend = 80, torch = 80, recoil = 80 }

local function copy(spec, over)
  local s = {}
  for k, v in pairs(spec) do s[k] = v end
  for k, v in pairs(over or {}) do s[k] = v end
  return s
end

-- Everything above the hips moved `dx` across and `dy` down: a lean.
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

-- A blade from (x0, y0) to (x1, y1): its edge in the lighter step, its flat a row below.
local function blade(cv, kind, x0, y0, x1, y1)
  cv:line(x0, y0 + 1, x1, y1 + 1, c("stone", 4))
  cv:line(x0, y0, x1, y1, kind == "saber" and c("stone", 6) or c("stone", 7))
end

local function guard(cv, kind, x, y)
  if kind == "sword" then cv:vline(x, y - 1, 3, c("gold", 4))
  elseif kind == "saber" then cv:vline(x, y - 1, 3, c("crimson", 4))
  else cv:set(x, y, c("wood", 3)) end
end

-- A pose of #184's motions: the body, unarmed, over `over` and leant by `dx` and `dy`, armed by
-- `arm` at its hand, in front or `behind`, the eyes `shut`, outlined sel-out in the garment's darkest
-- step as art/sprites.lua outlines a body, on a frame.
local function pose(spec, over, dx, dy, arm, shut, behind)
  local raw = {}
  for k, v in pairs(spec) do raw[k] = v end
  for k, v in pairs(over or {}) do raw[k] = v end
  raw.weapon, raw.raw = nil, true
  local cv = S.body(raw)
  if shut then
    for _, e in ipairs(EYES) do cv:set(e[1], e[2], c("skin", 4)) end
  end
  cv = lean(cv, dx, dy)
  local hx, hy = HAND[1] + dx, HAND[2] + dy
  if arm and behind then
    local back = px.canvas(cv.w, cv.h)
    arm(back, hx, hy)
    back:stamp(cv, 0, 0)
    cv = back
  elseif arm then
    arm(cv, hx, hy)
  end
  cv:selout(0, 0, 16, 18, px.ramp_of[S.scheme(spec.faction).R][1])
  local out = px.canvas(W, H)
  out:stamp(cv, 0, TOP)
  return out
end

-- #184's motions for a faction `f`, as { tag, frames }, after the mockup the PM approved
-- (art/mockup/attack_motion.lua).
local function motions(f)
  local spec, k = f.spec, f.spec.weapon
  local r = REACH[k]
  return {
    { "swing", {
      pose(spec, {}, -1, 0, function(cv, x, y) blade(cv, k, x - 2, y - 3, x - 3 - r // 2, y - 3 - r); guard(cv, k, x - 1, y - 2) end,
        false, true),
      pose(spec, { body = "run" }, 1, 0, function(cv, x, y) blade(cv, k, x + 1, y - 1, math.min(14, x + r), y - 1); guard(cv, k, x, y) end),
      pose(spec, { body = "run" }, 1, 1, function(cv, x, y)
        blade(cv, k, x + 1, y, math.min(14, x + 1 + r // 2), y + r // 2 + 1); guard(cv, k, x, y)
      end),
    } },
    { "lunge", {
      pose(spec, { body = "run" }, 2, 0, function(cv, x, y) blade(cv, k, x + 1, y, math.min(14, x + r), y); guard(cv, k, x, y) end),
      pose(spec, {}, 1, 0, function(cv, x, y) blade(cv, k, x, y - 1, x + 1, y - 1 - r); guard(cv, k, x, y) end),
    } },
    { "cast", {
      pose(spec, {}, 0, 0, function(cv, x, y) blade(cv, k, x - 1, y - 4, x - 1, math.max(1, y - 4 - r)); guard(cv, k, x - 1, y - 3) end),
      pose(spec, {}, 0, -1, function(cv, x, y)
        blade(cv, k, x - 1, y - 4, x - 1, math.max(1, y - 4 - r)); guard(cv, k, x - 1, y - 3)
        local tip = math.max(1, y - 4 - r)
        cv:set(x - 1, tip, c("fire", 5)); cv:set(x - 2, tip, c(f.accent, 6)); cv:set(x, tip, c(f.accent, 6))
      end),
    } },
    { "throw", {
      pose(spec, {}, -1, 0, function(cv, x, y) cv:hline(x - 6, y - 3, 3, c("stone", 7)); cv:set(x - 3, y - 3, c("wood", 3)) end),
      pose(spec, { body = "run" }, 1, 0, function(cv, x, y)
        cv:hline(x, y - 1, 2, c("skin", 6)); cv:hline(x, y, 2, c("skin", 4)); cv:set(x + 2, y - 1, c("skin", 6))
      end),
    } },
    { "mend", {
      pose(spec, { body = "run" }, 1, 1, function(cv, x, y)
        cv:vline(x, y - 5, 5, c("wood", 4)); cv:rect(x - 1, y - 7, 3, 2, c("stone", 5)); cv:set(x - 1, y - 7, c("stone", 7))
      end),
      pose(spec, { body = "run" }, 1, 1, function(cv, x, y)
        cv:line(x, y, x + 1, y + 2, c("wood", 4)); cv:rect(x + 1, y + 2, 2, 2, c("stone", 5)); cv:set(x + 1, y + 2, c("stone", 7))
      end),
    } },
    { "torch", {
      pose(spec, {}, 0, 0, function(cv, x, y)
        cv:vline(x, y - 5, 6, c("wood", 3)); cv:rect(x - 1, y - 8, 3, 3, c("fire", 3))
        cv:set(x, y - 7, c("fire", 5)); cv:set(x, y - 9, c("fire", 4)); cv:set(x - 1, y - 8, c("fire", 2))
      end),
      pose(spec, { body = "run" }, 0, 0, function(cv, x, y)
        cv:line(x, y, x + 2, y - 3, c("wood", 3)); cv:rect(x + 1, y - 6, 3, 3, c("fire", 3))
        cv:set(x + 2, y - 5, c("fire", 5)); cv:set(x + 2, y - 7, c("fire", 4)); cv:set(x + 3, y - 6, c("fire", 4))
      end),
    } },
    { "recoil", {
      pose(spec, {}, -2, 1, function(cv, x, y) blade(cv, k, x, y, math.min(14, x + 2), y + r // 2 + 1) end, true),
      pose(spec, {}, -1, 0, function(cv, x, y) blade(cv, k, x, y - 1, x + 1, y - r); guard(cv, k, x, y) end),
    } },
  }
end

-- bob raises everything above the feet by a pixel and stretches the legs to fill the gap.
local function frame(spec, bob)
  local body = S.body(spec)
  local cv = px.canvas(W, H)
  for k, i in pairs(body.p) do cv:set(k % body.w, TOP + k // body.w, i) end
  if bob then
    local feet = TOP + 16
    for y = 1, feet - 1 do
      for x = 0, W - 1 do
        local i = cv:get(x, y)
        cv:clear(x, y - 1); cv:set(x, y - 1, i)
      end
    end
    for x = 0, W - 1 do cv:clear(x, feet - 1); cv:set(x, feet - 1, cv:get(x, feet - 2)) end
  end
  return cv
end

local function edge(cv, x, y)
  return cv:get(x - 1, y) == 0 or cv:get(x + 1, y) == 0 or cv:get(x, y - 1) == 0 or cv:get(x, y + 1) == 0
end

-- The same body seen from behind: skin and eyes on the head become hair.
local function back(cv)
  local out = px.canvas(W, H)
  local skin = { [c("skin", 4)] = true, [c("skin", 6)] = true }
  for k, i in pairs(cv.p) do
    local x, y = k % W, k // W
    if y <= HEAD_BOTTOM and (skin[i] or (i == c("ink", 1) and not edge(cv, x, y))) then i = c("ink", 2) end
    out:set(x, y, i)
  end
  return out
end

-- Pixels of blood on the body, at offsets from (6, 16), the chest's left: on solid pixels only and
-- clear of the boots in every frame, so the outline stays whole and keeps the colour of what it
-- borders; `spread` 1 is a stain on the chest, 2 runs it down the side, the leg and the arm.
local STAIN = {
  { { 0, 0, 4 }, { 1, 0, 2 }, { 0, 1, 2 }, { 1, 1, 4 }, { 2, 1, 2 } },
  { { 2, 2, 4 }, { 3, 2, 2 }, { 2, 3, 2 }, { 3, 4, 4 }, { 2, 5, 2 }, { -1, 2, 2 },
    { 3, 3, 4 }, { 2, 4, 4 }, { -1, 3, 4 }, { -2, 2, 2 }, { 4, 1, 4 } },
}
local function stained(cv, spread)
  local out = px.canvas(W, H)
  for k, i in pairs(cv.p) do out:set(k % W, k // W, i) end
  for n = 1, spread do
    for _, d in ipairs(STAIN[n]) do
      local x, y = 6 + d[1], 16 + d[2]
      local i = out:get(x, y)
      if i ~= 0 and not edge(cv, x, y) then out:set(x, y, c("crimson", d[3])) end
    end
  end
  return out
end

-- The body stooping: everything above the legs, rows HEAD_BOTTOM + 4 and up, a pixel forward and a
-- pixel lower, over the top of the legs.
local function stooped(cv)
  local out = px.canvas(W, H)
  local hips = HEAD_BOTTOM + 4
  for k, i in pairs(cv.p) do
    local x, y = k % W, k // W
    if y > hips then out:set(x, y, i) end
  end
  for k, i in pairs(cv.p) do
    local x, y = k % W, k // W
    if y <= hips then out:set(x + 1, y + 1, i) end
  end
  return out
end

-- Only the silhouette's outline, in `colour`.
local function xray(cv, colour)
  local out = px.canvas(W, H)
  for k in pairs(cv.p) do
    local x, y = k % W, k // W
    if edge(cv, x, y) then out:set(x, y, colour) end
  end
  return out
end

for name, f in pairs(factions) do
  local spec = f.spec
  local walk = {
    frame(copy(spec, { step = "left" })), frame(spec, true), frame(copy(spec, { step = "right" })), frame(spec, true),
  }
  local sets = {
    { "idle", { frame(spec) } },
    { "walk", walk },
    { "back", { back(walk[1]), back(walk[2]), back(walk[3]), back(walk[4]) } },
  }
  local moves = motions(f)
  sets[#sets + 1] = moves[1]
  local hurt = function(cv) return stooped(stained(cv, 1)) end
  local maimed = function(cv) return stooped(stained(cv, 2)) end
  sets[#sets + 1] = { "idle_hurt", { hurt(frame(spec)) } }
  sets[#sets + 1] = { "walk_hurt", { hurt(walk[1]), hurt(walk[2]), hurt(walk[3]), hurt(walk[4]) } }
  sets[#sets + 1] = { "idle_maimed", { maimed(frame(spec)) } }
  -- The limp: a step, a pass, then the hurt leg dragged as the body stands on the other, a pass.
  sets[#sets + 1] = { "walk_maimed",
    { maimed(walk[1]), maimed(walk[2]), maimed(frame(spec)), maimed(walk[4]) } }
  for n = 2, #moves do sets[#sets + 1] = moves[n] end
  local frames, tags = {}, {}
  for _, suffix in ipairs({ "", "_xray" }) do
    for _, set in ipairs(sets) do
      local first = #frames + 1
      for _, cv in ipairs(set[2]) do frames[#frames + 1] = suffix == "" and cv or xray(cv, f.xray) end
      local ms = set[1]:sub(1, 4) == "idle" and 1000 or (MOTION_MS[set[1]] or 150)
      tags[#tags + 1] = { set[1] .. suffix, first, #frames, ms }
    end
  end
  px.save_frames(W, H, frames, tags, here .. "/" .. name .. ".aseprite")
end
