#!/bin/sh
# #209's closing bench on main, its own extension: one make bench (tick, M1 frame, pan) as it stands,
# then three rounds of the M1 battle at high and raining, the night village at high, raining, with
# a hearth listed so the bloom runs, and the village at morning at high (fog and clouds; rain turns
# the clouds' shadows off, so they are measured dry). Each run starts once the 1-minute load is
# below 4 and is killed after 120 s if it hangs; the load, power source and other builders are
# logged before it.
set -u
cd "$(dirname "$0")/../../.."
L=build/issue209/close/logs/bench.log
gate() {
  until [ "$(sysctl -n vm.loadavg | awk '{gsub(/[{}]/,""); print ($1 < 4)}')" = 1 ]; do sleep 5; done
  echo "== $1 $(git rev-parse --short HEAD) $(date +%H:%M:%S) load $(sysctl -n vm.loadavg) $(pmset -g batt | head -1) others: $(ps -axo comm= | grep -cE '(^|/)(ctest|murim_core_test|murim_core_sweep|murim_core_bench|ninja|clang-tidy|cc1plus)$')" >> $L
}
gate "make bench"; make bench >> $L 2>&1; echo "exit $?" >> $L
for round in 1 2 3; do
  gate "round $round m1-rain"; perl -e "alarm 120; exec @ARGV" godot --path . --script res://build/issue209/close/m1_rain.gd >> $L 2>&1; echo "exit $?" >> $L
  gate "round $round night-rain"; RAIN=1 HEARTH=1 AT=0.0 QUALITY=2 TAG=close perl -e "alarm 120; exec @ARGV" godot --path . --script res://build/issue209/rain/village.gd >> $L 2>&1; echo "exit $?" >> $L
  gate "round $round morning"; AT=0.3 QUALITY=2 TAG=close perl -e "alarm 120; exec @ARGV" godot --path . --script res://build/issue209/rain/village.gd >> $L 2>&1; echo "exit $?" >> $L
done
