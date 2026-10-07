-- Shared by the HUD panel mockups: the dimmed game behind a panel, the panel's frame and a
-- person's card, after docs/art-direction.md §Effects and HUD.
-- Load: local panel = dofile(here .. "/panel.lua")(px, bdf)

return function(px, bdf)
  local c = px.c
  local M = {}

  M.CARD_W, M.CARD_H = 66, 40

  -- The game behind a panel: the first frame of the target screen at `path`, three steps darker
  -- along each colour's ramp under a checker of ink, so the panel reads as the one thing open.
  function M.backdrop(path, w, h)
    local cv = px.canvas(w, h)
    for k, i in pairs(px.load_frames(path)[1].p) do
      local x, y = k % w, k // w
      cv.p[k] = (x + y) % 2 == 0 and c("ink", 1) or px.shift(i, -3)
    end
    return cv
  end

  -- A HUD panel: ink with a gold trim and lacquer corner studs.
  function M.frame(cv, x, y, w, h, fill)
    cv:rect(x, y, w, h, c("ink", 2))
    cv:rect(x + 1, y + 1, w - 2, h - 2, c("ink", 1))
    cv:rect(x + 2, y + 2, w - 4, h - 4, fill or c("ink", 2))
    cv:hline(x + 2, y, w - 4, c("gold", 3)); cv:hline(x + 2, y + h - 1, w - 4, c("gold", 2))
    cv:vline(x, y + 2, h - 4, c("gold", 2)); cv:vline(x + w - 1, y + 2, h - 4, c("gold", 2))
    for _, p in ipairs({ { x, y }, { x + w - 3, y }, { x, y + h - 3 }, { x + w - 3, y + h - 3 } }) do
      cv:rect(p[1], p[2], 3, 3, c("gold", 3)); cv:set(p[1] + 1, p[2] + 1, c("lacquer", 3))
    end
  end

  -- A person's card at (x, y): `body` (a canvas) on a slip of ground, `name` and `line` beside
  -- it, greyed when `dead`. A `ramp` frames the card in that ramp and sets `tag` over it.
  function M.card(cv, font, x, y, p)
    local W, H = M.CARD_W, M.CARD_H
    cv:rect(x, y, W, H, p.ramp and c(p.ramp, 3) or c("ink", 1))
    cv:rect(x + 1, y + 1, W - 2, H - 2, p.ramp and c(p.ramp, 2) or c("ink", 3))
    cv:rect(x + 2, y + 2, W - 4, H - 4, c("ink", 2))
    -- The body stands on a cell of ground, as it does in the world.
    cv:rect(x + 4, y + 20, 16, 16, c("grass", 3))
    cv:hline(x + 4, y + 20, 16, c("grass", 4))
    for k, i in pairs(p.body.p) do
      local bx, by = k % p.body.w, k // p.body.w
      cv:set(x + 4 + bx, y + 12 + by, p.dead and px.shift(i, -2) or i)
    end
    bdf.text(cv, font, x + 23, y + 6, p.name, p.dead and c("stone", 4) or c("paper", 3))
    bdf.text(cv, font, x + 23, y + 21, p.line, p.dead and c("stone", 3) or c("stone", 4))
    if p.tag then
      local w = bdf.width(font, p.tag) + 6
      cv:rect(x + W - w - 3, y - 5, w, 11, c(p.ramp, 2))
      bdf.text(cv, font, x + W - w, y - 5, p.tag, c("paper", 3))
    end
  end

  return M
end
