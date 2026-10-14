#!/bin/zsh
# Renderer mutations: name, file, perl substitution; runs grade_check and restores the file.
cd "$(dirname "$0")/../../.."
L=build/issue209/r1/logs
rmut() {
  cp "$2" /tmp/rmut.bak.$$
  perl -0pi -e "$3" "$2"
  if cmp -s "$2" /tmp/rmut.bak.$$; then echo "$1: MUTATION DID NOT APPLY"; cp /tmp/rmut.bak.$$ "$2"; return; fi
  [ -e /private/tmp/murim-quiet ] && { echo "quiet held: skipped $1"; cp /tmp/rmut.bak.$$ "$2"; return; }
  godot --path . --script res://grade/grade_check.gd > $L/rmut-$1.log 2>&1
  echo "$1: exit $?"; grep -E ": FAIL$|checks passed|out of frames" $L/rmut-$1.log | cut -c1-200
  cp /tmp/rmut.bak.$$ "$2"; rm -f /tmp/rmut.bak.$$
}
rmut field-flipped grade/grade.gdshader 's/texelFetch\(light_field, ivec2\(cell\), 0\)/texelFetch(light_field, ivec2(int(cell.x), textureSize(light_field, 0).y - 1 - int(cell.y)), 0)/'
rmut field-at-screen-pixels grade/light_field.gdshader 's/vec2 at = FRAGCOORD.xy \* art_pixel;/vec2 at = FRAGCOORD.xy;/'
