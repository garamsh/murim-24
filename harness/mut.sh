#!/bin/zsh
# Each mutation: name, file, perl substitution, test. Restores the file after.
cd "$(dirname "$0")/../../.."
L=build/issue209/rain/logs
mut() {
  cp "$2" /tmp/mut.bak.$$
  perl -0pi -e "$3" "$2"
  if cmp -s "$2" /tmp/mut.bak.$$; then echo "$1: MUTATION DID NOT APPLY"; cp /tmp/mut.bak.$$ "$2"; return; fi
  godot --headless --path . --script res://run_tests.gd -- "$4" > $L/mut-$1.log 2>&1
  echo "$1: exit $? $(grep -cE ': FAIL$' $L/mut-$1.log) FAIL lines"
  cp /tmp/mut.bak.$$ "$2"; rm -f /tmp/mut.bak.$$
}
mut rain-at-low grade/grade.gd 's/var rains := raining and not low/var rains := raining/' res://grade/grade_test.gd
mut no-overcast grade/grade.gd 's/\tif rains:\n\t\tmaterial.set_shader_parameter\("gain"[^\n]*\n[^\n]*\n//' res://grade/grade_test.gd
mut clouds-in-rain grade/grade.gd 's/0.0 if low or rains else _grade.clouds/0.0 if low else _grade.clouds/' res://grade/grade_test.gd
mut rain-default-on hud/settings.gd 's/"rain": 0\}/"rain": 1}/' res://hud/settings_test.gd
mut rain-read-inverted hud/settings.gd 's/return chosen.rain == 1/return chosen.rain == 0/' res://hud/settings_test.gd
mut play-ignores-rain play/play.gd 's/\t_grade.raining = _hud.settings.rain\(\)\n//' res://play/play_test.gd
