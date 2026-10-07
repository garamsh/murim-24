-- Shared by the HUD panel mockups: the dimmed game behind a panel, the panel's frame, a dialog
-- with its title bar and keys, a menu, a slip, a key's cap and a person's card, after
-- docs/art-direction.md §Effects and HUD.
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

  -- A word on a slip `w` wide at (x, y): jade when `on`, ink when not.
  function M.slip(cv, font, x, y, w, text, on)
    if on then
      cv:rect(x, y, w, 14, c("jade", 2)); cv:hline(x, y, w, c("jade", 3))
    else
      cv:rect(x, y, w, 14, c("ink", 3)); cv:hline(x, y + 13, w, c("ink", 4))
    end
    bdf.text(cv, font, x + (w - bdf.width(font, text)) // 2, y + 1, text, on and c("paper", 3) or c("stone", 4))
  end

  -- A key's cap at (x, y) with `text` on it; returns its width.
  function M.keycap(cv, font, x, y, text)
    local w = math.max(bdf.width(font, text) + 6, 13)
    cv:rect(x, y, w, 13, c("ink", 4)); cv:rect(x, y, w, 12, c("ink", 3))
    bdf.text(cv, font, x + (w - bdf.width(font, text)) // 2, y, text, c("paper", 3))
    return w
  end

  -- A panel in (x, y, w, h) with `title` and `close` in its title bar and `keys` along its foot.
  function M.dialog(cv, font, x, y, w, h, title, close, keys)
    M.frame(cv, x, y, w, h)
    cv:rect(x + 3, y + 3, w - 6, 16, c("ink", 1))
    cv:hline(x + 3, y + 19, w - 6, c("gold", 2))
    bdf.text(cv, font, x + 8, y + 4, title, c("paper", 3), c("ink", 1))
    bdf.text(cv, font, x + w - 8 - bdf.width(font, close), y + 4, close, c("stone", 4))
    cv:hline(x + 3, y + h - 20, w - 6, c("gold", 2))
    bdf.text(cv, font, x + (w - bdf.width(font, keys)) // 2, y + h - 17, keys, c("stone", 4))
  end

  -- A menu of `items` from (x, y), `w` wide, a row every 18 pixels. An item is { text, chosen,
  -- note }: the chosen row is lit and marked in jade, an item with a `note` is greyed with the
  -- note beside it, as an entry that cannot be taken yet.
  function M.menu(cv, font, x, y, w, items)
    for n, item in ipairs(items) do
      local ry = y + (n - 1) * 18
      if n > 1 then cv:hline(x, ry - 3, w, c("ink", 3)) end
      if item.chosen then
        cv:rect(x, ry - 2, w, 16, c("ink", 2))
        cv:vline(x, ry - 2, 16, c("jade", 3))
        for k = 0, 3 do cv:vline(x + 4 + k, ry + 2 + k, 7 - 2 * k, c("gold", 4)) end
      end
      local colour = item.note and c("stone", 3) or (item.chosen and c("gold", 4) or c("paper", 3))
      bdf.text(cv, font, x + 12, ry, item[1], colour)
      if item.note then
        bdf.text(cv, font, x + w - 4 - bdf.width(font, item.note), ry, item.note, c("stone", 3))
      end
    end
  end

  return M
end
