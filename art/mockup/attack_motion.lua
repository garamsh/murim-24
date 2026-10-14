-- The mockup of #184's phase B, for the PM's approval before the build: attack motion at the 2.5D
-- angle (ADR 0059) on ADR 0069's ramps with sel-out. It draws, from the production body template
-- (art/sprites.lua) and palette (art/pixel.lua):
--   attack_<faction>.aseprite, 16x24 frames, the feet on row 22 as art/bodies' sheets, tagged
--     swing (three: the weapon raised back, swung level, followed through low), lunge (two: a
--     thrust two pixels forward, then the recovery), cast (two: the weapon raised overhead, then
--     its tip alight in the faction's accent), hurt (two: recoiling two pixels back with the head
--     dropped a pixel and the eyes shut, then a pixel back, recovering), throw (two: the arm drawn back, then flung forward; the Unorthodox's darts),
--     mend (two: a mallet raised, then struck down) and torch (two: a torch held up, then thrust);
--   attack_effects.aseprite, tagged slash_<faction> (three 24x16 crescents in the faction's accent
--     ramp, a white-hot edge first), dart (eight 8x8 headings and a streak), burst_<faction>
--     (three 32x20 rings of force at the angle, a white-hot core, sparks), spark (three 8x8 hit
--     stars), mend (three 12x16 jade motes rising) and torch (three 12x16 flames with smoke);
--   attack_motion.aseprite, the contact sheet: every frame on grass at native size, the effects,
--   the damage numbers in each technique's colour, and a swing landing on a foe as a sequence.
-- Body frames keep art/bodies' 16x24, so the view's atlas is unchanged; a swing's reach beyond the
-- frame is the slash effect's, drawn by the view from the core's strike as phase A's arc is.
-- Run: aseprite --batch --script art/mockup/attack_motion.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)
local c = px.c

local W, H = 16, 24
local TOP = 6
-- The raw body's hand, where art/sprites.lua holds a weapon, and the rows above the hips.
local HAND = { 11, 13 }
local HIPS = 13
-- The eyes of the template's head, in the raw body's pixels.
local EYES = { { 6, 8 }, { 9, 8 } }

local factions = {
  { name = "orthodox", spec = { faction = "orthodox", head = "topknot" }, weapon = "sword", accent = "azure" },
  { name = "demonic", spec = { faction = "demonic", head = "spiky", body = "tunic" }, weapon = "saber", accent = "violet" },
  { name = "unorthodox", spec = { faction = "unorthodox", head = "topknot", hat = "straw", body = "tunic" }, weapon = "knife",
    accent = "amber" },
}

local function copy(spec, over)
  local s = {}
  for k, v in pairs(spec) do s[k] = v end
  for k, v in pairs(over or {}) do s[k] = v end
  return s
end

-- Moves everything above the hips `dx` across and `dy` down: a lean.
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
  local edge, flat = c("stone", 7), c("stone", 4)
  if kind == "saber" then edge = c("stone", 6) end
  cv:line(x0, y0 + 1, x1, y1 + 1, flat)
  cv:line(x0, y0, x1, y1, edge)
end

local function guard(cv, kind, x, y)
  if kind == "sword" then cv:vline(x, y - 1, 3, c("gold", 4))
  elseif kind == "saber" then cv:vline(x, y - 1, 3, c("crimson", 4))
  else cv:set(x, y, c("wood", 3)) end
end

-- How far each weapon reaches from the hand, in pixels.
local REACH = { sword = 6, saber = 6, knife = 3 }

-- A pose: the raw body, posed and armed by `arm`, outlined sel-out in its garment's darkest step and
-- set on a 16x24 frame. `behind` draws the weapon behind the body, as when raised over the back.
local function pose(f, over, dx, dy, arm, shut, behind)
  local spec = copy(f.spec, copy(over, { raw = true }))
  local cv = S.body(spec)
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
  local map = S.scheme(f.spec.faction)
  cv:selout(0, 0, 16, 18, px.ramp_of[map.R][1])
  local out = px.canvas(W, H)
  out:stamp(cv, 0, TOP)
  return out
end

