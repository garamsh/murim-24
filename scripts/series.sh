#!/bin/sh
# Captures main and the head, then runs one bench round, aff1434, main and the head in turn, each run
# started at a 1-minute load under 4, with the load and power source recorded.
S=/private/tmp/claude-501/-Users-sauce-orca-workspaces-murim-24-m8-linux/11b8dc98-dd86-420a-888f-93143eb07166/scratchpad
E=$S/evidence
HEAD_TREE=/Users/sauce/orca/workspaces/murim-24/m8-linux
wait_idle() { while [ "$(sysctl -n vm.loadavg | awk '{print ($2 < 4.0)}')" != "1" ]; do sleep 10; done; }
for scenario in m1 village; do
  for pair in "before:$S/wt-main" "after:$HEAD_TREE"; do
    name=${pair%%:*}; tree=${pair#*:}
    wait_idle
    (cd $tree && godot --path . --script $E/scripts/capture.gd -- $scenario $E/${name}_$scenario) > $E/capture_${name}_$scenario.log 2>&1
  done
done
mkdir -p $E/bench
for pair in "aff1434:$S/wt-aff1434" "main_2cc1292:$S/wt-main" "head_$(git -C $HEAD_TREE rev-parse --short HEAD):$HEAD_TREE"; do
  name=${pair%%:*}; tree=${pair#*:}
  wait_idle
  { date; sysctl -n vm.loadavg; pmset -g batt | head -1; } > $E/bench/$name.log
  (cd $tree && make bench >> $E/bench/$name.log 2>&1; echo "make bench exit $?" >> $E/bench/$name.log)
  wait_idle
  { echo "-- village"; sysctl -n vm.loadavg; } >> $E/bench/$name.log
  (cd $tree && godot --path . --script res://world_view/village_bench.gd >> $E/bench/$name.log 2>&1; echo "village exit $?" >> $E/bench/$name.log)
done
echo done > $E/finished
