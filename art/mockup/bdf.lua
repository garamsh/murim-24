-- Draws text into a pixel.lua canvas from a BDF bitmap font, so that a mockup can carry Hangul:
-- the 5-pixel font in pixel.lua has Latin capitals and numerals only. Only the glyphs a mockup
-- asks for are read from the file.

local M = {}

-- Reads the glyphs of every character in `texts` from the BDF file at `path`. Returns a font:
-- { ascent, glyphs = { [codepoint] = { width, w, h, xoff, yoff, rows } } }.
function M.load(path, texts)
  local wanted = {}
  for _, text in ipairs(texts) do
    for _, code in utf8.codes(text) do wanted[code] = true end
  end
  local font = { ascent = 0, glyphs = {} }
  local glyph, code, reading
  for line in io.lines(path) do
    local key, rest = line:match("^(%S+)%s*(.*)$")
    if key == "FONT_ASCENT" then
      font.ascent = tonumber(rest)
    elseif key == "ENCODING" then
      code = tonumber(rest)
      glyph = wanted[code] and { rows = {} } or nil
    elseif glyph and key == "DWIDTH" then
      glyph.width = tonumber(rest:match("^(%-?%d+)"))
    elseif glyph and key == "BBX" then
      local w, h, x, y = rest:match("^(%-?%d+) (%-?%d+) (%-?%d+) (%-?%d+)")
      glyph.w, glyph.h, glyph.xoff, glyph.yoff = tonumber(w), tonumber(h), tonumber(x), tonumber(y)
    elseif glyph and key == "BITMAP" then
      reading = true
    elseif key == "ENDCHAR" then
      if glyph then font.glyphs[code] = glyph end
      glyph, reading = nil, false
    elseif reading and glyph then
      glyph.rows[#glyph.rows + 1] = tonumber(key, 16)
    end
  end
  return font
end

-- The advance of `text` in pixels.
function M.width(font, text)
  local w = 0
  for _, code in utf8.codes(text) do
    local glyph = font.glyphs[code]
    w = w + (glyph and glyph.width or 0)
  end
  return w
end

-- Draws `text` with its top at row `y` (the font's ascent above the baseline) from column `x`,
-- in palette index `colour`; `shadow`, when given, is drawn a pixel down and right first.
function M.text(cv, font, x, y, text, colour, shadow)
  if shadow then M.text(cv, font, x + 1, y + 1, text, shadow) end
  local baseline = y + font.ascent
  local cx = x
  for _, code in utf8.codes(text) do
    local glyph = font.glyphs[code]
    if glyph then
      local bytes = math.ceil(glyph.w / 8)
      local top = baseline - glyph.h - glyph.yoff
      for row, bits in ipairs(glyph.rows) do
        for col = 0, glyph.w - 1 do
          if (bits >> (bytes * 8 - 1 - col)) & 1 == 1 then
            cv:set(cx + glyph.xoff + col, top + row - 1, colour)
          end
        end
      end
      cx = cx + glyph.width
    end
  end
  return cx - x
end

return M
