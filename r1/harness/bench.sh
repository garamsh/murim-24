#!/bin/sh
# Alternates main and part 1's head over three rounds: the frame bench (M1 battle), the night
# village at high and at low, and on the head the night village at high with a hearth listed, so
# the bloom runs. Logs the load, power source and other builders before each run. Leaves the tree
# on feat/night-lights.
set -u
cd "$(dirname "$0")/../../.."
L=build/issue209/r1/logs/bench.log
MAIN=$(git rev-parse origin/main)
for round in 1 2 3; do
  for tree in main head; do
    if [ $tree = main ]; then git checkout -q --detach $MAIN; runs="frame high low"; else git checkout -q feat/night-lights; runs="frame high low hearth"; fi
    for b in $runs; do
      echo "== round $round $tree $b $(git rev-parse --short HEAD) $(date +%H:%M:%S) load $(sysctl -n vm.loadavg) $(pmset -g batt | head -1) others: $(ps -axo comm= | grep -cE '(^|/)(ctest|murim_core_test|murim_core_sweep|murim_core_bench|ninja|clang-tidy|cc1plus)$')" >> $L
      case $b in
        frame) godot --path . --script res://world_view/frame_bench.gd >> $L 2>&1 ;;
        high) QUALITY=2 TAG=$tree godot --path . --script res://build/issue209/r1/village.gd >> $L 2>&1 ;;
        low) QUALITY=0 TAG=$tree godot --path . --script res://build/issue209/r1/village.gd >> $L 2>&1 ;;
        hearth) QUALITY=2 HEARTH=1 TAG=$tree godot --path . --script res://build/issue209/r1/village.gd >> $L 2>&1 ;;
      esac
      echo "exit $?" >> $L
    done
  done
done
git checkout -q feat/night-lights
