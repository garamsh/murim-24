#!/bin/zsh
# Renderer mutations: name, file, perl substitution; runs grade_check and restores the file.
cd "$(dirname "$0")/../../.."
L=build/issue209/p2/logs
rmut() {
  cp "$2" /tmp/rmut.bak.$$
  perl -0pi -e "$3" "$2"
  if cmp -s "$2" /tmp/rmut.bak.$$; then echo "$1: MUTATION DID NOT APPLY"; cp /tmp/rmut.bak.$$ "$2"; return; fi
  godot --path . --script res://grade/grade_check.gd > $L/rmut-$1.log 2>&1
  echo "$1: exit $?"; grep -E ": FAIL$|checks passed|out of frames" $L/rmut-$1.log | cut -c1-200
  cp /tmp/rmut.bak.$$ "$2"; rm -f /tmp/rmut.bak.$$
}
rmut no-fog grade/grade.gdshader 's/g = mix\(g, FOG_COLOUR \* gain \+ lift, band \* 0.7\);/g = g;/'
rmut no-height grade/grade.gdshader 's/float clear = clamp\(1.0 - level_at\(world\) \/ FOG_LEVELS, 0.0, 1.0\);/float clear = 1.0;/'
rmut no-far grade/grade.gdshader 's/float far = smoothstep\(0.1, 1.0, 1.0 - FRAGCOORD.y \* SCREEN_PIXEL_SIZE.y\);/float far = 1.0;/'
rmut no-clouds grade/clouds.gdshader 's/COLOR.rgb = mix\(COLOR.rgb, SHADE, band \* SHARE\);/COLOR.rgb = COLOR.rgb;/'
rmut clouds-lighten grade/clouds.gdshader 's/COLOR.rgb = mix\(COLOR.rgb, SHADE, band \* SHARE\);/COLOR.rgb = mix(COLOR.rgb, vec3(1.0), band * SHARE);/'