local function frames_of(f)
  local k, r = f.weapon, REACH[f.weapon]
  local sets = {}
  sets[#sets + 1] = { "idle", { pose(f, {}, 0, 0, function(cv, x, y)
    blade(cv, k, x, y - 1, x, y - 1 - r); guard(cv, k, x, y)
  end) } }
  sets[#sets + 1] = { "swing", {
    pose(f, {}, -1, 0, function(cv, x, y) blade(cv, k, x - 2, y - 3, x - 3 - r // 2, y - 3 - r); guard(cv, k, x - 1, y - 2) end,
      false, true),
    pose(f, { body = "run" }, 1, 0, function(cv, x, y) blade(cv, k, x + 1, y - 1, math.min(15, x + r), y - 1); guard(cv, k, x, y) end),
    pose(f, { body = "run" }, 1, 1, function(cv, x, y) blade(cv, k, x + 1, y, math.min(15, x + 1 + r // 2), y + r // 2 + 1); guard(cv, k, x, y) end),
  } }
  sets[#sets + 1] = { "lunge", {
    pose(f, { body = "run" }, 2, 0, function(cv, x, y) blade(cv, k, x + 1, y, math.min(15, x + r), y); guard(cv, k, x, y) end),
    pose(f, {}, 1, 0, function(cv, x, y) blade(cv, k, x, y - 1, x + 1, y - 1 - r); guard(cv, k, x, y) end),
  } }
  sets[#sets + 1] = { "cast", {
    pose(f, {}, 0, 0, function(cv, x, y) blade(cv, k, x - 1, y - 4, x - 1, math.max(0, y - 4 - r)); guard(cv, k, x - 1, y - 3) end),
    pose(f, {}, 0, -1, function(cv, x, y)
      blade(cv, k, x - 1, y - 4, x - 1, math.max(0, y - 4 - r)); guard(cv, k, x - 1, y - 3)
      local tip = math.max(0, y - 4 - r)
      cv:set(x - 1, tip, c("fire", 5)); cv:set(x - 2, tip, c(f.accent, 6)); cv:set(x, tip, c(f.accent, 6))
    end),
  } }
  -- The recoil: two pixels back and the head dropped a pixel, eyes shut, the weapon fallen low;
  -- then a pixel back, recovering.
  sets[#sets + 1] = { "hurt", {
    pose(f, {}, -2, 1, function(cv, x, y) blade(cv, k, x, y, math.min(15, x + 2), y + r // 2 + 1) end, true),
    pose(f, {}, -1, 0, function(cv, x, y) blade(cv, k, x, y - 1, x + 1, y - r); guard(cv, k, x, y) end),
  } }
  sets[#sets + 1] = { "throw", {
    pose(f, {}, -1, 0, function(cv, x, y) cv:hline(x - 6, y - 3, 3, c("stone", 7)); cv:set(x - 3, y - 3, c("wood", 3)) end),
    pose(f, { body = "run" }, 1, 0, function(cv, x, y)
      cv:hline(x + 1, y - 1, 3, c("skin", 6)); cv:hline(x + 1, y, 3, c("skin", 4)); cv:set(x + 4, y - 1, c("skin", 6))
    end),
  } }
  sets[#sets + 1] = { "mend", {
    pose(f, { body = "run" }, 1, 1, function(cv, x, y)
      cv:vline(x, y - 5, 5, c("wood", 4)); cv:rect(x - 1, y - 7, 3, 2, c("stone", 5)); cv:set(x - 1, y - 7, c("stone", 7))
    end),
    pose(f, { body = "run" }, 2, 1, function(cv, x, y)
      cv:line(x, y, x + 3, y + 2, c("wood", 4)); cv:rect(x + 3, y + 2, 2, 3, c("stone", 5)); cv:set(x + 3, y + 2, c("stone", 7))
    end),
  } }
  sets[#sets + 1] = { "torch", {
    pose(f, {}, 0, 0, function(cv, x, y)
      cv:vline(x, y - 5, 6, c("wood", 3)); cv:rect(x - 1, y - 8, 3, 3, c("fire", 3))
      cv:set(x, y - 7, c("fire", 5)); cv:set(x, y - 9, c("fire", 4)); cv:set(x - 1, y - 8, c("fire", 2))
    end),
    pose(f, { body = "run" }, 1, 0, function(cv, x, y)
      cv:line(x, y, x + 3, y - 3, c("wood", 3)); cv:rect(x + 3, y - 6, 3, 3, c("fire", 3))
      cv:set(x + 4, y - 5, c("fire", 5)); cv:set(x + 4, y - 7, c("fire", 4)); cv:set(x + 5, y - 6, c("fire", 4))
    end),
  } }
  return sets
end

-- Effects ----------------------------------------------------------------------------------------

