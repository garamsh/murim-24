-- Assembles the frames angle.gd wrote into an animated GIF at 1x and 2x, a frame each FRAME_MS,
-- and a strip of every STRIDE-th frame in a grid of COLUMNS.
-- Run: aseprite --batch --script-param dir=<dir> --script sequence.lua
local dir = app.params.dir
local FRAME_MS, STRIDE, COLUMNS = 100, 15, 4
local paths = {}
for n = 0, 999 do
  local p = string.format("%s/frames/frame_%03d.png", dir, n)
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
spr:saveCopyAs(dir .. "/sequence.gif")
spr:resize(first.width * 2, first.height * 2)
spr:saveCopyAs(dir .. "/sequence_2x.gif")
spr:close()
local picks = {}
for n = 1, #paths, STRIDE do picks[#picks + 1] = paths[n] end
local rows = math.ceil(#picks / COLUMNS)
local strip = Image(first.width * COLUMNS, first.height * rows, ColorMode.RGB)
for k, p in ipairs(picks) do
  strip:drawImage(Image { fromFile = p }, Point(((k - 1) % COLUMNS) * first.width, ((k - 1) // COLUMNS) * first.height))
end
strip:saveAs(dir .. "/strip.png")
print(string.format("sequence: %d frames, %d in the strip", #paths, #picks))
