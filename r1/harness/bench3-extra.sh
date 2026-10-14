#!/bin/sh
# The clean round #324's review asked for: #323's head (p1) against this head (p2), alternating,
# three rounds of the M1 frame bench and the village at morning (fog and clouds thickest), at high.
# Each run starts only once the 1-minute load is below 4; the load, power source and other
# builders are logged before it. Leaves the tree on feat/rain.
set -u
cd "$(dirname "$0")/../../.."
L=build/issue209/p2/logs/bench3.log
P1=$(git rev-parse feat/night-lights)
P2=$(git rev-parse feat/height-fog)
for round in 4; do
  for tree in p1 p2; do
    if [ $tree = p1 ]; then git checkout -q --detach $P1; else git checkout -q --detach $P2; fi
    for b in frame morning; do
      until [ "$(sysctl -n vm.loadavg | awk '{gsub(/[{}]/,""); print ($1 < 4)}')" = 1 ]; do sleep 5; done
      echo "== round $round $tree $b $(git rev-parse --short HEAD) $(date +%H:%M:%S) load $(sysctl -n vm.loadavg) $(pmset -g batt | head -1) others: $(ps -axo comm= | grep -cE '(^|/)(ctest|murim_core_test|murim_core_sweep|murim_core_bench|ninja|clang-tidy|cc1plus)$')" >> $L
      case $b in
        frame) godot --path . --script res://world_view/frame_bench.gd >> $L 2>&1 ;;
        morning) AT=0.3 QUALITY=2 TAG=$tree godot --path . --script res://build/issue209/r1/village.gd >> $L 2>&1 ;;
      esac
      echo "exit $?" >> $L
    done
  done
done
git checkout -q feat/rain
