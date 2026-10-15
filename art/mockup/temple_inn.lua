-- The mockup of #364's buildings, for the PM's approval before the build: a temple and an inn for
-- the base pack at the 2.5D angle (ADR 0059) on ADR 0069's ramps with sel-out, in the style of
-- art/structures/. Both take ADR 0076's square role: the temple by name, the inn as the nearest case,
-- facing the square. It draws, from the production palette (art/pixel.lua):
--   temple_inn/temple.aseprite, 5 x 4 cells: a main hall of 4 x 3 under a hip-and-gable roof with
--     upswept eaves and painted eave boards, on a raised platform with steps, its doors open on a
--     gilt figure; before it, on a front court, a bronze incense burner smoking, a bell hung in a
--     frame at its left and a stone lantern at its right;
--   temple_inn/inn.aseprite, 5 x 3 cells: two storeys, the upper with a railed gallery, a signboard
--     over the door bearing an ink mark, a cloth banner on a pole, two paper lanterns at the lower
--     eave with a flame in each, and two tables with benches on the square side;
--   each with four frames tagged intact, damaged, destroyed and rubble, as the core's structure
--   stages, from the same damage helpers art/structures uses, the footprint's bottom-left corner at
--   (8, height - 8);
--   temple_inn/lights.json, the night lights in world_art/lights.json's form, [x, y, w, h, reach]
--     from the frame's top-left: lit windows as rects, the inn's lanterns and the temple's burner
--     as points, which count as hearths;
--   temple_inn/temple_inn_sheet.aseprite, the contact sheet: both buildings in every stage beside
--     the village hall, the light rects marked, and a village square at play zoom, 3x, by day with
--     both buildings, the hall, a monk and an innkeeper from #170's approved trade looks
--     (temple_inn/trades/, copied from art/trades-mockup at 746d0157);
--   temple_inn/square.json and temple_inn/square_*.png, the square's layout and layers, which a
--     throwaway harness on the evidence branch draws under the game's grade for the night capture;
--   temple_inn/temple_inn_frames.aseprite, every frame, for the 6x export.
-- Run: art/mockup/temple_inn.sh [path to aseprite]

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)
local c = px.c

local CW, CD = 16, 12

