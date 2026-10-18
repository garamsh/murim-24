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
--   <trade>_idle, <trade>_walk and <trade>_work (one, four and two) for each of ADR 0081's trades:
--   the trade's garment and the prop it carries in place of the weapon, standing, walking as walk
--   does, and at its work (weighing goods on a steelyard, grinding herbs in a mortar, hammering at
--   an anvil, hoeing, pouring from a jug, begging with a bowl, praying),
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
-- A trade's work motion shows 250 ms a frame, looped.
local WORK_MS = 250

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

-- ADR 0081's trades, as the PM approved their looks on #170 (art/trades-mockup at 746d0157): each
-- palette and prop over the faction's body in place of its weapon, the monk alone shaved. The view
-- finds a trade's frames by its occupation's id in the pack, `<id>_idle`, `<id>_walk` and
-- `<id>_work`.
local TRADES = { "merchant", "physician", "smith", "farmer", "innkeeper", "beggar", "monk" }
-- The raw body's other hand, where the beggar's stick stands, and the row its feet stand on.
local OTHER = { 4, 13 }
local FEET = 16

-- Recolours every pixel of `from` within rows y0 to y1 to `to`.
local function recolour(cv, y0, y1, from, to)
  for k, i in pairs(cv.p) do
    local y = k // cv.w
    if y >= y0 and y <= y1 and i == from then cv.p[k] = to end
  end
end

-- Garment pieces, drawn over the raw body: m is the faction's letter map.
local garments = {}

-- The merchant: a coin pouch hung at the sash; its pole and baskets are its prop.
function garments.merchant(cv, m)
  cv:set(9, 13, c("gold", 4)); cv:set(9, 14, c("gold", 3)); cv:set(8, 13, c("gold", 3))
end

-- The physician: a medicine box on the back, wood with a paper label, its strap across the chest.
function garments.physician(cv, m)
  cv:rect(1, 9, 3, 5, c("wood", 4)); cv:vline(1, 9, 5, c("wood", 3)); cv:hline(1, 9, 3, c("wood", 5))
  cv:set(2, 11, c("paper", 6)); cv:set(2, 12, c("paper", 5))
  cv:set(5, 11, c("wood", 3)); cv:set(6, 12, c("wood", 3)); cv:set(7, 13, c("wood", 3))
end

-- The smith: a leather apron over the robe from the chest down, and the sleeves rolled to bare
-- forearms.
function garments.smith(cv, m)
  for y = 11, 15 do
    for x = 5, 9 do
      if cv:get(x, y) ~= 0 then cv:set(x, y, (x == 9 or y == 15) and c("wood", 3) or c("wood", 4)) end
    end
  end
  cv:set(6, 11, c("wood", 5))
  cv:set(4, 12, c("skin", 5)); cv:set(11, 12, c("skin", 4))
end

-- The farmer: the robe's hem tucked up, bare shins above the feet.
function garments.farmer(cv, m)
  for x = 0, cv.w - 1 do
    if cv:get(x, FEET) == m.F and cv:get(x, FEET - 1) ~= 0 then cv:set(x, FEET - 1, c("skin", 4)) end
  end
  cv:set(4, 12, c("skin", 5))
end

-- The innkeeper: a white cloth apron tied at the waist.
function garments.innkeeper(cv, m)
  for y = 12, 15 do
    for x = 5, 9 do
      if cv:get(x, y) ~= 0 then cv:set(x, y, (x == 9) and c("paper", 4) or c("paper", 6)) end
    end
  end
  cv:hline(5, 12, 5, c("paper", 5))
end

-- The beggar: a torn hem, patches in the garment's darker steps, bare feet.
function garments.beggar(cv, m)
  for _, x in ipairs({ 5, 8, 10 }) do cv:clear(x, FEET - 1) end
  cv:set(6, 11, m.D); cv:set(7, 11, m.D); cv:set(9, 14, m.D); cv:set(5, 13, px.shift(m.M, -1))
  recolour(cv, FEET - 1, FEET, m.F, c("skin", 4))
end

-- The monk: prayer beads round the neck, wood beads on the chest.
function garments.monk(cv, m)
  for _, p in ipairs({ { 6, 10 }, { 7, 11 }, { 8, 11 }, { 9, 10 }, { 7, 12 } }) do cv:set(p[1], p[2], c("wood", 5)) end
  cv:set(7, 12, c("wood", 6))
end

-- Props, drawn at the hand (x, y), each a function of the frame of its motion: 0 for carrying, 1
-- and 2 for the two frames of work.
local props = {}

-- A woven basket of goods, 3 wide and 4 tall from its top-left (x0, y0): its rim lit, the weave a
-- step lighter across it, goods heaped in it.
local function basket(cv, x0, y0)
  cv:rect(x0, y0 + 1, 3, 3, c("wood", 4)); cv:hline(x0, y0 + 2, 3, c("wood", 5)); cv:vline(x0 + 2, y0 + 1, 3, c("wood", 3))
  cv:hline(x0, y0 + 1, 3, c("wood", 6)); cv:set(x0 + 1, y0, c("cloth", 5)); cv:set(x0, y0, c("earth", 5))
