#!/bin/zsh
# Renderer mutations: name, file, perl substitution; runs grade_check and restores the file.
cd "$(dirname "$0")/../../.."
L=build/issue209/rain/logs
rmut() {
  cp "$2" /tmp/rmut.bak.$$
  perl -0pi -e "$3" "$2"
  if cmp -s "$2" /tmp/rmut.bak.$$; then echo "$1: MUTATION DID NOT APPLY"; cp /tmp/rmut.bak.$$ "$2"; return; fi
  [ -e /private/tmp/murim-quiet ] && { echo "quiet held: skipped $1"; cp /tmp/rmut.bak.$$ "$2"; return; }
  godot --path . --script res://grade/grade_check.gd > $L/rmut-$1.log 2>&1
  echo "$1: exit $?"; grep -E ": FAIL$|checks passed|out of frames" $L/rmut-$1.log | cut -c1-200
  cp /tmp/rmut.bak.$$ "$2"; rm -f /tmp/rmut.bak.$$
}
rmut no-streaks grade/grade.gdshader 's/g = mix\(g, RAIN_COLOUR \* gain \+ lift, RAIN_SHARE \* rain\);/g = g;/'
rmut still-streaks grade/grade.gdshader 's/cell.y - drift \* RAIN_SPEED/cell.y/'
rmut rain-everywhere grade/grade.gdshader 's/if \(along < RAIN_LENGTH\) \{/if (true) {/'
