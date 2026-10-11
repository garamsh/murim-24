-- Maps the master palette's former ramps, of three to six steps, onto pixel.lua's seven-step ramps
-- (ADR 0069), the first pass of the restyle before hand touch-up. Each former step goes to the new
-- step nearest it in lightness, keeping the steps in order and apart. The tool then
--   rewrites each literal step in the art scripts it is given, c("grass", 3) and c("grass3"),
--   lists every line naming a step it cannot rewrite, one computed or shifted, for a hand to fix,
--   and prints each former colour beside the one it maps to, for the colours the view names.
-- Run: aseprite --batch --script art/palette_map.lua --script-param files="<script> ..."
-- [--script-param write=1]; without write it only reports. It stops itself after 60 seconds of
-- wall-clock time.

local here = debug.getinfo(1, "S").source:sub(2):match("^(.*)/[^/]*$") or "."
local px = dofile(here .. "/pixel.lua")
local started = os.time()

local FORMER = {
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
  jade = { "1f5c4a", "2f8c6e", "4fc49a", "a6f0cf" },
  gold = { "5c4418", "8c6a2a", "c99a3c", "f0d080" },
  paper = { "a8946c", "d9c9a3", "efe3c2" },
  lacquer = { "3d1418", "7a2a24", "a83a2c" },
  violet = { "2a1438", "46205e", "6a3290", "9a5cd0", "c99cf0" },
}

-- CIE L* of an sRGB colour.
local function lightness(hex)
  local function linear(v)
    v = v / 255
    return v <= 0.04045 and v / 12.92 or ((v + 0.055) / 1.055) ^ 2.4
  end
  local r, g, b = tonumber(hex:sub(1, 2), 16), tonumber(hex:sub(3, 4), 16), tonumber(hex:sub(5, 6), 16)
  local y = 0.2126 * linear(r) + 0.7152 * linear(g) + 0.0722 * linear(b)
  return y > 0.008856 and 116 * y ^ (1 / 3) - 16 or 903.3 * y
end

-- The new step of each former step: increasing, and of least summed lightness error.
local function map_ramp(former, ramp)
  local m, n = #former, #ramp
  local best = {} -- best[k][j]: least error placing former steps 1..k with step k at j
  local from = {}
  for k = 1, m do
    best[k], from[k] = {}, {}
    for j = k, n - (m - k) do
      local err = math.abs(lightness(former[k]) - lightness(ramp[j]))
      if k == 1 then
        best[k][j] = err
      else
        for i = k - 1, j - 1 do
          if best[k - 1][i] and (not best[k][j] or best[k - 1][i] + err < best[k][j]) then
            best[k][j], from[k][j] = best[k - 1][i] + err, i
          end
        end
      end
    end
  end
  local j, least = nil, nil
  for i, e in pairs(best[m]) do if not least or e < least then j, least = i, e end end
  local steps = {}
  for k = m, 1, -1 do steps[k] = j; j = from[k][j] end
  return steps
end

local maps = {}
for name, former in pairs(FORMER) do maps[name] = map_ramp(former, px.ramps[name]) end

local params = app and app.params or {}
for _, name in ipairs(px.order) do
  if maps[name] then
    local parts = {}
    for k, j in ipairs(maps[name]) do
      parts[#parts + 1] = string.format("%d %s -> %d %s", k, FORMER[name][k], j, px.ramps[name][j])
    end
    print("palette_map: " .. name .. ": " .. table.concat(parts, ", "))
  end
end

local total, manual = 0, 0
for path in (params.files or ""):gmatch("%S+") do
  assert(os.time() - started < 60, "palette_map: past 60 seconds of wall-clock time, stopped")
  local f = assert(io.open(path, "r"))
  local text = f:read("a")
  f:close()
  local count = 0
  local function step(name, k)
    local m = maps[name]
    if not m then return nil end
    local j = m[tonumber(k)]
    assert(j, "palette_map: " .. path .. ": no former step " .. name .. k)
    count = count + 1
    return j
  end
  text = text:gsub('([^%w_.])c%("(%a+)", ?(%d+)%)', function(before, name, k)
    local j = step(name, k)
    return j and string.format('%sc("%s", %d)', before, name, j)
  end)
  text = text:gsub('([^%w_.])c%("(%a+)(%d+)"%)', function(before, name, k)
    local j = step(name, k)
    return j and string.format('%sc("%s%d")', before, name, j)
  end)
  local n = 0
  for line in text:gmatch("[^\n]*") do
    n = n + 1
    if line:find("shift%(") or line:find('[^%w_.]c%("%a+", ?[^%d)%s]') or line:find("[^%w_.]c%(%a") then
      manual = manual + 1
      print(string.format("palette_map: by hand: %s:%d: %s", path, n, line:gsub("^%s+", "")))
    end
  end
  total = total + count
  print(string.format("palette_map: %s: %d steps rewritten", path, count))
  if params.write == "1" then
    local out = assert(io.open(path, "w"))
    out:write(text)
    out:close()
  end
end
print(string.format("palette_map: %d steps rewritten, %d lines left for a hand%s", total, manual,
  params.write == "1" and "" or " (reported only: no write)"))
