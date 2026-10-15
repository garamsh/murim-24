-- The mockup of #170's trade looks, for the PM's approval before the build: the seven trades the
-- owner named, merchant, physician, smith, farmer, innkeeper, beggar and monk, each on the
-- Orthodox, Unorthodox and Demonic Cult bodies, at the production body's size on ADR 0069's ramps
-- with sel-out. A trade is palette and prop over the shared body (art/sprites.lua): it keeps the
-- faction's three cues, its accent, its garment's value and its head, and adds one garment piece
-- and the prop it carries in place of the faction's weapon. The monk alone changes the head, shaved.
-- It draws, from the production palette (art/pixel.lua) and body template (art/sprites.lua):
--   trades/<trade>.aseprite, 16x24 frames with the feet on row 22 as art/bodies' sheets, tagged
--     <faction>_idle (one), <faction>_walk (four, 150 ms: step, pass, other step, pass, a passing
--     frame a pixel higher) and <faction>_work (two, 250 ms, looped): weighing goods on a
--     steelyard, grinding herbs in a mortar, hammering at an anvil, hoeing, pouring from a jug,
--     begging with a bowl, praying;
--   trades/trades_sheet.aseprite, the contact sheet: every frame on paving at native size, and a
--     village square at play zoom, 3x, with all seven trades and two fighters;
--   trades/trades_frames.aseprite, every frame, for the 6x export.
-- Run: art/mockup/trades.sh [path to aseprite]

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)
local c = px.c

local W, H = 16, 24
-- The body canvas from sprites.lua puts the feet on its row 16; this lands them on row 22.
local TOP = 6
-- In the raw body's 16x18 pixels: the hand that holds a weapon, the other hand, the last row
-- above the hips, the eyes, the sash's row and the torso's columns.
local HAND = { 11, 13 }
local OTHER = { 4, 13 }
local HIPS = 13
local EYES = { { 6, 8 }, { 9, 8 } }
local FEET = 16

local FACTIONS = {
  { name = "orthodox", label = "ORTHODOX", spec = { faction = "orthodox", head = "topknot" } },
  { name = "unorthodox", label = "UNORTHODOX", spec = { faction = "unorthodox", head = "topknot", hat = "straw", body = "tunic" } },
  { name = "demonic", label = "DEMONIC CULT", spec = { faction = "demonic", head = "spiky", body = "tunic" } },
}
local TRADES = { "merchant", "physician", "smith", "farmer", "innkeeper", "beggar", "monk" }
local WORK = { merchant = "weighing", physician = "grinding", smith = "hammering", farmer = "hoeing",
  innkeeper = "pouring", beggar = "begging", monk = "praying" }

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
local function frame(trade, f, work, step, bob)
  local spec = copy(f.spec, { raw = true, step = step })
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

