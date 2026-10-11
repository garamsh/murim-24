#!/bin/sh
# Runs make bench and the village bench in each tree, alternately, ROUNDS times; each run waits for
# a 1-minute load under 4 and records the load and the power source before it starts.
S=/private/tmp/claude-501/-Users-sauce-orca-workspaces-murim-24-m8-linux/11b8dc98-dd86-420a-888f-93143eb07166/scratchpad
OUT=$S/bench
ROUNDS=${ROUNDS:-3}
trees="aff1434:$S/wt-aff1434 main:$S/wt-main branch:/Users/sauce/orca/workspaces/murim-24/m8-linux"
wait_idle() {
  while [ "$(sysctl -n vm.loadavg | awk '{print ($2 < 4.0)}')" != "1" ]; do sleep 10; done
}
for round in $(seq 1 $ROUNDS); do
  for entry in $trees; do
    name=${entry%%:*}; dir=${entry#*:}
    wait_idle
    { echo "== $name round $round"; date; sysctl -n vm.loadavg; pmset -g batt | head -1; } > $OUT/$name-$round.txt
    (cd $dir && make bench >> $OUT/$name-$round.txt 2>&1; echo "make bench exit $?" >> $OUT/$name-$round.txt)
    wait_idle
    { echo "-- village"; sysctl -n vm.loadavg; } >> $OUT/$name-$round.txt
    (cd $dir && godot --path . --script res://world_view/village_bench.gd >> $OUT/$name-$round.txt 2>&1; echo "village exit $?" >> $OUT/$name-$round.txt)
  done
done
echo done > $OUT/finished
