#!/bin/zsh
# #236: 3 rounds of main's frame_bench, then this head's, each started on a quiet machine
# (wait_quiet), into $S/b236_<main|head>_<round>.log and $S/series236.log.
S=$1; C=/Users/sauce/orca/workspaces/murim-24/m2-art
# Quiet: on AC, the 1-minute load under 4, and no process but the window server and the kernel
# using more than 40% of a core, sampled twice 10 s apart.
busy() { ps -Ao pcpu=,comm= | awk '$1 > 40 && $2 !~ /WindowServer|kernel_task|airportd/ {n++} END {print n+0}'; }
wait_quiet() {
  while true; do
    l=$(sysctl -n vm.loadavg | awk '{print $2}')
    ac=$(pmset -g batt | head -1 | grep -c "AC Power")
    if [[ $ac == 1 ]] && (( l < 4.0 )); then
      sleep 10
      echo "$l busy $(busy)"; return
    fi
    sleep 20
  done
}
MAIN=/private/tmp/claude-501/-Users-sauce-orca-workspaces-murim-24-m2-art/fd317a05-d3a7-45fd-973d-0a7a27cb92ba/scratchpad/wt-main
for r in 1 2 3; do
  l=$(wait_quiet); echo "round $r main start load $l $(date +%T)" >> $S/series236.log
  (cd $MAIN && perl -e 'alarm 120; exec @ARGV' godot --path . --script res://world_view/frame_bench.gd) > $S/b236_main_$r.log 2>&1
  echo "round $r main exit $? $(date +%T)" >> $S/series236.log
  l=$(wait_quiet); echo "round $r head start load $l $(date +%T)" >> $S/series236.log
  (cd $C && perl -e 'alarm 120; exec @ARGV' godot --path . --script res://world_view/frame_bench.gd) > $S/b236_head_$r.log 2>&1
  echo "round $r head exit $? $(date +%T)" >> $S/series236.log
done
echo "series done $(date +%T)" >> $S/series236.log
