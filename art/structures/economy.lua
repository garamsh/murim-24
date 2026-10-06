-- Draws the buildings the economy needs, which block movement and break like any structure
-- (ADR 0013): granary.aseprite over 2x2 cells, a store of grain raised on stilts under a straw
-- roof; and storehouse.aseprite over 3x2 cells, a long plastered store with double doors under a
-- slate roof. One frame per damage stage, tagged intact, damaged, destroyed and rubble; each frame
-- holds the footprint at (8, 8), as the house's does. The cast shadow is not in the frame.
-- Run: aseprite --batch --script art/structures/economy.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c
local INK = c("ink", 1)
local STAGES = { "intact", "damaged", "destroyed", "rubble" }
local INSET = 8

-- The granary, intact: stone footings and stilts, a plank store, a ladder to its door, and a
-- hipped straw roof with a dark ridge and a ragged fringe at the eaves.
local function granary(cv)
  local x, y = INSET, INSET
  -- Stilts on stone footings, under the store.
  for _, sx in ipairs({ x + 3, x + 12, x + 19, x + 27 }) do
    cv:rect(sx, y + 24, 2, 7, c("wood", 2)); cv:vline(sx, y + 24, 7, c("wood", 3))
    cv:rect(sx - 1, y + 30, 4, 2, c("stone", 4)); cv:set(sx - 1, y + 30, c("stone", 5))
  end
  -- The store: vertical planks, a dark seam every third.
  for xx = x + 2, x + 29 do
    for yy = y + 14, y + 24 do
      local col = ((xx - x) % 3 == 0) and c("wood", 2) or (((xx - x) % 3 == 1) and c("wood", 4) or c("wood", 3))
      if yy == y + 24 then col = c("wood", 1) end
      cv:set(xx, yy, col)
    end
  end
  cv:hline(x + 2, y + 14, 28, c("wood", 5))
  -- The door, and a ladder down from it.
  cv:rect(x + 13, y + 16, 6, 8, c("ink", 2)); cv:rect(x + 14, y + 17, 4, 7, c("wood", 1))
  cv:line(x + 13, y + 24, x + 9, y + 31, c("wood", 4)); cv:line(x + 18, y + 24, x + 14, y + 31, c("wood", 4))
  for k = 0, 2 do cv:line(x + 12 - k, y + 26 + k * 2, x + 17 - k, y + 26 + k * 2, c("wood", 3)) end
  -- Straw roof: rows widening down to the eaves, lit from the upper left.
  local top, eave = y - 4, y + 14
  for yy = top, eave do
    local inset = math.max(0, (eave - yy) // 2 - 1)
    for xx = x - 1 + inset, x + 32 - inset do
      local n = px.noise(xx, yy, 2, 77)
      local col = c("earth", 5)
      if xx < x + 12 - (yy - top) // 3 then col = c("earth", 6) end
      if xx > x + 22 + (yy - top) // 3 then col = c("earth", 4) end
      if (xx + yy) % 4 == 0 and n > 0.4 then col = px.shift(col, -1) end
      cv:set(xx, yy, col)
    end
  end
  cv:hline(x + 9, top, 14, c("earth", 3)); cv:hline(x + 9, top + 1, 14, c("earth", 3))
  for xx = x - 1, x + 32 do
    if (xx * 7) % 3 ~= 0 then cv:set(xx, eave + 1, c("earth", 3)) end
  end
  return { roof = { x - 1, top, 34, eave - top + 1 }, walls = { x + 2, y + 14, 28, 11 } }
end

-- The storehouse, intact: a stone plinth, plaster between lacquered pillars, iron-studded double
-- doors and vents under a low slate roof with a ridge and upturned eaves; a crate by the door.
local function storehouse(cv)
  local x, y = INSET, INSET
  local w = 48
  cv:rect(x + 1, y + 28, w - 2, 4, c("stone", 4))
  cv:hline(x + 1, y + 28, w - 2, c("stone", 5)); cv:hline(x + 1, y + 31, w - 2, c("stone", 2))
  cv:rect(x + 3, y + 15, w - 6, 13, c("paper", 2))
  cv:hline(x + 3, y + 26, w - 6, c("paper", 1))
  for _, pxx in ipairs({ x + 3, x + 23, x + w - 5 }) do
    cv:vline(pxx, y + 15, 13, c("lacquer", 3)); cv:vline(pxx + 1, y + 15, 13, c("lacquer", 2))
  end
  -- Vents near the eaves.
  for _, vx in ipairs({ x + 8, x + 14, x + 31, x + 37 }) do cv:hline(vx, y + 17, 3, c("ink", 2)) end
  -- Double doors with iron studs and a bar.
  local dx = x + 17
  cv:rect(dx, y + 19, 14, 9, c("wood", 2))
  cv:vline(dx + 7, y + 19, 9, c("wood", 1))
  cv:hline(dx, y + 22, 14, c("wood", 4))
  for _, sx in ipairs({ dx + 2, dx + 5, dx + 9, dx + 12 }) do
    cv:set(sx, y + 20, c("stone", 5)); cv:set(sx, y + 25, c("stone", 5))
  end
  -- A crate by the door.
  cv:rect(x + 34, y + 23, 6, 5, c("wood", 4)); cv:hline(x + 34, y + 23, 6, c("wood", 5))
  cv:line(x + 34, y + 23, x + 39, y + 27, c("wood", 2))
  -- Slate roof: the ridge, the slope down to the eaves, tile ends along them.
  local ridge, eave = y + 1, y + 15
  for yy = y - 3, eave do
    local inset = yy < ridge and (ridge - yy) or 0
    for xx = x - 1 + inset, x + w - inset do
      local ch = (xx - x) % 3
      local col = ch == 0 and c("stone", 2) or (ch == 1 and c("stone", 4) or c("stone", 3))
      if yy < ridge then col = ch == 0 and c("stone", 3) or c("stone", 4) end
      if eave - yy < 3 and ch ~= 1 then col = px.shift(col, -1) end
      cv:set(xx, yy, col)
    end
  end
  cv:hline(x + 1, ridge, w - 2, c("ink", 2)); cv:hline(x + 1, ridge - 1, w - 2, c("stone", 2))
  for xx = x - 3, x + w + 2 do cv:set(xx, eave, ((xx - x) % 3 == 1) and c("stone", 5) or c("stone", 1)) end
  cv:set(x - 4, eave - 1, c("stone", 2)); cv:set(x + w + 3, eave - 1, c("stone", 2))
  return { roof = { x - 1, y - 3, w + 2, eave - y + 4 }, walls = { x + 3, y + 15, w - 6, 13 } }
end

-- Breaks an intact building drawn in `cv` to `stage`: `parts` are its roof and wall rectangles.
local function damage(cv, parts, stage, seed, footprint)
  local r = px.rng(seed)
  local rx, ry, rw, rh = table.unpack(parts.roof)
  local wx, wy, ww, wh = table.unpack(parts.walls)
  if stage == "damaged" then
    -- Two holes in the roof showing rafters, scorch up the wall, litter at its foot.
    for _, hole in ipairs({ { rx + rw * 2 // 3, ry + rh // 3, 7, 5 }, { rx + rw // 4, ry + rh // 2, 5, 4 } }) do
      for yy = hole[2], hole[2] + hole[4] - 1 do
        for xx = hole[1], hole[1] + hole[3] - 1 do
          if cv:get(xx, yy) ~= 0 then cv:set(xx, yy, ((xx - hole[1]) % 3 == 1) and c("wood", 3) or c("ink", 1)) end
        end
      end
    end
    for yy = wy, wy + wh - 1 do
      for xx = wx + ww // 2, wx + ww - 1 do
        if cv:get(xx, yy) ~= 0 and px.noise(xx, yy, 4, seed) > 0.5 then cv:shade_at(xx, yy, -2) end
      end
    end
    for k = 1, 8 do cv:set(r.int(INSET, INSET + footprint[1] - 1), r.int(INSET + footprint[2] - 2, INSET + footprint[2] + 1), c("stone", r.int(2, 4))) end
  elseif stage == "destroyed" then
    -- The roof fallen in to a charred interior, beams across it; the walls broken to stumps.
    for yy = ry, ry + rh - 1 do
      for xx = rx, rx + rw - 1 do
        if cv:get(xx, yy) ~= 0 then
          local keep = xx < rx + rw // 4 - (yy - ry) // 2
          if not keep then
            local n = px.noise(xx, yy, 4, seed)
            cv:set(xx, yy, n > 0.6 and c("ink", 3) or (n > 0.3 and c("ink", 2) or c("wood", 1)))
          else
            cv:shade_at(xx, yy, -1)
          end
        end
      end
    end
    for k = 0, 2 do
      local bx = rx + rw // 4 + k * rw // 4
      cv:line(bx, ry + 1, bx + r.int(-4, 4), ry + rh - 1, c("wood", 1))
    end
    for xx = wx, wx + ww - 1 do
      local stump = math.floor(px.noise(xx, 0, 3, seed + 3) * wh * 0.8)
      for yy = wy, wy + wh - 1 - stump do
        if cv:get(xx, yy) ~= 0 then cv:set(xx, yy, c("ink", 2)) end
      end
      cv:shade_at(xx, wy + wh - stump, -2)
    end
    for k = 1, 12 do cv:set(r.int(INSET, INSET + footprint[1] - 1), r.int(INSET + footprint[2] - 3, INSET + footprint[2] + 1), c("ink", r.int(2, 4))) end
  elseif stage == "rubble" then
    for k in pairs(cv.p) do cv.p[k] = nil end
    local fx, fy, fw, fh = INSET, INSET, footprint[1], footprint[2]
    cv:each_ellipse(fx + fw / 2, fy + fh - 6, fw / 2 - 1, fh / 4, function(xx, yy, d)
      if d < 0.8 + px.noise(xx, yy, 3, seed) * 0.35 then
        local n = px.noise(xx, yy, 2, seed + 1)
        local col = n > 0.68 and c("stone", 4) or (n > 0.45 and c("earth", 2) or (n > 0.25 and c("wood", 1) or c("ink", 3)))
        cv:set(xx, yy, col)
      end
    end)
    for k = 1, 4 do
      local bx, by = r.int(fx + 2, fx + fw - 12), r.int(fy + fh - 12, fy + fh - 4)
      cv:line(bx, by, bx + r.int(6, 10), by + r.int(-2, 2), c("wood", 1))
    end
    for k = 1, 4 do cv:set(r.int(fx, fx + fw - 1), r.int(fy + fh - 4, fy + fh), c("grass", 4)) end
  end
  cv:outline(0, 0, cv.w, cv.h, INK)
end

local buildings = {
  { "granary", { 32, 32 }, granary, 400 },
  { "storehouse", { 48, 32 }, storehouse, 410 },
}
for _, b in ipairs(buildings) do
  local w, h = b[2][1] + 2 * INSET, b[2][2] + 2 * INSET
  local frames, tags = {}, {}
  for n, stage in ipairs(STAGES) do
    local cv = px.canvas(w, h)
    local parts = b[3](cv)
    damage(cv, parts, stage, b[4] + n, b[2])
    frames[n] = cv
    tags[n] = { stage, n, n, 100 }
  end
  px.save_frames(w, h, frames, tags, here .. "/" .. b[1] .. ".aseprite")
end