-- Each trade's frames on each faction, as { faction, { { tag, frames, ms } } }.
local sheets = {}
for _, trade in ipairs(TRADES) do
  local per = {}
  local frames, tags = {}, {}
  for _, f in ipairs(FACTIONS) do
    local sets = {
      { "idle", { frame(trade, f, 0) }, 500 },
      { "walk", { frame(trade, f, 0, "left"), frame(trade, f, 0, nil, true), frame(trade, f, 0, "right"),
        frame(trade, f, 0, nil, true) }, 150 },
      { "work", { frame(trade, f, 1), frame(trade, f, 2) }, 250 },
    }
    per[#per + 1] = { f, sets }
    for _, s in ipairs(sets) do
      local first = #frames + 1
      for _, cv in ipairs(s[2]) do frames[#frames + 1] = cv end
      tags[#tags + 1] = { f.name .. "_" .. s[1], first, #frames, s[3] }
    end
  end
  sheets[trade] = per
  px.save_frames(W, H, frames, tags, here .. "/trades/" .. trade .. ".aseprite")
end

-- The contact sheet -------------------------------------------------------------------------------

-- A trade's row: for each faction, idle, the walk's four, a gap, the work's two.
local CELL, GAP = 17, 6
local ROW_W = 3 * (7 * CELL + 3 * GAP + 10)
local ZOOM, SQ_W, SQ_H = 3, 150, 92
local SW = math.max(96 + ROW_W, SQ_W * ZOOM + 16)
local SH = 40 + #TRADES * 40 + 14 + SQ_H * ZOOM + 12
local bg = px.canvas(SW, SH)
local ground = px.canvas(SW, SH)
local shadows = px.canvas(SW, SH)
local things = px.canvas(SW, SH)
local labels = px.canvas(SW, SH)
bg:rect(0, 0, SW, SH, c("ink", 2))

-- Packed earth laid with large flags, as a village square lies: tan, so that both grey tools and
-- dark wood stand apart from it.
local function paving(cv, x, y, w, h)
  for yy = y, y + h - 1 do
    for xx = x, x + w - 1 do
      local lx, ly = xx - x, yy - y
      local row = ly // 9
      local off = (row % 2) * 6
      local edge = (ly % 9 == 0) or ((lx + off) % 12 == 0)
      local col = c("earth", 5)
      if px.hash((lx + off) // 12, row, 5) > 0.6 then col = c("earth", 6) end
      if edge then col = c("earth", 4) end
      cv:set(xx, yy, col)
    end
  end
end

-- The body shadow of art/markers under feet at (x, y).
local function shadow(cv, x, y)
  cv:each_ellipse(x, y, 6, 2.5, function(sx, sy, d)
    if px.bayer(sx, sy) < 0.8 - 0.7 * d then cv:set(sx, sy, c("ink", 1)) end
  end)
end

labels:text(8, 5, "MURIM24  TRADE LOOKS MOCKUP (#170)", c("paper", 6))
labels:text(8, 12, "NATIVE PIXELS. THE PRODUCTION BODY (16X24, SEL-OUT, ADR 0069 RAMPS). A TRADE IS PALETTE AND PROP OVER THE SHARED BODY;",
  c("stone", 5))
labels:text(8, 19, "EACH KEEPS ITS FACTION'S ACCENT, GARMENT VALUE AND HEAD. THE MONK ALONE IS SHAVED.", c("stone", 5))

local y = 40
for n, trade in ipairs(TRADES) do
  labels:text(8, y + 12, trade:upper(), c("paper", 6))
  labels:text(8, y + 20, WORK[trade]:upper(), c("stone", 4))
  paving(ground, 92, y + 2, ROW_W, 28)
  local x = 96
  for _, entry in ipairs(sheets[trade]) do
    local f, sets = entry[1], entry[2]
    if n == 1 then labels:text(x, y - 7, f.label, c("paper", 5)) end
    for _, s in ipairs(sets) do
      if n == 1 then labels:text(x, y + 31, s[1]:upper(), c("stone", 5)) end
      for _, cv in ipairs(s[2]) do
        things:stamp(cv, x, y + 4)
        x = x + CELL
      end
      x = x + (s[1] == "work" and 0 or GAP)
    end
    x = x + GAP + 10
  end
  y = y + 40
end

-- A village square at play zoom: all seven trades, each on a different faction's body, some at
-- work and some walking, with two fighters among them, armed.
local sq = { ground = px.canvas(SQ_W, SQ_H), shadows = px.canvas(SQ_W, SQ_H), things = px.canvas(SQ_W, SQ_H) }
paving(sq.ground, 0, 0, SQ_W, SQ_H)
local by_name = {}
for _, f in ipairs(FACTIONS) do by_name[f.name] = f end
local function trade_at(trade, faction, set, n, x, fy, flip)
  local per = sheets[trade]
  for _, entry in ipairs(per) do
    if entry[1].name == faction then
      for _, s in ipairs(entry[2]) do
        if s[1] == set then
          local cv = s[2][n]
          if flip then
            local out = px.canvas(W, H)
            for k, i in pairs(cv.p) do out:set(W - 1 - k % W, k // W, i) end
            cv = out
          end
          shadow(sq.shadows, x, fy)
          sq.things:stamp(cv, x - 8, fy - 22)
        end
      end
    end
  end
end
local function fighter(faction, weapon, x, fy, flip)
  local f = by_name[faction]
  local body = S.body(copy(f.spec, { weapon = weapon }))
  local cv = px.canvas(W, H)
  cv:stamp(body, 0, TOP)
  if flip then
    local out = px.canvas(W, H)
    for k, i in pairs(cv.p) do out:set(W - 1 - k % W, k // W, i) end
    cv = out
  end
  shadow(sq.shadows, x, fy)
  sq.things:stamp(cv, x - 8, fy - 22)
end
-- Back to front.
local cast = {
  { 26, function(fy) trade_at("merchant", "orthodox", "walk", 1, 22, fy) end },
  { 28, function(fy) trade_at("monk", "unorthodox", "work", 2, 118, fy, true) end },
  { 36, function(fy) trade_at("physician", "demonic", "work", 1, 64, fy) end },
  { 44, function(fy) fighter("orthodox", "sword", 96, fy, true) end },
  { 50, function(fy) trade_at("innkeeper", "unorthodox", "work", 2, 136, fy, true) end },
  { 58, function(fy) trade_at("farmer", "orthodox", "walk", 1, 40, fy) end },
  { 62, function(fy) fighter("demonic", "saber", 80, fy) end },
  { 72, function(fy) trade_at("smith", "unorthodox", "work", 2, 16, fy) end },
  { 80, function(fy) trade_at("beggar", "demonic", "work", 1, 108, fy, true) end },
}
for _, actor in ipairs(cast) do actor[2](actor[1]) end
local zy = y + 2
labels:text(8, zy, "PLAY ZOOM 3X  A VILLAGE SQUARE: THE SEVEN TRADES ON MIXED FACTIONS, AND TWO FIGHTERS (ORTHODOX SWORD, CULT SABER)",
  c("paper", 5))
for _, layer in ipairs({ sq.ground, sq.shadows, sq.things }) do
  for k, i in pairs(layer.p) do
    local x, yy = k % SQ_W, k // SQ_W
    ground:rect(8 + x * ZOOM, zy + 8 + yy * ZOOM, ZOOM, ZOOM, i)
  end
end

px.save(SW, SH, { { "bg", bg }, { "ground", ground }, { "shadows", shadows }, { "things", things }, { "labels", labels } },
  here .. "/trades/trades_sheet.aseprite")

-- Every frame on paving, a trade to a row, the factions left to right, for the 6x export.
do
  local FW, FH = 4 + 3 * (7 * 18 + 8), 2 + #TRADES * 26
  local fb = px.canvas(FW, FH)
  paving(fb, 0, 0, FW, FH)
  for n, trade in ipairs(TRADES) do
    local x = 4
    for _, entry in ipairs(sheets[trade]) do
      for _, s in ipairs(entry[2]) do
        for _, cv in ipairs(s[2]) do fb:stamp(cv, x, 2 + (n - 1) * 26); x = x + 18 end
      end
      x = x + 8
    end
  end
  px.save(FW, FH, { { "frames", fb } }, here .. "/trades/trades_frames.aseprite")
end
