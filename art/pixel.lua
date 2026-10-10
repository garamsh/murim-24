-- Shared by the art scripts: the master palette, an indexed canvas, a seeded random source,
-- value noise and a small pixel font. Every pixel a canvas holds is a palette index, so every
-- image drawn with it stays inside the master palette.

local M = {}

-- Ramps run dark to light; shading moves along a ramp, never off it.
M.ramps = {
  ink = { "14101a", "2a2433", "3d3647", "524a5e" },
  stone = { "3b4250", "4a5361", "66707d", "8a94a0", "b4bcc4", "dfe3e6" },
  grass = { "1f3a2b", "2c4a32", "3b6138", "4f7a3f", "6b9447", "8fae5a" },
  leaf = { "142a22", "1e3328", "284536", "35593d", "4a7044" },
  earth = { "3a2c24", "4a3a2c", "6b523a", "8c6d4a", "b08d60", "d1b787" },
  wood = { "2e1f1a", "3b2a22", "5c3d2a", "7d5535", "9c7048" },
  water = { "1a2f40", "1f3a4d", "2b5770", "3d7a8f", "5fa3a8", "a6d4cf" },
  crimson = { "3d0d14", "5c1418", "9c1f24", "d63a2f", "f06a3c" },
  azure = { "152a55", "1f3d7a", "2f63b8", "5b9ae0", "a8d0f5" },
  amber = { "4a2e0c", "6b4310", "b07418", "e8a830", "f5d27a" },
  skin = { "5c3a28", "8a5a3c", "c48a62", "e8b98e" },
  cloth = { "6b6052", "8c7f68", "b4a68a", "d9cdb0" },
  robe = { "8a94a0", "c8ccd0", "f4f1e6" },
  fire = { "9c1f24", "f06a3c", "f5a33c", "fbe37a", "fff8dc" },
  jade = { "1f5c4a", "2f8c6e", "4fc49a", "a6f0cf" },
  gold = { "5c4418", "8c6a2a", "c99a3c", "f0d080" },
  paper = { "a8946c", "d9c9a3", "efe3c2" },
  lacquer = { "3d1418", "7a2a24", "a83a2c" },
  violet = { "2a1438", "46205e", "6a3290", "9a5cd0", "c99cf0" },
}

