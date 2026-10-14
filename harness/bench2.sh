#!/bin/sh
# Alternates part 1's head (p1) and part 2's head (p2) over three rounds: the frame bench (M1
# battle), the village at midnight and at morning (fog and clouds thickest), at high. Logs the load,
# power source and other builders before each run. Leaves the tree on feat/night-lights.
set -u
cd "$(dirname "$0")/../../.."
L=build/issue209/p2/logs/bench2.log
for round in 1 2 3; do
  for tree in p1 p2; do
    if [ $tree = p1 ]; then git checkout -q feat/night-lights; else git checkout -q feat/height-fog; fi
    for b in frame night morning; do
      echo "== round $round $tree $b $(git rev-parse --short HEAD) $(date +%H:%M:%S) load $(sysctl -n vm.loadavg) $(pmset -g batt | head -1) others: $(ps -axo comm= | grep -cE '(^|/)(ctest|murim_core_test|murim_core_sweep|murim_core_bench|ninja|clang-tidy|cc1plus)$')" >> $L
      case $b in
        frame) godot --path . --script res://world_view/frame_bench.gd >> $L 2>&1 ;;
        night) AT=0.0 QUALITY=2 TAG=$tree godot --path . --script res://build/issue209/r1/village.gd >> $L 2>&1 ;;
        morning) AT=0.3 QUALITY=2 TAG=$tree godot --path . --script res://build/issue209/r1/village.gd >> $L 2>&1 ;;
      esac
      echo "exit $?" >> $L
    done
  done
done
git checkout -q feat/night-lights
