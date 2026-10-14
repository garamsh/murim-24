#!/bin/sh
# Alternates main and the branch: frame bench (M1 battle) then the night village, three rounds.
# Logs the load, power source and other builders before each run.
set -u
cd "$(dirname "$0")/../.."
L=build/issue209/logs
for round in 1 2 3; do
  for tree in main head; do
    if [ $tree = main ]; then git checkout -q --detach 22f03a29; else git checkout -q feat/night-lights; fi
    for b in frame village; do
      echo "== round $round $tree $b $(git rev-parse --short HEAD) $(date +%H:%M:%S) load $(sysctl -n vm.loadavg) $(pmset -g batt | head -1) others: $(ps -axo comm= | grep -cE '(^|/)(ctest|murim_core_test|murim_core_sweep|murim_core_bench|ninja|clang-tidy|cc1plus)$')" >> $L/bench.log
      if [ $b = frame ]; then
        godot --path . --script res://world_view/frame_bench.gd >> $L/bench.log 2>&1
      else
        TAG=$tree godot --path . --script res://build/issue209/night-village.gd >> $L/bench.log 2>&1
      fi
      echo "exit $?" >> $L/bench.log
    done
  done
done
git checkout -q feat/night-lights
