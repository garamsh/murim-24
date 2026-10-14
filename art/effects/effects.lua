-- Draws the effects of #184's attack motion over the uses the core records, after the mockup the PM
-- approved (art/mockup/attack_motion.lua): attack.aseprite, one strip of 32x20 frames, each effect
-- centred in its frame, tagged in world_art.gd's EFFECT_TAGS order
--   slash_orthodox, slash_demonic, slash_unorthodox (three each): a crescent swept ahead of a
--     striker facing right, in the faction's accent ramp, azure, violet and amber, a white-hot edge
--     first, then whole, then its tail dithered away,
--   dart (nine): a black-fang dart at each of eight headings from the right, a quarter turn apart
--     clockwise down the screen, then a streak,
--   burst_orthodox, burst_demonic, burst_unorthodox (three each): a ring of force at the angle,
--     growing, a white-hot core first, sparks round it,
--   spark (three): a white-hot star on a struck body,
--   mend (three): jade motes rising over a mending user,
--   torch (three): flames rising over a burning user, smoke above.
-- Run: aseprite --batch --script art/effects/effects.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c

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
px.save_frames(32, 20, frames, tags, here .. "/attack.aseprite")
