#!/bin/zsh
# Each mutation: name, file, perl substitution, test. Restores the file after.
cd "$(dirname "$0")/../../.."
L=build/issue209/p2/logs/f
mut() {
  cp "$2" /tmp/mut.bak.$$
  perl -0pi -e "$3" "$2"
  if cmp -s "$2" /tmp/mut.bak.$$; then echo "$1: MUTATION DID NOT APPLY"; cp /tmp/mut.bak.$$ "$2"; return; fi
  godot --headless --path . --script res://run_tests.gd -- "$4" > $L/mut-$1.log 2>&1
  echo "$1: exit $? $(grep -cE ': FAIL$' $L/mut-$1.log) FAIL lines"
  cp /tmp/mut.bak.$$ "$2"; rm -f /tmp/mut.bak.$$
}
mut fog-at-low grade/grade.gd 's/var fog: float = 0.0 if low else _grade.fog/var fog: float = _grade.fog/' res://grade/grade_test.gd
mut clouds-at-low grade/grade.gd 's/var shade: float = 0.0 if low else _grade.clouds/var shade: float = _grade.clouds/' res://grade/grade_test.gd
mut no-fog-key grade/grade.gd 's/\["lamp", "fog", "clouds"\]/["lamp"]/' res://grade/grade_test.gd
mut no-view-origin grade/grade.gd 's/\t_view_origin = view.position\n//' res://grade/grade_test.gd
mut no-relief grade/grade.gd 's/\t_map_size = relief.size\n//' res://grade/grade_test.gd
mut clouds-not-reading-field grade/grade.gd 's/\tclouds.set_shader_parameter\("field", \$LightField.get_texture\(\)\)\n//' res://grade/grade_test.gd
mut field-not-drawn-for-fog grade/grade.gd 's/ or fog > 0.0 or shade > 0.0//' res://grade/grade_test.gd
mut shared-clouds grade/grade.gd 's/var clouds := ShaderMaterial.new\(\)/static var clouds := ShaderMaterial.new()/' res://grade/grade_test.gd
mut ground-unshaded world_art/ground.gd 's/\tground.use_parent_material = true\n//' res://world_art/ground_test.gd
mut strips-unshaded world_art/ground.gd 's/\t\tstrip.material = material\n//' res://world_art/ground_test.gd
mut trees-shaded world_art/ground.gd 's/(\t\tsprite.offset = -TREE_FOOT)/\t\tsprite.use_parent_material = true\n$1/' res://world_art/ground_test.gd
