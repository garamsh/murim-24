-- Draws the structures of a sect's seat (ADR 0026), one frame per damage stage, tagged intact,
-- damaged, destroyed and rubble. The cast shadow is not in the frame.
--   training_ground.aseprite: an open yard over 6x4 cells, curbed in stone, with weapon racks and
--     wooden dummies along its back row, plum-blossom poles and stone weights on the sand. A
--     112x80 frame with the footprint at (8, 8).
--   manual_hall.aseprite: a two-storey library over 4x3 cells on a high stone platform with
--     steps, a name plaque under the upper roof. An 80x64 frame with the footprint at (8, 8).
--   seat_gate.aseprite: a one-bay paifang over 3x1 cells, two lacquered pillars on stone bases
--     under a tiled roof, a name board between them. A 64x64 frame with the footprint at (8, 40):
--     the 32 rows above it hold what rises.
-- Run: aseprite --batch --script art/structures/seat.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c
local INK = c("ink", 1)
local STAGES = { "intact", "damaged", "destroyed", "rubble" }

-- A tiled roof from `top` to the eave at `bottom`, `w` wide from x0, hipped above `ridge`, with
-- upturned corners and gold finials on the ridge's curled ends, as the house's.
local function roof(cv, x0, w, top, ridge, bottom)
  for yy = top, bottom do
    local inset = yy < ridge and math.max(0, (ridge - yy) - 1) or 0
    local flare = (bottom - yy < 3) and (3 - (bottom - yy)) or 0
    for xx = x0 - 2 - flare + inset, x0 + w + 1 + flare - inset do
      local ch = (xx - x0) % 3
      local col
      if yy < ridge then
        col = ch == 0 and c("stone", 3) or c("stone", 4)
      else
        col = ch == 0 and c("stone", 2) or (ch == 1 and c("stone", 4) or c("stone", 3))
        if bottom - yy < 3 and ch ~= 1 then col = px.shift(col, -1) end
      end
      cv:set(xx, yy, col)
    end
  end
  for xx = x0 - 4, x0 + w + 3 do cv:set(xx, bottom, ((xx - x0) % 3 == 1) and c("stone", 5) or c("stone", 1)) end
  cv:hline(x0 + 1, ridge - 1, w - 2, c("stone", 2)); cv:hline(x0 + 1, ridge, w - 2, c("ink", 2))
  cv:hline(x0 + 1, ridge + 1, w - 2, c("stone", 1))
  for _, s in ipairs({ { x0, -1 }, { x0 + w - 1, 1 } }) do
    cv:set(s[1] + s[2], ridge - 1, c("ink", 2)); cv:set(s[1] + s[2] * 2, ridge - 2, c("ink", 2))
    cv:set(s[1] + s[2] * 2, ridge - 3, c("ink", 2)); cv:set(s[1] + s[2], ridge - 3, c("gold", 3))
  end
  cv:set(x0 - 6, bottom - 2, c("stone", 2)); cv:set(x0 - 5, bottom - 1, c("stone", 2))
  cv:set(x0 + w + 5, bottom - 2, c("stone", 2)); cv:set(x0 + w + 4, bottom - 1, c("stone", 2))
end

-- A hole through a roof showing rafters, its rim darkened.
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

-- Broken beams and ash in a box: the charred inside of a fallen roof.
local function charred(cv, x, y, w, h, seed, ragged)
  for yy = y, y + h - 1 do
    for xx = x, x + w - 1 do
      local edge = math.min(xx - x, x + w - 1 - xx, yy - y)
      if not ragged or edge > px.noise(xx, yy, 3, seed) * 4 then
        local n = px.noise(xx, yy, 4, seed)
        cv:set(xx, yy, n > 0.62 and c("ink", 3) or (n > 0.35 and c("ink", 2) or c("wood", 1)))
      end
    end
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

-- The training ground --------------------------------------------------------------------------