-- The helpers below, from frame() to rubble(), are copied from art/structures/structures.lua at
-- main 64a61d52, where they are local to that script, so these buildings stand in its style.
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
    cv:set(s[1] + s[2] * 2, ridge - 3, c("gold", 4))
  end
  -- Round tile ends along the front eave, its corners turned up.
  for xx = x0 - 2, x1 + 2 do cv:set(xx, eave, ((xx - x0) % 3 == 1) and c(tone, 6) or c(tone, 1)) end
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
  cv:rect(x, top, w, h, c("paper", 4))
  cv:hline(x, base - 2, w, c("paper", 3))
  cv:hline(x, top, w, c("paper", 3))
  local pillars = { x, x + w - 2 }
  for _, p in ipairs(pillars) do cv:vline(p, top, h, c("lacquer", 5)); cv:vline(p + 1, top, h, c("lacquer", 4)) end
  if door ~= false then
    local dx = x + w // 2 - 4
    cv:rect(dx, top + 3, 8, h - 3, c("wood", 2))
    cv:vline(dx + 4, top + 3, h - 3, c("wood", 1))
    cv:hline(dx - 1, top + 2, 10, c("wood", 3))
    cv:set(dx + 2, top + 3 + (h - 3) // 2, c("gold", 4)); cv:set(dx + 5, top + 3 + (h - 3) // 2, c("gold", 4))
    cv:vline(dx - 2, top, h, c("lacquer", 5)); cv:vline(dx + 9, top, h, c("lacquer", 4))
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
      if i ~= 0 then cv:set(x, y, y == keep and c("ink", 4) or i) end
    end
  end
  -- The open inside, charred, behind the stumps.
  for y = base - 3 - D, base - 6 do
    for x = 10, 8 + W - 3 do
      local n = px.noise(x, y, 4, seed)
      if cv:get(x, y) == 0 and n > 0.3 then
        cv:set(x, y, n > 0.7 and c("stone", 2) or (n > 0.5 and c("ink", 4) or c("wood", 1)))
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
  for _ = 1, 10 + w * 3 do cv:set(r.int(6, cv.w - 6), r.int(base - 2, base + 5), r.pick({ c("ink", 2), c("ink", 4), c("ink", 5) })) end
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


-- Lights ------------------------------------------------------------------------------------------

-- A building's lights as it is drawn: rects of lit windows and points, each [x, y, w, h, reach] in
-- the frame's pixels from its top-left.
local function lights_of()
  local l = { list = {} }
  function l.rect(x, y, w, h, reach) l.list[#l.list + 1] = { x, y, w, h, reach } end
  function l.point(x, y, reach) l.list[#l.list + 1] = { x, y, 0, 0, reach } end
  return l
end

-- A latticed window, as window() draws it, recorded as a lit rect of `reach`.
local function lit_window(cv, l, x, y, w, h, reach)
  window(cv, x, y, w, h)
  l.rect(x, y, w, h, reach)
end

-- Of `lights`, those a damaged frame still shows: a rect while half its panes remain, a point while
-- the pixel at it is unchanged.
local function surviving(lights, intact, frame_)
  local kept = {}
  for _, light in ipairs(lights) do
    local x, y, w, h = light[1], light[2], light[3], light[4]
    local same, total = 0, 0
    for yy = y, y + math.max(1, h) - 1 do
      for xx = x, x + math.max(1, w) - 1 do
        total = total + 1
        if frame_:get(xx, yy) == intact:get(xx, yy) and intact:get(xx, yy) ~= 0 then same = same + 1 end
      end
    end
    if same * 2 >= total then kept[#kept + 1] = light end
  end
  return kept
end

-- The temple ---------------------------------------------------------------------------------------

-- Painted eave boards under a roof's front eave, from x0 to x1: leaf and paper in alternating
-- panels, lacquer at their joins.
local function eave_boards(cv, x0, x1, y)
  for x = x0, x1 do
    local panel = ((x - x0) // 4) % 2
    cv:set(x, y, panel == 0 and c("leaf", 5) or c("paper", 6))
    cv:set(x, y + 1, panel == 0 and c("leaf", 4) or c("paper", 5))
    if (x - x0) % 4 == 0 then cv:set(x, y + 1, c("lacquer", 4)) end
  end
end

-- Smoke rising from (x, y): grey puffs lit from the upper left, thinning and drifting with height.
local function smoke(cv, x, y)
  for k = 0, 9 do
    local px_ = x + (k // 3) % 2 - k // 5
    local py = y - k
    if px.bayer(px_, py) < 0.85 - k * 0.07 then cv:set(px_, py, k < 4 and c("stone", 6) or c("stone", 5)) end
  end
end

local temple_lights = lights_of()
local function temple()
  local cv, x, base = frame(5, 4, 52)
  local l = temple_lights
  local W, D = 80, 48
  -- The platform over the whole footprint, the court on its front row, steps at its middle.
  block(cv, x, base, W, D, 4, "stone", 3)
  for k = 0, 2 do cv:hline(x + 32 - k, base - 1 - k, 16 + 2 * k, c("stone", 5 - (k % 2))) end
  -- The main hall, 4 x 3, at the back of the platform.
  local hx, hfoot = x + 8, base - 4 - 12
  block(cv, hx, hfoot, 64, 36, 3, "stone", 4)
  local foot = hfoot - 3
  front(cv, hx + 3, foot, 58, 15, {}, false)
  local top = foot - 15
  -- Its walls lacquered, as a temple's woodwork is, not plastered as the hall's and the houses'.
  for yy = top, foot - 1 do
    for xx = hx + 3, hx + 60 do
      local i = cv:get(xx, yy)
      if i == c("paper", 4) then cv:set(xx, yy, c("lacquer", 4)) elseif i == c("paper", 3) then cv:set(xx, yy, c("lacquer", 3)) end
    end
  end
  lit_window(cv, l, hx + 7, top + 4, 7, 7, 20)
  lit_window(cv, l, hx + 44, top + 4, 7, 7, 20)
  -- Its doors open on the dim inside and a bronze figure seated in it.
  local dx = hx + 25
  cv:rect(dx, top + 3, 12, 12, c("ink", 1))
  cv:vline(dx - 1, top + 3, 12, c("wood", 3)); cv:vline(dx + 12, top + 3, 12, c("wood", 3))
  cv:rect(dx + 4, top + 8, 4, 5, c("earth", 4)); cv:rect(dx + 5, top + 6, 2, 2, c("earth", 5)); cv:set(dx + 5, top + 6, c("earth", 6))
  cv:hline(dx + 3, top + 13, 6, c("earth", 3))
  l.rect(dx, top + 3, 12, 12, 20)
  local eave = foot - 15 + 2
  local back = foot - 36 - 15 + 3
  local ridge = foot - 18 - 15 - 12
  roof(cv, hx + 1, hx + 62, eave, ridge, back, 9)
  for xx = hx + 3, hx + 60 do cv:shade_at(xx, eave + 1, -1) end
  eave_boards(cv, hx + 1, hx + 62, eave + 1)
  -- A row of brackets under the boards, and a jewel finial at the ridge's middle.
  for xx = hx + 4, hx + 59, 4 do cv:set(xx, eave + 3, c("leaf", 5)); cv:set(xx + 1, eave + 3, c("lacquer", 5)) end
  local mid = hx + 32
  cv:rect(mid - 1, ridge - 4, 3, 3, c("earth", 4)); cv:set(mid, ridge - 5, c("earth", 5)); cv:set(mid, ridge - 6, c("earth", 6))
  cv:set(mid - 1, ridge - 4, c("earth", 5))
  -- Eaves swept up further at the corners.
  for _, s in ipairs({ { hx - 3, -1 }, { hx + 64, 1 } }) do
    cv:set(s[1] + s[2] * 2, eave - 3, c("stone", 2)); cv:set(s[1] + s[2] * 3, eave - 4, c("stone", 2))
    cv:set(s[1] + s[2] * 3, eave - 5, c("stone", 5))
  end
  -- The bell frame at the court's left: lacquered posts, a capped beam, an iron bell.
  local bx, by = x + 3, base - 4
  cv:vline(bx, by - 16, 16, c("lacquer", 4)); cv:vline(bx + 9, by - 16, 16, c("lacquer", 3))
  cv:hline(bx - 1, by - 17, 12, c("wood", 3)); cv:hline(bx - 2, by - 18, 14, c("stone", 3)); cv:hline(bx - 1, by - 19, 12, c("stone", 4))
  cv:rect(bx + 3, by - 14, 4, 6, c("stone", 3)); cv:hline(bx + 2, by - 9, 6, c("stone", 2)); cv:vline(bx + 3, by - 14, 5, c("stone", 4))
  cv:set(bx + 4, by - 15, c("stone", 2))
  -- The incense burner before the steps: a bronze pot on three legs under a lid, embers in it.
  local ix, iy = x + 37, base - 6
  cv:rect(ix, iy - 5, 6, 4, c("earth", 3)); cv:hline(ix, iy - 5, 6, c("earth", 4)); cv:hline(ix - 1, iy - 6, 8, c("earth", 4))
  cv:set(ix, iy - 1, c("earth", 2)); cv:set(ix + 5, iy - 1, c("earth", 2)); cv:set(ix + 2, iy - 1, c("earth", 2))
  cv:hline(ix + 1, iy - 7, 4, c("earth", 5)); cv:set(ix + 2, iy - 7, c("fire", 3)); cv:set(ix + 3, iy - 7, c("fire", 4))
  smoke(cv, ix + 3, iy - 8)
  l.point(ix + 3, iy - 7, 32)
  -- The stone lantern at the court's right, its opening lit at night.
  local lx, ly = x + 68, base - 4
  cv:rect(lx + 2, ly - 6, 3, 6, c("stone", 4)); cv:vline(lx + 4, ly - 6, 6, c("stone", 3))
  cv:rect(lx + 1, ly - 10, 5, 4, c("stone", 4)); cv:rect(lx + 2, ly - 9, 3, 2, c("wood", 1))
  l.rect(lx + 2, ly - 9, 3, 2, 24)
  cv:hline(lx, ly - 11, 7, c("stone", 5)); cv:hline(lx + 1, ly - 12, 5, c("stone", 4)); cv:set(lx + 3, ly - 13, c("stone", 5))
  return cv
end

-- The inn ------------------------------------------------------------------------------------------

-- A paper lantern hung at (x, y): a wood cap, a paper body round a flame, a tassel.
local function lantern(cv, l, x, y)
  cv:hline(x, y, 3, c("wood", 2)); cv:set(x + 1, y - 1, c("wood", 3))
  cv:rect(x - 1, y + 1, 5, 4, c("paper", 6)); cv:vline(x + 3, y + 1, 4, c("paper", 5)); cv:hline(x - 1, y + 4, 5, c("paper", 5))
  cv:set(x + 1, y + 2, c("fire", 4)); cv:set(x + 1, y + 3, c("fire", 3))
  cv:hline(x, y + 5, 3, c("wood", 2)); cv:set(x + 1, y + 6, c("wood", 3))
  l.point(x + 1, y + 2, 44)
end

-- A table with a bench either side, its top at (x, y).
local function table_(cv, x, y)
  cv:hline(x - 3, y + 3, 3, c("wood", 4)); cv:hline(x + 9, y + 3, 3, c("wood", 4))
  cv:set(x - 3, y + 4, c("wood", 2)); cv:set(x - 1, y + 4, c("wood", 2)); cv:set(x + 9, y + 4, c("wood", 2)); cv:set(x + 11, y + 4, c("wood", 2))
  cv:rect(x, y, 8, 3, c("wood", 5)); cv:hline(x, y, 8, c("wood", 6)); cv:hline(x, y + 2, 8, c("wood", 4))
  cv:vline(x + 1, y + 3, 3, c("wood", 3)); cv:vline(x + 6, y + 3, 3, c("wood", 3))
  cv:set(x + 2, y - 1, c("stone", 6)); cv:set(x + 5, y - 1, c("earth", 5))
end

local inn_lights = lights_of()
local function inn()
  local cv, x, base = frame(5, 3, 60)
  local l = inn_lights
  local W = 80
  -- The building over the back two rows, the terrace with its tables on the front row.
  local bfoot = base - 12
  block(cv, x, base, W, 12, 2, "earth", 4)
  block(cv, x, bfoot, W, 24, 3, "stone", 3)
  local foot = bfoot - 3
  front(cv, x + 3, foot, W - 6, 14, {}, true)
  local top = foot - 14
  lit_window(cv, l, x + 7, top + 4, 9, 6, 28)
  lit_window(cv, l, x + 60, top + 4, 9, 6, 28)
  -- The lower roof, a skirt round the upper storey: its front slope only.
  local eave = foot - 14 + 2
  roof(cv, x + 1, x + W - 2, eave, eave - 8, eave - 8, 3)
  -- The upper storey, its gallery railed.
  local foot2 = eave - 7
  front(cv, x + 9, foot2, W - 18, 12, {}, false)
  local top2 = foot2 - 12
  for _, wx in ipairs({ 5, 27, 49 }) do lit_window(cv, l, x + 9 + wx, top2 + 3, 8, 5, 28) end
  cv:hline(x + 8, foot2 - 4, W - 16, c("wood", 4)); cv:hline(x + 8, foot2, W - 16, c("wood", 3))
  for xx = x + 8, x + W - 9, 3 do cv:vline(xx, foot2 - 3, 3, c("wood", 3)) end
  local eave2 = foot2 - 12 + 2
  roof(cv, x + 5, x + W - 6, eave2, eave2 - 13, eave2 - 19, 8)
  -- The signboard over the door: a dark board, its paper face bearing the inn's mark in ink.
  local sx, sy = x + 33, top - 1
  cv:rect(sx, sy, 14, 6, c("wood", 2)); cv:rect(sx + 1, sy + 1, 12, 4, c("paper", 6))
  cv:hline(sx + 3, sy + 2, 3, c("ink", 2)); cv:vline(sx + 4, sy + 2, 3, c("ink", 2)); cv:hline(sx + 8, sy + 2, 3, c("ink", 2))
  cv:set(sx + 9, sy + 3, c("ink", 2)); cv:hline(sx + 8, sy + 4, 3, c("ink", 2))
  -- The lanterns at the lower eave's corners.
  lantern(cv, l, x + 4, eave + 1)
  lantern(cv, l, x + W - 7, eave + 1)
  -- The banner on its pole at the terrace's right: a long cloth with the mark, its foot ragged.
  local px0, pb = x + W - 3, base - 2
  cv:vline(px0, pb - 40, 40, c("wood", 4)); cv:hline(px0 - 7, pb - 38, 8, c("wood", 3))
  cv:rect(px0 - 7, pb - 37, 6, 16, c("cloth", 6)); cv:vline(px0 - 2, pb - 37, 16, c("cloth", 5))
  for k = 0, 5, 2 do cv:set(px0 - 7 + k, pb - 21, c("cloth", 6)) end
  cv:hline(px0 - 6, pb - 34, 3, c("ink", 2)); cv:vline(px0 - 5, pb - 33, 4, c("ink", 2)); cv:hline(px0 - 6, pb - 28, 3, c("ink", 2))
  -- Two tables with benches on the terrace.
  table_(cv, x + 12, base - 9)
  table_(cv, x + 48, base - 9)
  return cv
end

-- Stages and sheets ----------------------------------------------------------------------------------

local function stages(name, intact, w, d, l)
  local seed = #name * 97 + w * 13 + d
  local frames = { intact, damaged(intact, w, d, seed), destroyed(intact, w, d, seed), rubble(intact, w, d, seed) }
  local lit = { intact = l.list, damaged = surviving(l.list, intact, frames[2]) }
  for _, f in ipairs(frames) do f:selout(0, 0, f.w, f.h) end
  px.save_frames(intact.w, intact.h, frames,
    { { "intact", 1, 1, 1000 }, { "damaged", 2, 2, 1000 }, { "destroyed", 3, 3, 1000 }, { "rubble", 4, 4, 1000 } },
    here .. "/temple_inn/" .. name .. ".aseprite")
  return frames, lit
end
local temple_frames, temple_lit = stages("temple", temple(), 5, 4, temple_lights)
local inn_frames, inn_lit = stages("inn", inn(), 5, 3, inn_lights)
local hall_frames = px.load_frames(here .. "/../structures/hall.aseprite")

-- The night lights, in world_art/lights.json's form, for the harness's temporary copy.
local function json_list(list)
  local out = {}
  for _, l in ipairs(list) do out[#out + 1] = string.format("[%d, %d, %d, %d, %d]", l[1], l[2], l[3], l[4], l[5]) end
  return "[" .. table.concat(out, ", ") .. "]"
end
do
  local f = assert(io.open(here .. "/temple_inn/lights.json", "w"))
  f:write("{\n  \"temple\": {\n    \"intact\": " .. json_list(temple_lit.intact) .. ",\n    \"damaged\": " ..
    json_list(temple_lit.damaged) .. "\n  },\n  \"inn\": {\n    \"intact\": " .. json_list(inn_lit.intact) ..
    ",\n    \"damaged\": " .. json_list(inn_lit.damaged) .. "\n  }\n}\n")
  f:close()
end

-- The contact sheet ------------------------------------------------------------------------------------

local ZOOM, QW, QH = 3, 300, 180
local SW = math.max(8 + 4 * 104 + 120, QW * ZOOM + 16)
local SH = 30 + 2 * 130 + 120 + QH * ZOOM + 24
local bg = px.canvas(SW, SH)
local ground = px.canvas(SW, SH)
local things = px.canvas(SW, SH)
local marks = px.canvas(SW, SH)
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
-- Packed earth laid with large flags, as the approved trade looks' square lies.
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
-- The cast shadow past a standing structure's right and bottom edges, as the view draws it.
local function cast_shadow(cv, fx, fy, w, d, width)
  width = width or 6
  local W, D = w * CW, d * CD
  for yy = fy - D, fy + width - 1 do
    for xx = fx, fx + W + width - 1 do
      local inside = xx < fx + W and yy < fy
      if not inside then
        local dist = math.max(xx - (fx + W) + 1, yy - fy + 1) / width
        if px.bayer(xx, yy) < 0.8 - 0.7 * dist then cv:set(xx, yy, c("ink", 1)) end
      end
    end
  end
end

labels:text(8, 5, "MURIM24  TEMPLE AND INN MOCKUP (#364)", c("paper", 6))
labels:text(8, 12, "NATIVE PIXELS. THE STRUCTURE STYLE OF ART/STRUCTURES, ADR 0069 RAMPS, SEL-OUT. BOTH TAKE ADR 0076'S SQUARE ROLE.", c("stone", 5))
labels:text(8, 19, "TEMPLE 5 X 4 CELLS, INN 5 X 3. LIT RECTS MARKED IN THE LAST COLUMN; DOTS ARE POINT LIGHTS (HEARTHS).", c("stone", 5))

local function row(y, name, frames_, lit, w, d)
  labels:text(8, y, name, c("paper", 6))
  grass(ground, 8, y + 7, SW - 16, frames_[1].h + 6)
  local x = 10
  for n, tag in ipairs({ "INTACT", "DAMAGED", "DESTROYED", "RUBBLE" }) do
    things:stamp(frames_[n], x, y + 10)
    labels:text(x, y + frames_[1].h + 15, tag, c("stone", 5))
    x = x + frames_[1].w + 8
  end
  if lit then
    things:stamp(frames_[1], x, y + 10)
    for _, l in ipairs(lit.intact) do
      if l[3] == 0 then
        marks:set(x + l[1], y + 10 + l[2], c("jade", 5)); marks:set(x + l[1] - 1, y + 10 + l[2], c("jade", 5))
        marks:set(x + l[1] + 1, y + 10 + l[2], c("jade", 5))
      else
        for xx = x + l[1] - 1, x + l[1] + l[3] do marks:set(xx, y + 9 + l[2], c("jade", 5)); marks:set(xx, y + 10 + l[2] + l[4], c("jade", 5)) end
        for yy = y + 9 + l[2], y + 10 + l[2] + l[4] do marks:set(x + l[1] - 1, yy, c("jade", 5)); marks:set(x + l[1] + l[3], yy, c("jade", 5)) end
      end
    end
    labels:text(x, y + frames_[1].h + 15, "LIGHTS", c("stone", 5))
  end
end
local y = 30
row(y, "TEMPLE  A MAIN HALL ON A PLATFORM, PAINTED EAVES, INCENSE BURNER, BELL, STONE LANTERN", temple_frames, temple_lit, 5, 4)
y = y + temple_frames[1].h + 26
row(y, "INN  TWO STOREYS, GALLERY, SIGNBOARD, BANNER, PAPER LANTERNS, TABLES OUTSIDE", inn_frames, inn_lit, 5, 3)
y = y + inn_frames[1].h + 26
row(y, "VILLAGE HALL  (ART/STRUCTURES, FOR COMPARISON)", hall_frames, nil, 5, 3)
y = y + hall_frames[1].h + 26

-- The village square: the hall on its far side, the temple on its left, the inn on its right, a
-- monk praying before the temple and the innkeeper pouring at its tables.
local sq = { ground = px.canvas(QW, QH), shadows = px.canvas(QW, QH), things = px.canvas(QW, QH) }
grass(sq.ground, 0, 0, QW, QH)
local SQ = { x = 102, y = 88, w = 96, h = 48 }
paving(sq.ground, SQ.x - 6, SQ.y - 4, SQ.w + 12, SQ.h + 10)
paving(sq.ground, 140, SQ.y + SQ.h, 20, QH - SQ.y - SQ.h)
local placed = {}
-- A structure's frame with its footprint's bottom-left corner at (fx, fy).
local function put(name, frames_, w, d, fx, fy, lit)
  cast_shadow(sq.shadows, fx, fy, w, d)
  local ox, oy = fx - 8, fy - (frames_[1].h - 8)
  sq.things:stamp(frames_[1], ox, oy)
  placed[#placed + 1] = { name = name, x = ox, y = oy, lights = lit and lit.intact or {} }
end
local function person(cv, x, fy, flip)
  sq.shadows:each_ellipse(x, fy, 6, 2.5, function(sx, sy, d)
    if px.bayer(sx, sy) < 0.8 - 0.7 * d then sq.shadows:set(sx, sy, c("ink", 1)) end
  end)
  if flip then
    local out = px.canvas(cv.w, cv.h)
    for k, i in pairs(cv.p) do out:set(cv.w - 1 - k % cv.w, k // cv.w, i) end
    cv = out
  end
  sq.things:stamp(cv, x - 8, fy - 22)
end
local monk_frames, monk_tags = px.load_frames(here .. "/temple_inn/trades/monk.aseprite")
local inn_frames_p, inn_tags = px.load_frames(here .. "/temple_inn/trades/innkeeper.aseprite")
-- Back to front by the row each footprint ends on.
put("hall", hall_frames, 5, 3, SQ.x + 8, SQ.y)
put("temple", temple_frames, 5, 4, SQ.x - 84, SQ.y + 40, temple_lit)
put("inn", inn_frames, 5, 3, SQ.x + SQ.w + 4, SQ.y + 40, inn_lit)
person(monk_frames[monk_tags.orthodox_work[1]], SQ.x - 44, SQ.y + 56)
person(inn_frames_p[inn_tags.unorthodox_work[2]], SQ.x + SQ.w + 20, SQ.y + 56, true)
person(monk_frames[monk_tags.demonic_walk[1]], SQ.x + 30, SQ.y + 30)
labels:text(8, y, "PLAY ZOOM 3X  A VILLAGE SQUARE BY DAY: TEMPLE, HALL, INN, A MONK PRAYING AND AN INNKEEPER POURING", c("paper", 5))
labels:text(8, y + 7, "THE NIGHT IS CAPTURED FROM THE GAME'S GRADE BY A HARNESS ON THE EVIDENCE BRANCH (SEE THE REPORT).", c("stone", 5))
for _, layer in ipairs({ sq.ground, sq.shadows, sq.things }) do
  for k, i in pairs(layer.p) do
    local x, yy = k % QW, k // QW
    ground:rect(8 + x * ZOOM, y + 16 + yy * ZOOM, ZOOM, ZOOM, i)
  end
end

px.save(SW, SH, { { "bg", bg }, { "ground", ground }, { "things", things }, { "marks", marks }, { "labels", labels } },
  here .. "/temple_inn/temple_inn_sheet.aseprite")

-- The square's layers and layout, for the night harness.
do
  local function flat(layers)
    local cv = px.canvas(QW, QH)
    for _, layer in ipairs(layers) do cv:stamp(layer, 0, 0) end
    return cv
  end
  px.save(QW, QH, { { "ground", flat({ sq.ground, sq.shadows }) } }, here .. "/temple_inn/square_ground.aseprite")
  px.save(QW, QH, { { "things", sq.things } }, here .. "/temple_inn/square_things.aseprite")
  local f = assert(io.open(here .. "/temple_inn/square.json", "w"))
  local parts = {}
  for _, p in ipairs(placed) do
    parts[#parts + 1] = string.format("    {\"name\": \"%s\", \"x\": %d, \"y\": %d, \"lights\": %s}", p.name, p.x, p.y, json_list(p.lights))
  end
  f:write("{\n  \"size\": [" .. QW .. ", " .. QH .. "],\n  \"structures\": [\n" .. table.concat(parts, ",\n") .. "\n  ]\n}\n")
  f:close()
end

-- Every frame on grass, for the 6x export.
do
  local FW = 8 + 4 * (temple_frames[1].w + 6)
  local FH = 4 + temple_frames[1].h + inn_frames[1].h + 8
  local fb = px.canvas(FW, FH)
  grass(fb, 0, 0, FW, FH)
  local fy = 2
  for _, set in ipairs({ temple_frames, inn_frames }) do
    local x = 4
    for _, f in ipairs(set) do fb:stamp(f, x, fy); x = x + f.w + 6 end
    fy = fy + set[1].h + 4
  end
  px.save(FW, FH, { { "frames", fb } }, here .. "/temple_inn/temple_inn_frames.aseprite")
end