end

-- A shoulder pole across the shoulders past both sides of the body, a basket of goods hung on a
-- cord from each end, so the merchant's outline changes as the other trades' do; at work, the
-- baskets set down on the ground behind it and a steelyard in hand, its beam tipping as the
-- weight slides over the goods in its pan.
function props.merchant(cv, x, y, f)
  local ox = x - HAND[1]
  if f == 0 then
    cv:hline(1 + ox, 10, 14, c("wood", 5)); cv:hline(2 + ox, 10, 2, c("wood", 6))
    cv:vline(2 + ox, 11, 2, c("wood", 3)); cv:vline(13 + ox, 11, 2, c("wood", 3))
    basket(cv, 1 + ox, 13); basket(cv, 12 + ox, 13)
    return
  end
  basket(cv, 0, FEET - 3); basket(cv, 2, FEET - 4)
  local tip = f == 1 and -1 or 1
  cv:line(x, y - 1, x + 4, y - 1 + tip, c("wood", 3))
  cv:set(x + 4, y + tip, c("stone", 4))
  cv:hline(x + 2, y + 2 + tip, 5, c("stone", 4)); cv:hline(x + 3, y + 1 + tip, 3, c("grass", 5))
  cv:set(x + 1, y - 2, c("gold", 5))
end

-- A small box carried; at work, a stone mortar at the waist, the pestle up then ground in, green
-- herb in the bowl.
function props.physician(cv, x, y, f)
  if f == 0 then cv:rect(x, y, 2, 2, c("wood", 3)); cv:set(x, y, c("paper", 6)); return end
  cv:hline(x - 3, y + 1, 5, c("stone", 4)); cv:hline(x - 2, y + 2, 3, c("stone", 3))
  cv:hline(x - 2, y, 2, c("grass", 5))
  local up = f == 1 and 2 or 0
  cv:vline(x, y - 2 - up, 3, c("wood", 3))
end

-- A hammer at rest; at work, raised overhead, then struck on the anvil before the smith, sparks
-- flying.
function props.smith(cv, x, y, f)
  if f == 0 then
    cv:vline(x, y - 3, 4, c("wood", 3)); cv:rect(x - 1, y - 5, 3, 2, c("stone", 3))
    return
  end
  -- The anvil on the ground before it.
  cv:hline(x + 1, FEET - 3, 5, c("stone", 4)); cv:hline(x + 1, FEET - 2, 5, c("stone", 2))
  cv:hline(x + 2, FEET - 1, 3, c("stone", 2)); cv:hline(x + 2, FEET, 3, c("stone", 2))
  if f == 1 then
    cv:line(x, y, x - 1, y - 5, c("wood", 3)); cv:rect(x - 2, y - 7, 3, 2, c("stone", 3))
  else
    cv:line(x, y, x + 2, y - 1, c("wood", 3)); cv:rect(x + 3, FEET - 5, 2, 2, c("stone", 3))
    cv:set(x + 6, FEET - 5, c("fire", 4)); cv:set(x + 2, FEET - 6, c("fire", 4)); cv:set(x + 5, FEET - 7, c("fire", 3))
  end
end

-- A long hoe over the shoulder, its blade standing out above the head; at work, raised, then struck
-- down into the ground before the farmer, earth thrown up.
function props.farmer(cv, x, y, f)
  if f == 0 then
    cv:line(x, y + 1, x - 5, y - 8, c("wood", 3))
    cv:rect(x - 7, y - 9, 2, 3, c("stone", 4)); cv:set(x - 6, y - 10, c("stone", 5))
    return
  end
  if f == 1 then
    cv:line(x, y, x - 3, y - 8, c("wood", 3)); cv:rect(x - 3, y - 10, 3, 2, c("stone", 4))
  else
    cv:line(x, y, x + 3, FEET - 2, c("wood", 3)); cv:rect(x + 3, FEET - 2, 3, 2, c("stone", 4))
    cv:set(x + 6, FEET - 3, c("earth", 3)); cv:set(x + 5, FEET - 4, c("earth", 3))
  end
end

-- A dark glazed jug in hand; at work, raised and tipped, the stream falling into the cup in the
-- other hand.
function props.innkeeper(cv, x, y, f)
  if f == 0 then
    cv:rect(x, y - 1, 3, 3, c("wood", 3)); cv:set(x + 1, y - 2, c("wood", 4)); cv:set(x, y - 1, c("wood", 5))
    return
  end
  local lift = f == 1 and 1 or 2
  cv:rect(x, y - 2 - lift, 3, 3, c("wood", 3)); cv:set(x, y - 2 - lift, c("wood", 5)); cv:set(x + 3, y - 2 - lift, c("wood", 4))
  cv:vline(x + 4, y - 1 - lift, lift + 2, c("water", 6))
  cv:hline(x + 3, y + 1, 3, c("stone", 6)); cv:set(x + 4, y + 2, c("stone", 5))