-- A crescent swept ahead of a striker facing right, 24x16: `stage` 1 a thin white-hot edge, 2 the
-- full crescent, 3 its tail dithered away.
local function slash(ramp, stage)
  local cv = px.canvas(24, 16)
  local cx, cy = 2, 8
  for y = 0, 15 do
    for x = 0, 23 do
      local dx, dy = (x - cx) / 20, (y - cy) / 8
      local d = math.sqrt(dx * dx + dy * dy)
      local inner = math.sqrt(((x - cx + 5) / 17) ^ 2 + dy * dy)
      if d <= 1 and inner > 1 and x > cx then
        local edge = d > 0.9
        if stage == 1 then
          if edge then cv:set(x, y, c("fire", 5)) elseif d > 0.82 then cv:set(x, y, c(ramp, 6)) end
        elseif stage == 2 then
          cv:set(x, y, edge and c(ramp, 7) or (d > 0.8 and c(ramp, 5) or c(ramp, 4)))
        elseif px.bayer(x, y) < 0.5 - (1 - d) then
          cv:set(x, y, c(ramp, 3))
        end
      end
    end
  end
  return cv
end

-- A black-fang dart flying at `angle` degrees, 8x8: an ink shaft, a steel tip and an amber tail.
local function dart(angle)
  local cv = px.canvas(8, 8)
  local a = math.rad(angle)
  local ux, uy = math.cos(a), math.sin(a)
  local function at(t) return math.floor(3.5 + ux * t + 0.5), math.floor(3.5 + uy * t + 0.5) end
  for t = -3, 2 do local x, y = at(t); cv:set(x, y, c("ink", 2)) end
  local x, y = at(3); cv:set(x, y, c("stone", 7))
  x, y = at(-3); cv:set(x, y, c("amber", 5))
  return cv
end

local function streak()
  local cv = px.canvas(8, 8)
  for x = 0, 7 do if px.bayer(x, 3) < x / 8 then cv:set(x, 3, c("amber", 3)) end end
  cv:hline(5, 4, 3, c("ink", 2)); cv:set(7, 4, c("stone", 7))
  return cv
end

-- A ring of force at the angle, 32x20, `stage` growing: the accent ramp, a white-hot core, sparks.
local function burst(ramp, stage)
  local cv = px.canvas(32, 20)
  local rx, ry = 6 + stage * 4, 4 + stage * 2.5
  cv:each_ellipse(16, 10, rx, ry, function(x, y, d)
    if d > 0.75 then cv:set(x, y, d > 0.9 and c(ramp, 6) or c(ramp, 4))
    elseif stage == 1 and d < 0.35 then cv:set(x, y, c("fire", 5)) end
  end)
  local r = px.rng(stage * 7)
  for _ = 1, 4 + stage * 2 do
    local t = r.next() * math.pi * 2
    cv:set(math.floor(16 + math.cos(t) * (rx + 2)), math.floor(10 + math.sin(t) * (ry + 1)), c("fire", 4))
  end
  if stage >= 2 then
    for k = 0, 3 do cv:set(13 + k * 2, 11 + (k % 2), c("ink", 2)) end
  end
  return cv
end

local function spark(stage)
  local cv = px.canvas(8, 8)
  local r = stage == 2 and 3 or 2
  cv:set(3, 3, c("fire", 5))
  for k = 1, r do
    local col = k == r and c("fire", 3) or c("paper", 7)
    if stage == 3 and k < r then col = nil end
    if col then cv:set(3 + k, 3, col); cv:set(3 - k, 3, col); cv:set(3, 3 + k, col); cv:set(3, 3 - k, col) end
  end
  if stage == 3 then cv:clear(3, 3) end
  return cv
end

local function motes(stage)
  local cv = px.canvas(12, 16)
  local spots = { { 3, 13 }, { 7, 11 }, { 5, 9 }, { 9, 14 }, { 2, 8 } }
  for n, p in ipairs(spots) do
    local y = p[2] - stage * 3 + (n % 2)
    if y >= 0 then
      cv:set(p[1], y, n % 2 == 0 and c("jade", 6) or c("jade", 7))
      if stage < 3 then cv:set(p[1], y + 1, c("jade", 4)) end
    end
  end
  cv:set(5, 15, c("wood", 4)); cv:set(8, 15, c("wood", 3))
  return cv
end

local function flame(stage)
  local cv = px.canvas(12, 16)
  local h = 6 + stage * 2
  for y = 0, h - 1 do
    local w = math.max(1, math.floor((h - y) / 2))
    for x = -w, w do
      local ramp_step = y < h / 3 and 2 or (y < h * 2 / 3 and 3 or 4)
      if math.abs(x) <= 1 and y < h / 2 then ramp_step = 5 end
      cv:set(6 + x + ((y + stage) % 3 == 0 and 1 or 0), 15 - y, c("fire", ramp_step))
    end
  end
  for k = 0, stage do cv:set(4 + k * 2, 15 - h - 1 - k, c("stone", 4)) end
  return cv
