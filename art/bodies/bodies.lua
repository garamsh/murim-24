-- Draws one body sprite per faction at the 2.5D angle (ADR 0059), <faction>.aseprite: 16x24
-- frames, the feet on row 22, tagged
--   idle (one frame), walk (four: step, pass, other step, pass; a passing frame rides a pixel
--   higher), facing the viewer turned to the right,
--   back (four) walking away from the viewer, the head seen from behind,
--   strike (two): the weapon swung, then the standing frame,
-- and each again with _xray after it: only the silhouette's outline, in the faction's light
-- accent, which the view draws over whatever hides the body. The view mirrors a body to face left.
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
  orthodox = { spec = { faction = "orthodox", head = "topknot", weapon = "sword" }, xray = c("azure", 4) },
  demonic = { spec = { faction = "demonic", head = "spiky", body = "tunic", weapon = "saber" }, xray = c("crimson", 4) },
  unorthodox = { spec = { faction = "unorthodox", head = "topknot", hat = "straw", body = "tunic", weapon = "knife" },
    xray = c("amber", 4) },
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
  local skin = { [c("skin", 3)] = true, [c("skin", 4)] = true }
  for k, i in pairs(cv.p) do
    local x, y = k % W, k // W
    if y <= HEAD_BOTTOM and (skin[i] or (i == c("ink", 1) and not edge(cv, x, y))) then i = c("ink", 2) end
    out:set(x, y, i)
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
  local frames, tags = {}, {}
  for _, suffix in ipairs({ "", "_xray" }) do
    for _, set in ipairs(sets) do
      local first = #frames + 1
      for _, cv in ipairs(set[2]) do frames[#frames + 1] = suffix == "" and cv or xray(cv, f.xray) end
      tags[#tags + 1] = { set[1] .. suffix, first, #frames, set[1] == "idle" and 1000 or 150 }
    end
  end
  px.save_frames(W, H, frames, tags, here .. "/" .. name .. ".aseprite")
end
