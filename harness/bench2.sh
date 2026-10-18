#!/bin/sh
# Alternates main and head, ROUNDS rounds, through the frame bench (M1) and the village bench.
# Each start waits until the CPU summed over every other process is at most 50% and the
# 1-minute load is under 4, then logs both and the busiest processes.
# Usage: bench2.sh <main tree> <head tree> <rounds>
main=$1; head=$2; rounds=$3; me=$$
outside() { ps -axo pid=,ppid=,pcpu= | awk -v me=$me '$1!=me && $2!=me {s+=$3} END {printf "%.1f", s}'; }
load1() { sysctl -n vm.loadavg | awk '{print $2}'; }
gate() {
  waited=0
  until awk -v c="$(outside)" -v l="$(load1)" 'BEGIN{exit !(c<=50 && l<4)}'; do sleep 15; waited=$((waited+15)); done
  echo "   gate: outside CPU $(outside)%, load1 $(load1), after ${waited}s wait; busiest:"
  ps -axo pcpu=,comm= | sort -rn | head -5 | sed 's/^/     /'
}
for r in $(seq 1 $rounds); do
  for side in main head; do
    tree=$main; [ $side = head ] && tree=$head
    for tool in frame_bench_tool village_bench_tool; do
      gate
      echo "== round $r $side $(git -C $tree rev-parse HEAD) $tool $(date +%H:%M:%S) load $(sysctl -n vm.loadavg)"
      (cd $tree && godot --path . --script res://world_view/$tool.gd < /dev/null 2>&1 | grep -E "p50|PASS|FAIL|setup|in view" | head -6)
    done
  done
done
