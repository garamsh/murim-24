-- Marks of ruin shared by the structure scripts, each drawn onto a canvas from pixel.lua: a
-- hole through a roof, scorch, and a heap of rubble.

return function(px)
  local c = px.c
  local M = {}

  -- A hole through a roof showing rafters, its rim darkened.
  function M.hole(cv, hx, hy, hw, hh, r)
    for yy = hy, hy + hh - 1 do
      for xx = hx, hx + hw - 1 do
        local edge = (xx == hx or xx == hx + hw - 1 or yy == hy or yy == hy + hh - 1)
        if cv:get(xx, yy) ~= 0 and (not edge or r.next() > 0.45) then
          cv:set(xx, yy, ((xx - hx) % 3 == 1) and c("wood", 3) or c("ink", 1))
        end
      end
    end
  end

  -- Darkens what is drawn in a box where noise passes `level`: scorch.
  function M.scorch(cv, x, y, w, h, level, seed, steps)
    for yy = y, y + h - 1 do
      for xx = x, x + w - 1 do
        if cv:get(xx, yy) ~= 0 and px.noise(xx, yy, 4, seed) > level then cv:shade_at(xx, yy, steps or -2) end
      end
    end
  end

  -- A low heap of tile, stone and burnt beams over an ellipse, with weeds coming back.
  function M.heap(cv, cx, cy, rx, ry, seed, r)
    cv:each_ellipse(cx, cy, rx, ry, function(xx, yy, d)
      if d < 0.8 + px.noise(xx, yy, 3, seed) * 0.35 then
        local n = px.noise(xx, yy, 2, seed + 1)
        local col = n > 0.68 and c("stone", 4) or (n > 0.45 and c("stone", 3) or (n > 0.25 and c("wood", 1) or c("ink", 3)))
        if yy - cy < -ry * 0.4 and n > 0.45 then col = px.shift(col, 1) end
        cv:set(xx, yy, col)
      end
    end)
    for k = 1, 3 do
      local bx, by = r.int(math.floor(cx - rx + 2), math.floor(cx + rx - 10)), r.int(math.floor(cy - ry / 2), math.floor(cy + ry / 2))
      cv:line(bx, by, bx + r.int(6, 10), by + r.int(-2, 2), c("wood", 1))
    end
    for k = 1, 4 do
      local gx, gy = r.int(math.floor(cx - rx), math.floor(cx + rx)), r.int(math.floor(cy), math.floor(cy + ry))
      cv:set(gx, gy, c("grass", 4)); cv:set(gx + 1, gy - 1, c("grass", 5))
    end
  end

  return M
end
