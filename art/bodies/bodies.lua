-- Draws one body sprite per faction at the 2.5D angle (ADR 0059), <faction>.aseprite: 16x24
-- frames, the feet on row 22, tagged
--   idle (one frame), walk (four: step, pass, other step, pass; a passing frame rides a pixel
--   higher), facing the viewer turned to the right,
--   back (four) walking away from the viewer, the head seen from behind,
--   strike (two): the weapon swung, then the standing frame,
--   idle_hurt and walk_hurt (one and four), the body below half its health: a blood stain on its
--   chest, stooping forward,
--   idle_maimed and walk_maimed (one and four), below a quarter: the stain spread down its side
--   and leg, stooping, and limping, the last step dragged,
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
local factions = {
  orthodox = { spec = { faction = "orthodox", head = "topknot", weapon = "sword" }, xray = c("azure", 5) },
  demonic = { spec = { faction = "demonic", head = "spiky", body = "tunic", weapon = "saber" }, xray = c("violet", 5) },
  unorthodox = { spec = { faction = "unorthodox", head = "topknot", hat = "straw", body = "tunic", weapon = "knife" },
    xray = c("amber", 5) },
}

local function copy(spec, over)
  local s = {}
  for k, v in pairs(spec) do s[k] = v end
  for k, v in pairs(over or {}) do s[k] = v end
  return s
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

-- Pixels of blood on the body, at offsets from (6, 16), the chest's left: on solid pixels only, so
-- the outline stays whole; `spread` 1 is a stain on the chest, 2 runs it down the side, the leg and
-- the arm.
local STAIN = {
  { { 0, 0, 4 }, { 1, 0, 2 }, { 0, 1, 2 }, { 1, 1, 4 }, { 2, 1, 2 } },
  { { 2, 2, 4 }, { 3, 2, 2 }, { 2, 3, 2 }, { 3, 4, 4 }, { 2, 5, 2 }, { -1, 2, 2 },
    { 3, 3, 4 }, { 2, 4, 4 }, { 3, 5, 2 }, { 3, 6, 4 }, { -1, 3, 4 }, { -2, 2, 2 }, { 4, 1, 4 } },
}
local function stained(cv, spread)
  local out = px.canvas(W, H)
  for k, i in pairs(cv.p) do out:set(k % W, k // W, i) end
  for n = 1, spread do
    for _, d in ipairs(STAIN[n]) do
      local x, y = 6 + d[1], 16 + d[2]
      local i = out:get(x, y)
      if i ~= 0 and i ~= c("ink", 1) then out:set(x, y, c("crimson", d[3])) end
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
    { "strike", { frame(copy(spec, { body = "run", weapon = spec.weapon .. "_strike" })), frame(spec) } },
  }
  local hurt = function(cv) return stooped(stained(cv, 1)) end
  local maimed = function(cv) return stooped(stained(cv, 2)) end
  sets[#sets + 1] = { "idle_hurt", { hurt(frame(spec)) } }
  sets[#sets + 1] = { "walk_hurt", { hurt(walk[1]), hurt(walk[2]), hurt(walk[3]), hurt(walk[4]) } }
  sets[#sets + 1] = { "idle_maimed", { maimed(frame(spec)) } }
  -- The limp: a step, a pass, then the hurt leg dragged as the body stands on the other, a pass.
  sets[#sets + 1] = { "walk_maimed",
    { maimed(walk[1]), maimed(walk[2]), maimed(frame(spec)), maimed(walk[4]) } }
  local frames, tags = {}, {}
  for _, suffix in ipairs({ "", "_xray" }) do
    for _, set in ipairs(sets) do
      local first = #frames + 1
      for _, cv in ipairs(set[2]) do frames[#frames + 1] = suffix == "" and cv or xray(cv, f.xray) end
      local ms = set[1]:sub(1, 4) == "idle" and 1000 or 150
      tags[#tags + 1] = { set[1] .. suffix, first, #frames, ms }
    end
  end
  px.save_frames(W, H, frames, tags, here .. "/" .. name .. ".aseprite")
end
