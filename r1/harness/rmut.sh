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
rmut old-pools grade/grade.gdshader 's/vec3 warm = mix\(vec3\(l\), c, OWN_HUE\) \* LAMPLIGHT;/vec3 warm = c * vec3(1.12, 0.9, 0.62);/'
rmut old-panes grade/grade.gdshader 's/distance\(c, PANE\) < 0.01/l < 0.15/'
