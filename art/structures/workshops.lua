-- Draws the workshops a growing village builds, one frame per damage stage, tagged intact,
-- damaged, destroyed and rubble. Each frame holds its footprint at (8, 8), as the house's does.
-- The cast shadow is not in the frame.
--   smithy.aseprite: a forge shed over 3x2 cells, open at the front on a glowing hearth, its
--     chimney rising through a slate roof, an anvil, a quench tub and charcoal on the apron.
--     A 64x48 frame.
--   apothecary.aseprite: a shop over 4x2 cells, a cabinet of drawers behind its counter and herbs
--     hung under its eaves, a herb rack and a drying tray in the yard at its right. An 80x48 frame.
-- Run: aseprite --batch --script art/structures/workshops.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c
local INK = c("ink", 1)
local STAGES = { "intact", "damaged", "destroyed", "rubble" }

-- A low slate roof from `top` to the eave at `bottom`, `w` wide from x0, hipped above `ridge`.
local function roof(cv, x0, w, top, ridge, bottom)
  for yy = top, bottom do
    local inset = yy < ridge and (ridge - yy) or 0
    for xx = x0 - 1 + inset, x0 + w - inset do
      local ch = (xx - x0) % 3
      local col = ch == 0 and c("stone", 2) or (ch == 1 and c("stone", 4) or c("stone", 3))
      if yy < ridge then col = ch == 0 and c("stone", 3) or c("stone", 4) end
      if bottom - yy < 3 and ch ~= 1 then col = px.shift(col, -1) end
      cv:set(xx, yy, col)
    end
  end
  cv:hline(x0 + 1, ridge, w - 2, c("ink", 2)); cv:hline(x0 + 1, ridge - 1, w - 2, c("stone", 2))
  for xx = x0 - 3, x0 + w + 2 do cv:set(xx, bottom, ((xx - x0) % 3 == 1) and c("stone", 5) or c("stone", 1)) end
  cv:set(x0 - 4, bottom - 1, c("stone", 2)); cv:set(x0 + w + 3, bottom - 1, c("stone", 2))
end

-- A hole through a roof showing rafters, its rim ragged.
local function hole(cv, hx, hy, hw, hh, r)
  for yy = hy, hy + hh - 1 do
    for xx = hx, hx + hw - 1 do
      local edge = (xx == hx or xx == hx + hw - 1 or yy == hy or yy == hy + hh - 1)
      if cv:get(xx, yy) ~= 0 and (not edge or r.next() > 0.45) then
        cv:set(xx, yy, ((xx - hx) % 3 == 1) and c("wood", 3) or c("ink", 1))
      end
    end
  end
end

-- Darkens what is drawn in a box where noise passes `level`: scorch.
local function scorch(cv, x, y, w, h, level, seed, steps)
  for yy = y, y + h - 1 do
    for xx = x, x + w - 1 do
      if cv:get(xx, yy) ~= 0 and px.noise(xx, yy, 4, seed) > level then cv:shade_at(xx, yy, steps or -2) end
    end
  end
end

-- Broken beams and ash over what is drawn in a box, its top edge ragged: a fallen roof.
local function charred(cv, x, y, w, h, seed)
  for yy = y, y + h - 1 do
    for xx = x, x + w - 1 do
      if cv:get(xx, yy) ~= 0 and yy - y > px.noise(xx, yy, 3, seed) * 5 then
        local n = px.noise(xx, yy, 4, seed + 1)
        cv:set(xx, yy, n > 0.62 and c("ink", 3) or (n > 0.35 and c("ink", 2) or c("wood", 1)))
      elseif cv:get(xx, yy) ~= 0 then
        cv:clear(xx, yy)
      end
    end
  end
end

-- Breaks a wall to stumps of noise height, burnt black above them.
local function stumps(cv, x, y, w, h, seed)
  for xx = x, x + w - 1 do
    local stump = math.floor(px.noise(xx, 0, 3, seed) * h * 0.7) + 1
    for yy = y, y + h - 1 - stump do
      if cv:get(xx, yy) ~= 0 then cv:clear(xx, yy) end
    end
    cv:set(xx, y + h - stump, c("ink", 3))
  end
end

