-- Assembles the frames angle.gd wrote into an animated GIF and a strip of every STRIDE-th frame
-- in a grid of COLUMNS. Params: dir, the directory angle.gd wrote; set, frames (the village
-- sequence, 100 ms a frame, also at 2x) or level (the levelling skill, 67 ms a frame, also at
-- half size).
-- Run: aseprite --batch --script-param dir=<dir> --script-param set=<set> --script sequence.lua
local dir, set = app.params.dir, app.params.set or "frames"
local FRAME_MS = set == "level" and 1000 / 15 or 100
local STRIDE, COLUMNS = 15, 4
local name = set == "level" and "level" or "sequence"
local paths = {}
for n = 0, 999 do
  local p = string.format("%s/%s/frame_%03d.png", dir, set, n)
  if not app.fs.isFile(p) then break end
  paths[#paths + 1] = p
end
local first = Image { fromFile = paths[1] }
local spr = Sprite(first.width, first.height, ColorMode.RGB)
local layer = spr.layers[1]
for n, p in ipairs(paths) do
  if n > 1 then spr:newEmptyFrame() end
  spr:newCel(layer, n, Image { fromFile = p }, Point(0, 0))
  spr.frames[n].duration = FRAME_MS / 1000
end
spr:saveCopyAs(dir .. "/" .. name .. ".gif")
if set == "frames" then
  spr:resize(first.width * 2, first.height * 2)
  spr:saveCopyAs(dir .. "/" .. name .. "_2x.gif")
else
  spr:resize(first.width // 2, first.height // 2)
  spr:saveCopyAs(dir .. "/" .. name .. "_half.gif")
end
spr:close()
local picks = {}
for n = 1, #paths, STRIDE do picks[#picks + 1] = paths[n] end
local rows = math.ceil(#picks / COLUMNS)
local strip = Image(first.width * COLUMNS, first.height * rows, ColorMode.RGB)
for k, p in ipairs(picks) do
  strip:drawImage(Image { fromFile = p }, Point(((k - 1) % COLUMNS) * first.width, ((k - 1) // COLUMNS) * first.height))
end
strip:saveAs(dir .. "/" .. (set == "level" and "level_strip.png" or "strip.png"))
print(string.format("%s: %d frames, %d in the strip", name, #paths, #picks))
