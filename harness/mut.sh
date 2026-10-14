#!/bin/zsh
# Each mutation: name, file, perl substitution, test. Restores the file after.
cd "$(dirname "$0")/../../.."
L=build/issue209/3b/logs
mut() {
  cp "$2" /tmp/mut.bak.$$
  perl -0pi -e "$3" "$2"
  if cmp -s "$2" /tmp/mut.bak.$$; then echo "$1: MUTATION DID NOT APPLY"; cp /tmp/mut.bak.$$ "$2"; return; fi
  godot --headless --path . --script res://run_tests.gd -- "$4" > $L/mut-$1.log 2>&1
  echo "$1: exit $? $(grep -cE ': FAIL$' $L/mut-$1.log) FAIL lines"
  cp /tmp/mut.bak.$$ "$2"; rm -f /tmp/mut.bak.$$
}
mut camera-world-pixels play/play.gd 's/return \(view_centre\(at, screen, bounds\) \* zoom\).round\(\) \/ zoom/return view_centre(at, screen, bounds).round()/' res://play/play_test.gd
mut scale-not-forwarded world_art/world_art.gd 's/\t\tif combat != null:\n\t\t\tcombat.bodies.set_pixel_scale\(value\)\n//' res://play/play_test.gd
mut grid-offset-zero grade/grade.gd 's/_grid_offset = \(_view_origin - view.position\) \* art_pixel/_grid_offset = Vector2.ZERO/' res://grade/grade_test.gd
mut view-scale-fractional view_scale.gd 's/static func world_scale\(window: Vector2i, touch: bool\) -> int:\n(.*?)return maxi\(mini\(window.x \/ least.x, window.y \/ least.y\), 1\)/static func world_scale(window: Vector2i, touch: bool) -> float:\n$1return maxf(minf(float(window.x) \/ least.x, float(window.y) \/ least.y), 1.0)/s' res://view_scale_test.gd