-- A low heap of tile, stone and burnt beams over an ellipse, with weeds coming back.
local function heap(cv, cx, cy, rx, ry, seed, r)
  cv:each_ellipse(cx, cy, rx, ry, function(xx, yy, d)
    if d < 0.8 + px.noise(xx, yy, 3, seed) * 0.35 then
      local n = px.noise(xx, yy, 2, seed + 1)
      local col = n > 0.68 and c("stone", 4) or (n > 0.45 and c("stone", 3) or (n > 0.25 and c("wood", 1) or c("ink", 3)))
      if yy - cy < -ry * 0.4 and n > 0.45 then col = px.shift(col, 1) end
      cv:set(xx, yy, col)
    end
  end)
  for k = 1, 3 do
    local bx, by = r.int(math.floor(cx - rx + 2), math.floor(cx + rx - 10)), r.int(math.floor(cy - ry / 2), math.floor(cy + ry / 2))
    cv:line(bx, by, bx + r.int(6, 10), by + r.int(-2, 2), c("wood", 1))
  end
  for k = 1, 4 do
    local gx, gy = r.int(math.floor(cx - rx), math.floor(cx + rx)), r.int(math.floor(cy), math.floor(cy + ry))
    cv:set(gx, gy, c("grass", 4)); cv:set(gx + 1, gy - 1, c("grass", 5))
  end
end

-- Litter of `n` pixels from `ramp` in a box.
local function litter(cv, r, x, y, w, h, n, ramp, lo, hi)
  for k = 1, n do cv:set(r.int(x, x + w - 1), r.int(y, y + h - 1), c(ramp, r.int(lo, hi))) end
end

-- Firelight on what is drawn round (cx, cy), dithered: darks warm to amber, the rest lifts a step.
local function glow(cv, cx, cy, rx, ry, strength)
  cv:each_ellipse(cx, cy, rx, ry, function(xx, yy, d)
    local i = cv:get(xx, yy)
    if i == 0 or px.bayer(xx, yy) >= (1 - d) * strength then return end
    local ramp = px.ramp_of[i][1]
    if ramp == "ink" then
      cv:set(xx, yy, c("amber", d < 0.45 and 2 or 1))
    elseif ramp == "earth" then
      cv:set(xx, yy, d < 0.4 and c("amber", 3) or px.shift(i, 1))
    elseif ramp ~= "fire" and ramp ~= "amber" then
      cv:shade_at(xx, yy, 1)
    end
  end)
end


-- Coals in the hearth's mouth: white-hot at the heart, red at the rim; `heat` 0 to 1.
local function coals(cv, x, y, w, h, heat, seed)
  for yy = y, y + h - 1 do
    for xx = x, x + w - 1 do
      local dx, dy = (xx - (x + (w - 1) / 2)) / (w / 2), (yy - (y + h - 1)) / h
      local d = math.sqrt(dx * dx + dy * dy)
      local t = (1 - d) * heat + (px.noise(xx, yy, 2, seed) - 0.5) * 0.3
      local col
      if t > 0.7 then col = c("fire", 5) elseif t > 0.5 then col = c("fire", 4)
      elseif t > 0.32 then col = c("fire", 3) elseif t > 0.16 then col = c("fire", 2)
      elseif t > 0.04 then col = px.bayer(xx, yy) < 0.5 and c("fire", 1) or c("ink", 2)
      else col = c("ink", 2) end
      cv:set(xx, yy, col)
    end
  end
end