M.colours = {} -- index -> "rrggbb"
M.index = {} -- "ramp" .. n -> index
M.ramp_of = {} -- index -> { ramp name, position }
local by_hex = {}
local order = {
  "ink", "stone", "grass", "leaf", "earth", "wood", "water", "crimson", "azure", "amber",
  "skin", "cloth", "robe", "fire", "jade", "gold", "paper", "lacquer", "violet",
}
M.order = order
for _, name in ipairs(order) do
  for n, hex in ipairs(M.ramps[name]) do
    local i = by_hex[hex]
    if not i then
      M.colours[#M.colours + 1] = hex
      i = #M.colours
      by_hex[hex] = i
      M.ramp_of[i] = { name, n }
    end
    M.index[name .. n] = i
  end
end

-- c("grass", 3) or c("grass3") -> palette index.
function M.c(name, n)
  local i = M.index[n and (name .. n) or name]
  assert(i, "no colour " .. tostring(name) .. tostring(n or ""))
  return i
end

-- Moves an index along its ramp: negative darker, positive lighter, clamped at the ends.
function M.shift(i, steps)
  local r = M.ramp_of[i]
  if not r then return i end
  local ramp = M.ramps[r[1]]
  local n = math.max(1, math.min(#ramp, r[2] + steps))
  return M.index[r[1] .. n]
end

-- Seeded generator, so a script draws the same image on every run.
function M.rng(seed)
  local s = seed % 2147483647
  if s <= 0 then s = s + 2147483646 end
  local r = {}
  function r.next()
    s = (s * 48271) % 2147483647
    return (s - 1) / 2147483646
  end
  function r.int(a, b) return a + math.floor(r.next() * (b - a + 1)) end
  function r.pick(t) return t[r.int(1, #t)] end
  return r
end

function M.hash(x, y, seed)
  local h = (x * 374761393 + y * 668265263 + (seed or 0) * 2246822519) % 4294967296
  h = ((h ~ (h >> 13)) * 1274126177) % 4294967296
  h = h ~ (h >> 16)
  return (h % 1000003) / 1000003
end

function M.noise(x, y, scale, seed)
  local fx, fy = x / scale, y / scale
  local x0, y0 = math.floor(fx), math.floor(fy)
  local tx, ty = fx - x0, fy - y0
  tx = tx * tx * (3 - 2 * tx)
  ty = ty * ty * (3 - 2 * ty)
  local a = M.hash(x0, y0, seed)
  local b = M.hash(x0 + 1, y0, seed)
  local c = M.hash(x0, y0 + 1, seed)
  local d = M.hash(x0 + 1, y0 + 1, seed)
  return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty
end

-- Value noise that repeats every `period` pixels in x and y, so a 16-pixel tile drawn with a
-- period of 16 meets its neighbours without a seam.
function M.tile_noise(x, y, scale, period, seed)
  local cells = period // scale
  local fx, fy = (x % period) / scale, (y % period) / scale
  local x0, y0 = math.floor(fx), math.floor(fy)
  local tx, ty = fx - x0, fy - y0
  tx = tx * tx * (3 - 2 * tx)
  ty = ty * ty * (3 - 2 * ty)
  local function h(a, b) return M.hash(a % cells, b % cells, seed) end
  local a, b, c, d = h(x0, y0), h(x0 + 1, y0), h(x0, y0 + 1), h(x0 + 1, y0 + 1)
  return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty
end

-- 4x4 Bayer threshold in [0, 1), for ordered dithering between two indices.
local bayer = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 }
function M.bayer(x, y) return (bayer[(y % 4) * 4 + (x % 4) + 1] + 0.5) / 16 end

local Canvas = {}
Canvas.__index = Canvas

function M.canvas(w, h)
  local cv = setmetatable({ w = w, h = h, p = {} }, Canvas)
  return cv
end

function Canvas:get(x, y)
  if x < 0 or y < 0 or x >= self.w or y >= self.h then return 0 end
  return self.p[y * self.w + x] or 0
end

function Canvas:set(x, y, i)
  if not i or i == 0 then return end
  if x < 0 or y < 0 or x >= self.w or y >= self.h then return end
  self.p[y * self.w + x] = i
end

function Canvas:clear(x, y)
  if x < 0 or y < 0 or x >= self.w or y >= self.h then return end
  self.p[y * self.w + x] = nil
end

function Canvas:rect(x, y, w, h, i)
  for yy = y, y + h - 1 do
    for xx = x, x + w - 1 do self:set(xx, yy, i) end
  end
end

function Canvas:hline(x, y, w, i) self:rect(x, y, w, 1, i) end
function Canvas:vline(x, y, h, i) self:rect(x, y, 1, h, i) end

function Canvas:line(x0, y0, x1, y1, i)
  local dx, dy = math.abs(x1 - x0), -math.abs(y1 - y0)
  local sx, sy = x0 < x1 and 1 or -1, y0 < y1 and 1 or -1
  local err = dx + dy
  while true do
    self:set(x0, y0, i)
    if x0 == x1 and y0 == y1 then break end
    local e2 = 2 * err
    if e2 >= dy then err = err + dy; x0 = x0 + sx end
    if e2 <= dx then err = err + dx; y0 = y0 + sy end
  end
end

-- Calls fn(x, y, d) for every pixel of an ellipse, d being 0 at the centre and 1 at the rim.
function Canvas:each_ellipse(cx, cy, rx, ry, fn)
  for y = math.floor(cy - ry), math.ceil(cy + ry) do
    for x = math.floor(cx - rx), math.ceil(cx + rx) do
      local dx, dy = (x - cx) / rx, (y - cy) / ry
      local d = math.sqrt(dx * dx + dy * dy)
      if d <= 1 then fn(x, y, d) end
    end
  end
end

function Canvas:ellipse(cx, cy, rx, ry, i)
  self:each_ellipse(cx, cy, rx, ry, function(x, y) self:set(x, y, i) end)
end

-- Shades every set pixel inside a region: fn(x, y) returns how many ramp steps to move.
function Canvas:shade_at(x, y, steps)
  local i = self:get(x, y)
  if i ~= 0 then self.p[y * self.w + x] = M.shift(i, steps) end
end

function Canvas:shade_ellipse(cx, cy, rx, ry, steps, dither)
  self:each_ellipse(cx, cy, rx, ry, function(x, y, d)
    if not dither or d < 0.7 or M.bayer(x, y) > (d - 0.7) / 0.3 then
      self:shade_at(x, y, steps)
    end
  end)
end

-- Draws a sprite given as rows of characters; map turns a character into an index
-- (characters absent from map are transparent). flip mirrors it horizontally.
function Canvas:sprite(x, y, rows, map, flip)
  local w = 0
  for _, r in ipairs(rows) do w = math.max(w, #r) end
  for yy, r in ipairs(rows) do
    for xx = 1, #r do
      local i = map[r:sub(xx, xx)]
      if i then
        local px = flip and (x + w - xx) or (x + xx - 1)
        self:set(px, y + yy - 1, i)
      end
    end
  end
  return w, #rows
end

-- Draws another canvas onto this one at an offset.
function Canvas:stamp(other, ox, oy)
  for k, i in pairs(other.p) do
    local x, y = k % other.w, k // other.w
    self:set(ox + x, oy + y, i)
  end
end

-- Adds a one-pixel outline in index i around every set pixel inside the given box.
function Canvas:outline(x0, y0, w, h, i)
  local add = {}
  for y = y0 - 1, y0 + h do
    for x = x0 - 1, x0 + w do
      if self:get(x, y) == 0 then
        if self:get(x - 1, y) ~= 0 or self:get(x + 1, y) ~= 0 or self:get(x, y - 1) ~= 0
            or self:get(x, y + 1) ~= 0 then
          add[#add + 1] = { x, y }
        end
      end
    end
  end
  for _, p in ipairs(add) do self:set(p[1], p[2], i) end
end

-- Writes the canvas into an Aseprite RGB image.
function Canvas:to_image()
  local img = Image(self.w, self.h, ColorMode.RGB)
  local rgba = {}
  for i, hex in ipairs(M.colours) do
    rgba[i] = app.pixelColor.rgba(tonumber(hex:sub(1, 2), 16), tonumber(hex:sub(3, 4), 16),
      tonumber(hex:sub(5, 6), 16), 255)
  end
  for k, i in pairs(self.p) do
    img:drawPixel(k % self.w, k // self.w, rgba[i])
  end
  return img
end

-- A 5-pixel-high font of capitals, digits and a little punctuation; lower case draws as capitals.
local glyphs = {
  A = { ".#.", "#.#", "###", "#.#", "#.#" }, B = { "##.", "#.#", "##.", "#.#", "##." },
  C = { ".##", "#..", "#..", "#..", ".##" }, D = { "##.", "#.#", "#.#", "#.#", "##." },
  E = { "###", "#..", "##.", "#..", "###" }, F = { "###", "#..", "##.", "#..", "#.." },
  G = { ".##", "#..", "#.#", "#.#", ".##" }, H = { "#.#", "#.#", "###", "#.#", "#.#" },
  I = { "###", ".#.", ".#.", ".#.", "###" }, J = { "..#", "..#", "..#", "#.#", ".#." },
  K = { "#.#", "#.#", "##.", "#.#", "#.#" }, L = { "#..", "#..", "#..", "#..", "###" },
  M = { "#...#", "##.##", "#.#.#", "#...#", "#...#" }, N = { "#..#", "##.#", "#.##", "#..#", "#..#" },
  O = { ".#.", "#.#", "#.#", "#.#", ".#." }, P = { "##.", "#.#", "##.", "#..", "#.." },
  Q = { ".#.", "#.#", "#.#", "#.#", ".##" }, R = { "##.", "#.#", "##.", "#.#", "#.#" },
  S = { ".##", "#..", ".#.", "..#", "##." }, T = { "###", ".#.", ".#.", ".#.", ".#." },
  U = { "#.#", "#.#", "#.#", "#.#", "###" }, V = { "#.#", "#.#", "#.#", "#.#", ".#." },
  W = { "#...#", "#...#", "#.#.#", "##.##", "#...#" }, X = { "#.#", "#.#", ".#.", "#.#", "#.#" },
  Y = { "#.#", "#.#", ".#.", ".#.", ".#." }, Z = { "###", "..#", ".#.", "#..", "###" },
  ["0"] = { "###", "#.#", "#.#", "#.#", "###" }, ["1"] = { ".#", "##", ".#", ".#", ".#" },
  ["2"] = { "##.", "..#", ".#.", "#..", "###" }, ["3"] = { "##.", "..#", ".#.", "..#", "##." },
  ["4"] = { "#.#", "#.#", "###", "..#", "..#" }, ["5"] = { "###", "#..", "##.", "..#", "##." },
  ["6"] = { ".##", "#..", "###", "#.#", "###" }, ["7"] = { "###", "..#", ".#.", ".#.", ".#." },
  ["8"] = { "###", "#.#", "###", "#.#", "###" }, ["9"] = { "###", "#.#", "###", "..#", "##." },
  ["."] = { ".", ".", ".", ".", "#" }, [","] = { "..", "..", "..", ".#", "#." },
  [":"] = { ".", "#", ".", "#", "." }, ["-"] = { "...", "...", "###", "...", "..." },
  ["'"] = { "#", "#", ".", ".", "." }, ["/"] = { "..#", "..#", ".#.", "#..", "#.." },
  ["+"] = { "...", ".#.", "###", ".#.", "..." }, ["!"] = { "#", "#", "#", ".", "#" },
  ["x"] = { "...", "#.#", ".#.", "#.#", "..." }, ["="] = { "...", "###", "...", "###", "..." }, ["%"] = { "#.#", "..#", ".#.", "#..", "#.#" },
  ["("] = { ".#", "#.", "#.", "#.", ".#" }, [")"] = { "#.", ".#", ".#", ".#", "#." },
}

function M.text_width(s)
  local w = 0
  for ch in s:gmatch(".") do
    if ch == " " then
      w = w + 3
    else
      local g = glyphs[ch] or glyphs[ch:upper()]
      w = w + (g and #g[1] or 3) + 1
    end
  end
  return math.max(0, w - 1)
end

-- Draws text; shadow, when given, is drawn one pixel down-right first.
function Canvas:text(x, y, s, i, shadow)
  if shadow then self:text(x + 1, y + 1, s, shadow) end
  local cx = x
  for ch in s:gmatch(".") do
    if ch == " " then
      cx = cx + 3
    else
      local g = glyphs[ch] or glyphs[ch:upper()]
      if g then
        for yy, r in ipairs(g) do
          for xx = 1, #r do
            if r:sub(xx, xx) == "#" then self:set(cx + xx - 1, y + yy - 1, i) end
          end
        end
        cx = cx + #g[1] + 1
      else
        cx = cx + 4
      end
    end
  end
  return cx - x - 1
end

-- Puts every layer into a new sprite, bottom first, sets the master palette and saves it.
function M.save(w, h, layers, path)
  local spr = Sprite(w, h, ColorMode.RGB)
  local pal = Palette(#M.colours)
  for i, hex in ipairs(M.colours) do
    pal:setColor(i - 1, Color { r = tonumber(hex:sub(1, 2), 16), g = tonumber(hex:sub(3, 4), 16),
      b = tonumber(hex:sub(5, 6), 16), a = 255 })
  end
  spr:setPalette(pal)
  for n, l in ipairs(layers) do
    local layer = n == 1 and spr.layers[1] or spr:newLayer()
    layer.name = l[1]
    if n == 1 then spr:deleteCel(layer, 1) end
    spr:newCel(layer, 1, l[2]:to_image(), Point(0, 0))
  end
  spr:saveAs(path)
  spr:close()
end

-- Saves one layer of frames as a sprite: frames is a list of canvases, tags a list of
-- { name, first frame, last frame, duration in ms }.
function M.save_frames(w, h, frames, tags, path)
  local spr = Sprite(w, h, ColorMode.RGB)
  local pal = Palette(#M.colours)
  for i, hex in ipairs(M.colours) do
    pal:setColor(i - 1, Color { r = tonumber(hex:sub(1, 2), 16), g = tonumber(hex:sub(3, 4), 16),
      b = tonumber(hex:sub(5, 6), 16), a = 255 })
  end
  spr:setPalette(pal)
  local layer = spr.layers[1]
  for n, cv in ipairs(frames) do
    if n > 1 then spr:newEmptyFrame() end
    if n == 1 then spr:deleteCel(layer, 1) end
    spr:newCel(layer, n, cv:to_image(), Point(0, 0))
  end
  for _, t in ipairs(tags) do
    local tag = spr:newTag(t[2], t[3])
    tag.name = t[1]
    for f = t[2], t[3] do spr.frames[f].duration = t[4] / 1000 end
  end
  spr:saveAs(path)
  spr:close()
end

-- Reads every frame of an Aseprite file back into canvases, with its tags as
-- { name = { first, last } }. Pixels outside the master palette are an error.
function M.load_frames(path)
  local by_rgb = {}
  for i, hex in ipairs(M.colours) do by_rgb[tonumber(hex, 16)] = i end
  local spr = app.open(path)
  local frames, tags = {}, {}
  for n = 1, #spr.frames do
    local img = Image(spr.width, spr.height, ColorMode.RGB)
    img:drawSprite(spr, n)
    local cv = M.canvas(spr.width, spr.height)
    for it in img:pixels() do
      local v = it()
      if app.pixelColor.rgbaA(v) > 0 then
        local rgb = app.pixelColor.rgbaR(v) * 65536 + app.pixelColor.rgbaG(v) * 256 + app.pixelColor.rgbaB(v)
        cv:set(it.x, it.y, assert(by_rgb[rgb], path .. ": colour outside the palette"))
      end
    end
    frames[n] = cv
  end
  for _, t in ipairs(spr.tags) do tags[t.name] = { t.fromFrame.frameNumber, t.toFrame.frameNumber } end
  spr:close()
  return frames, tags
end

-- The directory holding the script that called this, so outputs land beside the scripts.
function M.here()
  local src = debug.getinfo(2, "S").source:sub(2)
  return src:match("^(.*)/[^/]*$") or "."
end

return M
