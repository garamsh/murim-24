-- Draws skill_icons.aseprite: 16x16 icons for the technique bar, tagged by the shape of skill
-- they stand for: strike, a blow at one body, drawn as a blade with an arc of sword qi; and burst, a blow
-- whose radius reaches the bodies round its target, drawn as an open palm in a ring of force.
-- Run: aseprite --batch --script art/icons/icons.lua

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c

local strike = px.canvas(16, 16)
-- The qi arc behind the blade, then the blade from hilt to point, lit on its upper edge.
for t = 0, 10 do
  local a = math.rad(200 + t * 9)
  strike:set(math.floor(7 + math.cos(a) * -6.5 + 0.5), math.floor(7 + math.sin(a) * -6.5 + 0.5), c("azure", 5))
  strike:set(math.floor(7 + math.cos(a) * -5.5 + 0.5), math.floor(7 + math.sin(a) * -5.5 + 0.5), c("azure", 4))
end
strike:line(4, 11, 12, 3, c("stone", 6)); strike:line(5, 11, 13, 3, c("stone", 4))
strike:line(2, 11, 5, 14, c("gold", 3)); strike:line(2, 13, 3, 14, c("wood", 3))
strike:outline(0, 0, 16, 16, c("ink", 1))

local burst = px.canvas(16, 16)
burst:each_ellipse(8, 8, 7.5, 7.5, function(x, y, d)
  if d > 0.82 then burst:set(x, y, c("crimson", 4)) elseif d > 0.7 and (x + y) % 2 == 0 then burst:set(x, y, c("crimson", 3)) end
end)
burst:sprite(4, 3, { ".#.#.#..", ".#.#.#..", "######..", "######.#", "#######.", ".#####..", ".####...", "..###..." },
  { ["#"] = c("skin", 4) })
burst:outline(3, 2, 10, 10, c("ink", 1))

px.save_frames(16, 16, { strike, burst }, { { "strike", 1, 1, 100 }, { "burst", 2, 2, 100 } },
  here .. "/skill_icons.aseprite")
