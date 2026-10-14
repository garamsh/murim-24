#!/bin/zsh
# Each mutation: name, file, perl substitution, test. Restores the file after.
cd "$(dirname "$0")/../../.."
L=build/issue209/r1/logs
mut() {
  cp "$2" /tmp/mut.bak.$$
  perl -0pi -e "$3" "$2"
  if cmp -s "$2" /tmp/mut.bak.$$; then echo "$1: MUTATION DID NOT APPLY"; cp /tmp/mut.bak.$$ "$2"; return; fi
  godot --headless --path . --script res://run_tests.gd -- "$4" > $L/mut-$1.log 2>&1
  echo "$1: exit $? $(grep -cE ': FAIL$' $L/mut-$1.log) FAIL lines"
  cp /tmp/mut.bak.$$ "$2"; rm -f /tmp/mut.bak.$$
}
mut field-always grade/grade.gd 's/var drawn: bool = _light_rects.size\(\) > 0 and _grade.lamp > 0.0/var drawn: bool = true/' res://grade/grade_test.gd
mut field-never grade/grade.gd 's/var drawn: bool = _light_rects.size\(\) > 0 and _grade.lamp > 0.0/var drawn: bool = false/' res://grade/grade_test.gd
mut field-no-rects grade/grade.gd 's/\tfield.set_shader_parameter\("light_rects", _light_rects\)\n//' res://grade/grade_test.gd
