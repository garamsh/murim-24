-- Shared by the art scripts: the body template and its faction palettes, the village
-- building in its four damage stages, terrain objects, effects and the aggregate-army marker.
-- Each function draws onto a canvas from pixel.lua.

return function(px)
  local c = px.c
  local S = {}
  local INK = c("ink", 1)

  -- Body parts are fills only; the outline is added around the finished silhouette, so a
  -- weapon or a hat changes the outline with it.
  local heads = {
    topknot = { "..HH..", ".HHHh.", "HHHHHh", "HSSSSh", "SESSES", "sSSSSs" },
    bun = { ".HHHh.", "HHHHHh", "HHHHHh", "HSSSSh", "SESSES", "sSSSSs" },
    bald = { "......", ".SSSs.", "SSSSSs", "SSSSSs", "SESSEs", "sSSSSs" },
    white = { "..WW..", ".WWWw.", "WWWWWw", "WSSSSw", "SESSES", "WWWWWw" },
    spiky = { "H.H.H.", "HHHHHh", "HHHHHh", "XXXXXx", "SESSES", "sSSSSs" },
    hood = { ".XXXx.", "XXXXXx", "XXXXXx", "XsssXx", "XEssEx", "XsssXx" },
    mask = { ".HHHh.", "HHHHHh", "HHHHHh", "HSSSSh", "SESSES", "XXXXXx" },
  }
  -- Hats wider than the head sit over it; rows count from the top of the head.
  local hats = {
    straw = { ".....X.....", "....XXx....", "...XXXXx...", "..XXXXXxx..", "XXXXXXXXXxx",
      ".YYYYYYYYY." },
  }
  local bodies = {
    robe = { ".MRRRM..", "MRRRRRMD", "RRAAAAMD", "SRRRRRMs", ".RRRRMD.", ".RRRMMD.", ".FF..FF." },
    tunic = { ".MRRRM..", "MRRRRRMD", "RRAAAAMD", "SRRRRRMs", ".PPPPPP.", ".PP..PP.", ".FF..FF." },
    big = { "..MRRRM...", ".MRRRRRMD.", "SRRRRRRRMs", "SSAAAAAAss", "SRRRRRRRMs", ".PPPP.PPP.",
      ".PPP..PPP.", ".FFF..FFF." },
    fat = { "..MRRRM...", ".MRRRRRMD.", "MRRRRRRRMD", "SRAAAAAAMs", "SRRRRRRRMs", ".RRRRRRMD.",
      ".FFF..FFF." },
    -- Mid-stride, for a running or striking body.
    run = { ".MRRRM..", "MRRRRRMD", "RRAAAAMD", "SRRRRRMs", ".RRRRMD.", "RRM..MD.", "FF....FF" },
  }

  -- What each template letter becomes, per faction. Faction reads from the accent (A, X)
  -- and the robe value; the hair and skin ramps are shared.
  S.schemes = {
    orthodox = {
      R = c("robe", 3), M = c("robe", 2), D = c("robe", 1), A = c("azure", 3), P = c("stone", 3),
      F = c("ink", 2), X = c("azure", 3), x = c("azure", 2), Y = c("azure", 1),
    },
    demonic = {
      R = c("ink", 4), M = c("ink", 3), D = c("ink", 2), A = c("crimson", 4), P = c("ink", 2),
      F = c("ink", 1), X = c("crimson", 4), x = c("crimson", 3), Y = c("crimson", 2),
      E = c("crimson", 5),
    },
    unorthodox = {
      R = c("earth", 4), M = c("earth", 3), D = c("earth", 2), A = c("amber", 4), P = c("wood", 3),
      F = c("ink", 2), X = c("earth", 6), x = c("earth", 5), Y = c("amber", 3),
    },
    villager = {
      R = c("cloth", 3), M = c("cloth", 2), D = c("cloth", 1), A = c("wood", 4), P = c("cloth", 1),
      F = c("wood", 2), X = c("cloth", 4), x = c("cloth", 3), Y = c("wood", 3),
    },
  }
  local shared = {
    H = c("ink", 2), h = c("ink", 1), S = c("skin", 4), s = c("skin", 3), E = c("ink", 1),
    W = c("stone", 5), w = c("stone", 4),
  }

  local function scheme(name, over)
    local m = {}
    for k, v in pairs(shared) do m[k] = v end
    for k, v in pairs(S.schemes[name]) do m[k] = v end
    for k, v in pairs(over or {}) do m[k] = v end
    return m
  end
  S.scheme = scheme

  local weapons = {
    sword = function(cv, x, y)
      cv:vline(x, y - 7, 7, c("stone", 6)); cv:vline(x + 1, y - 6, 6, c("stone", 4))
      cv:hline(x - 1, y, 4, c("gold", 3))
    end,
    sword_strike = function(cv, x, y)
      for k = 0, 7 do cv:set(x + 1 + k, y - k // 2, c("stone", 6)); cv:set(x + 1 + k, y - k // 2 + 1, c("stone", 4)) end
      cv:vline(x, y - 1, 3, c("gold", 3))
    end,
    saber = function(cv, x, y)
      cv:vline(x, y - 6, 6, c("stone", 5)); cv:vline(x + 1, y - 7, 6, c("stone", 4))
      cv:set(x + 1, y - 8, c("stone", 5)); cv:hline(x - 1, y, 3, c("crimson", 3))
    end,
    saber_strike = function(cv, x, y)
      for k = 0, 6 do
        cv:set(x + 1 + k, y - 2 + (k * k) // 12, c("stone", 6))
        cv:set(x + 1 + k, y - 1 + (k * k) // 12, c("stone", 4))
      end
      cv:vline(x, y - 1, 3, c("crimson", 3))
    end,
    spear = function(cv, x, y)
      cv:vline(x, y - 9, 13, c("wood", 4)); cv:set(x, y - 10, c("stone", 6)); cv:set(x, y - 11, c("stone", 5))
      cv:set(x - 1, y - 9, c("crimson", 4))
    end,
    staff = function(cv, x, y) cv:vline(x, y - 9, 14, c("wood", 4)); cv:vline(x, y - 9, 2, c("wood", 5)) end,
    knife = function(cv, x, y) cv:vline(x, y - 3, 3, c("stone", 6)); cv:set(x, y, c("wood", 3)) end,
    knife_strike = function(cv, x, y)
      cv:hline(x + 1, y - 1, 4, c("stone", 6)); cv:set(x, y - 1, c("wood", 3))
    end,
    fan = function(cv, x, y)
      cv:hline(x, y - 2, 4, c("amber", 5)); cv:hline(x, y - 1, 4, c("amber", 4)); cv:set(x, y, c("wood", 3))
    end,
    torch = function(cv, x, y)
      cv:vline(x, y - 5, 7, c("wood", 3)); cv:rect(x - 1, y - 8, 3, 3, c("fire", 3))
      cv:set(x, y - 7, c("fire", 5)); cv:set(x, y - 9, c("fire", 4)); cv:set(x - 1, y - 8, c("fire", 2))
    end,
    palm = function(cv, x, y)
      cv:hline(x + 1, y - 1, 3, c("skin", 4)); cv:hline(x + 1, y, 3, c("skin", 3))
      cv:set(x + 4, y - 1, c("skin", 4))
    end,
    bow = function(cv, x, y)
      for k = -5, 5 do cv:set(x + 2 - (k * k) // 10, y + k, c("wood", 4)) end
      cv:vline(x - 1, y - 5, 11, c("paper", 2))
    end,
  }

  -- Builds one body on a fresh 16x18 canvas, anchored so its feet sit on row 16.
  -- spec: faction, head, hat, body, weapon, over (letter overrides).
  function S.body(spec)
    local cv = px.canvas(16, 18)
    local map = scheme(spec.faction, spec.over)
    local b = bodies[spec.body or "robe"]
    -- A step lifts one foot into the row above: "left" or "right".
    if spec.step then
      b = { table.unpack(b) }
      local feet, above = b[#b], b[#b - 1]
      local mid = #feet // 2
      local new_feet, new_above = {}, {}
      for k = 1, #feet do
        local ch = feet:sub(k, k)
        local lifted = (spec.step == "left") == (k > mid)
        new_feet[k] = (lifted and ch == "F") and "." or ch
        new_above[k] = (lifted and ch == "F") and "F" or above:sub(k, k)
      end
      b[#b] = table.concat(new_feet)
      b[#b - 1] = table.concat(new_above)
    end
    local bw = #b[1]
    local bx = 8 - bw // 2
    local by = 17 - #b
    cv:sprite(bx, by, b, map)
    local hx = 8 - 3 + (bw > 8 and 0 or -1) + 1
    local hy = by - 6
    cv:sprite(hx, hy, heads[spec.head or "topknot"], map)
    if spec.hat then cv:sprite(hx - 3 + 1, hy - 3, hats[spec.hat], map) end
    if spec.weapon then weapons[spec.weapon](cv, bx + bw - 1, by + 3) end
    cv:outline(0, 0, 16, 18, INK)
    return cv
  end

  -- Stamps a body canvas at (x, y) = the cell's top-left, mirrored when flip is set, or lying
  -- on its side when down is set.
  function S.place(dst, body, x, y, flip, down)
    for k, i in pairs(body.p) do
      local bx, by = k % body.w, k // body.w
      if down then
        local rx = 17 - by
        dst:set(x + (flip and (17 - rx) or rx) - 1, y + bx - 1, i)
      else
        dst:set(x + (flip and (15 - bx) or bx), y + by - 2, i)
      end
    end
  end

  -- A soft contact shadow under a body, on the terrain canvas.
  function S.shadow(terrain, x, y, w)
    terrain:shade_ellipse(x + 8, y + 15, (w or 5), 1.6, -2)
  end

  -- The village house, 3x3 cells (48x48), or 5x3 (80x48) when wide, in one of four stages:
  -- intact, damaged, destroyed, rubble. Drawn on the structure canvas; its cast shadow and
  -- scorch go on the terrain canvas.
  function S.house(cv, terrain, x, y, stage, seed, wide)
    local r = px.rng(seed)
    local W = wide and 80 or 48
    local wall_y, wall_h = y + 26, 16
    local roof_bottom = wall_y + 3
    local ridge = y + 8
    local door_x = x + W // 2 - 4

    -- Cast shadow to the lower right, shorter as the building loses height.
    local reach = ({ intact = 0, damaged = 0, destroyed = 10, rubble = 24 })[stage]
    for yy = y + 6 + reach, y + 48 do
      for xx = x + 4, x + W + 4 do
        if xx > x + W - 3 or yy > y + 45 then terrain:shade_at(xx, yy, -2) end
      end
    end

    -- Stone plinth, shared by every stage.
    cv:rect(x + 1, y + 42, W - 2, 4, c("stone", 4))
    cv:hline(x + 1, y + 42, W - 2, c("stone", 5))
    cv:hline(x + 1, y + 45, W - 2, c("stone", 2))
    for xx = x + 5, x + W - 4, 8 do cv:vline(xx, y + 43, 2, c("stone", 3)) end

    local function wall_and_windows()
      cv:rect(x + 3, wall_y, W - 6, wall_h, c("paper", 2))
      cv:hline(x + 3, wall_y + wall_h - 2, W - 6, c("paper", 1))
      local step = wide and 19 or 21
      for xx = x + 3, x + W - 4, step do
        cv:vline(xx, wall_y, wall_h, c("lacquer", 3)); cv:vline(xx + 1, wall_y, wall_h, c("lacquer", 2))
      end
      cv:vline(x + W - 5, wall_y, wall_h, c("lacquer", 3)); cv:vline(x + W - 4, wall_y, wall_h, c("lacquer", 2))
      cv:rect(door_x, wall_y + 4, 8, wall_h - 4, c("wood", 2))
      cv:vline(door_x + 4, wall_y + 4, wall_h - 4, c("wood", 1))
      cv:rect(door_x - 1, wall_y + 3, 10, 1, c("wood", 3))
      cv:set(door_x + 2, wall_y + 9, c("gold", 3)); cv:set(door_x + 5, wall_y + 9, c("gold", 3))
      local function window(wx)
        cv:rect(wx, wall_y + 5, 7, 6, c("wood", 1))
        for k = 0, 6, 2 do cv:vline(wx + k, wall_y + 5, 6, c("wood", 4)) end
        cv:hline(wx, wall_y + 5, 7, c("wood", 4)); cv:hline(wx, wall_y + 8, 7, c("wood", 4))
        cv:hline(wx, wall_y + 10, 7, c("wood", 3))
      end
      window(x + 8); window(x + W - 15)
      if wide then window(x + 22); window(x + W - 29) end
    end

    local function roof()
      for yy = y, roof_bottom do
        local inset = yy < ridge and math.max(0, (ridge - yy) - 1) or 0
        local flare = (roof_bottom - yy < 3) and (3 - (roof_bottom - yy)) or 0
        for xx = x - 2 - flare + inset, x + W + 1 + flare - inset do
          local ch = (xx - x) % 3
          local col
          if yy < ridge then
            col = ch == 0 and c("stone", 3) or c("stone", 4)
          else
            col = ch == 0 and c("stone", 2) or (ch == 1 and c("stone", 4) or c("stone", 3))
            if (yy - ridge) % 5 == 4 and ch ~= 0 then col = px.shift(col, -1) end
            if roof_bottom - yy < 4 and ch ~= 1 then col = px.shift(col, -1) end
          end
          cv:set(xx, yy, col)
        end
      end
      -- Eave: round tile ends along the lower edge.
      for xx = x - 4, x + W + 3 do
        cv:set(xx, roof_bottom, ((xx - x) % 3 == 1) and c("stone", 5) or c("stone", 1))
      end
      cv:hline(x + 3, roof_bottom + 1, W - 6, c("paper", 1))
      -- Ridge with curled ends.
      cv:hline(x + 1, ridge - 1, W - 2, c("stone", 2)); cv:hline(x + 1, ridge, W - 2, c("ink", 2))
      cv:hline(x + 1, ridge + 1, W - 2, c("stone", 1))
      for _, s in ipairs({ { x, -1 }, { x + W - 1, 1 } }) do
        cv:set(s[1] + s[2], ridge - 1, c("ink", 2)); cv:set(s[1] + s[2] * 2, ridge - 2, c("ink", 2))
        cv:set(s[1] + s[2] * 2, ridge - 3, c("ink", 2)); cv:set(s[1] + s[2], ridge - 3, c("gold", 3))
      end
      -- Eave corners turn up.
      cv:set(x - 6, roof_bottom - 2, c("stone", 2)); cv:set(x - 5, roof_bottom - 1, c("stone", 2))
      cv:set(x + W + 5, roof_bottom - 2, c("stone", 2)); cv:set(x + W + 4, roof_bottom - 1, c("stone", 2))
    end

    if stage == "intact" or stage == "damaged" then
      wall_and_windows()
      roof()
    end

    if stage == "damaged" then
      -- Holes in the roof show rafters; scorch climbs the wall; the door is broken in.
      local function hole(hx, hy, hw, hh)
        for yy = hy, hy + hh - 1 do
          for xx = hx, hx + hw - 1 do
            local edge = (xx == hx or xx == hx + hw - 1 or yy == hy or yy == hy + hh - 1)
            if not edge or r.next() > 0.45 then
              cv:set(xx, yy, ((xx - hx) % 3 == 1) and c("wood", 3) or c("ink", 1))
            end
          end
          cv:shade_at(hx - 1, yy, -1); cv:shade_at(hx + hw, yy, -1)
        end
        for xx = hx - 1, hx + hw do cv:shade_at(xx, hy + hh, -1) end
      end
      hole(x + W - 19, ridge + 4, 10, 8)
      hole(x + 9, ridge + 11, 6, 4)
      for yy = wall_y, wall_y + wall_h - 1 do
        for xx = x + W - 22, x + W - 4 do
          if px.noise(xx, yy, 4, seed) > 0.5 then cv:shade_at(xx, yy, -2) end
        end
      end
      cv:line(x + 12, wall_y + 2, x + 14, wall_y + 7, c("paper", 1))
      cv:line(x + 14, wall_y + 7, x + 13, wall_y + 12, c("paper", 1))
      cv:rect(door_x, wall_y + 4, 4, wall_h - 4, c("ink", 1))
      cv:line(door_x + 4, wall_y + 4, door_x + 7, wall_y + 14, c("wood", 3))
      for k = 1, 9 do
        local tx, ty = r.int(x, x + W), r.int(y + 46, y + 49)
        cv:set(tx, ty, c("stone", r.int(2, 4)))
      end
    elseif stage == "destroyed" then
      -- The roof has fallen in: a dark, ragged interior, a slab of roof still hanging on the
      -- left, charred beams, and the front wall broken to stumps.
      for yy = y + 4, wall_y + 2 do
        for xx = x + 1, x + W - 2 do
          local edge = math.min(xx - x - 1, x + W - 2 - xx, yy - y - 4)
          if edge > px.noise(xx, yy, 3, seed) * 5 then
            local n = px.noise(xx, yy, 5, seed)
            cv:set(xx, yy, n > 0.62 and c("ink", 3) or (n > 0.35 and c("ink", 2) or c("wood", 1)))
          end
        end
      end
      for yy = y + 10, wall_y + 1 do
        for xx = x - 1, x + 20 do
          if xx - x < 22 - (yy - y - 10) * 0.9 - px.noise(xx, yy, 2, seed) * 4 then
            local ch = (xx - x) % 3
            cv:set(xx, yy, ch == 0 and c("stone", 1) or (ch == 1 and c("stone", 3) or c("stone", 2)))
          end
        end
      end
      for k = 0, 3 do
        local bx = x + 10 + k * (W - 16) // 3
        cv:line(bx, y + 6 + r.int(0, 4), bx + r.int(-5, 5), wall_y, c("wood", 2))
        cv:line(bx + 1, y + 6 + r.int(0, 4), bx + 1 + r.int(-4, 4), wall_y - 2, c("ink", 1))
      end
      cv:line(x + 4, y + 16, x + W - 6, y + 22, c("wood", 2))
      cv:line(x + 4, y + 17, x + W - 6, y + 23, c("ink", 1))
      for xx = x + 3, x + W - 4 do
        local hgt = math.floor(px.noise(xx, 0, 4, seed + 3) * 15)
        if xx >= door_x - 2 and xx <= door_x + 9 then hgt = 0 end
        for yy = wall_y + wall_h - hgt, wall_y + wall_h - 1 do
          local col = c("paper", 2)
          if px.noise(xx, yy, 3, seed + 7) > 0.48 then col = c("paper", 1) end
          if yy - (wall_y + wall_h - hgt) < 2 then col = c("ink", 3) end
          cv:set(xx, yy, col)
        end
      end
      for _, pxx in ipairs({ x + 3, x + W - 5, x + 24 }) do
        local top = wall_y - r.int(-4, 6)
        cv:rect(pxx, top, 2, wall_y + wall_h - top, c("ink", 2)); cv:vline(pxx, top, 3, c("wood", 1))
      end
      for k = 1, 16 do cv:set(r.int(door_x - 6, door_x + 12), r.int(y + 44, y + 49), c("ink", r.int(2, 4))) end
      terrain:shade_ellipse(x + W / 2, y + 47, W / 2, 4, -2, true)
    elseif stage == "rubble" then
      -- A low heap over the plinth: tiles, stone and burnt beams, with weeds coming back.
      local cx, cy, rx, ry = x + W / 2, y + 37, W / 2 - 2, 8
      cv:each_ellipse(cx, cy, rx, ry, function(xx, yy, d)
        if d < 0.8 + px.noise(xx, yy, 3, seed) * 0.35 then
          local n = px.noise(xx, yy, 2, seed + 1)
          local col = c("stone", 3)
          if n > 0.7 then col = c("stone", 4) elseif n > 0.5 then col = c("earth", 2)
          elseif n < 0.25 then col = c("ink", 3) end
          if yy - cy < -ry * 0.4 and n > 0.45 then col = px.shift(col, 1) end
          cv:set(xx, yy, col)
        end
      end)
      for k = 1, 5 do
        local bx, by = r.int(x + 4, x + W - 14), r.int(y + 30, y + 40)
        local len = r.int(7, 13)
        local dy = r.int(-3, 3)
        cv:line(bx, by, bx + len, by + dy, c("wood", 1)); cv:line(bx, by + 1, bx + len, by + dy + 1, c("ink", 2))
      end
      for k = 1, 14 do
        local tx, ty = r.int(x + 4, x + W - 6), r.int(y + 30, y + 43)
        cv:set(tx, ty, c("stone", 5)); cv:set(tx + 1, ty, c("stone", 3))
      end
      for k = 1, 5 do
        local gx, gy = r.int(x + 2, x + W - 4), r.int(y + 40, y + 45)
        cv:set(gx, gy, c("grass", 4)); cv:set(gx + 1, gy - 1, c("grass", 5)); cv:set(gx - 1, gy - 1, c("grass", 4))
      end
      cv:rect(x + 5, y + 31, 2, 9, c("ink", 2)); cv:set(x + 5, y + 31, c("wood", 1))
    end
    cv:outline(x - 7, y - 4, W + 14, 56, INK)
  end

  -- Flames over a burning roof, on the effects canvas; a few tongues of fire plus embers.
  function S.fire(cv, x, y, w, seed)
    local r = px.rng(seed)
    for k = 1, math.max(3, w // 6) do
      local fx = x + r.int(2, w - 4)
      local fh = r.int(7, 14)
      local fy = y + r.int(0, 10)
      for yy = 0, fh do
        local half = math.max(0, math.floor((fh - yy) / 3.2 + 0.5 * math.sin(yy + k)))
        for xx = -half, half do
          local t = yy / fh
          local col = c("fire", 1)
          local core = math.abs(xx) < half * 0.5
          if t < 0.35 then col = core and c("fire", 5) or c("fire", 4)
          elseif t < 0.65 then col = core and c("fire", 4) or c("fire", 3)
          elseif t < 0.9 then col = c("fire", 2) end
          cv:set(fx + xx, fy + fh - yy, col)
        end
      end
    end
    for k = 1, 10 do cv:set(x + r.int(0, w), y - r.int(2, 14), r.pick({ c("fire", 3), c("fire", 4) })) end
  end

  -- A smoke column of puffs drifting by drift pixels per pixel risen; puffs are lit from the
  -- upper left and thin out with height.
  function S.smoke(cv, x, y, h, drift, seed)
    local puffs = {}
    local t, k = 0, 0
    while t < h do
      local rad = 4 + t / h * 10
      puffs[#puffs + 1] = { x + t * drift + math.sin(k * 1.7) * 2, y - t, rad, t / h }
      t = t + rad * 0.6
      k = k + 1
    end
    for n = #puffs, 1, -1 do
      local p = puffs[n]
      cv:each_ellipse(p[1], p[2], p[3], p[3] * 0.85, function(xx, yy, d)
        if d > 0.8 and px.bayer(xx, yy) < (d - 0.8) / 0.2 then return end
        if p[4] > 0.6 and px.bayer(xx + 1, yy) < (p[4] - 0.6) * 2 then return end
        local l = (xx - p[1]) + (yy - p[2])
        local col = l < -p[3] * 0.4 and c("stone", 4) or (l < p[3] * 0.4 and c("stone", 3) or c("stone", 2))
        cv:set(xx, yy, col)
      end)
    end
  end

  -- A broadleaf tree, canopy radius rr, trunk foot at (x, y).
  function S.tree(cv, terrain, x, y, rr, seed)
    terrain:shade_ellipse(x + rr * 0.5, y + 1, rr * 1.1, rr * 0.5, -2, true)
    cv:rect(x - 1, y - 6, 3, 7, c("wood", 2)); cv:vline(x - 1, y - 6, 7, c("wood", 3))
    local cy = y - 6 - rr * 0.7
    cv:each_ellipse(x, cy, rr, rr * 0.85, function(xx, yy, d)
      local n = px.noise(xx, yy, 3, seed)
      local lx, ly = (xx - x) / rr, (yy - cy) / rr
      local light = -lx * 0.5 - ly * 0.7 + (n - 0.5) * 0.9
      local col = c("leaf", 3)
      if light > 0.55 then col = c("leaf", 5) elseif light > 0.15 then col = c("leaf", 4)
      elseif light < -0.45 then col = c("leaf", 2) end
      if d > 0.92 and n < 0.45 then return end
      cv:set(xx, yy, col)
    end)
    cv:outline(math.floor(x - rr - 1), math.floor(cy - rr - 1), math.ceil(rr * 2 + 3), math.ceil(rr * 2 + 10), c("leaf", 1))
  end

  -- A bamboo clump: stems with leaf sprays.
  function S.bamboo(cv, terrain, x, y, seed)
    local r = px.rng(seed)
    terrain:shade_ellipse(x + 6, y + 1, 9, 3, -1, true)
    for k = 1, 6 do
      local sx = x + r.int(-5, 7)
      local sh = r.int(18, 28)
      cv:vline(sx, y - sh, sh, c("jade", 2)); cv:vline(sx + 1, y - sh, sh, c("jade", 1))
      for yy = y - sh + 4, y - 2, 5 do cv:hline(sx, yy, 2, c("leaf", 2)) end
      for j = 1, 3 do
        local ly = y - sh + r.int(0, 10)
        local dir = r.pick({ -1, 1 })
        for t = 1, 4 do cv:set(sx + dir * t, ly + t // 2, c("grass", 5)); cv:set(sx + dir * t, ly + t // 2 + 1, c("grass", 4)) end
      end
    end
  end

  -- An area skill landing: a ring of force, a bright core and cracked ground.
  -- palette: a ramp name, dark to light.
  function S.area_skill(fx, terrain, cx, cy, rad, ramp, seed)
    local r = px.rng(seed)
    -- Cracks and scorched ground on the terrain.
    terrain:shade_ellipse(cx, cy, rad * 0.75, rad * 0.55, -1, true)
    for k = 1, 9 do
      local a = k / 9 * math.pi * 2 + r.next() * 0.4
      local len = rad * (0.5 + r.next() * 0.5)
      local x0, y0 = cx, cy
      for t = 1, math.floor(len) do
        local x1 = math.floor(cx + math.cos(a) * t + 0.5)
        local y1 = math.floor(cy + math.sin(a) * t * 0.72 + 0.5)
        terrain:shade_at(x1, y1, -3)
        x0, y0 = x1, y1
        a = a + (r.next() - 0.5) * 0.25
      end
    end
    local ramp_n = #px.ramps[ramp]
    -- Shock ring.
    fx:each_ellipse(cx, cy, rad, rad * 0.72, function(xx, yy, d)
      if d > 0.86 then
        fx:set(xx, yy, d > 0.95 and c(ramp, ramp_n - 1) or c(ramp, ramp_n))
      elseif d > 0.72 and px.bayer(xx, yy) > 0.5 then
        fx:set(xx, yy, c(ramp, ramp_n - 2))
      elseif d < 0.1 then
        fx:set(xx, yy, c("fire", 5))
      elseif d < 0.2 and px.bayer(xx, yy) > 0.3 then
        fx:set(xx, yy, c(ramp, ramp_n))
      elseif d < 0.3 and px.bayer(xx, yy) > 0.75 then
        fx:set(xx, yy, c(ramp, ramp_n - 1))
      end
    end)
    -- Rising sparks and radial streaks.
    for k = 1, 16 do
      local a = r.next() * math.pi * 2
      local d0 = rad * (0.35 + r.next() * 0.3)
      local x0, y0 = cx + math.cos(a) * d0, cy + math.sin(a) * d0 * 0.72
      local x1, y1 = cx + math.cos(a) * (d0 + 5), cy + math.sin(a) * (d0 + 5) * 0.72
      fx:line(math.floor(x0), math.floor(y0), math.floor(x1), math.floor(y1), c(ramp, ramp_n - 1))
    end
    for k = 1, 18 do
      local a = r.next() * math.pi * 2
      local d0 = rad * (1.05 + r.next() * 0.3)
      fx:set(math.floor(cx + math.cos(a) * d0), math.floor(cy + math.sin(a) * d0 * 0.72) - r.int(0, 4),
        r.pick({ c(ramp, ramp_n), c("fire", 4), c(ramp, ramp_n - 1) }))
    end
  end

  -- A sword-qi crescent: a bright arc from (x, y) of radius rad, opening toward dir (1 right, -1 left).
  function S.slash(fx, cx, cy, rad, dir, ramp)
    local n = #px.ramps[ramp]
    for t = -60, 60, 2 do
      local a = math.rad(t)
      for w = 0, 2 do
        local rr = rad - w
        local x = math.floor(cx + dir * math.cos(a) * rr + 0.5)
        local y = math.floor(cy + math.sin(a) * rr + 0.5)
        local fade = math.abs(t) / 60
        if w == 0 then fx:set(x, y, fade > 0.75 and c(ramp, n - 1) or c("robe", 3))
        elseif fade < 0.8 then fx:set(x, y, c(ramp, n - (w == 1 and 0 or 1))) end
      end
    end
  end

  -- An impact spark: a small star.
  function S.spark(fx, x, y, col)
    fx:set(x, y, c("fire", 5))
    fx:set(x - 1, y, col); fx:set(x + 1, y, col); fx:set(x, y - 1, col); fx:set(x, y + 1, col)
    fx:set(x - 2, y, col); fx:set(x + 2, y, col); fx:set(x, y - 2, col); fx:set(x, y + 2, col)
  end

  -- A faction banner on a pole, base at (x, y).
  function S.banner(cv, x, y, ramp)
    cv:vline(x, y - 16, 17, c("wood", 4)); cv:vline(x + 1, y - 16, 17, c("wood", 2))
    cv:set(x, y - 17, c("gold", 4))
    for yy = 0, 9 do
      local w = 7 - (yy > 6 and (yy - 6) * 2 or 0)
      for xx = 0, w - 1 do
        local col = c(ramp, 3)
        if xx == 0 or yy == 0 then col = c(ramp, 4) end
        if xx >= w - 2 then col = c(ramp, 2) end
        cv:set(x + 2 + xx, y - 15 + yy + (xx // 4), col)
      end
    end
    -- A glyph-like mark, a stroke and a dot.
    cv:vline(x + 5, y - 13, 5, c("paper", 3)); cv:hline(x + 4, y - 11, 3, c("paper", 3))
    cv:outline(x - 1, y - 18, 12, 20, INK)
  end

  -- The aggregate-army marker: one block standing for a whole army, packed with heads of
  -- the faction's colours, under a banner and a count plate. (x, y) is its top-left.
  function S.army(cv, terrain, x, y, cols, rows, faction, ramp, seed)
    local r = px.rng(seed)
    local map = scheme(faction)
    local w, h = cols * 4 + 4, rows * 5 + 4
    for yy = y - 2, y + h + 6 do
      for xx = x + 6, x + w + 6 do
        if px.noise(xx, yy, 3, seed) > 0.35 then terrain:shade_at(xx, yy, -1) end
      end
    end
    terrain:shade_ellipse(x + w / 2 + 3, y + h + 1, w / 2 + 2, 4, -2, true)
    for row = 0, rows - 1 do
      for col = 0, cols - 1 do
        local bx = x + 2 + col * 4 + (row % 2) * 2 + r.int(0, 1)
        local by = y + 2 + row * 5 + r.int(0, 1)
        if faction == "unorthodox" then
          cv:hline(bx - 1, by, 4, map.X); cv:hline(bx, by - 1, 2, map.x)
        else
          cv:rect(bx, by - 1, 2, 2, map.H)
        end
        cv:rect(bx, by + 1, 2, 2, map.R)
        cv:set(bx + 1, by + 2, map.A)
        cv:set(bx + 2, by + 1, c("stone", 5))
      end
    end
    cv:outline(x, y - 3, w + 2, h + 4, INK)
    S.banner(cv, x + w // 2 - 4, y + 1, ramp)
    -- Bracket corners frame the block as one marker.
    local bc = c(ramp, 4)
    for _, p in ipairs({ { x - 3, y - 5, 1, 1 }, { x + w + 2, y - 5, -1, 1 }, { x - 3, y + h + 3, 1, -1 }, { x + w + 2, y + h + 3, -1, -1 } }) do
      cv:hline(math.min(p[1], p[1] + p[3] * 3), p[2], 4, bc)
      cv:vline(p[1], math.min(p[2], p[2] + p[4] * 3), 4, bc)
    end
  end

  return S
end
