#!/bin/sh
# Alternates main and head, ROUNDS rounds, through the frame bench (M1) and the village bench.
# Holds while /private/tmp/murim-quiet exists. Usage: bench.sh <main tree> <head tree> <rounds>
main=$1; head=$2; rounds=$3
for r in $(seq 1 $rounds); do
  for side in main head; do
    tree=$main; [ $side = head ] && tree=$head
    for tool in frame_bench_tool village_bench_tool; do
      while [ -e /private/tmp/murim-quiet ]; do sleep 30; done
      echo "== round $r $side $(git -C $tree rev-parse --short HEAD) $tool $(date +%H:%M:%S) load $(sysctl -n vm.loadavg)"
      (cd $tree && godot --path . --script res://world_view/$tool.gd < /dev/null 2>&1 | grep -E "p50|PASS|FAIL|setup|in view" | head -6)
    done
  done
done
