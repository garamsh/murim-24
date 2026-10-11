-- Draws the structures of a village and a sect's seat at the 2.5D angle (ADR 0059): the ground
-- seen from above at 12 pixels a cell deep and 16 wide, fronts and roofs standing up from it, light
-- from the upper left. Each <kind>.aseprite has four frames, tagged intact, damaged, destroyed and
-- rubble as the core's structure stages, and holds its footprint's bottom-left corner at
-- (8, height - 8), with what rises above the footprint in the rows above: house (3 x 3 cells),
-- hall (5 x 3), storehouse (3 x 2), manual_hall (4 x 3), granary (2 x 2), well (2 x 2), seat_gate
-- (3 x 1), barrels (1 x 1), fence (5 x 1) and training_ground (6 x 4), which is laid with the
-- ground; and the walled town's pieces: its wall along the south and north (1 x 2) and the west and
-- east (2 x 1), corner tower (3 x 3), gate tower (5 x 2), steps (1 x 2), temple (4 x 3), market
-- stall (2 x 1) and courtyard house (5 x 5), each named town_<piece>. The smithy and the
-- apothecary are workshops.lua's. The cast shadow is not in the frame.
-- Run: aseprite --batch --script art/structures/structures.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c
local INK = c("ink", 1)

local CW, CD = 16, 12

-- A canvas for a footprint of w x d cells rising `rise` pixels above its back edge, and the
-- footprint's left, front (bottom) and back edges in it.
local function frame(w, d, rise)
  local W, H = w * CW + 16, d * CD + rise + 16
  local cv = px.canvas(W, H)
  return cv, 8, H - 8, H - 8 - d * CD
end

-- A block standing on the ground: its top face `d` deep and its front face `h` high, from x for
-- w, its front's foot on `base`. top and front are ramp names, and n the step its faces sit on.
local function block(cv, x, base, w, d, h, ramp, n)
  cv:rect(x, base - h - d, w, d, c(ramp, n + 1))
  cv:hline(x, base - h - d, w, c(ramp, n + 2))
  cv:rect(x, base - h, w, h, c(ramp, n))
  cv:hline(x, base - h, w, c(ramp, n + 1))
  cv:vline(x, base - h - d, d + h, c(ramp, n + 1))
  cv:vline(x + w - 1, base - h - d, d + h, c(ramp, n - 1))
end

-- A tiled roof seen from the angle: the front slope from its eave up to the ridge, the back
-- slope from the ridge up to the back eave, hipped ends shaded as they face, and upturned eave
-- corners. x0..x1 is the eave's span; the ridge is `hip` shorter at each end.
local function roof(cv, x0, x1, eave, ridge, back, hip, tone)
  tone = tone or "stone"
  local function course(xx, yy, k)
    local ch = (xx - x0) % 3
    local col = ch == 0 and c(tone, 2) or (ch == 1 and c(tone, 4) or c(tone, 3))
    if (yy - ridge) % 4 == 3 and ch ~= 0 then col = px.shift(col, -1) end
    return px.shift(col, k)
  end
  -- Front slope: rows from the ridge down to the eave, widening from the ridge's span.
  for yy = ridge, eave do
    local t = (yy - ridge) / math.max(1, eave - ridge)
    local inset = math.floor(hip * (1 - t) + 0.5)
    local flare = (eave - yy < 2) and (2 - (eave - yy)) or 0
    for xx = x0 - flare, x1 + flare do
      local left, right = xx < x0 + inset, xx > x1 - inset
      local k = 0
      if left then k = 1 elseif right then k = -2 end
      cv:set(xx, yy, course(xx, yy, k))
    end
  end
  -- Back slope: lighter and steeper to the eye, narrowing back up to the back eave.
  for yy = back, ridge - 1 do
    local t = (ridge - yy) / math.max(1, ridge - back)
    local inset = math.floor(hip * (1 - t) + 0.5)
    for xx = x0 + 1, x1 - 1 do
      local left, right = xx < x0 + inset, xx > x1 - inset
      local col = (xx - x0) % 3 == 0 and c(tone, 3) or c(tone, 4)
      if right then col = px.shift(col, -1) elseif left then col = px.shift(col, 1) end
      cv:set(xx, yy, col)
    end
  end
  -- Ridge with curled, gold-tipped ends.
  local r0, r1 = x0 + hip, x1 - hip
  cv:hline(r0, ridge - 1, r1 - r0 + 1, c(tone, 2)); cv:hline(r0, ridge, r1 - r0 + 1, c("ink", 2))
  for _, s in ipairs({ { r0, -1 }, { r1, 1 } }) do
    cv:set(s[1] + s[2], ridge - 1, c("ink", 2)); cv:set(s[1] + s[2] * 2, ridge - 2, c("ink", 2))
    cv:set(s[1] + s[2] * 2, ridge - 3, c("gold", 3))
  end
  -- Round tile ends along the front eave, its corners turned up.
  for xx = x0 - 2, x1 + 2 do cv:set(xx, eave, ((xx - x0) % 3 == 1) and c(tone, 5) or c(tone, 1)) end
  cv:set(x0 - 3, eave - 1, c(tone, 2)); cv:set(x0 - 4, eave - 2, c(tone, 2))
  cv:set(x1 + 3, eave - 1, c(tone, 2)); cv:set(x1 + 4, eave - 2, c(tone, 2))
