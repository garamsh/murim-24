#!/bin/zsh
# Renderer mutations: name, file, perl substitution; runs grade_check and restores the file.
cd "$(dirname "$0")/../../.."
L=build/issue209/p2/logs/f
rmut() {
  cp "$2" /tmp/rmut.bak.$$
  perl -0pi -e "$3" "$2"
  if cmp -s "$2" /tmp/rmut.bak.$$; then echo "$1: MUTATION DID NOT APPLY"; cp /tmp/rmut.bak.$$ "$2"; return; fi
  godot --path . --script res://grade/grade_check.gd > $L/rmut-$1.log 2>&1
  echo "$1: exit $?"; grep -E ": FAIL$|checks passed|out of frames" $L/rmut-$1.log | cut -c1-200
  cp /tmp/rmut.bak.$$ "$2"; rm -f /tmp/rmut.bak.$$
}
rmut clouds-read-fog-channel grade/clouds.gdshader 's/art_pixel\)\), 0\).a;/art_pixel)), 0).b;/'