end

-- Sheets -----------------------------------------------------------------------------------------

local sheets = {}
for _, f in ipairs(factions) do
  local frames, tags = {}, {}
  local sets = frames_of(f)
  for _, set in ipairs(sets) do
    local first = #frames + 1
    for _, cv in ipairs(set[2]) do frames[#frames + 1] = cv end
    tags[#tags + 1] = { set[1], first, #frames, set[1] == "idle" and 1000 or 80 }
  end
  px.save_frames(W, H, frames, tags, here .. "/attack/attack_" .. f.name .. ".aseprite")
  sheets[f.name] = sets
end

local effects = {
  { "slash_orthodox", { slash("azure", 1), slash("azure", 2), slash("azure", 3) }, 24, 16 },
  { "slash_demonic", { slash("violet", 1), slash("violet", 2), slash("violet", 3) }, 24, 16 },
  { "slash_unorthodox", { slash("amber", 1), slash("amber", 2), slash("amber", 3) }, 24, 16 },
  { "dart", { dart(0), dart(45), dart(90), dart(135), dart(180), dart(225), dart(270), dart(315), streak() }, 8, 8 },
  { "burst_orthodox", { burst("azure", 1), burst("azure", 2), burst("azure", 3) }, 32, 20 },
  { "burst_demonic", { burst("violet", 1), burst("violet", 2), burst("violet", 3) }, 32, 20 },
  { "burst_unorthodox", { burst("amber", 1), burst("amber", 2), burst("amber", 3) }, 32, 20 },
  { "spark", { spark(1), spark(2), spark(3) }, 8, 8 },
  { "mend", { motes(1), motes(2), motes(3) }, 12, 16 },
  { "torch", { flame(1), flame(2), flame(3) }, 12, 16 },
}
-- Every effect on one 32x20 frame size, each centred in it, so the sheet has one frame size.
do
  local frames, tags = {}, {}
  for _, e in ipairs(effects) do
    local first = #frames + 1
    for _, cv in ipairs(e[2]) do
      local out = px.canvas(32, 20)
      out:stamp(cv, (32 - e[3]) // 2, (20 - e[4]) // 2)
      frames[#frames + 1] = out
    end
    tags[#tags + 1] = { e[1], first, #frames, 60 }
  end
  px.save_frames(32, 20, frames, tags, here .. "/attack/attack_effects.aseprite")
end

-- The contact sheet --------------------------------------------------------------------------------

local SW, SH = 448, 348
local bg = px.canvas(SW, SH)
local ground = px.canvas(SW, SH)
local things = px.canvas(SW, SH)
local labels = px.canvas(SW, SH)
bg:rect(0, 0, SW, SH, c("ink", 2))

local function grass(x, y, w, h)
  for yy = y, y + h - 1 do
    for xx = x, x + w - 1 do
      local n = px.noise(xx, yy, 5, 3) + (px.hash(xx, yy, 9) - 0.5) * 0.25
      local col = c("grass", 4)
      if n > 0.62 then col = c("grass", 5) elseif n < 0.35 then col = c("grass", 3) end
      ground:set(xx, yy, col)
    end
  end
end

labels:text(8, 5, "MURIM24  ATTACK MOTION  PHASE B MOCKUP (#184)", c("paper", 6))
labels:text(8, 12, "NATIVE PIXELS. BODIES 16X24 AS IN PLAY, SEL-OUT, ADR 0069 RAMPS. FRAMES 80 MS.", c("stone", 5))

local order = { "idle", "swing", "lunge", "cast", "hurt", "throw", "mend", "torch" }
local y0 = 22
for row, f in ipairs(factions) do
  local y = y0 + (row - 1) * 40
  labels:text(8, y, f.name:upper(), c(f.accent, 6))
  grass(8, y + 7, SW - 16, 28)
  local x = 10
  for _, name in ipairs(order) do
    for _, set in ipairs(sheets[f.name]) do
      if set[1] == name then
        labels:text(x, y + 36 - 1, name:upper(), c("stone", 4))
        local start = x
        for _, cv in ipairs(set[2]) do
          things:stamp(cv, x, y + 9)
          x = x + 17
        end
        x = math.max(x, start + px.text_width(name:upper()) + 2) + 8
      end
    end
  end
end

-- Effects, each in a labelled cell: its frames left to right.
local ey = y0 + 3 * 40 + 2
labels:text(8, ey, "EFFECTS  SLASH AND BURST IN THE FACTION'S ACCENT, DARTS, HIT SPARK, MEND, TORCH", c("paper", 5))
grass(8, ey + 7, SW - 16, 92)
local ex, erow = 12, 0
for _, e in ipairs(effects) do
  local width = math.max(px.text_width(e[1]:upper()), #e[2] * (e[3] + 2))
  if ex + width > SW - 12 then ex = 12; erow = erow + 1 end
  local top = ey + 9 + erow * 30
  labels:text(ex, top, e[1]:upper(), c("stone", 5))
  local bx = ex
  for _, cv in ipairs(e[2]) do
    things:stamp(cv, bx, top + 7 + (20 - e[4]) // 2)
    bx = bx + e[3] + 2
  end
  ex = ex + width + 10
end

-- Damage numbers in each technique's colour, with an ink shadow.
local ny = 280
labels:text(8, ny, "DAMAGE NUMBERS  TECHNIQUE COLOURS (PHASE A: PAPER, CRIMSON FOR THE PLAYER'S LOSS)", c("paper", 5))
grass(8, ny + 7, SW - 16, 16)
local numbers = {
  { "PLUM BLOSSOM 20", c("azure", 6) }, { "HEAVENLY DEMON 20", c("violet", 6) }, { "BLACK FANG 20", c("amber", 6) },
  { "GUARD 15", c("paper", 6) }, { "YOUR LOSS 20", c("crimson", 6) },
}
local nx = 12
for _, n in ipairs(numbers) do
  labels:text(nx, ny + 12, n[1], n[2], c("ink", 1))
  nx = nx + px.text_width(n[1]) + 12
end

-- A swing landing on a foe, frame by frame: the Orthodox striker swings, its slash sweeps, the
-- Demonic foe flinches with a spark and is knocked a pixel back.
local sy = 308
labels:text(8, sy, "SEQUENCE  A SWING LANDING: STRIKER SWINGS, SLASH SWEEPS, FOE FLINCHES AND IS KNOCKED BACK", c("paper", 5))
grass(8, sy + 7, SW - 16, 30)
local orth, dem = sheets.orthodox, sheets.demonic
local function set_of(sets, name) for _, s in ipairs(sets) do if s[1] == name then return s[2] end end end
local steps = {
  { set_of(orth, "swing")[1], set_of(dem, "idle")[1], nil, 0, nil },
  { set_of(orth, "swing")[2], set_of(dem, "idle")[1], slash("azure", 1), 0, nil },
  { set_of(orth, "swing")[3], set_of(dem, "hurt")[1], slash("azure", 2), 1, spark(2) },
  { set_of(orth, "idle")[1], set_of(dem, "hurt")[2], slash("azure", 3), 2, spark(3) },
}
for n, st in ipairs(steps) do
  local x = 12 + (n - 1) * 108
  things:stamp(st[1], x, sy + 10)
  -- The foe faces the striker: mirrored.
  local foe = px.canvas(W, H)
  for k, i in pairs(st[2].p) do foe:set(W - 1 - k % W, k // W, i) end
  things:stamp(foe, x + 30 + st[4], sy + 10)
  if st[3] then things:stamp(st[3], x + 12, sy + 16) end
  if st[5] then things:stamp(st[5], x + 32, sy + 18) end
  labels:text(x, sy + 7, tostring(n), c("stone", 5))
end

px.save(SW, SH, { { "bg", bg }, { "ground", ground }, { "things", things }, { "labels", labels } },
  here .. "/attack/attack_motion.aseprite")

-- Every body frame at native size on plain ground, a faction to a row in the order above, for the
-- 6x export that shows each pose's pixels.
do
  local count = 0
  for _, set in ipairs(sheets.orthodox) do count = count + #set[2] end
  local FW, FH = 4 + count * 18 + #order * 4, 4 + #factions * 26
  local fb = px.canvas(FW, FH)
  fb:rect(0, 0, FW, FH, c("grass", 4))
  for row, f in ipairs(factions) do
    local x = 4
    for _, name in ipairs(order) do
      local cvs = set_of(sheets[f.name], name)
      for _, cv in ipairs(cvs) do fb:stamp(cv, x, 2 + (row - 1) * 26); x = x + 18 end
      x = x + 4
    end
  end
  px.save(FW, FH, { { "frames", fb } }, here .. "/attack/attack_frames.aseprite")
end