end

-- A wooden bowl held low; at work, held out, raised then lowered.
function props.beggar(cv, x, y, f)
  local up = (f == 1) and 2 or 0
  cv:hline(x, y - up, 3, c("wood", 4)); cv:hline(x + 1, y + 1 - up, 1, c("wood", 3))
  if f > 0 then cv:set(x + 1, y - 1 - up, c("stone", 6)) end
end

-- A string of beads in hand; at work, the palms pressed together before the chest.
function props.monk(cv, x, y, f)
  if f == 0 then cv:vline(x, y + 1, 2, c("wood", 5)); return end
  cv:set(9, 11, c("skin", 6)); cv:set(9, 12, c("skin", 5)); cv:set(10, 11, c("skin", 5))
end

-- The beggar's stick in the other hand, behind it.
local function stick(cv, x, y)
  cv:vline(x - 1, y - 6, 10, c("wood", 3))
end

-- How each trade's work leans its body, by frame: { dx, dy }, and whether its eyes shut.
local WORK_LEAN = {
  merchant = { { 0, 0 }, { 0, 0 } }, physician = { { 0, 1 }, { 0, 1 } }, smith = { { -1, 0 }, { 1, 1 } },
  farmer = { { -1, 0 }, { 1, 1 } }, innkeeper = { { 0, 0 }, { 0, 0 } }, beggar = { { 1, 1 }, { 1, 1 } },
  monk = { { 0, 0 }, { 1, 1 } },
}

-- One frame of `trade` on faction `f`: work 0 for idle and walking, 1 or 2 for the work's frames;
-- step "left" or "right" for a walk's step, bob for its passing frame.
local function trade_frame(trade, f, work, step, bob)
  local spec = copy(f.spec, { raw = true, step = step })
  spec.weapon = nil
  if trade == "monk" then spec.head, spec.hat = "bald", nil end
  local m = S.scheme(f.spec.faction)
  local cv = S.body(spec)
  garments[trade](cv, m)
  local dx, dy = 0, 0
  if work > 0 then dx, dy = WORK_LEAN[trade][work][1], WORK_LEAN[trade][work][2] end
  if work > 0 and (trade == "monk" or trade == "beggar") then
    for _, e in ipairs(EYES) do cv:set(e[1], e[2], c("skin", 4)) end
  end
  cv = lean(cv, dx, dy)
  if trade == "beggar" then
    local back = px.canvas(cv.w, cv.h)
    stick(back, OTHER[1] + dx, OTHER[2] + dy)
    back:stamp(cv, 0, 0)
    cv = back
  end
  props[trade](cv, HAND[1] + dx, HAND[2] + dy, work)
  cv:selout(0, 0, 16, 18, px.ramp_of[m.R][1])
  local out = px.canvas(W, H)
  out:stamp(cv, 0, TOP)
  if bob then
    -- Everything above the feet a pixel higher, the legs stretched to fill the gap.
    local feet = TOP + FEET
    local lifted = px.canvas(W, H)
    for k, i in pairs(out.p) do
      local x, y = k % W, k // W
      if y < feet - 1 then lifted:set(x, y - 1, i) else lifted:set(x, y, i) end
    end
    for x = 0, W - 1 do
      if lifted:get(x, feet - 2) == 0 then lifted:set(x, feet - 2, out:get(x, feet - 2)) end
    end
    out = lifted
  end
  return out
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
  for _, trade in ipairs(TRADES) do
    sets[#sets + 1] = { trade .. "_idle", { trade_frame(trade, f, 0) } }
    sets[#sets + 1] = { trade .. "_walk", { trade_frame(trade, f, 0, "left"), trade_frame(trade, f, 0, nil, true),
      trade_frame(trade, f, 0, "right"), trade_frame(trade, f, 0, nil, true) } }
    sets[#sets + 1] = { trade .. "_work", { trade_frame(trade, f, 1), trade_frame(trade, f, 2) } }
  end
  local frames, tags = {}, {}
  for _, suffix in ipairs({ "", "_xray" }) do
    for _, set in ipairs(sets) do
      local first = #frames + 1
      for _, cv in ipairs(set[2]) do frames[#frames + 1] = suffix == "" and cv or xray(cv, f.xray) end
      local ms = set[1]:find("idle") and 1000 or set[1]:find("_work$") and WORK_MS or (MOTION_MS[set[1]] or 150)
      tags[#tags + 1] = { set[1] .. suffix, first, #frames, ms }
    end
  end
  px.save_frames(W, H, frames, tags, here .. "/" .. name .. ".aseprite")
end
