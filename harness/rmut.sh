#!/bin/zsh
# Renderer mutations: name, file, perl substitution; runs grade_check and restores the file.
cd "$(dirname "$0")/../../.."
L=build/issue209/3b/logs
rmut() {
  cp "$2" /tmp/rmut.bak.$$
  perl -0pi -e "$3" "$2"
  if cmp -s "$2" /tmp/rmut.bak.$$; then echo "$1: MUTATION DID NOT APPLY"; cp /tmp/rmut.bak.$$ "$2"; return; fi
  [ -e /private/tmp/murim-quiet ] && { echo "quiet held: skipped $1"; cp /tmp/rmut.bak.$$ "$2"; return; }
  godot --path . --script res://grade/grade_check.gd > $L/rmut-$1.log 2>&1
  echo "$1: exit $?"; grep -E ": FAIL$|checks passed|out of frames" $L/rmut-$1.log | cut -c1-200
  cp /tmp/rmut.bak.$$ "$2"; rm -f /tmp/rmut.bak.$$
}
rmut pass-ignores-offset grade/grade.gdshader 's/vec2 cell = floor\(\(FRAGCOORD.xy - grid_offset\) \/ art_pixel\);/vec2 cell = floor(FRAGCOORD.xy \/ art_pixel);/'
rmut field-ignores-offset grade/light_field.gdshader 's/vec2 at = FRAGCOORD.xy \* art_pixel \+ grid_offset;/vec2 at = FRAGCOORD.xy * art_pixel;/'
