-- Shared by the mobile HUD mockups: the target screen's world without its mouse-and-keyboard
-- HUD, and the touch HUD drawn over it at the native 480x270 - the status, leader, rumour and
-- map panels moved off the thumbs' corners, a virtual joystick at the bottom left and round skill
-- buttons in an arc at the bottom right, after docs/art-direction.md §Effects and HUD. Every word
-- is checked against its room in Korean and in English before it is drawn.
-- Load: local touch = dofile(here .. "/touch.lua")(px, bdf, ui, here)

return function(px, bdf, ui, here)
  local c = px.c
  local M = {}

  M.W, M.H = 480, 270

  -- Where the player stands in the target screen: the middle of the ground under their feet.
  M.PLAYER = { x = 304, y = 154 }
  -- The ground is seen from above at the target screen's slant: a ring on it is this flat.
  M.SLANT = 0.72

  -- The skill buttons, the main one first: centre, drawn radius and the radius of the circle a
  -- touch lands in. The others sit on an arc 58 pixels round the main one, the fifth on its
  -- left arm, so that no two touch circles meet.
  M.SKILLS = {
    { x = 440, y = 226, r = 20, hit = 24, icon = "strike" },
    { x = 382, y = 226, r = 16, hit = 21, icon = "burst" },
    { x = 399, y = 185, r = 16, hit = 21, icon = "feather" },
    { x = 440, y = 168, r = 16, hit = 21, icon = "swirl" },
    { x = 330, y = 238, r = 14, hit = 21, icon = "gourd" },
  }
  -- The cancel zone, shown only while a skill is dragged, on the arc's upper arm.
  M.CANCEL = { x = 452, y = 116, r = 18, hit = 24 }
  -- The joystick at rest; a touch anywhere in ZONE puts it under the thumb.
  M.STICK = { x = 64, y = 212, r = 30, knob = 12 }
  M.ZONE = { x = 0, y = 140, w = 200, h = 130 }
  -- How far a skill button's drag reaches, and how far the aim reaches on the ground at that.
  M.PAD, M.RANGE = 36, 88

  -- The panels, as { x, y, w, h }.
  M.STATUS = { 4, 4, 168, 62 }
  M.LEADER = { 4, 70, 168, 18 }
  M.MENU = { { 4, 92, 42, 42 }, { 50, 92, 42, 42 }, { 96, 92, 42, 42 } }
  M.RUMOUR = { 176, 4, 166, 42 }
  M.MAP = { 346, 4, 84, 62 }
  M.PAUSE = { 434, 4, 42, 42 }

  -- Every word the touch HUD draws, in Korean and in English, with the room it is given.
  local words = {
    player = { "협객", "Wanderer", 60 },
    faction = { "정파", "Orthodox", 60 },
    hp = { "체력", "HP", 40 }, hp_value = { "184", "184", 40 },
    qi = { "내공", "QI", 40 }, qi_value = { "92", "92", 40 },
    art = { "경지", "Art", 40 }, art_value = { "검 3", "Sword 3", 60 },
    leader = { "수장", "Leader", 36 }, leader_value = { "남궁현", "Namgung Hyeon", 120 },
    rumour = { "소문", "Rumour", 60 },
    rumour_line = { "버들목 마을, 폐허가 되다", "Willow Ford village in ruins", 156 },
    rumour_older = { "흑아방 멸문", "Black Fang falls", 156 },
    season = { "봄 · 3년", "Spring, Y3", 74 },
    genealogy = { "족보", "Family", 36 }, sect = { "문파", "Sects", 36 }, duel = { "도전", "Duel", 36 },
    cancel = { "취소", "Cancel", 40 },
    self = { "자신에게", "On self", 50 },
    cooldown = { "3", "3", 12 },
  }
  M.words = words

  -- Loads Galmuri9 with the glyphs of `extra` and of every word above in both languages, and
  -- refuses a word that overruns its room in either.
  function M.font(extra)
    local texts = {}
    for _, w in pairs(words) do texts[#texts + 1] = w[1]; texts[#texts + 1] = w[2] end
    for _, t in ipairs(extra or {}) do texts[#texts + 1] = t end
    local font = bdf.load(here .. "/../../hud/fonts/Galmuri9.bdf", texts)
    for key, w in pairs(words) do
      for lang = 1, 2 do
        local reach = bdf.width(font, w[lang])
        assert(reach > 0 and reach <= w[3],
          string.format("%s (%s) reaches %d past %d", key, w[lang], reach, w[3]))
      end
    end
    M.f = font
    return font
  end

  -- The target screen's layers read back: the world, every layer but the HUD, and the HUD. What
  -- stands on the ground or flies over it is noted in M.covered, so marks on the ground pass
  -- under it.
  function M.world(path)
    local by_rgb = {}
    for i, hex in ipairs(px.colours) do by_rgb[tonumber(hex, 16)] = i end
    local spr = app.open(path)
    local world, hud = px.canvas(spr.width, spr.height), px.canvas(spr.width, spr.height)
    M.covered = px.canvas(spr.width, spr.height)
    for _, layer in ipairs(spr.layers) do
      local cel = layer:cel(1)
      if cel then
        local into = layer.name == "hud" and hud or world
        for it in cel.image:pixels() do
          local v = it()
          if app.pixelColor.rgbaA(v) > 0 then
            local rgb = app.pixelColor.rgbaR(v) * 65536 + app.pixelColor.rgbaG(v) * 256 + app.pixelColor.rgbaB(v)
            local x, y = it.x + cel.position.x, it.y + cel.position.y
            into:set(x, y, assert(by_rgb[rgb], path .. ": colour outside the palette"))
            if layer.name ~= "hud" and layer.name ~= "terrain" then M.covered:set(x, y, 1) end
          end
        end
      end
    end
    spr:close()
    return world, hud
  end

  -- Darkens the world under (x, y) by `steps` along its ramp, on the HUD's layer.
  local function shade(cv, world, x, y, steps)
    local i = cv:get(x, y)
    if i == 0 then i = world:get(x, y) end
    if i ~= 0 then cv:set(x, y, px.shift(i, steps)) end
  end
  M.shade = shade

  -- The icons, each a canvas: the production set's strike and burst, and the target screen's
  -- lightness skill, meditation and medicine.
  local icons = {}
  local skill_icons = px.load_frames(here .. "/frozen/icons/skill_icons.aseprite")
  icons.strike, icons.burst = skill_icons[1], skill_icons[2]
  icons.feather = px.canvas(20, 20)
  icons.feather:line(5, 15, 14, 4, c("paper", 3)); icons.feather:line(6, 13, 13, 4, c("jade", 3))
  icons.feather:line(7, 14, 15, 6, c("jade", 2))
  icons.swirl = px.canvas(20, 20)
  icons.swirl:each_ellipse(10, 10, 6, 6, function(x, y, d) if d > 0.7 then icons.swirl:set(x, y, c("azure", 4)) end end)
  icons.swirl:rect(9, 9, 3, 3, c("azure", 5)); icons.swirl:set(13, 5, c("azure", 5))
  icons.gourd = px.canvas(20, 20)
  icons.gourd:each_ellipse(10, 13, 4, 4, function(x, y) icons.gourd:set(x, y, c("amber", 4)) end)
  icons.gourd:each_ellipse(10, 7, 3, 2.5, function(x, y) icons.gourd:set(x, y, c("amber", 4)) end)
  icons.gourd:hline(9, 9, 3, c("lacquer", 3)); icons.gourd:rect(9, 3, 3, 2, c("wood", 3))

  -- A round button at (x, y) of radius r: an ink well in a gold rim lit from the upper left, or
  -- in `rim`'s ramp when given.
  local function disc(cv, x, y, r, rim)
    cv:each_ellipse(x, y, r + 1.5, r + 1.5, function(xx, yy) cv:set(xx, yy, c("ink", 1)) end)
    cv:each_ellipse(x, y, r + 0.5, r + 0.5, function(xx, yy, d)
      local lit = (xx - x) + (yy - y) < 0
      if d > (r - 2) / r then
        cv:set(xx, yy, rim and c(rim, lit and 4 or 3) or c("gold", lit and 3 or 2))
      elseif d > (r - 4) / r then
        cv:set(xx, yy, c("ink", 1))
      else
        cv:set(xx, yy, c("ink", 2))
      end
    end)
  end
  M.disc = disc

  -- Skill button `n`. `opts.cooldown` is { fraction left, seconds left }: the well is dimmed, that
  -- share of it darkened clockwise from the top, and the seconds stand over it outlined in ink. `opts.rim` lights the rim.
  function M.skill(cv, n, opts)
    opts = opts or {}
    local s = M.SKILLS[n]
    disc(cv, s.x, s.y, s.r, opts.rim)
    local icon = icons[s.icon]
    cv:stamp(icon, s.x - icon.w // 2, s.y - icon.h // 2)
    if opts.cooldown then
      local left = opts.cooldown[1]
      cv:each_ellipse(s.x, s.y, s.r - 2, s.r - 2, function(xx, yy)
        local a = math.atan(xx - s.x, s.y - yy) / (2 * math.pi)
        if a < 0 then a = a + 1 end
        cv:shade_at(xx, yy, a > 1 - left and -3 or -1)
      end)
      local t = opts.cooldown[2]
      local tx, ty = s.x - bdf.width(M.f, t) // 2, s.y - 7
      for oy = -1, 1 do
        for ox = -1, 1 do bdf.text(cv, M.f, tx + ox, ty + oy, t, c("ink", 1)) end
      end
      bdf.text(cv, M.f, tx, ty, t, c("paper", 3))
    end
  end

  -- The joystick's base with its knob pushed (dx, dy) from the middle; the knob is jade while a
  -- thumb holds it.
  function M.stick(cv, world, dx, dy)
    local st = M.STICK
    cv:each_ellipse(st.x, st.y, st.r, st.r, function(xx, yy, d)
      if d > (st.r - 1) / st.r then cv:set(xx, yy, c("ink", 1))
      elseif d > (st.r - 2) / st.r then cv:set(xx, yy, c("gold", 2))
      elseif (xx + yy) % 2 == 0 then shade(cv, world, xx, yy, -2) end
    end)
    -- A chevron at each of the four ways.
    local g = c("gold", 3)
    for k = 0, 2 do
      cv:hline(st.x - k, st.y - st.r + 5 + k, 2 * k + 1, g); cv:hline(st.x - k, st.y + st.r - 5 - k, 2 * k + 1, g)
      cv:vline(st.x - st.r + 5 + k, st.y - k, 2 * k + 1, g); cv:vline(st.x + st.r - 5 - k, st.y - k, 2 * k + 1, g)
    end
    local held = dx ~= 0 or dy ~= 0
    disc(cv, st.x + dx, st.y + dy, st.knob, held and "jade" or nil)
    cv:each_ellipse(st.x + dx - 3, st.y + dy - 3, 3, 3, function(xx, yy) cv:set(xx, yy, c("ink", 3)) end)
  end

  -- Draws a word from the table at (x, y) in `colour`.
  local function say(cv, key, x, y, colour, shadow)
    return bdf.text(cv, M.f, x, y, words[key][1], colour, shadow)
  end
  local function say_right(cv, key, right, y, colour)
    bdf.text(cv, M.f, right - bdf.width(M.f, words[key][1]), y, words[key][1], colour)
  end
  local function bar(cv, x, y, w, frac, ramp)
    cv:rect(x, y, w, 4, c("ink", 1))
    local fill = math.floor((w - 2) * frac)
    cv:rect(x + 1, y + 1, fill, 2, c(ramp, 3)); cv:hline(x + 1, y + 1, fill, c(ramp, 4))
    cv:rect(x + 1 + fill, y + 1, w - 2 - fill, 2, c("ink", 3))
  end
  local function copy(cv, from, x, y, w, h, to_x, to_y)
    for yy = 0, h - 1 do
      for xx = 0, w - 1 do cv:set(to_x + xx, to_y + yy, from:get(x + xx, y + yy)) end
    end
  end

  -- A square button in the HUD's frame with `sprite` over the word `key`.
  local function menu_button(cv, rect, key, sprite, map)
    local x, y, w = rect[1], rect[2], rect[3]
    ui.frame(cv, x, y, w, rect[4])
    local sw = #sprite[1]
    cv:sprite(x + (w - sw) // 2, y + 7, sprite, map)
    local t = words[key][1]
    bdf.text(cv, M.f, x + (w - bdf.width(M.f, t)) // 2, y + 24, t, c("paper", 3))
  end

  -- The panels and the menu buttons, with the target screen's portrait and map read from `old`,
  -- its HUD.
  function M.panels(cv, old)
    local s = M.STATUS
    ui.frame(cv, s[1], s[2], s[3], s[4])
    copy(cv, old, 7, 8, 34, 32, 7, 8)
    local x0 = 46
    local w = say(cv, "player", x0, 5, c("paper", 3), c("ink", 1))
    say(cv, "faction", x0 + w + 6, 5, c("stone", 4))
    say(cv, "hp", x0, 17, c("crimson", 4)); say_right(cv, "hp_value", 166, 17, c("paper", 3))
    bar(cv, x0, 29, 120, 0.72, "crimson")
    say(cv, "qi", x0, 31, c("azure", 4)); say_right(cv, "qi_value", 166, 31, c("paper", 3))
    bar(cv, x0, 43, 120, 0.55, "azure")
    say(cv, "art", x0, 50, c("gold", 3)); say_right(cv, "art_value", 166, 50, c("paper", 3))

    local l = M.LEADER
    ui.frame(cv, l[1], l[2], l[3], l[4])
    say(cv, "leader", l[1] + 5, l[2] + 2, c("gold", 3))
    say(cv, "leader_value", x0, l[2] + 2, c("paper", 3))

    local gold, paper, lacquer = c("gold", 3), c("paper", 3), c("lacquer", 3)
    -- Genealogy: a tree of three kin. Sects: a banner on its pole. Duel: crossed swords.
    menu_button(cv, M.MENU[1], "genealogy",
      { "...###...", "...#.#...", "...###...", "....#....", ".#######.", ".#.....#.", "###...###", "#.#...#.#", "###...###" },
      { ["#"] = gold })
    menu_button(cv, M.MENU[2], "sect",
      { "#........", "#######..", "#ppppp#..", "#ppppp##.", "#pp#pp#..", "#######..", "#........", "#........", "##......." },
      { ["#"] = gold, p = lacquer })
    menu_button(cv, M.MENU[3], "duel",
      { "#.......#", ".#.....#.", "..#...#..", "...#.#...", "....#....", "...#.#...", ".##...##.", ".#.....#.", "#.......#" },
      { ["#"] = paper })

    local r = M.RUMOUR
    ui.frame(cv, r[1], r[2], r[3], r[4])
    say(cv, "rumour", r[1] + 5, r[2] + 1, c("gold", 3))
    -- The two latest rumours, and a chevron saying a tap opens the panel to the older ones.
    cv:sprite(r[1] + r[3] - 12, r[2] + 6, { "#####", ".###.", "..#.." }, { ["#"] = c("stone", 4) })
    say(cv, "rumour_line", r[1] + 5, r[2] + 13, c("paper", 3))
    say(cv, "rumour_older", r[1] + 5, r[2] + 25, c("stone", 4))

    local m = M.MAP
    ui.frame(cv, m[1], m[2], m[3], m[4])
    copy(cv, old, 394, 6, 80, 46, m[1] + 2, m[2] + 2)
    say(cv, "season", m[1] + 5, m[2] + 47, c("paper", 3))

    local p = M.PAUSE
    ui.frame(cv, p[1], p[2], p[3], p[4])
    cv:rect(p[1] + 15, p[2] + 13, 4, 16, paper); cv:rect(p[1] + 23, p[2] + 13, 4, 16, paper)
  end

  -- The whole touch HUD at rest: panels, the joystick and every skill button; `opts` per skill
  -- goes to M.skill, and `skip` leaves a skill for the caller to draw.
  function M.hud(cv, world, old, opts, skip)
    M.panels(cv, old)
    M.stick(cv, world, 0, 0)
    for n = 1, #M.SKILLS do
      if n ~= skip then M.skill(cv, n, opts and opts[n]) end
    end
  end

  -- A ring on the ground round (x, y), `rad` across at its widest: every pixel of its rim in
  -- `colour`, or every other run of four when `dashed`, with a dither of `fill` inside.
  function M.ground_ring(cv, x, y, rad, colour, dashed, fill)
    cv:each_ellipse(x, y, rad, rad * M.SLANT, function(xx, yy, d)
      if M.covered:get(xx, yy) ~= 0 then return end
      if d > 1 - 1.6 / rad then
        local a = math.atan(yy - y, xx - x)
        if not dashed or math.floor((a + math.pi) * rad / 4) % 2 == 0 then cv:set(xx, yy, colour) end
      elseif fill and px.bayer(xx, yy) > 0.8 then
        cv:set(xx, yy, fill)
      end
    end)
  end

  -- A dotted line on the ground from (x0, y0) to (x1, y1), a dot every `step` pixels.
  function M.dots(cv, x0, y0, x1, y1, step, colour)
    local len = math.sqrt((x1 - x0) ^ 2 + (y1 - y0) ^ 2)
    for t = 0, len, step do
      local x = math.floor(x0 + (x1 - x0) * t / len + 0.5)
      local y = math.floor(y0 + (y1 - y0) * t / len + 0.5)
      if M.covered:get(x, y) == 0 then cv:rect(x, y, 2, 2, colour) end
    end
  end

  -- Where a skill button dragged (dx, dy) aims on the ground, the drag clamped to the pad.
  function M.aim(dx, dy)
    local len = math.sqrt(dx * dx + dy * dy)
    if len > M.PAD then dx, dy = dx * M.PAD / len, dy * M.PAD / len end
    return math.floor(M.PLAYER.x + dx / M.PAD * M.RANGE + 0.5),
      math.floor(M.PLAYER.y + dy / M.PAD * M.RANGE * M.SLANT + 0.5), dx, dy
  end

  -- The pad round a dragged skill button, its reach dashed, and the knob where the thumb is, or
  -- none when the thumb has left the pad. It passes under the other buttons, so it is drawn
  -- after them.
  function M.pad(cv, world, n, dx, dy, ramp)
    local s = M.SKILLS[n]
    cv:each_ellipse(s.x, s.y, M.PAD, M.PAD, function(xx, yy, d)
      if cv:get(xx, yy) ~= 0 then return end
      if d > 1 - 1.2 / M.PAD then
        local a = math.atan(yy - s.y, xx - s.x)
        if math.floor((a + math.pi) * M.PAD / 3) % 2 == 0 then cv:set(xx, yy, c(ramp, 4)) end
      elseif (xx + yy) % 2 == 0 then
        shade(cv, world, xx, yy, -2)
      end
    end)
    M.skill(cv, n, { rim = ramp })
    if dx then disc(cv, math.floor(s.x + dx + 0.5), math.floor(s.y + dy + 0.5), 6, ramp) end
  end

  -- The cancel zone; `hot` while the thumb is on it, and without its word when `bare`.
  function M.cancel(cv, hot, bare)
    local z = M.CANCEL
    disc(cv, z.x, z.y, z.r, "crimson")
    if hot then
      cv:each_ellipse(z.x, z.y, z.r - 2, z.r - 2, function(xx, yy) cv:set(xx, yy, c("crimson", 2)) end)
    end
    local x = hot and c("paper", 3) or c("crimson", 4)
    for k = -5, 5 do
      cv:rect(z.x + k, z.y + k, 2, 2, x); cv:rect(z.x + k, z.y - k - 1, 2, 2, x)
    end
    if not bare then M.slip(cv, "cancel", z.x, z.y + z.r + 3, hot and "crimson" or "ink") end
  end

  -- The word `key` on an ink slip centred on `x` with its top at `y`, ruled along its top in
  -- `ramp`.
  function M.slip(cv, key, x, y, ramp)
    local t = words[key][1]
    local w = bdf.width(M.f, t) + 8
    cv:rect(x - w // 2, y, w, 14, c("ink", 1)); cv:hline(x - w // 2, y, w, c(ramp, 3))
    bdf.text(cv, M.f, x - w // 2 + 4, y + 1, t, c("paper", 3))
  end

  -- Writes the mockup: the world and the touch HUD as two layers.
  function M.save(world, cv, name)
    px.save(M.W, M.H, { { "game", world }, { "touch hud", cv } }, here .. "/" .. name .. ".aseprite")
  end

  return M
end
