-- Draws the village props that are structures, which block movement and break like any other
-- (ADR 0013): well.aseprite over 2x2 cells, barrels.aseprite over 1x1 and fence.aseprite over
-- 5x1, one frame per damage stage, tagged intact, damaged, destroyed and rubble. Each frame holds
-- its footprint at (8, 8), as the house's does, with the margin for what rises above it and the
-- debris that spills past it. The cast shadow is not in the frame; the view draws it.
-- Run: aseprite --batch --script art/structures/props.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c
local INK = c("ink", 1)
local STAGES = { "intact", "damaged", "destroyed", "rubble" }
local INSET = 8

-- A low heap of debris over the footprint, for the rubble stage: stones, splinters and ash.
local function heap(cv, x, y, w, h, seed, tones)
  local r = px.rng(seed)
  cv:each_ellipse(x + w / 2, y + h - 3, w / 2, 3.5, function(xx, yy, d)
    if d < 0.85 + px.noise(xx, yy, 2, seed) * 0.3 then
      cv:set(xx, yy, px.noise(xx, yy, 2, seed + 1) > 0.5 and tones[1] or tones[2])
    end
  end)
  for k = 1, math.max(3, w // 4) do
    cv:set(r.int(x + 1, x + w - 2), r.int(y + h - 5, y + h - 2), tones[3])
  end
  cv:outline(x - 2, y + h - 10, w + 4, 12, INK)
end

-- The well: a stone ring round dark water under a small tiled roof on two posts.
local function well(cv, stage, seed)
  local x, y = INSET, INSET
  local r = px.rng(seed)
  if stage == "rubble" then
    heap(cv, x + 2, y + 12, 28, 20, seed, { c("stone", 3), c("stone", 2), c("stone", 5) })
    return
  end
  local cx, cy = x + 16, y + 22
  -- Ring of stones, lit from the upper left; the water inside.
  cv:each_ellipse(cx, cy, 13, 8.5, function(xx, yy, d)
    local col
    if d < 0.62 then
      col = c("water", 2)
      if d < 0.3 and (xx + yy) % 5 == 0 then col = c("water", 4) end
    else
      local light = -(xx - cx) / 13 - (yy - cy) / 8.5
      col = light > 0.4 and c("stone", 5) or (light > -0.3 and c("stone", 4) or c("stone", 2))
      if (xx + (yy * 3)) % 7 == 0 then col = c("stone", 3) end
    end
    cv:set(xx, yy, col)
  end)
  cv:each_ellipse(cx, cy + 5, 13, 4, function(xx, yy, d)
    if yy > cy + 4 and d < 1 then cv:set(xx, yy, (xx % 4 == 0) and c("stone", 2) or c("stone", 3)) end
  end)
  if stage == "intact" or stage == "damaged" then
    -- Posts, crossbeam, rope and bucket, and the roof.
    cv:rect(x + 4, y + 4, 2, 18, c("wood", 3)); cv:vline(x + 4, y + 4, 18, c("wood", 4))
    cv:rect(x + 26, y + 4, 2, 18, c("wood", 2)); cv:vline(x + 26, y + 4, 18, c("wood", 3))
    cv:hline(x + 4, y + 6, 24, c("wood", 4)); cv:hline(x + 4, y + 7, 24, c("wood", 2))
    cv:vline(cx, y + 8, 8, c("paper", 1))
    cv:rect(cx - 2, y + 15, 5, 4, c("wood", 3)); cv:hline(cx - 2, y + 15, 5, c("wood", 4))
    for yy = y - 4, y + 3 do
      local inset = (y + 3 - yy) // 2
      for xx = x + 1 + inset, x + 30 - inset do
        local col = ((xx - x) % 3 == 0) and c("stone", 2) or (yy < y and c("stone", 4) or c("stone", 3))
        cv:set(xx, yy, col)
      end
    end
    cv:hline(x, y + 4, 32, c("stone", 1))
    if stage == "damaged" then
      -- Tiles gone from the roof, the beam cracked, stones knocked from the ring.
      for k = 1, 6 do cv:set(r.int(x + 6, x + 24), r.int(y - 3, y + 2), c("wood", 2)) end
      cv:rect(x + 18, y - 2, 4, 3, c("ink", 2))
      cv:set(x + 15, y + 6, c("ink", 2)); cv:set(x + 16, y + 7, c("ink", 2))
      cv:rect(x + 23, y + 26, 3, 2, c("earth", 2)); cv:set(x + 30, y + 31, c("stone", 4))
    end
  else
    -- Destroyed: the roof fallen, one post snapped, the ring broken and blackened.
    cv:rect(x + 4, y + 12, 2, 10, c("wood", 1)); cv:set(x + 4, y + 12, c("ink", 3))
    cv:line(x + 26, y + 20, x + 30, y + 10, c("wood", 2))
    cv:line(x + 2, y + 26, x + 22, y + 30, c("stone", 3)); cv:line(x + 3, y + 27, x + 21, y + 31, c("stone", 2))
    for yy = cy - 8, cy + 8 do
      for xx = cx - 13, cx + 13 do
        if px.noise(xx, yy, 3, seed) > 0.62 and cv:get(xx, yy) ~= 0 then cv:shade_at(xx, yy, -2) end
      end
    end
    for k = 1, 8 do cv:set(r.int(x, x + 31), r.int(y + 28, y + 33), c("stone", r.int(2, 4))) end
  end
  cv:outline(0, 0, cv.w, cv.h, INK)
end

-- One barrel, 6 wide and 8 tall, its top-left at (x, y).
local function barrel(cv, x, y, broken)
  cv:rect(x, y, 6, 8, c("wood", 4))
  cv:vline(x, y, 8, c("wood", 5)); cv:vline(x + 5, y, 8, c("wood", 2))
  cv:hline(x, y + 1, 6, c("ink", 3)); cv:hline(x, y + 6, 6, c("ink", 3))
  cv:hline(x + 1, y, 4, c("wood", 5))
  if broken then
    cv:rect(x + 2, y, 3, 4, c("ink", 2)); cv:set(x + 3, y + 4, c("amber", 4))
  end
end

-- Barrels: two side by side and one on top.
local function barrels(cv, stage, seed)
  local x, y = INSET, INSET
  local r = px.rng(seed)
  if stage == "rubble" then
    heap(cv, x, y + 4, 16, 12, seed, { c("wood", 2), c("wood", 3), c("amber", 4) })
    return
  end
  if stage == "destroyed" then
    -- Staves split and spread, grain spilled.
    for k = 0, 5 do cv:line(x + k * 3 - 1, y + 15, x + k * 3 + r.int(-1, 1), y + 10, c("wood", 3)) end
    cv:hline(x - 1, y + 15, 18, c("wood", 2))
    for k = 1, 6 do cv:set(r.int(x, x + 15), r.int(y + 12, y + 16), c("amber", 4)) end
    barrel(cv, x + 9, y + 8, true)
  else
    barrel(cv, x + 1, y + 8, stage == "damaged")
    barrel(cv, x + 9, y + 8, false)
    if stage == "intact" then barrel(cv, x + 5, y + 1, false) end
  end
  cv:outline(0, 0, cv.w, cv.h, INK)
end

-- The fence: posts every 8 pixels with two rails, along a 5-cell run.
local function fence(cv, stage, seed)
  local x, y = INSET, INSET
  local w = 80
  local r = px.rng(seed)
  if stage == "rubble" then
    for k = 0, 9 do
      local px0 = x + k * 8 + r.int(0, 3)
      cv:hline(px0, y + 14 - r.int(0, 2), r.int(4, 7), c("wood", 2))
    end
    cv:outline(0, 0, cv.w, cv.h, INK)
    return
  end
  local fallen = stage == "destroyed" and 0.6 or (stage == "damaged" and 0.2 or 0)
  for k = 0, w / 8 do
    local px0 = x + k * 8
    if r.next() >= fallen then
      cv:vline(px0, y + 2, 14, c("wood", 4)); cv:vline(px0 + 1, y + 2, 14, c("wood", 2))
      cv:set(px0, y + 1, c("wood", 5))
    else
      cv:hline(px0 - 2, y + 15, 7, c("wood", 2))
    end
  end
  for _, ry in ipairs({ y + 5, y + 10 }) do
    for xx = x, x + w do
      local gone = stage ~= "intact" and px.noise(xx, ry, 6, seed) < fallen + 0.1
      if not gone then
        cv:set(xx, ry, c("wood", 4)); cv:set(xx, ry + 1, c("wood", 3))
      end
    end
  end
  if stage == "destroyed" then
    for k = 1, 10 do cv:set(r.int(x, x + w), r.int(y + 13, y + 16), c("wood", r.int(1, 3))) end
  end
  cv:outline(0, 0, cv.w, cv.h, INK)
end

local props = {
  { "well", 48, 48, well, 300 },
  { "barrels", 32, 32, barrels, 310 },
  { "fence", 96, 32, fence, 320 },
}
for _, p in ipairs(props) do
  local frames, tags = {}, {}
  for n, stage in ipairs(STAGES) do
    local cv = px.canvas(p[2], p[3])
    p[4](cv, stage, p[5] + n)
    frames[n] = cv
    tags[n] = { stage, n, n, 100 }
  end
  px.save_frames(p[2], p[3], frames, tags, here .. "/" .. p[1] .. ".aseprite")
end
