#!/bin/sh
# Counted bench for #209's parts 3a and 3b, alternating, three rounds, each run started once the
# 1-minute load is below 4 and killed after 120 s if it hangs: main (8dc3d774, in the worktree
# build/wtmain on its own extension) against 3b (this tree, feat/subpixel-camera with rain under it,
# on its own extension) on the M1 frame bench and the village at morning, at high; and on 3b the
# village at noon dry and raining. Logs each tree's commit, the load, power source and other
# builders before each run.
set -u
cd "$(dirname "$0")/../../.."
L=build/issue209/3b/logs/bench.log
run() {
  dir=$3
  until [ "$(sysctl -n vm.loadavg | awk '{gsub(/[{}]/,""); print ($1 < 4)}')" = 1 ]; do sleep 5; done
  echo "== round $1 $2 $4 $(git -C $dir rev-parse --short HEAD) $(date +%H:%M:%S) load $(sysctl -n vm.loadavg) $(pmset -g batt | head -1) others: $(ps -axo comm= | grep -cE '(^|/)(ctest|murim_core_test|murim_core_sweep|murim_core_bench|ninja|clang-tidy|cc1plus)$')" >> $L
  case $4 in
    frame) perl -e "alarm 120; exec @ARGV" godot --path $dir --script res://world_view/frame_bench.gd >> $L 2>&1 ;;
    noon-dry) RAIN=0 AT=0.5 QUALITY=2 TAG=$2 perl -e "alarm 120; exec @ARGV" godot --path $dir --script res://build/issue209/rain/village.gd >> $L 2>&1 ;;
    noon-rain) RAIN=1 AT=0.5 QUALITY=2 TAG=$2 perl -e "alarm 120; exec @ARGV" godot --path $dir --script res://build/issue209/rain/village.gd >> $L 2>&1 ;;
    morning) AT=0.3 QUALITY=2 TAG=$2 perl -e "alarm 120; exec @ARGV" godot --path $dir --script res://build/issue209/rain/village.gd >> $L 2>&1 ;;
  esac
  echo "exit $?" >> $L
}
for round in 1 2 3; do
  run $round main build/wtmain frame; run $round main build/wtmain morning
  run $round sub . frame; run $round sub . morning; run $round sub . noon-dry; run $round sub . noon-rain
done
