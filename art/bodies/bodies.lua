-- Draws one body sprite per faction, <faction>.aseprite: 16x24 frames whose bottom 16 rows are
-- the body's cell, tagged idle (one frame) and walk (four frames: step, pass, other step,
-- pass; a passing frame rides a pixel higher). The body faces the viewer turned to the right;
-- the view mirrors it to face left.
-- Run: aseprite --batch --script art/bodies/bodies.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local S = dofile(here .. "/../sprites.lua")(px)

local W, H = 16, 24
-- The body canvas from sprites.lua puts the feet on its row 16; this lands them on the frame's
-- bottom row of the cell.
local TOP = 6

-- The body that stands for each faction: the same as the approved line-up's first column.
local factions = {
  orthodox = { faction = "orthodox", head = "topknot", weapon = "sword" },
  demonic = { faction = "demonic", head = "spiky", body = "tunic", weapon = "saber" },
  unorthodox = { faction = "unorthodox", head = "topknot", hat = "straw", body = "tunic", weapon = "knife" },
}

-- bob raises everything above the feet by a pixel and stretches the legs to fill the gap,
-- for the passing frames of the walk.
local function frame(spec, step, bob)
  local s = {}
  for k, v in pairs(spec) do s[k] = v end
  s.step = step
  local body = S.body(s)
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

for name, spec in pairs(factions) do
  local frames = {
    frame(spec),
    frame(spec, "left"), frame(spec, nil, true), frame(spec, "right"), frame(spec, nil, true),
  }
  px.save_frames(W, H, frames, { { "idle", 1, 1, 1000 }, { "walk", 2, 5, 150 } }, here .. "/" .. name .. ".aseprite")
end