-- A weapon rack standing on `base`: two posts, two bars, and `n` hafts with their heads above.
-- `lost` leaves that many slots empty from the right; `charred` burns it to its frame.
local function rack(cv, rx, base, n, lost, burnt)
  local w = n * 4 + 3
  local wood = burnt and function(k) return c("ink", math.min(4, k)) end or function(k) return c("wood", k) end
  for k = 0, n - 1 - (lost or 0) do
    local wx = rx + 3 + k * 4
    cv:vline(wx, base - 15, 16, wood(4))
    local kind = k % 3
    if kind == 0 then
      -- A spear: a leaf blade and a red tassel under it.
      cv:set(wx, base - 18, c("stone", 6)); cv:vline(wx, base - 17, 2, c("stone", 5))
      cv:set(wx - 1, base - 15, c("lacquer", 3)); cv:set(wx + 1, base - 15, c("lacquer", 2))
    elseif kind == 1 then
      -- A glaive: a broad curved blade on one side.
      cv:rect(wx + 1, base - 18, 2, 5, c("stone", 5)); cv:set(wx + 2, base - 18, c("stone", 6))
      cv:set(wx + 2, base - 14, c("stone", 4)); cv:vline(wx, base - 18, 3, c("stone", 4))
    else
      -- A staff with brass caps.
      cv:vline(wx, base - 17, 2, wood(5)); cv:set(wx, base - 17, c("gold", 3))
    end
  end
  cv:rect(rx, base - 14, 2, 15, wood(3)); cv:vline(rx, base - 14, 15, wood(4))
  cv:rect(rx + w - 2, base - 14, 2, 15, wood(3)); cv:vline(rx + w - 2, base - 14, 15, wood(4))
  cv:hline(rx, base - 11, w, wood(4)); cv:hline(rx, base - 10, w, wood(2))
  cv:hline(rx, base - 2, w, wood(4)); cv:hline(rx, base - 1, w, wood(2))
end

-- A rack knocked flat: its frame and hafts lying on the sand.
local function fallen_rack(cv, rx, y, n, burnt)
  local w = n * 4 + 3
  local col = burnt and c("ink", 2) or c("wood", 3)
  cv:hline(rx, y, w, col); cv:hline(rx, y + 4, w, col)
  for k = 0, n - 1 do cv:line(rx + 2 + k * 4, y - 1, rx + 4 + k * 4, y + 6, burnt and c("ink", 3) or c("wood", 4)) end
  if not burnt then cv:set(rx + 5, y + 7, c("stone", 5)); cv:set(rx + 13, y + 7, c("stone", 6)) end
end

-- A wooden dummy on a stone footing: a post with two arms and a leg, bound with rope.
-- `snapped` leaves its stump and lays its top beside it; `burnt` chars it.
local function dummy(cv, dx, base, snapped, burnt)
  cv:rect(dx - 1, base - 1, 6, 2, c("stone", 3)); cv:hline(dx - 1, base - 1, 6, c("stone", 4))
  local cols = burnt and { c("ink", 4), c("ink", 3), c("ink", 2), c("ink", 2) }
    or { c("wood", 5), c("wood", 4), c("wood", 3), c("wood", 2) }
  local top = snapped and base - 6 or base - 15
  for yy = top, base - 2 do
    for k = 0, 3 do cv:set(dx + k, yy, cols[k + 1]) end
  end
  if snapped then
    cv:set(dx + 1, top - 1, cols[2]); cv:set(dx + 3, top - 1, cols[4])
    if not burnt then
      cv:rect(dx + 5, base - 3, 9, 3, cols[3]); cv:hline(dx + 5, base - 3, 9, cols[1])
      cv:vline(dx + 8, base - 6, 3, cols[2])
    end
    return
  end
  cv:hline(dx + 1, base - 16, 2, cols[2])
  cv:hline(dx - 3, base - 12, 3, cols[2]); cv:hline(dx + 4, base - 11, 3, cols[3])
  cv:hline(dx - 2, base - 6, 2, cols[3])
  if not burnt then cv:hline(dx, base - 9, 4, c("earth", 6)); cv:hline(dx, base - 4, 4, c("earth", 5)) end
end

-- A plum-blossom pole: a short post seen from above, its top lit.
local function pole(cv, x, y, height, burnt)
  local side = burnt and c("ink", 2) or c("wood", 3)
  cv:rect(x, y - height, 3, height, side); cv:vline(x + 2, y - height, height, burnt and c("ink", 1) or c("wood", 2))
  cv:hline(x, y - height - 1, 3, burnt and c("ink", 3) or c("wood", 5))
