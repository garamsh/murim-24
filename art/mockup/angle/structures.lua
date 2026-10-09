-- Draws the village's and the seat's structures at the 2.5D angle of #200: the ground seen from
-- above at 12 pixels a cell deep and 16 wide, fronts and roofs standing up from it, light from
-- the upper left. One intact frame each, tagged intact. Every frame holds its footprint's
-- bottom-left corner at (8, height - 8), with what rises above the footprint in the rows above.
-- The cast shadow is not in the frame; the mockup darkens the ground under it.
-- Run: aseprite --batch --script art/mockup/angle/structures.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../../pixel.lua")
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

local function save(name, cv)
  cv:outline(0, 0, cv.w, cv.h, INK)
  px.save_frames(cv.w, cv.h, { cv }, { { "intact", 1, 1, 1000 } }, here .. "/" .. name .. ".aseprite")
end

-- The village house, 3 x 3 cells.
do
  local cv, x, base = frame(3, 3, 40)
  building(cv, x, base, 3, 3, 14, 12, { 5, 36 }, { hip = 10 })
  save("house", cv)
end

-- The village hall, 5 x 3 cells: a longer front with four windows and a taller roof.
do
  local cv, x, base = frame(5, 3, 46)
  building(cv, x, base, 5, 3, 16, 15, { 6, 18, 55, 67 }, { hip = 14 })
  save("hall", cv)
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
  save("storehouse", cv)
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
  save("manual_hall", cv)
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
  save("granary", cv)
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
  save("well", cv)
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
  save("seat_gate", cv)
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
  save("barrels", cv)
end

-- A fence, 5 x 1 cells: posts every 8 pixels and two rails.
do
  local cv, x, base = frame(5, 1, 6)
  for xx = x + 1, x + 78, 8 do cv:rect(xx, base - 12, 2, 12, c("wood", 3)); cv:set(xx, base - 12, c("wood", 4)) end
  cv:hline(x, base - 10, 80, c("wood", 4)); cv:hline(x, base - 9, 80, c("wood", 2))
  cv:hline(x, base - 5, 80, c("wood", 4)); cv:hline(x, base - 4, 80, c("wood", 2))
  save("fence", cv)
end

-- The training ground, 6 x 4 cells, laid with the ground: raked sand in a stone curb, a ring
-- trodden where pupils spar, and weapon racks and dummies along its back row.
do
  local cv, x, base = frame(6, 4, 18)
  local W, D = 96, 48
  block(cv, x, base, W, D, 2, "stone", 3)
  for yy = base - 2 - D + 2, base - 4 do
    for xx = x + 2, x + W - 3 do
      local col = ((yy + (xx // 9)) % 3 == 0) and c("earth", 5) or c("earth", 6)
      if px.noise(xx, yy, 3, 7) > 0.7 then col = c("earth", 5) end
      cv:set(xx, yy, col)
    end
  end
  cv:each_ellipse(x + W / 2, base - 24, 26, 13, function(xx, yy, d)
    if d > 0.78 then cv:set(xx, yy, c("earth", 4)) end
  end)
  local back = base - 2 - D + 8
  for _, rx in ipairs({ x + 8, x + 62 }) do
    cv:rect(rx, back - 14, 1, 14, c("wood", 2)); cv:rect(rx + 22, back - 14, 1, 14, c("wood", 2))
    cv:hline(rx, back - 12, 23, c("wood", 3)); cv:hline(rx, back - 6, 23, c("wood", 3))
    for k = 0, 4 do
      local wx = rx + 3 + k * 4
      cv:vline(wx, back - 17, 16, c("wood", 4)); cv:set(wx, back - 18, c("stone", 6))
    end
  end
  for k = 0, 2 do
    local dx = x + 38 + k * 9
    cv:rect(dx, back - 9, 2, 9, c("wood", 3)); cv:rect(dx - 2, back - 12, 6, 4, c("wood", 4))
    cv:rect(dx - 1, back - 15, 4, 3, c("wood", 4))
  end
  save("training_ground", cv)
end
