-- Draws skill_icons.aseprite and economy_icons.aseprite, the HUD's 16x16 icons.
-- skill_icons.aseprite: icons for the technique bar, tagged by the shape of skill
-- they stand for: strike, a blow at one body, drawn as a blade with an arc of sword qi; and burst, a blow
-- whose radius reaches the bodies round its target, drawn as an open palm in a ring of force.
-- economy_icons.aseprite: icons for a faction's economy, tagged food (a bowl of rice), stock (a
-- tied sack of grain) and housing (a house under its slate roof).
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

local food = px.canvas(16, 16)
-- The bowl, lacquered red, with a mound of rice and chopsticks across it.
food:each_ellipse(8, 10, 6.5, 4, function(x, y, d)
  if y >= 10 then food:set(x, y, x < 6 and c("lacquer", 3) or c("lacquer", 2)) end
end)
food:hline(2, 10, 13, c("lacquer", 3))
food:each_ellipse(8, 9, 5, 3.5, function(x, y, d)
  if y <= 10 then food:set(x, y, (x + y) % 3 == 0 and c("robe", 2) or c("robe", 3)) end
end)
food:line(3, 3, 13, 7, c("wood", 4)); food:line(3, 4, 13, 8, c("wood", 3))
food:outline(0, 0, 16, 16, c("ink", 1))

local stock = px.canvas(16, 16)
-- A sack of grain, tied at the neck, grain at its mouth.
stock:each_ellipse(8, 10, 5.5, 5, function(x, y, d)
  stock:set(x, y, x < 7 and c("cloth", 4) or (d > 0.75 and c("cloth", 2) or c("cloth", 3)))
end)
stock:rect(6, 3, 5, 3, c("cloth", 3)); stock:hline(6, 3, 5, c("amber", 4)); stock:set(8, 2, c("amber", 5))
stock:hline(5, 6, 7, c("wood", 3))
stock:vline(8, 9, 4, c("cloth", 2))
stock:outline(0, 0, 16, 16, c("ink", 1))

local housing = px.canvas(16, 16)
-- A house: slate roof with upturned eaves, plaster wall, a door.
for y = 2, 7 do
  local inset = math.max(0, 6 - y)
  for x = 1 + inset, 14 - inset do housing:set(x, y, ((x % 3) == 0) and c("stone", 2) or (y < 4 and c("stone", 5) or c("stone", 4))) end
end
housing:set(0, 6, c("stone", 2)); housing:set(15, 6, c("stone", 2))
housing:rect(3, 8, 10, 6, c("paper", 2)); housing:vline(3, 8, 6, c("lacquer", 3)); housing:vline(12, 8, 6, c("lacquer", 3))
housing:rect(7, 10, 3, 4, c("wood", 2))
housing:hline(2, 14, 12, c("stone", 4))
housing:outline(0, 0, 16, 16, c("ink", 1))

px.save_frames(16, 16, { food, stock, housing }, { { "food", 1, 1, 100 }, { "stock", 2, 2, 100 }, { "housing", 3, 3, 100 } },
  here .. "/economy_icons.aseprite")
