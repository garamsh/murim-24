#!/bin/sh
# Throwaway (#364): the night village bench, main d60c33d2 (scratch worktree, same extension) against
# this head, alternating, three rounds; each run starts once the 1-minute load is below 4.
set -u
cd "$(dirname "$0")/../.."
L=build/issue364b/bench.log
run() {
  until [ "$(sysctl -n vm.loadavg | awk '{gsub(/[{}]/,""); print ($1 < 4)}')" = 1 ]; do sleep 5; done
  echo "== round $1 $2 $(git -C $3 rev-parse --short HEAD) $(date +%H:%M:%S) load $(sysctl -n vm.loadavg) others: $(ps -axo comm= | grep -cE '(^|/)(ctest|murim_core_test|murim_core_sweep|murim_core_bench|ninja|clang-tidy|cc1plus)$')" >> $L
  NOBLOOM=${4:-0} TAG=$2 perl -e "alarm 120; exec @ARGV" godot --path $3 --script res://build/issue364b/night-village.gd >> $L 2>&1
  echo "exit $?" >> $L
}
for round in 1 2 3; do
  run $round main <scratch worktree of main d60c33d2>
  run $round head .
  run $round head-nobloom . 1
done