-- An anvil on a wooden stump: a horned face lit from above, a waist and a foot; `tipped` lays
-- it on its side beside the stump.
local function anvil(cv, ax, base, tipped)
  if tipped then
    cv:rect(ax + 10, base - 4, 5, 4, c("wood", 3)); cv:hline(ax + 10, base - 4, 5, c("wood", 5))
    cv:rect(ax, base - 5, 3, 5, c("ink", 3)); cv:vline(ax, base - 5, 5, c("stone", 3))
    cv:rect(ax + 3, base - 4, 2, 3, c("ink", 3)); cv:rect(ax + 5, base - 5, 3, 5, c("ink", 4))
    cv:set(ax + 1, base - 6, c("ink", 3))
    return
  end
  cv:rect(ax + 3, base - 3, 6, 3, c("wood", 3)); cv:vline(ax + 3, base - 3, 3, c("wood", 4))
  cv:hline(ax + 3, base - 1, 6, c("wood", 2))
  cv:hline(ax + 2, base - 4, 8, c("ink", 3))
  cv:rect(ax + 4, base - 6, 4, 2, c("ink", 3)); cv:vline(ax + 4, base - 6, 2, c("ink", 4))
  cv:rect(ax + 1, base - 9, 10, 3, c("ink", 3))
  cv:hline(ax + 1, base - 9, 10, c("stone", 5)); cv:hline(ax + 1, base - 7, 10, c("ink", 2))
  cv:hline(ax - 1, base - 9, 2, c("stone", 3)); cv:set(ax - 2, base - 9, c("stone", 3))
  cv:set(ax - 1, base - 8, c("ink", 3))
end

