-- Shared by the arts mockups: the player's qi, realm and arts as ADR 0035's region data gives
-- them, the five-slot loadout, every word in Korean and English with the room it is given, the
-- skills' icons, and the realm and qi block, after docs/concept.md §Qi and martial arts.
-- Load: local arts = dofile(here .. "/arts.lua")(px, bdf, ui, here)

return function(px, bdf, ui, here)
  local c = px.c
  local M = {}

  -- The player: first-rate, a store of 6,400 qi of which 3,520 is left, on the ladder of the
  -- region's eight realms.
  M.REALMS = {
    { "삼류", "Third-rate" }, { "이류", "Second-rate" }, { "일류", "First-rate" }, { "절정", "Peak" },
    { "초절정", "Beyond peak" }, { "화경", "Transformation" }, { "현경", "Mystery" },
    { "생사경", "Life and death" },
  }
  M.RANK, M.QI, M.QI_CAPACITY = 3, 3520, 6400

  -- The arts listed: the two the player knows a skill of, then its faction's skill. Each skill is
  -- { name, qi cost, the rank it needs, cooldown in seconds, icon, state }; a state is "known",
  -- "unlearned" (the realm is reached but the skill is not learned) or "locked" (the realm is
  -- not reached). Costs, ranks and cooldowns are region_data()'s. The first art is the heart
  -- method; the second is of another nature, so it strikes weaker.
  M.ARTS = {
    {
      name = { "매화검법", "Plum Blossom Sword" }, nature = { "정", "Orthodox" }, polarity = { "양강", "Yang" },
      ramp = "azure", heart = true,
      skills = {
        { { "매화일검", "Plum Blossom Strike" }, "100", 1, "3", "strike", "known" },
        { { "이십사수매화검법", "Twenty-Four Plum Blossoms" }, "400", 3, "10", "burst", "known" },
        { { "매화낙영", "Falling Plum Blossoms" }, "1,200", 5, "20", "blossom", "locked" },
      },
    },
    {
      name = { "흑아비도술", "Black Fang Darts" }, nature = { "사", "Unorthodox" }, polarity = { "음유", "Yin" },
      ramp = "amber", weaker = true,
      skills = {
        { { "흑아표", "Black Fang Dart" }, "100", 1, "3", "dart", "known" },
        { { "만천화우", "Rain of Flowers" }, "400", 3, "10", "rain", "unlearned" },
        { { "흑아폭우", "Black Fang Storm" }, "1,200", 5, "20", "storm", "locked" },
      },
    },
    {
      name = { "세력 기술", "Faction skills" }, faction = true,
      skills = {
        { { "기본기", "Basic strike" }, nil, nil, nil, "fist", "known" },
      },
    },
  }
  -- The loadout, slot by slot: { art, skill } or false for an empty slot. The selected skill,
  -- the Black Fang Dart, has just been put in the fifth.
  M.LOADOUT = { { 1, 1 }, { 1, 2 }, { 3, 1 }, false, { 2, 1 } }
  M.SELECTED = { 2, 1 }
  M.JUST = 5
  M.KEYS = { "Q", "W", "E", "R", "T" }

  -- Every word the mockups draw, in Korean and English, with the room it is given.
  local words = {
    title = { "무공", "Arts", 40 },
    close = { "K 닫기", "K close", 50 },
    realm = { "경지", "Realm", 40 },
    next = { "다음", "Next", 30 },
    qi = { "내공", "Qi", 40 },
    qi_value = { "3,520 / 6,400", "3,520 / 6,400", 70 },
    heart = { "심법", "Heart method", 70 },
    heart_tag = { "심법", "Heart", 30 },
    weaker = { "약화", "Weaker", 36 },
    unlearned = { "아직 못 익힘", "Not yet learned", 84 },
    locked = { "경지 부족", "Realm too low", 84 },
    col_skill = { "기술", "Skill", 40 },
    col_qi = { "내공", "Qi", 32 },
    col_realm = { "경지", "Realm", 78 },
    col_cooldown = { "재사용", "Recast", 40 },
    shared = { "공용", "Shared", 40 },
    none = { "-", "-", 32 },
    seconds = { "초", "s", 10 },
    loadout = { "단축키", "Loadout", 44 },
    keys = { "방향키 기술 고르기 · Q~T 배정 · Del 비우기 · K 닫기",
      "Arrows choose · Q-T assign · Del clear · K close", 440 },
    tap_slot = { "놓을 단추를 누른다", "Tap a button", 110 },
    touch_keys = { "기술을 누른 뒤 단추를 누른다", "Tap a skill, then a button", 290 },
    range = { "사거리", "Range", 40 }, range_value = { "8칸", "8 cells", 40 },
    area = { "범위", "Area", 40 }, area_value = { "한 명", "One", 40 },
    menu = { "무공", "Arts", 36 },
  }
  M.words = words

  -- The room each name is given: a skill's in the list and the detail, an art's in its header,
  -- a realm's in the list's column and beside the ladder.
  local NAME_ROOM, ART_ROOM, REALM_ROOM = 140, 100, 78

  -- Loads Galmuri9 with every word above and every name in both languages, plus `extra`, and
  -- refuses one that overruns its room in either.
  function M.font(extra)
    local texts, checks = {}, {}
    local function add(pair, room, key)
      texts[#texts + 1] = pair[1]; texts[#texts + 1] = pair[2]
      checks[#checks + 1] = { pair, room, key }
    end
    for key, w in pairs(words) do add(w, w[3], key) end
    for _, r in ipairs(M.REALMS) do add(r, REALM_ROOM, "realm") end
    for _, a in ipairs(M.ARTS) do
      add(a.name, ART_ROOM, "art")
      if a.nature then add(a.nature, 60, "nature"); add(a.polarity, 30, "polarity") end
      for _, s in ipairs(a.skills) do add(s[1], NAME_ROOM, "skill") end
    end
    for _, k in ipairs(M.KEYS) do texts[#texts + 1] = k end
    for _, t in ipairs(extra or {}) do texts[#texts + 1] = t end
    texts[#texts + 1] = "0123456789,/·"
    local font = bdf.load(here .. "/../../hud/fonts/Galmuri9.bdf", texts)
    for _, ch in ipairs(checks) do
      for lang = 1, 2 do
        local reach = bdf.width(font, ch[1][lang])
        assert(reach > 0 and reach <= ch[2],
          string.format("%s (%s) reaches %d past %d", ch[3], ch[1][lang], reach, ch[2]))
      end
    end
    M.f = font
    return font
  end

  function M.say(cv, key, x, y, colour, shadow)
    return bdf.text(cv, M.f, x, y, words[key][1], colour, shadow)
  end
  function M.say_right(cv, key, right, y, colour)
    local t = words[key][1]
    bdf.text(cv, M.f, right - bdf.width(M.f, t), y, t, colour)
  end
  function M.text_right(cv, t, right, y, colour)
    bdf.text(cv, M.f, right - bdf.width(M.f, t), y, t, colour)
  end

  -- A word on a filled slip in `ramp` at (x, y); returns the slip's width.
  function M.tag(cv, x, y, t, ramp)
    local w = bdf.width(M.f, t) + 6
    cv:rect(x, y, w, 11, c(ramp, 2))
    bdf.text(cv, M.f, x + 3, y, t, c("paper", 3))
    return w
  end

  -- A bar `w` wide at (x, y), `frac` of it filled along `ramp`, as the HUD draws its own.
  function M.bar(cv, x, y, w, frac, ramp)
    cv:rect(x, y, w, 5, c("ink", 1))
    local fill = math.floor((w - 2) * frac)
    cv:rect(x + 1, y + 1, fill, 3, c(ramp, 3)); cv:hline(x + 1, y + 1, fill, c(ramp, 4))
    cv:rect(x + 1 + fill, y + 1, w - 2 - fill, 3, c("ink", 3))
  end

  -- The realm and qi block in a column `w` wide from (x, y): the realm by name, a pip for each
  -- of the eight realms - gold for one passed, jade for the one held, ink for one ahead - the
  -- next realm, then the qi left against the store and its bar. Returns the y below it.
  function M.status(cv, x, y, w)
    M.say(cv, "realm", x, y, c("gold", 3))
    M.text_right(cv, M.REALMS[M.RANK][1], x + w, y, c("paper", 3))
    local pip = (w + 2) // #M.REALMS
    for n = 1, #M.REALMS do
      local px0 = x + (n - 1) * pip
      local ramp = n < M.RANK and "gold" or (n == M.RANK and "jade" or nil)
      if ramp then
        cv:rect(px0, y + 15, pip - 2, 5, c(ramp, 2)); cv:hline(px0, y + 15, pip - 2, c(ramp, 4))
      else
        cv:rect(px0, y + 15, pip - 2, 5, c("ink", 3)); cv:hline(px0, y + 19, pip - 2, c("ink", 4))
      end
    end
    M.say(cv, "next", x, y + 24, c("stone", 4))
    M.text_right(cv, M.REALMS[M.RANK + 1][1] .. " 10,000", x + w, y + 24, c("stone", 4))
    cv:hline(x, y + 40, w, c("ink", 3))
    M.say(cv, "qi", x, y + 44, c("azure", 4))
    M.say_right(cv, "qi_value", x + w, y + 44, c("paper", 3))
    M.bar(cv, x, y + 58, w, M.QI / M.QI_CAPACITY, "azure")
    return y + 64
  end

  -- The icons, 16 x 16 each: the production set's strike and burst, and the rest drawn here.
  local icons = {}
  local skill_icons = px.load_frames(here .. "/frozen/icons/skill_icons.aseprite")
  icons.strike, icons.burst = skill_icons[1], skill_icons[2]
  local paper, jade, lac, amb, stone = c("paper", 3), c("jade", 3), c("lacquer", 3), c("amber", 4), c("stone", 5)
  local function sprite(rows, map)
    local cv = px.canvas(16, 16)
    cv:sprite(0, 0, rows, map)
    return cv
  end
  -- Petals falling past a blade.
  icons.blossom = sprite({
    "................", "............##..", "...........##...", "..p.......##....", ".ppp.....##.....",
    "..p.....##......", ".......##...p...", "......##...ppp..", ".....##.....p...", "....##..........",
    "...##...p.......", "..##...ppp......", ".#......p.......", "................", "................",
    "................",
  }, { ["#"] = paper, p = lac })
  -- A dart: a steel head and shaft, a red tassel at its tail.
  icons.dart = sprite({
    "................", "...........hhhh.", "...........hhhh.", "..........hhhh..", ".........shhh....",
    "........ss.h....", ".......ss.......", "......ss........", ".....ss.........", "....ss..........",
    "...ss...........", "..tt............", ".ttt............", ".tt.............", "t.t.............",
    "................",
  }, { h = c("stone", 6), s = c("stone", 4), t = lac })
  -- Darts falling as rain.
  icons.rain = sprite({
    "................", "..#.....#....#..", "..#.....#....#..", "..k.....k....k..", ".....#.....#....",
    ".....#.....#....", ".....k.....k....", "..#.....#....#..", "..#.....#....#..", "..k.....k....k..",
    ".....#.....#....", ".....#.....#....", ".....k.....k....", "................", "................",
    "................",
  }, { ["#"] = stone, k = amb })
  -- A burst of darts from a point.
  icons.storm = sprite({
    "................", ".#......#......#", "..#.....#.....#.", "...#....#....#..", "....k...k...k...",
    "................", "..#k...aa...k#..", "......aaaa......", "..#k...aa...k#..", "................",
    "....k...k...k...", "...#....#....#..", "..#.....#.....#.", ".#......#......#", "................",
    "................",
  }, { ["#"] = stone, k = amb, a = c("amber", 5) })
  -- A clenched fist.
  icons.fist = sprite({
    "................", "................", "................", "....######......", "...#ssssss#.....",
    "...#s#s#s#s#....", "...#ssssssss#...", "...#ssssssss#...", "....#sssssss#...", "....#ssssss#....",
    ".....#ssss#.....", ".....#bbbb#.....", ".....#bbbb#.....", "......####......", "................",
    "................",
  }, { ["#"] = c("ink", 1), s = c("skin", 4), b = jade })
  M.icons = icons

  -- A skill's icon at (x, y), dimmed three steps when it cannot be used yet.
  function M.icon(cv, name, x, y, dim)
    local icon = icons[name]
    for k, i in pairs(icon.p) do
      cv:set(x + k % icon.w, y + k // icon.w, dim and px.shift(i, -3) or i)
    end
  end

  -- The skill an { art, skill } pair names.
  function M.skill(ref) return M.ARTS[ref[1]].skills[ref[2]] end

  -- A square tile `size` across at (x, y) holding a skill's icon: an ink well in an ink rim, the
  -- rim jade when `selected`, gold when `lit`.
  function M.tile(cv, x, y, size, icon, opts)
    opts = opts or {}
    local rim = opts.selected and c("jade", 3) or (opts.lit and c("gold", 3) or c("ink", 4))
    cv:rect(x, y, size, size, c("ink", 1))
    cv:rect(x + 1, y + 1, size - 2, size - 2, rim)
    cv:rect(x + 2, y + 2, size - 4, size - 4, opts.selected and c("ink", 3) or c("ink", 2))
    if icon then M.icon(cv, icon, x + (size - 16) // 2, y + (size - 16) // 2, opts.dim) end
  end

  return M
end