end

-- A latticed window, w x h, its top-left at (x, y).
local function window(cv, x, y, w, h)
  cv:rect(x, y, w, h, c("wood", 1))
  for k = 0, w - 1, 2 do cv:vline(x + k, y, h, c("wood", 4)) end
  cv:hline(x, y, w, c("wood", 4)); cv:hline(x, y + h // 2, w, c("wood", 4))
  cv:hline(x, y + h - 1, w, c("wood", 3))
end

-- A plastered front between lacquered pillars, from x for w, its foot on `base` and `h` high,
-- with a door at its middle and windows at the given offsets.
local function front(cv, x, base, w, h, windows, door)
  local top = base - h
  cv:rect(x, top, w, h, c("paper", 2))
  cv:hline(x, base - 2, w, c("paper", 1))
  cv:hline(x, top, w, c("paper", 1))
  local pillars = { x, x + w - 2 }
  for _, p in ipairs(pillars) do cv:vline(p, top, h, c("lacquer", 3)); cv:vline(p + 1, top, h, c("lacquer", 2)) end
  if door ~= false then
    local dx = x + w // 2 - 4
    cv:rect(dx, top + 3, 8, h - 3, c("wood", 2))
    cv:vline(dx + 4, top + 3, h - 3, c("wood", 1))
    cv:hline(dx - 1, top + 2, 10, c("wood", 3))
    cv:set(dx + 2, top + 3 + (h - 3) // 2, c("gold", 3)); cv:set(dx + 5, top + 3 + (h - 3) // 2, c("gold", 3))
    cv:vline(dx - 2, top, h, c("lacquer", 3)); cv:vline(dx + 9, top, h, c("lacquer", 2))
  end
  for _, wx in ipairs(windows or {}) do window(cv, x + wx, top + 3, 7, math.max(4, h - 8)) end
end

-- A tiled building over w x d cells: a stone plinth, a plastered front `wall` high and a roof
-- whose ridge stands `ridge` above the wall's top.
local function building(cv, left, base, w, d, wall, ridge, windows, opts)
  opts = opts or {}
  local W, D = w * CW, d * CD
  block(cv, left, base, W, D, 3, "stone", 3)
  local foot = base - 3
  front(cv, left + 3, foot, W - 6, wall, windows, opts.door)
  local eave = foot - wall + 2
  local back = foot - D - wall + 3
  local ridge_y = foot - D // 2 - wall - ridge
  roof(cv, left + 1, left + W - 2, eave, ridge_y, back, opts.hip or 6, opts.tone)
  -- The eave's shade on the wall's top rows.
  for xx = left + 3, left + W - 4 do cv:shade_at(xx, eave + 1, -1) end
  return eave, ridge_y, back
end

-- The damage stages, drawn from the intact frame and the footprint, w x d cells, at (8, H - 8):
-- damaged holes the roof down to its rafters, scorches the front and drops tiles before it;
-- destroyed keeps the plinth and ragged stumps of the front over a charred, open inside with
-- fallen beams and one slab of roof; rubble is a heap on the plinth with weeds coming back.
local function damaged(intact, w, d, seed)
  local cv = px.canvas(intact.w, intact.h)
  cv:stamp(intact, 0, 0)
  local r = px.rng(seed)
  local base = cv.h - 8
  local top = 0
  while top < cv.h do
    local any = false
    for x = 0, cv.w - 1 do if cv:get(x, top) ~= 0 then any = true break end end
    if any then break end
    top = top + 1
  end
  for _ = 1, 2 + w // 2 do
    local hw, hh = r.int(5, 9), r.int(3, 6)
    local hx, hy = r.int(10, math.max(10, cv.w - 10 - hw)), r.int(top + 4, math.max(top + 4, base - d * CD // 2 - hh))
    for yy = hy, hy + hh - 1 do
      for xx = hx, hx + hw - 1 do
        if cv:get(xx, yy) ~= 0 then
          local edge = xx == hx or xx == hx + hw - 1 or yy == hy or yy == hy + hh - 1
          if not edge or r.next() > 0.5 then cv.p[yy * cv.w + xx] = ((xx - hx) % 3 == 1) and c("wood", 3) or c("ink", 1) end
        end
      end
    end
  end
  for yy = top, base do
    for xx = cv.w // 2, cv.w - 1 do
      if cv:get(xx, yy) ~= 0 and px.noise(xx, yy, 4, seed) > 0.55 and (xx + yy) % 2 == 0 then
        cv.p[yy * cv.w + xx] = c("ink", 2)
      end
    end
  end
  for _ = 1, 6 + w * 2 do cv:set(r.int(6, cv.w - 6), r.int(base + 1, base + 5), c("stone", r.int(2, 4))) end
  return cv
end

local function destroyed(intact, w, d, seed)
  local cv = px.canvas(intact.w, intact.h)
  local r = px.rng(seed)
  local base = cv.h - 8
  local W, D = w * CW, d * CD
  -- The plinth and the front's lower part, broken off along a ragged line.
  for x = 0, cv.w - 1 do
    local keep = base - 3 - math.floor(px.noise(x, 0, 4, seed) * math.min(12, D // 2))
    for y = keep, cv.h - 1 do
      local i = intact:get(x, y)
      if i ~= 0 then cv:set(x, y, y == keep and c("ink", 3) or i) end
    end
  end
  -- The open inside, charred, behind the stumps.
  for y = base - 3 - D, base - 6 do
    for x = 10, 8 + W - 3 do
      local n = px.noise(x, y, 4, seed)
      if cv:get(x, y) == 0 and n > 0.3 then
        cv:set(x, y, n > 0.7 and c("stone", 2) or (n > 0.5 and c("ink", 3) or c("wood", 1)))
      end
    end
  end
  -- Beams fallen across it, and a slab of roof still leaning on the left.
  for k = 0, w do
    local bx = 10 + k * (W - 6) // math.max(1, w)
    cv:line(bx, base - D - 2 - r.int(0, 6), bx + r.int(-6, 6), base - 6, c("wood", 2))
  end
  for y = 0, math.min(D, 14) do
    for x = 9, 9 + math.max(6, W // 3) - y do
      local i = intact:get(x, base - D - 12 + y)
      if i ~= 0 then cv:set(x, base - D - 4 + y, i) end
    end
  end
  for _ = 1, 10 + w * 3 do cv:set(r.int(6, cv.w - 6), r.int(base - 2, base + 5), c("ink", r.int(2, 4))) end
  return cv
end

local function rubble(intact, w, d, seed)
  local cv = px.canvas(intact.w, intact.h)
  local base = cv.h - 8
  local W, D = w * CW, d * CD
  for x = 0, cv.w - 1 do
    for y = base - 3, cv.h - 1 do
      local i = intact:get(x, y)
      if i ~= 0 then cv:set(x, y, i) end
    end
  end
  cv:rect(9, base - 3 - D, W - 2, D, c("stone", 3))
  local cx, cy = 8 + W / 2, base - 2 - D / 2
  cv:each_ellipse(cx, cy, W / 2 - 2, math.max(4, D / 2 - 1), function(xx, yy, dist)
    if dist < 0.7 + px.noise(xx, yy, 3, seed) * 0.35 then
      local n = px.noise(xx, yy, 2, seed + 1)
      local col = n > 0.7 and c("stone", 4) or (n > 0.45 and c("stone", 3) or (n > 0.3 and c("stone", 2) or c("wood", 2)))
      if dist > 0.6 then col = px.shift(col, -1) end
      cv:set(xx, yy, col)
    end
  end)
  for k = 1, 4 + w do
    local x, y = math.floor(cx + (px.hash(k, 1, seed) - 0.5) * W * 0.8), math.floor(cy + (px.hash(k, 2, seed) - 0.5) * D * 0.6)
    cv:set(x, y, c("grass", 5)); cv:set(x, y + 1, c("grass", 3))
  end
  return cv
end

-- Outlines a finished frame in ink.
local function save_frame(cv) cv:outline(0, 0, cv.w, cv.h, INK) end

-- Saves the intact frame and, given its footprint, the three damage stages after it. `trim`, for
-- a piece laid end to end in a run, takes the outline off each frame where the next piece meets it.
local function save(name, cv, w, d, trim)
  local frames = { cv }
  if w then
    local seed = #name * 97 + w * 13 + d
    frames[2] = damaged(cv, w, d, seed)
    frames[3] = destroyed(cv, w, d, seed)
    frames[4] = rubble(cv, w, d, seed)
  end
  for _, f in ipairs(frames) do
    f:outline(0, 0, f.w, f.h, INK)
    if trim then trim(f) end
  end
  local tags = { { "intact", 1, 1, 1000 } }
  if w then
    tags[2] = { "damaged", 2, 2, 1000 }; tags[3] = { "destroyed", 3, 3, 1000 }; tags[4] = { "rubble", 4, 4, 1000 }
  end
  px.save_frames(cv.w, cv.h, frames, tags, here .. "/" .. name .. ".aseprite")
end

-- The village house, 3 x 3 cells.
do
  local cv, x, base = frame(3, 3, 40)
  building(cv, x, base, 3, 3, 14, 12, { 5, 36 }, { hip = 10 })
  save("house", cv, 3, 3)
end

-- The village hall, 5 x 3 cells: a longer front with four windows and a taller roof.
do
  local cv, x, base = frame(5, 3, 46)
  building(cv, x, base, 5, 3, 16, 15, { 6, 18, 55, 67 }, { hip = 14 })
  save("hall", cv, 5, 3)
end

-- The storehouse, 3 x 2 cells: no windows, studded double doors, a lower roof, a crate.
do
  local cv, x, base = frame(3, 2, 32)
  building(cv, x, base, 3, 2, 12, 9, {}, { hip = 5 })
  local dx = x + 24 - 5
  for k = 0, 1 do for j = 0, 2 do cv:set(dx + 2 + k * 5, base - 12 + j * 3, c("stone", 5)) end end
  for k = 0, 1 do
    cv:rect(x + 6 + k * 30, base - 11, 3, 2, c("ink", 2))
  end
  block(cv, x + 36, base + 1, 8, 4, 6, "wood", 3)
  save("storehouse", cv, 3, 2)
end

-- The manual hall, 4 x 3 cells: two storeys on a high stone platform with steps, a gold-framed
-- plaque under the upper roof.
do
  local cv, x, base = frame(4, 3, 64)
  local W, D = 64, 36
  block(cv, x, base, W, D, 6, "stone", 3)
  for k = 0, 2 do cv:hline(x + 24 - k, base - 2 - k * 2, 16 + k * 2, c("stone", 5 - (k % 2))) end
  local foot = base - 6
  front(cv, x + 4, foot, W - 8, 13, { 6, 45 })
  local eave = foot - 13 + 2
  -- The lower roof is a skirt round the upper storey: its front slope only.
  roof(cv, x + 1, x + W - 2, eave, eave - 9, eave - 9, 3)
  local foot2 = eave - 8
  front(cv, x + 12, foot2, W - 24, 12, { 6, 27 }, false)
  cv:rect(x + 26, foot2 - 10, 12, 6, c("gold", 3)); cv:rect(x + 27, foot2 - 9, 10, 4, c("lacquer", 2))
  cv:hline(x + 29, foot2 - 7, 6, c("gold", 4))
  local eave2 = foot2 - 12 + 2
  roof(cv, x + 8, x + W - 9, eave2, eave2 - 16, eave2 - 22, 7)
  save("manual_hall", cv, 4, 3)
end

-- The granary, 2 x 2 cells: a plank store on stilts over stone footings, a ladder to its door,
-- under a hipped straw roof with a ragged fringe.
do
  local cv, x, base = frame(2, 2, 34)
  local W, D = 32, 24
  for _, p in ipairs({ { 3, 0 }, { W - 6, 0 }, { 3, -14 }, { W - 6, -14 } }) do
    block(cv, x + p[1] - 1, base + p[2], 5, 2, 2, "stone", 3)
    cv:rect(x + p[1], base + p[2] - 9, 2, 7, c("wood", 2)); cv:vline(x + p[1], base + p[2] - 9, 7, c("wood", 3))
  end
  local foot = base - 9
  cv:rect(x + 2, foot - 11, W - 4, 11, c("wood", 3))
  for xx = x + 2, x + W - 3, 3 do cv:vline(xx, foot - 11, 11, c("wood", 2)) end
  cv:hline(x + 2, foot - 1, W - 4, c("wood", 1))
  cv:rect(x + 12, foot - 9, 7, 8, c("wood", 1))
  for k = 0, 4 do
    cv:set(x + 13, foot + 1 + k * 2 - 1, c("wood", 4)); cv:set(x + 18, foot + 1 + k * 2 - 1, c("wood", 4))
    cv:hline(x + 13, foot + k * 2, 6, c("wood", 3))
  end
  local eave = foot - 9
  local ridge = eave - 18
  for yy = ridge - 4, eave do
    local t = (yy - (ridge - 4)) / (eave - ridge + 4)
    local inset = math.floor(10 * (1 - t))
    for xx = x - 1 + inset, x + W + inset * -1 do
      local n = px.noise(xx, yy, 2, 31)
      local col = n > 0.6 and c("earth", 6) or (n > 0.3 and c("earth", 5) or c("earth", 4))
      if yy < ridge then col = px.shift(col, 1) end
      if xx > x + W - inset - 3 then col = px.shift(col, -1) end
      cv:set(xx, yy, col)
    end
  end
  cv:hline(x + 9, ridge, W - 18, c("wood", 1))
  for xx = x - 1, x + W do
    if px.hash(xx, 0, 5) > 0.35 then cv:set(xx, eave + 1, c("earth", 3)) end
  end
  save("granary", cv, 2, 2)
end

-- The well, 2 x 2 cells: a stone ring round dark water, two posts and a small tiled roof, a
-- bucket on its rope.
do
  local cv, x, base = frame(2, 2, 26)
  local cx, cy = x + 16, base - 14
  -- The ring's front face, then its top seen from above with the water inside.
  for xx = cx - 11, cx + 11 do
    local dx = (xx - cx) / 11.5
    local lip = cy + math.floor(6 * math.sqrt(math.max(0, 1 - dx * dx)) + 0.5)
    for yy = lip, lip + 6 do
      local col = c("stone", 3)
      if (yy - lip) % 3 == 2 then col = c("stone", 2) end
      if xx < cx - 6 then col = px.shift(col, 1) elseif xx > cx + 6 then col = px.shift(col, -1) end
      cv:set(xx, yy, col)
    end
  end
  cv:ellipse(cx, cy, 11.5, 6.2, c("stone", 5))
  cv:ellipse(cx, cy + 0.5, 8.5, 4.2, c("water", 1))
  cv:ellipse(cx - 1, cy, 5, 2.2, c("water", 2))
  cv:set(cx - 3, cy - 1, c("water", 4))
  for _, p in ipairs({ cx - 12, cx + 11 }) do
    cv:rect(p, cy - 22, 2, 26, c("wood", 3)); cv:vline(p, cy - 22, 26, c("wood", 4))
  end
  cv:hline(cx - 11, cy - 15, 23, c("wood", 2))
  cv:vline(cx + 3, cy - 14, 9, c("paper", 1))
  cv:rect(cx + 1, cy - 6, 5, 4, c("wood", 3)); cv:hline(cx + 1, cy - 6, 5, c("wood", 4))
  roof(cv, cx - 15, cx + 15, cy - 21, cy - 29, cy - 32, 4)
  save("well", cv, 2, 2)
end

-- The seat gate, 3 x 1 cells: a paifang of two lacquered pillars on stone bases at the end
-- cells, a painted bracket band and a gold-framed name board under a tiled roof.
do
  local cv, x, base = frame(3, 1, 40)
  local W = 48
  for _, p in ipairs({ 3, W - 13 }) do
    block(cv, x + p, base, 10, 6, 4, "stone", 3)
    cv:rect(x + p + 2, base - 38, 6, 34, c("lacquer", 2))
    cv:vline(x + p + 2, base - 38, 34, c("lacquer", 3)); cv:vline(x + p + 7, base - 38, 34, c("lacquer", 1))
  end
  cv:rect(x + 1, base - 42, W - 2, 3, c("lacquer", 3))
  for xx = x + 2, x + W - 3, 4 do cv:rect(xx, base - 45, 2, 3, c("jade", 2)); cv:set(xx + 2, base - 44, c("gold", 3)) end
  cv:rect(x + 15, base - 39, 18, 9, c("gold", 3)); cv:rect(x + 16, base - 38, 16, 7, c("ink", 2))
  cv:hline(x + 18, base - 35, 4, c("gold", 4)); cv:hline(x + 25, base - 35, 4, c("gold", 4))
  cv:vline(x + 20, base - 37, 4, c("gold", 4)); cv:vline(x + 27, base - 37, 4, c("gold", 4))
  roof(cv, x - 1, x + W, base - 46, base - 53, base - 56, 4)
  save("seat_gate", cv, 3, 1)
end

-- Barrels, 1 x 1 cell: two barrels and one on top.
do
  local cv, x, base = frame(1, 1, 22)
  local function barrel(bx, by)
    cv:ellipse(bx + 4, by - 9, 4, 1.6, c("wood", 4))
    cv:rect(bx, by - 9, 9, 9, c("wood", 3))
    cv:vline(bx, by - 9, 9, c("wood", 4)); cv:vline(bx + 8, by - 9, 9, c("wood", 2))
    cv:hline(bx, by - 7, 9, c("stone", 2)); cv:hline(bx, by - 2, 9, c("stone", 2))
    cv:ellipse(bx + 4, by - 9, 3, 1, c("wood", 5))
  end
  barrel(x, base); barrel(x + 7, base - 1); barrel(x + 3, base - 9)
  save("barrels", cv, 1, 1)
end

-- A fence, 5 x 1 cells: posts every 8 pixels and two rails.
do
  local cv, x, base = frame(5, 1, 6)
  for xx = x + 1, x + 78, 8 do cv:rect(xx, base - 12, 2, 12, c("wood", 3)); cv:set(xx, base - 12, c("wood", 4)) end
  cv:hline(x, base - 10, 80, c("wood", 4)); cv:hline(x, base - 9, 80, c("wood", 2))
  cv:hline(x, base - 5, 80, c("wood", 4)); cv:hline(x, base - 4, 80, c("wood", 2))
  save("fence", cv, 5, 1)
end

-- The training ground, 6 x 4 cells, laid with the ground: raked sand in a stone curb, a ring
-- trodden where pupils spar, and weapon racks and dummies along its back row. Damaged scorches the
-- sand, knocks a rack flat and snaps a dummy; destroyed burns the racks to their frames and the
-- dummies and poles to stumps; rubble is grass and ash inside the curb with a heap of burnt wood at
-- the back. The curb stays in every stage.
local function training_ground(stage)
  local cv, x, base = frame(6, 4, 18)
  local W, D = 96, 48
  local seed = 97
  block(cv, x, base, W, D, 2, "stone", 3)
  for yy = base - 2 - D + 2, base - 4 do
    for xx = x + 2, x + W - 3 do
      local col = ((yy + (xx // 9)) % 3 == 0) and c("earth", 5) or c("earth", 6)
      if px.noise(xx, yy, 3, 7) > 0.7 then col = c("earth", 5) end
      if stage == "rubble" then
        local n = px.noise(xx, yy, 3, seed)
        col = n > 0.62 and c("grass", 4) or (n > 0.4 and c("ink", 3) or c("earth", 4))
      elseif stage ~= "intact" and px.noise(xx, yy, 4, seed + (stage == "destroyed" and 1 or 0)) > (stage == "destroyed" and 0.45 or 0.62) then
        col = (xx + yy) % 2 == 0 and c("ink", 3) or c("earth", 3)
      end
      cv:set(xx, yy, col)
    end
  end
  local back = base - 2 - D + 8
  if stage == "rubble" then
    cv:each_ellipse(x + W / 2, back - 2, 14, 4, function(xx, yy)
      local n = px.noise(xx, yy, 2, seed)
      cv:set(xx, yy, n > 0.6 and c("wood", 1) or (n > 0.35 and c("ink", 3) or c("ink", 2)))
    end)
    save_frame(cv)
    return cv
  end
  if stage ~= "destroyed" then
    cv:each_ellipse(x + W / 2, base - 24, 26, 13, function(xx, yy, d)
      if d > 0.78 then cv:set(xx, yy, c("earth", 4)) end
    end)
  end
  local burnt = stage == "destroyed"
  for n, rx in ipairs({ x + 8, x + 62 }) do
    if stage == "damaged" and n == 2 then
      -- Knocked flat: its frame and weapons lying on the sand.
      cv:hline(rx, back - 1, 23, c("wood", 2)); cv:hline(rx + 2, back + 1, 20, c("wood", 4))
      for k = 0, 4 do cv:set(rx + 3 + k * 4, back, c("stone", 6)) end
    else
      local post = burnt and c("ink", 3) or c("wood", 2)
      cv:rect(rx, back - 14, 1, 14, post); cv:rect(rx + 22, back - 14, 1, 14, post)
      cv:hline(rx, back - 12, 23, burnt and c("ink", 2) or c("wood", 3))
      cv:hline(rx, back - 6, 23, burnt and c("ink", 2) or c("wood", 3))
      if not burnt then
        for k = 0, 4 do
          local wx = rx + 3 + k * 4
          cv:vline(wx, back - 17, 16, c("wood", 4)); cv:set(wx, back - 18, c("stone", 6))
        end
      end
    end
  end
  for k = 0, 2 do
    local dx = x + 38 + k * 9
    if burnt or (stage == "damaged" and k == 1) then
      cv:rect(dx, back - 3, 2, 3, burnt and c("ink", 3) or c("wood", 3))
    else
      cv:rect(dx, back - 9, 2, 9, c("wood", 3)); cv:rect(dx - 2, back - 12, 6, 4, c("wood", 4))
      cv:rect(dx - 1, back - 15, 4, 3, c("wood", 4))
    end
  end
  save_frame(cv)
  return cv
end
do
  local frames = {}
  for n, stage in ipairs({ "intact", "damaged", "destroyed", "rubble" }) do frames[n] = training_ground(stage) end
  px.save_frames(frames[1].w, frames[1].h, frames, { { "intact", 1, 1, 1000 }, { "damaged", 2, 2, 1000 },
    { "destroyed", 3, 3, 1000 }, { "rubble", 4, 4, 1000 } }, here .. "/training_ground.aseprite")
end

-- The walled town's pieces (#200): a wall two cells thick and a level high, laid a piece a cell
-- along each side, its walkway behind crenellations on its outer edge; a corner tower; a gate
-- tower over each gate, the passage through its middle cell; steps up to the walkway; and the
-- town's own buildings, a temple, a market stall and a courtyard house.
local LEVEL = 16

-- A wall's face of stone courses `h` high, from x for w with its foot on `base`, the joints
-- staggered course by course so pieces laid side by side run on unbroken.
local function courses(cv, x, base, w, h)
  for y = base - h, base - 1 do
    local course = (y - (base - h)) // 4
    for xx = x, x + w - 1 do
      local joint = (y - (base - h)) % 4 == 3 or (xx + course * 4) % 8 == 0
      cv:set(xx, y, joint and c("stone", 2) or c("stone", 3))
    end
  end
  cv:hline(x, base - 1, w, c("stone", 2))
end

-- A paved walkway seen from above, from x for w and from row `top` for d rows.
local function walkway(cv, x, top, w, d)
  for y = top, top + d - 1 do
    for xx = x, x + w - 1 do
      local joint = (y - top) % 6 == 5 or (xx + ((y - top) // 6) * 4) % 8 == 0
      cv:set(xx, y, joint and c("stone", 4) or c("stone", 5))
    end
  end
end

-- Merlons along a row: a block every 4 pixels, 2 wide, rising 5 above `edge`, lit on its left.
local function merlons_along(cv, x, edge, w)
  cv:hline(x, edge - 2, w, c("stone", 3))
  for k = 0, w - 1, 4 do
    cv:rect(x + k, edge - 5, 2, 5, c("stone", 4))
    cv:set(x + k, edge - 5, c("stone", 5)); cv:set(x + k + 1, edge - 5, c("stone", 5))
  end
end

-- Merlons down a column, from row `top` for d rows: each a top 3 wide and 2 deep over a face 3
-- high, every 6 rows.
local function merlons_down(cv, x, top, d)
  cv:vline(x + 1, top, d, c("stone", 3))
  for y = top + 4, top + d - 1, 6 do
    cv:rect(x, y - 3, 3, 2, c("stone", 5))
    cv:rect(x, y - 1, 3, 3, c("stone", 3))
  end
end

-- Takes the outline off a piece's left and right, where the pieces beside it in its run meet it.
local function trim_sides(w)
  return function(f)
    for y = 0, f.h - 1 do
      for x = 0, 7 do f:clear(x, y) end
      for x = 8 + w * CW, f.w - 1 do f:clear(x, y) end
    end
  end
end

-- Takes the outline off the rows above a piece's walkway, where the piece behind it in its run
-- meets it.
local function trim_above(top)
  return function(f)
    for y = 0, top - 1 do
      for x = 0, f.w - 1 do f:clear(x, y) end
    end
  end
end

-- The wall along the south and north sides, 1 x 2 cells: its face a level high, the walkway two
-- cells deep on it, crenellated along the outer edge, the front for the south wall and the back
-- for the north, with a low parapet along the inner one.
for _, side in ipairs({ "south", "north" }) do
  local cv, x, base = frame(1, 2, LEVEL + 8)
  local W, D = CW, 2 * CD
  local top = base - LEVEL
  courses(cv, x, base, W, LEVEL)
  walkway(cv, x, top - D, W, D)
  if side == "south" then
    merlons_along(cv, x, top, W)
    cv:hline(x, top - D, W, c("stone", 3))
  else
    merlons_along(cv, x, top - D + 1, W)
    cv:hline(x, top - 1, W, c("stone", 3))
  end
  save("town_wall_" .. side, cv, 1, 2, trim_sides(1))
end

-- The wall along the west and east sides, 2 x 1 cells: the walkway two cells wide seen from
-- above, crenellated down its outer edge, the left for the west wall and the right for the east,
-- over a face that shows where the run ends.
for _, side in ipairs({ "west", "east" }) do
  local cv, x, base = frame(2, 1, LEVEL + 4)
  local W, D = 2 * CW, CD
  local top = base - LEVEL
  courses(cv, x, base, W, LEVEL)
  walkway(cv, x, top - D, W, D)
  if side == "west" then
    merlons_down(cv, x, top - D, D)
    cv:vline(x + W - 1, top - D, D, c("stone", 3))
  else
    merlons_down(cv, x + W - 3, top - D, D)
    cv:vline(x, top - D, D, c("stone", 3))
  end
  save("town_wall_" .. side, cv, 2, 1, trim_above(top - D))
end

-- A corner tower, 3 x 3 cells, two levels high, crenellated all round, with a small watch
-- pavilion on its top.
do
  local cv, x, base = frame(3, 3, 40)
  local W, D = 3 * CW, 3 * CD
  local top = base - 2 * LEVEL
  courses(cv, x, base, W, 2 * LEVEL)
  cv:vline(x, top, 2 * LEVEL, c("stone", 4)); cv:vline(x + W - 1, top, 2 * LEVEL, c("stone", 2))
  walkway(cv, x, top - D, W, D)
  merlons_along(cv, x, top - D + 1, W)
  merlons_down(cv, x, top - D, D)
  merlons_down(cv, x + W - 3, top - D, D)
  local foot = top - 10
  front(cv, x + 10, foot, W - 20, 9, {}, false)
  roof(cv, x + 7, x + W - 8, foot - 7, foot - 15, foot - 19, 5)
  merlons_along(cv, x, top, W)
  save("town_corner_tower", cv, 3, 3)
end

-- A gate tower, 5 x 2 cells, over the wall where a road passes it: the passage through its
-- middle cell under a gold-framed name board, and a gate house on top under a broad tiled roof.
do
  local cv, x, base = frame(5, 2, 48)
  local W, D = 5 * CW, 2 * CD
  local h = LEVEL + 4
  local top = base - h
  courses(cv, x, base, W, h)
  cv:vline(x, top, h, c("stone", 4)); cv:vline(x + W - 1, top, h, c("stone", 2))
  -- The passage, dark under its arch, its doors open against its sides.
  local px0 = x + 2 * CW
  cv:rect(px0, base - 13, CW, 13, c("ink", 2))
  cv:each_ellipse(px0 + CW / 2 - 0.5, base - 13, CW / 2, 4, function(xx, yy) if yy <= base - 13 then cv:set(xx, yy, c("ink", 2)) end end)
  cv:rect(px0, base - 12, 2, 12, c("wood", 2)); cv:rect(px0 + CW - 2, base - 12, 2, 12, c("wood", 2))
  cv:rect(px0 + 1, top + 1, CW - 2, 3, c("gold", 3)); cv:rect(px0 + 2, top + 2, CW - 4, 1, c("lacquer", 2))
  walkway(cv, x, top - D, W, D)
  merlons_along(cv, x, top - D + 1, W)
  local foot = top - 6
  front(cv, x + 10, foot, W - 20, 12, { 6, 47 })
  roof(cv, x + 4, x + W - 5, foot - 10, foot - 22, foot - 28, 12)
  merlons_along(cv, x, top, W)
  save("town_gate_tower", cv, 5, 2)
end

-- Steps, 1 x 2 cells, rising from the ground at their front to the walkway a level up at their
-- back: eight treads, each 3 rows deep over a riser 2 rows high, between plain cheeks.
do
  local cv, x, base = frame(1, 2, LEVEL + 2)
  local W = CW
  for k = 0, 7 do
    local y0 = base - k * 5
    cv:rect(x, y0 - 2, W, 2, c("stone", 3))
    cv:rect(x, y0 - 5, W, 3, c("stone", 5))
    cv:hline(x, y0 - 5, W, c("stone", 6))
  end
  cv:vline(x, base - 40, 40, c("stone", 4)); cv:vline(x + W - 1, base - 40, 40, c("stone", 2))
  save("town_steps", cv, 1, 2)
end

-- Repaints a front's plaster in lacquer, as a temple's walls are.
local function lacquer(cv, x, y, w, h)
  for yy = y, y + h - 1 do
    for xx = x, x + w - 1 do
      local i = cv:get(xx, yy)
      if i == c("paper", 2) then cv:set(xx, yy, c("lacquer", 3)) elseif i == c("paper", 1) then cv:set(xx, yy, c("lacquer", 2)) end
    end
  end
end

-- The temple, 4 x 3 cells: lacquered walls on a high plinth with steps, under a skirt roof and a
-- second, upper roof, a gold plaque between them.
do
  local cv, x, base = frame(4, 3, 44)
  local W, D = 4 * CW, 3 * CD
  block(cv, x, base, W, D, 6, "stone", 3)
  for k = 0, 2 do cv:hline(x + 24 - k, base - 2 - k * 2, 16 + k * 2, c("stone", 5 - (k % 2))) end
  local foot = base - 6
  front(cv, x + 4, foot, W - 8, 14, { 6, 45 })
  lacquer(cv, x + 4, foot - 14, W - 8, 14)
  local eave = foot - 14 + 2
  roof(cv, x + 1, x + W - 2, eave, eave - 9, eave - 9, 3)
  local foot2 = eave - 8
  front(cv, x + 14, foot2, W - 28, 10, {}, false)
  lacquer(cv, x + 14, foot2 - 10, W - 28, 10)
  cv:rect(x + 26, foot2 - 8, 12, 5, c("gold", 3)); cv:rect(x + 27, foot2 - 7, 10, 3, c("ink", 2))
  local eave2 = foot2 - 10 + 2
  roof(cv, x + 6, x + W - 7, eave2, eave2 - 14, eave2 - 20, 8)
  save("town_temple", cv, 4, 3)
end

-- A market stall, 2 x 1 cells: a striped awning on four posts over a table of pots, greens and
-- bolts of cloth.
do
  local cv, x, base = frame(2, 1, 22)
  local W, D = 2 * CW, CD
  for _, p in ipairs({ { 1, 0 }, { W - 2, 0 }, { 1, -D }, { W - 2, -D } }) do
    cv:vline(x + p[1], base + p[2] - 16, 16, c("wood", 2))
  end
  block(cv, x + 3, base - 1, W - 6, 7, 5, "wood", 3)
  local table_top = base - 1 - 5 - 7
  for k = 0, 2 do cv:each_ellipse(x + 7 + k * 3, table_top + 4, 1.6, 1.2, function(xx, yy) cv:set(xx, yy, c("earth", 3)) end) end
  cv:rect(x + 16, table_top + 2, 4, 3, c("grass", 5)); cv:set(x + 17, table_top + 2, c("grass", 6))
  cv:rect(x + 21, table_top + 1, 3, 4, c("azure", 4)); cv:rect(x + 24, table_top + 2, 3, 3, c("amber", 4))
  -- The awning, sloping from the back posts' tops down to the front's, in stripes.
  for yy = base - D - 18, base - 15 do
    for xx = x - 1, x + W do
      local stripe = ((xx - x) // 4) % 2 == 0
      cv:set(xx, yy, stripe and c("paper", 3) or c("crimson", 3))
    end
  end
  cv:hline(x - 1, base - 15, W + 2, c("ink", 3))
  save("town_market_stall", cv, 2, 1)
end

-- A courtyard house, 5 x 5 cells: a grey wall round a paved court, the main hall across its
-- back, a wing down each side, and a gate in the middle of its front wall under a small roof.
do
  local cv, x, base = frame(5, 5, 30)
  local W, D = 5 * CW, 5 * CD
  local back = base - D
  -- The court, paved from wall to wall.
  for yy = back + 1, base - 9 do
    for xx = x + 2, x + W - 3 do
      local joint = (yy - back) % 6 == 5 or (xx - x) % 8 == 0
      cv:set(xx, yy, joint and c("stone", 4) or c("stone", 5))
    end
  end
  cv:each_ellipse(x + 24, base - 18, 4, 3, function(xx, yy, dd) cv:set(xx, yy, dd > 0.7 and c("leaf", 2) or c("leaf", 4)) end)
  -- The side wings: long roofs seen from above, their ridges running back to front, their gable
  -- fronts facing the court's front.
  for _, wx in ipairs({ x + 2, x + W - 18 }) do
    for yy = back + 22, base - 20 do
      for xx = wx, wx + 15 do
        local col = xx < wx + 8 and c("stone", 4) or c("stone", 3)
        if (yy - back) % 3 == 0 then col = px.shift(col, -1) end
        cv:set(xx, yy, col)
      end
    end
    cv:vline(wx + 7, back + 22, base - 20 - back - 22, c("stone", 2))
    front(cv, wx + 1, base - 9, 14, 10, {}, true)
  end
  -- The main hall across the back.
  building(cv, x + 8, back + 24, 4, 2, 11, 9, { 6, 49 }, { hip = 8 })
  -- The wall round it: its cap down the sides and along the back, its face along the front with
  -- the gate.
  cv:rect(x, back - 2, W, 3, c("stone", 2)); cv:hline(x, back - 2, W, c("stone", 4))
  cv:rect(x, back, 2, D - 8, c("stone", 4)); cv:rect(x + W - 2, back, 2, D - 8, c("stone", 2))
  courses(cv, x, base, W, 9)
  cv:hline(x, base - 10, W, c("stone", 4))
  cv:rect(x + 34, base - 9, 12, 9, c("wood", 2)); cv:vline(x + 40, base - 9, 9, c("wood", 1))
  cv:set(x + 37, base - 5, c("gold", 3)); cv:set(x + 42, base - 5, c("gold", 3))
  roof(cv, x + 31, x + 48, base - 10, base - 15, base - 17, 3)
  save("town_courtyard_house", cv, 5, 5)
end
