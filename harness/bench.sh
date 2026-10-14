#!/bin/sh
# #184's counted bench: make bench's frame benchmark on the M1 battle, main (in the worktree
# build/wtmain on its own extension) against this head (on its own), alternating, three rounds; each
# run starts once the 1-minute load is below 4 and is killed after 120 s if it hangs.
set -u
cd "$(dirname "$0")/../.."
L=build/issue184/logs/bench.log
run() {
  until [ "$(sysctl -n vm.loadavg | awk '{gsub(/[{}]/,""); print ($1 < 4)}')" = 1 ]; do sleep 5; done
  echo "== round $1 $2 $(git -C $3 rev-parse --short HEAD) $(date +%H:%M:%S) load $(sysctl -n vm.loadavg) $(pmset -g batt | head -1) others: $(ps -axo comm= | grep -cE '(^|/)(ctest|murim_core_test|murim_core_sweep|murim_core_bench|ninja|clang-tidy|cc1plus)$')" >> $L
  perl -e "alarm 120; exec @ARGV" godot --path $3 --script res://world_view/frame_bench.gd >> $L 2>&1
  echo "exit $?" >> $L
}
for round in 1 2 3; do
  run $round main build/wtmain
  run $round head .
done
