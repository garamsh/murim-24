#!/bin/zsh
# PR C: 3 rounds of main's frame_bench, then this head's frame_bench (M1, no town), then its
# town_bench (M1 with the walled town round the camera), each started on a quiet machine (wait_quiet).
S=$1; C=/Users/sauce/orca/workspaces/murim-24/m2-art
# Quiet: on AC, the 1-minute load under 4, and no process but the window server and the kernel
# using more than 40% of a core, sampled twice 10 s apart.
busy() { ps -Ao pcpu=,comm= | awk '$1 > 40 && $2 !~ /WindowServer|kernel_task|airportd/ {n++} END {print n+0}'; }
wait_quiet() {
  while true; do
    l=$(sysctl -n vm.loadavg | awk '{print $2}')
    ac=$(pmset -g batt | head -1 | grep -c "AC Power")
    if [[ $ac == 1 ]] && (( l < 4.0 )) && [[ $(busy) == 0 ]]; then
      sleep 10
      if [[ $(busy) == 0 ]]; then echo "$l"; return; fi
    fi
    sleep 20
  done
}
MAIN=/private/tmp/claude-501/-Users-sauce-orca-workspaces-murim-24-m2-art/fd317a05-d3a7-45fd-973d-0a7a27cb92ba/scratchpad/wt-main
for r in 1 2 3; do
  l=$(wait_quiet); echo "round $r main start load $l $(date +%T)" >> $S/series_c.log
  (cd $MAIN && godot --path . --script res://world_view/frame_bench.gd) > $S/benchC_main_$r.log 2>&1
  echo "round $r main exit $? $(date +%T)" >> $S/series_c.log
  l=$(wait_quiet); echo "round $r plain start load $l $(date +%T)" >> $S/series_c.log
  (cd $C && godot --path . --script res://world_view/frame_bench.gd) > $S/benchC_plain_$r.log 2>&1
  echo "round $r plain exit $? $(date +%T)" >> $S/series_c.log
  l=$(wait_quiet); echo "round $r town start load $l $(date +%T)" >> $S/series_c.log
  (cd $C && godot --path . --script $S/town/town_bench.gd) > $S/benchC_town_$r.log 2>&1
  echo "round $r town exit $? $(date +%T)" >> $S/series_c.log
done
echo "series done $(date +%T)" >> $S/series_c.log