local function smithy(cv, stage, seed)
  local r = px.rng(seed)
  local x, y, w = 8, 8, 48
  -- The stone plinth under the shed, in every stage.
  cv:rect(x + 1, y + 22, w - 2, 4, c("stone", 4))
  cv:hline(x + 1, y + 22, w - 2, c("stone", 5)); cv:hline(x + 1, y + 25, w - 2, c("stone", 2))

  local bay = { x + 3, x + 25 } -- the open front, from its left post to its middle one
  local hearth = { x + 5, y + 14 } -- its stone block's top-left; the chimney rises from it
  local chimney_x = x + 7
  local stack_top = y - 6

  -- The shed: dark inside the open bay, the hearth and bellows in it; a plank wall hung with
  -- tools in the other bay; posts between.
  local function shed()
    for yy = y + 11, y + 21 do
      for xx = bay[1], bay[2] do cv:set(xx, yy, yy < y + 15 and c("ink", 1) or c("ink", 2)) end
    end
    cv:rect(hearth[1], hearth[2], 13, 8, c("stone", 3))
    cv:hline(hearth[1], hearth[2], 13, c("stone", 4)); cv:vline(hearth[1], hearth[2], 8, c("stone", 4))
    cv:vline(hearth[1] + 12, hearth[2], 8, c("stone", 2))
    for k = 1, 3 do cv:hline(hearth[1] + 1, hearth[2] + k * 2, 11, c("stone", 2)) end
    -- The bellows: a wedge of leather on a frame by the hearth.
    cv:rect(x + 19, y + 17, 5, 4, c("earth", 2)); cv:hline(x + 19, y + 17, 5, c("wood", 4))
    cv:hline(x + 19, y + 20, 5, c("wood", 3)); cv:set(x + 18, y + 19, c("wood", 4))
    -- The plank wall and the tools on it: tongs, two hammers, a horseshoe.
    for xx = bay[2] + 2, x + w - 4 do
      for yy = y + 11, y + 21 do
        local ch = (xx - x) % 3
        cv:set(xx, yy, ch == 0 and c("wood", 2) or (ch == 1 and c("wood", 4) or c("wood", 3)))
      end
    end
    cv:hline(bay[2] + 2, y + 11, x + w - 5 - bay[2] - 1, c("wood", 2))
    cv:line(x + 30, y + 13, x + 32, y + 19, c("ink", 3)); cv:line(x + 32, y + 13, x + 30, y + 19, c("ink", 3))
    for _, hx in ipairs({ x + 35, x + 39 }) do
      cv:vline(hx + 1, y + 14, 6, c("wood", 5)); cv:rect(hx, y + 13, 3, 2, c("ink", 3)); cv:set(hx, y + 13, c("stone", 3))
    end
    cv:set(x + 42, y + 14, c("ink", 3)); cv:vline(x + 41, y + 15, 2, c("ink", 3)); cv:vline(x + 43, y + 15, 2, c("ink", 3))
    for _, p in ipairs({ x + 1, bay[2] + 1, x + w - 3 }) do
      cv:vline(p, y + 11, 11, c("wood", 4)); cv:vline(p + 1, y + 11, 11, c("wood", 2))
    end
  end

  -- The chimney: a stack of stone courses from the hearth up through the roof, its mouth dark.
  local function chimney(top)
    for yy = top, hearth[2] - 1 do
      for xx = chimney_x, chimney_x + 6 do
        local course = (yy - top) % 3 == 2
        local col = xx == chimney_x and c("stone", 5) or (xx == chimney_x + 6 and c("stone", 2) or c("stone", 4))
        if course or ((xx - chimney_x + ((yy - top) // 3) * 3) % 6 == 0) then col = c("stone", 2) end
        cv:set(xx, yy, col)
      end
    end
  end

  if stage == "intact" or stage == "damaged" then
    shed()
    coals(cv, hearth[1] + 3, hearth[2] + 1, 7, 3, stage == "intact" and 1 or 0.55, seed)
    roof(cv, x, w, y - 3, y + 1, y + 10)
    chimney(stack_top)
    cv:rect(chimney_x - 1, stack_top - 1, 9, 2, c("stone", 4)); cv:hline(chimney_x - 1, stack_top - 1, 9, c("stone", 5))
    cv:rect(chimney_x + 1, stack_top - 1, 5, 1, c("ink", 1))
  end
  if stage == "intact" then
    glow(cv, hearth[1] + 6, hearth[2] + 2, 13, 9, 0.9)
    glow(cv, hearth[1] + 6, y + 23, 11, 3, 0.8)
  elseif stage == "damaged" then
    hole(cv, x + 30, y - 1, 8, 5, r)
    hole(cv, x + 18, y + 4, 6, 4, r)
    -- The stack's crown knocked off on one side.
    for yy = stack_top - 1, stack_top + 2 do
      for xx = chimney_x + 3, chimney_x + 7 do
        if yy - stack_top < xx - chimney_x - 3 then cv:clear(xx, yy) end
      end
    end
    scorch(cv, bay[2] + 2, y + 11, 18, 11, 0.45, seed + 1)
    glow(cv, hearth[1] + 6, hearth[2] + 2, 11, 8, 0.7)
    litter(cv, r, x + 2, y + 26, w - 4, 5, 8, "stone", 2, 4)
  elseif stage == "destroyed" then
    -- The roof fallen into the shed, the plank wall burnt to stumps, the stack broken half down;
    -- the hearth holds, a few embers in it.
    shed()
    coals(cv, hearth[1] + 3, hearth[2] + 1, 7, 3, 0.25, seed)
    stumps(cv, bay[2] + 2, y + 11, x + w - 5 - bay[2] - 1, 11, seed + 3)
    scorch(cv, x + 1, y + 11, w - 2, 11, 0.4, seed + 4)
    roof(cv, x, w, y + 2, y + 4, y + 10)
    charred(cv, x - 4, y + 2, w + 8, 9, seed + 5)
    for xx = x - 1, x + 12 do
      for yy = y + 4, y + 10 do
        if xx - x < 13 - (yy - y - 4) * 2 then
          local ch = (xx - x) % 3
          cv:set(xx, yy, ch == 0 and c("stone", 1) or (ch == 1 and c("stone", 3) or c("stone", 2)))
        end
      end
    end
    for k = 0, 2 do
      local bx = x + 14 + k * 11
      cv:line(bx, y + 4 + r.int(0, 2), bx + r.int(-4, 4), y + 18, c("wood", 1))
    end
    local broken = y + 2
    chimney(broken)
    for xx = chimney_x, chimney_x + 6 do
      local drop = math.floor(px.noise(xx, 0, 2, seed + 6) * 4)
      for yy = broken, broken + drop - 1 do cv:clear(xx, yy) end
    end
    litter(cv, r, x + 2, y + 26, w - 4, 5, 14, "ink", 2, 4)
    litter(cv, r, x + 2, y + 26, w - 4, 5, 6, "stone", 2, 4)
  elseif stage == "rubble" then
    -- A heap on the sill; the stub of the stack and the hearth's block standing out of it.
    heap(cv, x + w / 2, y + 18, w / 2 - 1, 6, seed, r)
    cv:rect(hearth[1], hearth[2] + 3, 13, 5, c("stone", 3)); cv:hline(hearth[1], hearth[2] + 3, 13, c("stone", 4))
    chimney(hearth[2] - 2)
    cv:hline(chimney_x, hearth[2] - 2, 7, c("ink", 3))
    litter(cv, r, x + 2, y + 26, w - 4, 5, 6, "ink", 3, 4)
  end

  -- Before the shed: the anvil in front of the hearth, a quench tub of water, a heap of charcoal. The
  -- anvil outlasts the building; the tub and the charcoal do not.
  anvil(cv, x + 15, y + 32, stage == "destroyed" or stage == "rubble")
  if stage ~= "rubble" then
    local tx, ty = x + 31, y + 28
    if stage == "destroyed" then
      cv:rect(tx - 1, ty - 1, 7, 3, c("ink", 2)); cv:hline(tx - 1, ty - 1, 7, c("wood", 1))
    else
      cv:rect(tx - 1, ty - 2, 7, 5, c("wood", 3)); cv:vline(tx - 1, ty - 2, 5, c("wood", 4))
      cv:vline(tx + 2, ty - 2, 5, c("wood", 2)); cv:hline(tx - 1, ty, 7, c("ink", 3))
      cv:rect(tx, ty - 3, 5, 2, c("water", 3)); cv:set(tx + 1, ty - 3, c("water", 5))
      cv:hline(tx - 1, ty - 4, 7, c("wood", 4))
    end
    local cx = x + 41
    local spill = stage == "intact" and 0 or 2
    cv:each_ellipse(cx, y + 29, 4 + spill, 2.5 - spill / 2, function(xx, yy, d)
      cv:set(xx, yy, (px.hash(xx, yy, seed) > 0.6) and c("ink", 4) or (d < 0.5 and c("ink", 3) or c("ink", 2)))
    end)
    if stage == "intact" then cv:rect(cx + 3, y + 26, 4, 3, c("earth", 4)); cv:hline(cx + 3, y + 26, 4, c("earth", 5)) end
  end
  cv:outline(0, 0, cv.w, cv.h, INK)
end


local HERBS = { { "grass", 4 }, { "leaf", 4 }, { "amber", 3 }, { "earth", 5 }, { "grass", 3 }, { "leaf", 5 } }

-- A bundle of herbs hung head down from (bx, by): a tie, then leaves fanning out below it.
local function bundle(cv, bx, by, k, burnt)
  local h = HERBS[(k - 1) % #HERBS + 1]
  local col = burnt and c("ink", 3) or c(h[1], h[2])
  local dark = burnt and c("ink", 2) or px.shift(col, -1)
  cv:set(bx + 1, by, c("earth", 3))
  cv:hline(bx, by + 1, 3, col); cv:hline(bx, by + 2, 3, dark); cv:set(bx + 1, by + 2, col)
  if k % 2 == 1 then cv:set(bx + 1, by + 3, dark) end
end

-- A drying tray of woven bamboo, `w` wide, at (tx, ty), herbs spread on it.
local function tray(cv, tx, ty, w, seed, burnt)
  cv:rect(tx, ty, w, 4, burnt and c("ink", 2) or c("earth", 5))
  cv:hline(tx, ty, w, burnt and c("ink", 3) or c("earth", 6)); cv:hline(tx, ty + 3, w, burnt and c("ink", 1) or c("earth", 3))
  for xx = tx + 1, tx + w - 2 do
    for yy = ty + 1, ty + 2 do
      local n = px.hash(xx, yy, seed)
      if burnt then
        if n > 0.6 then cv:set(xx, yy, c("ink", 4)) end
      elseif n > 0.45 then
        local h = HERBS[math.floor(px.hash(xx, yy, seed + 1) * #HERBS) + 1]
        cv:set(xx, yy, c(h[1], h[2]))
      end
    end
  end
end

local function apothecary(cv, stage, seed)
  local r = px.rng(seed)
  local x, y = 8, 8
  local sw = 44 -- the shop's width; the yard takes the rest of the 64
  local yard = x + sw + 2
  -- The plinth, in every stage.
  cv:rect(x + 1, y + 27, sw - 2, 5, c("stone", 4))
  cv:hline(x + 1, y + 27, sw - 2, c("stone", 5)); cv:hline(x + 1, y + 31, sw - 2, c("stone", 2))
  cv:rect(x + 23, y + 29, 10, 2, c("stone", 5)); cv:hline(x + 23, y + 30, 10, c("stone", 3))

  if stage == "intact" or stage == "damaged" or stage == "destroyed" then
    -- Plaster between lacquered pillars.
    cv:rect(x + 3, y + 14, sw - 6, 13, c("paper", 2))
    cv:hline(x + 3, y + 25, sw - 6, c("paper", 1))
    -- The counter window: a cabinet of drawers behind it, jars and a mortar on the sill.
    cv:rect(x + 6, y + 17, 13, 7, c("ink", 2))
    for row = 0, 2 do
      for col = 0, 3 do
        local dx, dy = x + 7 + col * 3, y + 18 + row * 2
        cv:hline(dx, dy, 2, c("wood", 3)); cv:set(dx + 1, dy, c("gold", 3))
      end
    end
    cv:rect(x + 5, y + 24, 15, 2, c("wood", 4)); cv:hline(x + 5, y + 25, 15, c("wood", 2))
    cv:rect(x + 7, y + 22, 2, 2, c("paper", 3)); cv:set(x + 7, y + 22, c("stone", 6))
    cv:rect(x + 11, y + 22, 2, 2, c("earth", 3)); cv:set(x + 11, y + 22, c("earth", 5))
    cv:rect(x + 15, y + 23, 3, 1, c("stone", 3)); cv:set(x + 16, y + 22, c("wood", 5))
    -- The door under a cloth curtain split down the middle, the name board above it.
    cv:rect(x + 24, y + 18, 8, 9, c("ink", 2))
    cv:rect(x + 24, y + 18, 8, 5, c("cloth", 2)); cv:vline(x + 28, y + 19, 4, c("ink", 2))
    cv:hline(x + 24, y + 18, 8, c("cloth", 3))
    cv:rect(x + 23, y + 14, 10, 3, c("wood", 2)); cv:hline(x + 23, y + 14, 10, c("wood", 4))
    cv:hline(x + 25, y + 15, 2, c("paper", 3)); cv:hline(x + 29, y + 15, 2, c("paper", 3))
    -- A second window, shuttered.
    cv:rect(x + 35, y + 18, 4, 5, c("wood", 3)); cv:vline(x + 37, y + 18, 5, c("wood", 2))
    for _, pxx in ipairs({ x + 3, x + 21, x + sw - 5 }) do
      cv:vline(pxx, y + 14, 13, c("lacquer", 3)); cv:vline(pxx + 1, y + 14, 13, c("lacquer", 2))
    end
  end
  if stage == "intact" or stage == "damaged" then
    roof(cv, x, sw, y - 3, y + 1, y + 13)
    -- Herbs hung from the eave over the counter.
    for k = 0, 3 do
      if not (stage == "damaged" and k == 2) then bundle(cv, x + 6 + k * 4, y + 14, k + 1) end
    end
  end
  if stage == "damaged" then
    hole(cv, x + 28, y - 1, 8, 5, r)
    hole(cv, x + 6, y + 5, 6, 4, r)
    scorch(cv, x + 22, y + 14, sw - 25, 13, 0.45, seed + 1)
    -- Drawers pulled out and spilled on the sill.
    cv:set(x + 10, y + 20, c("ink", 1)); cv:set(x + 11, y + 20, c("ink", 1)); cv:set(x + 16, y + 18, c("ink", 1))
    litter(cv, r, x + 2, y + 28, sw - 4, 4, 8, "stone", 2, 3)
  elseif stage == "destroyed" then
    stumps(cv, x + 3, y + 14, sw - 6, 13, seed + 3)
    for _, pxx in ipairs({ x + 3, x + 21, x + sw - 5 }) do
      local top = y + 14 + r.int(0, 7)
      cv:rect(pxx, top, 2, y + 27 - top, c("ink", 2)); cv:set(pxx, top, c("wood", 1))
    end
    roof(cv, x, sw, y + 6, y + 8, y + 15)
    charred(cv, x - 4, y + 6, sw + 8, 10, seed + 5)
    for xx = x - 1, x + 10 do
      for yy = y + 8, y + 15 do
        if xx - x < 11 - (yy - y - 8) * 2 then
          local ch = (xx - x) % 3
          cv:set(xx, yy, ch == 0 and c("stone", 1) or (ch == 1 and c("stone", 3) or c("stone", 2)))
        end
      end
    end
    for k = 0, 2 do
      local bx = x + 12 + k * 10
      cv:line(bx, y + 8 + r.int(0, 2), bx + r.int(-4, 4), y + 24, c("wood", 1))
    end
    litter(cv, r, x + 2, y + 27, sw - 4, 5, 12, "ink", 2, 4)
  elseif stage == "rubble" then
    heap(cv, x + sw / 2, y + 23, sw / 2 - 2, 6, seed, r)
    litter(cv, r, x + 4, y + 26, sw - 8, 4, 4, "paper", 1, 2)
  end

  -- The yard: a herb rack of two posts and two bars hung with bundles, a drying tray on trestles
  -- before it, a round tray on the ground.
  local p1, p2 = yard + 1, yard + 15
  if stage == "intact" or stage == "damaged" then
    for _, p in ipairs({ p1, p2 }) do
      cv:vline(p, y + 4, 20, c("wood", 4)); cv:vline(p + 1, y + 4, 20, c("wood", 2))
    end
    cv:hline(p1, y + 5, p2 - p1 + 2, c("wood", 5))
    if stage == "intact" then
      cv:hline(p1, y + 12, p2 - p1 + 2, c("wood", 4))
      for k = 0, 2 do bundle(cv, p1 + 3 + k * 4, y + 6, k + 1); bundle(cv, p1 + 3 + k * 4, y + 13, k + 4) end
    else
      -- The lower bar snapped and hanging, its bundles on the ground.
      cv:line(p1 + 1, y + 12, p1 + 8, y + 19, c("wood", 4))
      for k = 0, 1 do bundle(cv, p1 + 3 + k * 4, y + 6, k + 1) end
      for k = 1, 3 do bundle(cv, yard + k * 4 - 2, y + 28 + k % 2, k + 3) end
    end
    tray(cv, yard, y + 20, 18, seed + 7)
    for _, l in ipairs({ yard + 2, yard + 14 }) do cv:vline(l, y + 24, 3, c("wood", 2)); cv:vline(l + 1, y + 24, 3, c("wood", 3)) end
    if stage == "intact" then
      cv:each_ellipse(yard + 9, y + 29, 5, 1.8, function(xx, yy, d)
        local h = HERBS[math.floor(px.hash(xx, yy, seed + 2) * #HERBS) + 1]
        cv:set(xx, yy, d > 0.7 and c("earth", 4) or (px.hash(xx, yy, seed + 3) > 0.4 and c(h[1], h[2]) or c("earth", 5)))
      end)
    end
  elseif stage == "destroyed" then
    -- The rack burnt to two stumps; the tray charred where it fell.
    cv:rect(p1, y + 17, 2, 7, c("ink", 2)); cv:set(p1, y + 16, c("ink", 3))
    cv:rect(p2, y + 20, 2, 4, c("ink", 2))
    tray(cv, yard + 1, y + 27, 16, seed + 7, true)
    litter(cv, r, yard, y + 24, 18, 7, 8, "ink", 2, 4)
  elseif stage == "rubble" then
    cv:line(yard + 1, y + 27, yard + 14, y + 25, c("ink", 2)); cv:line(yard + 2, y + 29, yard + 16, y + 28, c("wood", 1))
    litter(cv, r, yard, y + 24, 18, 7, 6, "ink", 3, 4)
    litter(cv, r, yard, y + 22, 18, 9, 4, "grass", 4, 5)
  end
  cv:outline(0, 0, cv.w, cv.h, INK)
end

local buildings = {
  { "smithy", 64, 48, smithy, 700 },
  { "apothecary", 80, 48, apothecary, 710 },
}
for _, b in ipairs(buildings) do
  local frames, tags = {}, {}
  for n, stage in ipairs(STAGES) do
    local cv = px.canvas(b[2], b[3])
    b[4](cv, stage, b[5] + n)
    frames[n] = cv
    tags[n] = { stage, n, n, 100 }
  end
  px.save_frames(b[2], b[3], frames, tags, here .. "/" .. b[1] .. ".aseprite")
end
