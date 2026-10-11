-- Shared by the art scripts: the body template and its faction palettes, the faction banner and
-- the aggregate-army marker. Each function draws onto a canvas from pixel.lua.

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
      R = c("robe", 7), M = c("robe", 4), D = c("robe", 3), A = c("azure", 4), P = c("stone", 3),
      F = c("ink", 2), X = c("azure", 4), x = c("azure", 2), Y = c("azure", 1),
    },
    demonic = {
      R = c("ink", 5), M = c("ink", 4), D = c("ink", 2), A = c("crimson", 5), P = c("ink", 2),
      F = c("ink", 1), X = c("crimson", 5), x = c("crimson", 4), Y = c("crimson", 2),
      E = c("crimson", 6),
    },
    unorthodox = {
      R = c("earth", 4), M = c("earth", 3), D = c("earth", 2), A = c("amber", 5), P = c("wood", 3),
      F = c("ink", 2), X = c("earth", 6), x = c("earth", 5), Y = c("amber", 4),
    },
    villager = {
      R = c("cloth", 4), M = c("cloth", 3), D = c("cloth", 2), A = c("wood", 4), P = c("cloth", 2),
      F = c("wood", 2), X = c("cloth", 6), x = c("cloth", 4), Y = c("wood", 3),
    },
  }
  local shared = {
    H = c("ink", 2), h = c("ink", 1), S = c("skin", 6), s = c("skin", 4), E = c("ink", 1),
    W = c("stone", 6), w = c("stone", 4),
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
      cv:vline(x, y - 7, 7, c("stone", 7)); cv:vline(x + 1, y - 6, 6, c("stone", 4))
      cv:hline(x - 1, y, 4, c("gold", 4))
    end,
    sword_strike = function(cv, x, y)
      for k = 0, 7 do cv:set(x + 1 + k, y - k // 2, c("stone", 7)); cv:set(x + 1 + k, y - k // 2 + 1, c("stone", 4)) end
      cv:vline(x, y - 1, 3, c("gold", 4))
    end,
    saber = function(cv, x, y)
      cv:vline(x, y - 6, 6, c("stone", 6)); cv:vline(x + 1, y - 7, 6, c("stone", 4))
      cv:set(x + 1, y - 8, c("stone", 6)); cv:hline(x - 1, y, 3, c("crimson", 4))
    end,
    saber_strike = function(cv, x, y)
      for k = 0, 6 do
        cv:set(x + 1 + k, y - 2 + (k * k) // 12, c("stone", 7))
        cv:set(x + 1 + k, y - 1 + (k * k) // 12, c("stone", 4))
      end
      cv:vline(x, y - 1, 3, c("crimson", 4))
    end,
    spear = function(cv, x, y)
      cv:vline(x, y - 9, 13, c("wood", 4)); cv:set(x, y - 10, c("stone", 7)); cv:set(x, y - 11, c("stone", 6))
      cv:set(x - 1, y - 9, c("crimson", 5))
    end,
    staff = function(cv, x, y) cv:vline(x, y - 9, 14, c("wood", 4)); cv:vline(x, y - 9, 2, c("wood", 5)) end,
    knife = function(cv, x, y) cv:vline(x, y - 3, 3, c("stone", 7)); cv:set(x, y, c("wood", 3)) end,
    knife_strike = function(cv, x, y)
      cv:hline(x + 1, y - 1, 4, c("stone", 7)); cv:set(x, y - 1, c("wood", 3))
    end,
    fan = function(cv, x, y)
      cv:hline(x, y - 2, 4, c("amber", 7)); cv:hline(x, y - 1, 4, c("amber", 5)); cv:set(x, y, c("wood", 3))
    end,
    torch = function(cv, x, y)
      cv:vline(x, y - 5, 7, c("wood", 3)); cv:rect(x - 1, y - 8, 3, 3, c("fire", 3))
      cv:set(x, y - 7, c("fire", 5)); cv:set(x, y - 9, c("fire", 4)); cv:set(x - 1, y - 8, c("fire", 2))
    end,
    palm = function(cv, x, y)
      cv:hline(x + 1, y - 1, 3, c("skin", 6)); cv:hline(x + 1, y, 3, c("skin", 4))
      cv:set(x + 4, y - 1, c("skin", 6))
    end,
    bow = function(cv, x, y)
      for k = -5, 5 do cv:set(x + 2 - (k * k) // 10, y + k, c("wood", 4)) end
      cv:vline(x - 1, y - 5, 11, c("paper", 4))
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

  -- A faction banner on a pole, base at (x, y).
  function S.banner(cv, x, y, ramp)
    cv:vline(x, y - 16, 17, c("wood", 4)); cv:vline(x + 1, y - 16, 17, c("wood", 2))
    cv:set(x, y - 17, c("gold", 6))
    for yy = 0, 9 do
      local w = 7 - (yy > 6 and (yy - 6) * 2 or 0)
      for xx = 0, w - 1 do
        local col = c(ramp, 4)
        if xx == 0 or yy == 0 then col = c(ramp, 5) end
        if xx >= w - 2 then col = c(ramp, 2) end
        cv:set(x + 2 + xx, y - 15 + yy + (xx // 4), col)
      end
    end
    -- A glyph-like mark, a stroke and a dot.
    cv:vline(x + 5, y - 13, 5, c("paper", 6)); cv:hline(x + 4, y - 11, 3, c("paper", 6))
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
        cv:set(bx + 2, by + 1, c("stone", 6))
      end
    end
    cv:outline(x, y - 3, w + 2, h + 4, INK)
    S.banner(cv, x + w // 2 - 4, y + 1, ramp)
    -- Bracket corners frame the block as one marker.
    local bc = c(ramp, 5)
    for _, p in ipairs({ { x - 3, y - 5, 1, 1 }, { x + w + 2, y - 5, -1, 1 }, { x - 3, y + h + 3, 1, -1 }, { x + w + 2, y + h + 3, -1, -1 } }) do
      cv:hline(math.min(p[1], p[1] + p[3] * 3), p[2], 4, bc)
      cv:vline(p[1], math.min(p[2], p[2] + p[4] * 3), 4, bc)
    end
  end

  return S
end
