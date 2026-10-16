-- Draws contact_sheet.aseprite: every frame of the production set, read back from the .aseprite
-- sources, a row of frames under each sheet's name and tags. The title backdrop, which
-- art/mockup/title_screen.png shows, the application icon and the ground's ramps, which are not
-- drawn from the master palette, are left out. What the frames make assembled in the game is shown
-- by the in-game captures in each pull request's evidence. It is a review image; the game does not
-- load it.
-- Run: aseprite --batch --script art/contact_sheet/contact_sheet.lua (after the asset scripts)

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/../pixel.lua")
local c = px.c
local art = here .. "/.."

local W = 608
local GAP = 2
local SHEETS = {
  "terrain/trees", "terrain/detail", "bodies/orthodox", "bodies/demonic", "bodies/unorthodox", "effects/attack",
  "structures/house", "structures/hall", "structures/storehouse", "structures/manual_hall", "structures/granary",
  "structures/well", "structures/seat_gate", "structures/barrels", "structures/fence", "structures/temple", "structures/inn",
  "structures/training_ground", "structures/smithy", "structures/apothecary", "structures/town_wall_south",
  "structures/town_wall_north", "structures/town_wall_west", "structures/town_wall_east", "structures/town_corner_tower",
  "structures/town_gate_tower", "structures/town_steps", "structures/town_temple", "structures/town_market_stall",
  "structures/town_courtyard_house", "decorations/fields",
  "decorations/ground", "decorations/banner", "decorations/sect_banner", "markers/army_marker",
  "markers/player_marker", "markers/shadow", "icons/skill_icons", "icons/economy_icons", "icons/ware_icons",
}

-- A name as the 5-pixel capital font draws it: capitals, with a space for each slash and
-- underscore.
local function shown(s) return (s:upper():gsub("[/_]", " ")) end

-- Each sheet's frames and its tags in order of their first frame, laid out in rows of frames
-- that wrap at the sheet's width under a line of its name.
local laid = {}
local y = 20
for _, name in ipairs(SHEETS) do
  local frames, tags = px.load_frames(art .. "/" .. name .. ".aseprite")
  local names = {}
  for tag, range in pairs(tags) do names[#names + 1] = { range[1], tag } end
  table.sort(names, function(a, b) return a[1] < b[1] end)
  local words = {}
  for _, t in ipairs(names) do words[#words + 1] = shown(t[2]) end
  local entry = { name = name, frames = {}, y = y, words = table.concat(words, " ") }
  y = y + 10
  local x = 8
  local row = 0
  for _, f in ipairs(frames) do
    if x + f.w > W - 8 then
      x = 8
      y = y + row + GAP
      row = 0
    end
    entry.frames[#entry.frames + 1] = { f, x, y }
    x = x + f.w + GAP
    row = math.max(row, f.h)
  end
  y = y + row + 10
  laid[#laid + 1] = entry
end

local sheet = px.canvas(W, y)
sheet:rect(0, 0, W, y, c("ink", 2))
sheet:text(8, 6, "MURIM24  PRODUCTION SET  EVERY FRAME AT NATIVE SIZE", c("paper", 6))
for _, entry in ipairs(laid) do
  sheet:text(8, entry.y, shown(entry.name), c("paper", 6))
  sheet:text(8 + px.text_width(shown(entry.name)) + 8, entry.y, entry.words, c("stone", 4))
  for _, f in ipairs(entry.frames) do sheet:stamp(f[1], f[2], f[3]) end
end
px.save(W, y, { { "sheet", sheet } }, here .. "/contact_sheet.aseprite")