end

local function training_ground(cv, stage, seed)
  local r = px.rng(seed)
  local x, y, w, h = 8, 8, 96, 64
  -- The yard: raked sand inside a curb of stone, two pixels wide.
  for yy = y, y + h - 1 do
    for xx = x, x + w - 1 do
      local curb = xx < x + 2 or xx > x + w - 3 or yy < y + 2 or yy > y + h - 3
      local col
      if curb then
        col = c("stone", 4)
        if xx == x or yy == y then col = c("stone", 5) end
        if xx == x + w - 1 or yy == y + h - 1 then col = c("stone", 2) end
        if (yy < y + 2 or yy > y + h - 3) and (xx - x) % 8 == 0 then col = c("stone", 3) end
        if (xx < x + 2 or xx > x + w - 3) and (yy - y) % 8 == 0 then col = c("stone", 3) end
      else
        local n = px.noise(xx, yy, 3, seed) + (px.hash(xx, yy, seed + 1) - 0.5) * 0.12
        col = c("earth", 5)
        if n > 0.8 then col = c("earth", 6) elseif n < 0.18 then col = c("earth", 4) end
        -- A ring trodden into the sand where pupils spar.
        local dx, dy = (xx - (x + 62)) / 18, (yy - (y + 42)) / 12
        local d = math.sqrt(dx * dx + dy * dy)
        if d > 0.88 and d < 1.0 and px.hash(xx, yy, seed + 2) > 0.1 then col = c("earth", 4) end
      end
      cv:set(xx, yy, col)
    end
  end

  local racks_at = { { x + 4, 5 }, { x + w - 27, 5 } }
  local dummies_at = { x + 36, x + 46, x + 56 }
  local poles = {}
  for row = 0, 2 do
    for col = 0, 2 do poles[#poles + 1] = { x + 8 + col * 9 + (row % 2) * 4, y + 30 + row * 10 } end
  end
  local weights = { { x + 76, y + 54 }, { x + 84, y + 56 } }

  if stage == "intact" then
    for _, p in ipairs(poles) do pole(cv, p[1], p[2], 5) end
    for _, rk in ipairs(racks_at) do rack(cv, rk[1], y + 12, rk[2]) end
    for _, dx in ipairs(dummies_at) do dummy(cv, dx, y + 14) end
  elseif stage == "damaged" then
    scorch(cv, x + 2, y + 2, w - 4, h - 4, 0.74, seed + 3)
    for k, p in ipairs(poles) do pole(cv, p[1], p[2], (k == 2 or k == 7) and 1 or 5) end
    rack(cv, racks_at[1][1], y + 12, racks_at[1][2], 2)
    fallen_rack(cv, racks_at[2][1], y + 14, racks_at[2][2])
    dummy(cv, dummies_at[1], y + 14); dummy(cv, dummies_at[2], y + 14, true); dummy(cv, dummies_at[3], y + 14)
    for k = 1, 3 do
      local sx, sy = r.int(x + 40, x + 80), r.int(y + 24, y + 50)
      cv:line(sx, sy, sx + r.int(5, 9), sy + r.int(-2, 2), c("wood", 4)); cv:set(sx, sy, c("stone", 5))
    end
  elseif stage == "destroyed" then
    scorch(cv, x + 2, y + 2, w - 4, h - 4, 0.5, seed + 3)
    scorch(cv, x + 2, y + 2, w - 4, h - 4, 0.72, seed + 4, -1)
    for k = 1, 30 do cv:set(r.int(x + 4, x + w - 5), r.int(y + 4, y + h - 5), c("ink", r.int(2, 3))) end
    for k, p in ipairs(poles) do pole(cv, p[1], p[2], k % 2 == 0 and 1 or 3, true) end
    fallen_rack(cv, racks_at[1][1], y + 14, racks_at[1][2], true)
    rack(cv, racks_at[2][1], y + 12, racks_at[2][2], racks_at[2][2], true)
    for _, dx in ipairs(dummies_at) do dummy(cv, dx, y + 14, true, true) end
  elseif stage == "rubble" then
    -- The sand gone to grass and ash inside the curb, a heap of burnt wood at the back.
    for yy = y + 2, y + h - 3 do
      for xx = x + 2, x + w - 3 do
        local n = px.noise(xx, yy, 5, seed + 5)
        if n > 0.62 then cv:set(xx, yy, n > 0.8 and c("grass", 5) or c("grass", 4))
        elseif n < 0.2 then cv:set(xx, yy, c("earth", 3)) end
      end
    end
    for _, p in ipairs(poles) do cv:hline(p[1], p[2] - 1, 3, c("ink", 3)) end
    heap(cv, x + 46, y + 12, 26, 5, seed + 6, r)
    for _, dx in ipairs(dummies_at) do cv:rect(dx - 1, y + 13, 6, 2, c("stone", 3)) end
  end
  -- The stone weights lie in every stage: a block with a handle cut through it.
  for k, wt in ipairs(weights) do
    local tipped = stage == "rubble" and k == 2
    local bw, bh = tipped and 7 or 6, tipped and 4 or 5
    cv:rect(wt[1], wt[2] - bh, bw, bh, c("stone", 3))
    cv:hline(wt[1], wt[2] - bh, bw, c("stone", 5)); cv:vline(wt[1] + bw - 1, wt[2] - bh, bh, c("stone", 2))
    cv:rect(wt[1] + 2, wt[2] - bh + 1, 2, 2, c("ink", 2))
  end
  cv:outline(0, 0, cv.w, cv.h, INK)
end

-- The manual hall --------------------------------------------------------------------------------

local function manual_hall(cv, stage, seed)
  local r = px.rng(seed)
  local x, y, w = 8, 8, 64
  -- The platform and its steps, in every stage.
  cv:rect(x, y + 37, w, 11, c("stone", 4))
  cv:hline(x, y + 37, w, c("stone", 5)); cv:hline(x, y + 47, w, c("stone", 2))
  for yy = y + 40, y + 46, 3 do
    for xx = x + 1, x + w - 2 do if (xx - x + (yy - y) // 3 * 5) % 10 == 0 then cv:set(xx, yy, c("stone", 3)) end end
    cv:hline(x, yy - 1, w, c("stone", 3))
  end
  local sx, sw = x + 24, 16
  for k = 0, 3 do
    cv:hline(sx, y + 38 + k * 2, sw, c("stone", 5)); cv:hline(sx, y + 39 + k * 2, sw, c("stone", 3))
  end
  cv:vline(sx - 1, y + 37, 11, c("stone", 2)); cv:vline(sx + sw, y + 37, 11, c("stone", 2))

  -- The lower storey: lacquered pillars, latticed shutters over paper, a double door.
  local function lower_wall()
    local wy = y + 27
    cv:rect(x + 3, wy, w - 6, 10, c("paper", 2))
    for xx = x + 3, x + w - 4 do
      for yy = wy + 1, wy + 8 do
        if (xx - x) % 3 == 0 or (yy - wy) % 3 == 0 then cv:set(xx, yy, c("wood", 3)) end
      end
    end
    cv:hline(x + 3, wy + 9, w - 6, c("wood", 2))
    for _, pxx in ipairs({ x + 3, x + 18, x + 44, x + w - 5 }) do
      cv:vline(pxx, wy, 10, c("lacquer", 3)); cv:vline(pxx + 1, wy, 10, c("lacquer", 2))
    end
    cv:rect(x + 26, wy + 1, 12, 9, c("wood", 2)); cv:vline(x + 32, wy + 1, 9, c("wood", 1))
    cv:hline(x + 26, wy + 4, 12, c("wood", 4)); cv:set(x + 30, wy + 6, c("gold", 3)); cv:set(x + 33, wy + 6, c("gold", 3))
  end
  -- The upper storey: a narrower wall of lattice, the name plaque at its centre.
  local function upper_wall()
    local wy = y + 9
    cv:rect(x + 9, wy, w - 18, 9, c("wood", 2))
    for xx = x + 9, x + w - 10, 2 do cv:vline(xx, wy + 1, 7, c("wood", 4)) end
    for _, pxx in ipairs({ x + 9, x + w - 11 }) do
      cv:vline(pxx, wy, 9, c("lacquer", 3)); cv:vline(pxx + 1, wy, 9, c("lacquer", 2))
    end
    cv:rect(x + 23, wy + 1, 18, 7, c("gold", 3)); cv:rect(x + 24, wy + 2, 16, 5, c("ink", 2))
    for k = 0, 2 do
      local gx = x + 26 + k * 5
      cv:vline(gx + 1, wy + 3, 3, c("gold", 4)); cv:hline(gx, wy + 4, 3, c("gold", 4))
    end
  end

  if stage == "intact" or stage == "damaged" then
    upper_wall()
    roof(cv, x + 8, w - 16, y - 4, y + 1, y + 9)
    lower_wall()
    roof(cv, x, w, y + 17, y + 17, y + 27)
    -- Bracket band under the lower eave, painted.
    for xx = x + 3, x + w - 4 do cv:set(xx, y + 28, (xx % 4 < 2) and c("jade", 1) or c("wood", 4)) end
  end
  if stage == "damaged" then
    hole(cv, x + 38, y - 1, 8, 6, r)
    hole(cv, x + 8, y + 20, 9, 5, r)
    hole(cv, x + 46, y + 21, 6, 4, r)
    scorch(cv, x + 40, y + 27, 21, 10, 0.45, seed + 1)
    cv:rect(x + 47, y + 29, 5, 6, c("ink", 1))
    -- Pages blown out of the hall onto the steps.
    for k = 1, 7 do
      local tx, ty = r.int(x + 6, x + 56), r.int(y + 40, y + 46)
      cv:hline(tx, ty, 2, c("paper", 3)); cv:set(tx, ty + 1, c("paper", 1))
    end
  elseif stage == "destroyed" then
    -- The upper storey gone; the lower roof fallen in, one slab left; pillars charred.
    charred(cv, x + 1, y + 12, w - 2, 16, seed, true)
    for yy = y + 18, y + 27 do
      for xx = x - 2, x + 18 do
        if xx - x < 20 - (yy - y - 18) * 1.2 - px.noise(xx, yy, 2, seed) * 3 then
          local ch = (xx - x) % 3
          cv:set(xx, yy, ch == 0 and c("stone", 1) or (ch == 1 and c("stone", 3) or c("stone", 2)))
        end
      end
    end
    for k = 0, 2 do
      local bx = x + 14 + k * 16
      cv:line(bx, y + 13 + r.int(0, 3), bx + r.int(-5, 5), y + 28, c("wood", 2))
    end
    for xx = x + 3, x + w - 4 do
      local hgt = math.floor(px.noise(xx, 0, 4, seed + 3) * 9)
      if xx >= x + 25 and xx <= x + 38 then hgt = 0 end
      for yy = y + 37 - hgt, y + 36 do cv:set(xx, yy, yy - (y + 37 - hgt) < 2 and c("ink", 3) or c("paper", 1)) end
    end
    for _, pxx in ipairs({ x + 3, x + 18, x + 44, x + w - 5 }) do
      local top = y + 27 - r.int(-3, 8)
      cv:rect(pxx, top, 2, y + 37 - top, c("ink", 2)); cv:set(pxx, top, c("wood", 1))
    end
    for k = 1, 10 do
      local tx, ty = r.int(x + 12, x + 52), r.int(y + 43, y + 51)
      cv:set(tx, ty, k % 3 == 0 and c("paper", 1) or c("ink", r.int(2, 4)))
    end
  elseif stage == "rubble" then
    heap(cv, x + w / 2, y + 33, w / 2 - 3, 7, seed, r)
    for k = 1, 4 do cv:set(r.int(x + 6, x + w - 6), r.int(y + 28, y + 36), c("paper", 1)) end
  end
  cv:outline(0, 0, cv.w, cv.h, INK)
end

-- The seat gate ----------------------------------------------------------------------------------

local function seat_gate(cv, stage, seed)
  local r = px.rng(seed)
  local x, y = 8, 40 -- the footprint's top-left; the gate rises 32 rows above it
  local pillars = { x + 4, x + 41 }
  -- Stone bases in every stage.
  for _, p in ipairs(pillars) do
    cv:rect(p - 3, y + 8, 9, 6, c("stone", 4))
    cv:hline(p - 3, y + 8, 9, c("stone", 5)); cv:hline(p - 3, y + 13, 9, c("stone", 2))
    cv:vline(p + 5, y + 8, 6, c("stone", 3))
  end
  local function pillar(p, top, bottom, burnt)
    cv:vline(p, top, bottom - top, burnt and c("ink", 3) or c("lacquer", 3))
    cv:vline(p + 1, top, bottom - top, burnt and c("ink", 2) or c("lacquer", 2))
    cv:vline(p + 2, top, bottom - top, burnt and c("ink", 1) or c("lacquer", 1))
  end
  local function board(bx, by)
    for k = 0, 17 do
      cv:vline(bx + k, by, 9, c("gold", 3))
      if k > 0 and k < 17 then cv:vline(bx + k, by + 1, 7, c("ink", 2)) end
    end
    for k = 0, 2 do
      local gx = bx + 3 + k * 5
      cv:vline(gx + 1, by + 2, 5, c("gold", 4)); cv:hline(gx, by + 3, 3, c("gold", 4))
      cv:set(gx, by + 5, c("gold", 4))
    end
  end
  if stage == "intact" or stage == "damaged" then
    for _, p in ipairs(pillars) do pillar(p, y - 24, y + 9) end
    -- The lintels and the painted bracket band.
    cv:rect(x + 1, y - 16, 46, 2, c("lacquer", 3)); cv:hline(x + 1, y - 14, 46, c("lacquer", 1))
    cv:rect(x + 1, y - 26, 46, 3, c("lacquer", 2))
    for xx = x, x + 47 do cv:set(xx, y - 27, (xx % 4 < 2) and c("jade", 2) or c("wood", 4)) end
    for _, p in ipairs(pillars) do cv:rect(p - 2, y - 23, 2, 2, c("wood", 4)); cv:rect(p + 3, y - 23, 2, 2, c("wood", 4)) end
    board(x + 15, y - 23)
    roof(cv, x + 2, 44, y - 37, y - 33, y - 28)
  end
  if stage == "damaged" then
    hole(cv, x + 28, y - 34, 8, 4, r)
    scorch(cv, x + 38, y - 26, 8, 35, 0.4, seed + 1)
    cv:line(x + 21, y - 22, x + 24, y - 15, c("ink", 1)); cv:line(x + 24, y - 15, x + 23, y - 13, c("ink", 1))
    for k = 1, 6 do cv:set(r.int(x + 2, x + 46), r.int(y + 12, y + 15), c("stone", r.int(2, 4))) end
  elseif stage == "destroyed" then
    -- The roof fallen; the left pillar stands charred with the lintel hanging from it, the
    -- right one broken low; the board in the dust.
    pillar(pillars[1], y - 24, y + 9, true)
    pillar(pillars[2], y - 2, y + 9, true)
    cv:set(pillars[2] + 1, y - 3, c("ink", 2))
    cv:line(x + 2, y - 25, x + 40, y + 2, c("lacquer", 1)); cv:line(x + 2, y - 24, x + 40, y + 3, c("ink", 2))
    for xx = x + 6, x + 30 do
      for yy = y - 30, y - 26 do
        if xx - x - 6 < 22 - (yy - y + 30) * 3 then
          local ch = (xx - x) % 3
          cv:set(xx, yy, ch == 0 and c("stone", 1) or (ch == 1 and c("stone", 3) or c("stone", 2)))
        end
      end
    end
    board(x + 16, y + 6)
    scorch(cv, x + 16, y + 6, 18, 9, 0.45, seed + 2)
    for k = 1, 12 do cv:set(r.int(x, x + 47), r.int(y + 10, y + 15), c(k % 2 == 0 and "stone" or "ink", r.int(2, 3))) end
  elseif stage == "rubble" then
    heap(cv, x + 24, y + 9, 16, 4, seed, r)
    cv:line(x + 6, y + 6, x + 30, y + 3, c("ink", 2)); cv:line(x + 6, y + 7, x + 30, y + 4, c("wood", 1))
  end
  cv:outline(0, 0, cv.w, cv.h, INK)
end

local buildings = {
  { "training_ground", 112, 80, training_ground, 600 },
  { "manual_hall", 80, 64, manual_hall, 610 },
  { "seat_gate", 64, 64, seat_gate, 620 },
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
