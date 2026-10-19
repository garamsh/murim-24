#!/bin/sh
# Alternates two trees, ROUNDS rounds, through one bench tool. Each start waits until the CPU
# summed over every other process, but those under /System/ and /usr/libexec/ (macOS daemons), is at most 50% and the 1-minute load is under 4, and logs the
# reading it passed on and the busiest processes.
# Usage: bench4.sh <tree A> <label A> <tree B> <label B> <rounds> <tool>
a=$1; la=$2; b=$3; lb=$4; rounds=$5; tool=$6; me=$$
outside() { ps -axo pid=,ppid=,pcpu=,comm= | awk -v me=$me '$1!=me && $2!=me && $4 !~ /^\/(System|usr\/libexec)\// {s+=$3} END {printf "%.1f", s}'; }
gate() {
  waited=0
  while :; do
    c=$(outside); l=$(sysctl -n vm.loadavg | awk '{print $2}')
    awk -v c=$c -v l=$l 'BEGIN{exit !(c<=50 && l<4)}' && break
    sleep 15; waited=$((waited+15))
  done
  echo "   gate passed on: outside CPU, less /System/ and /usr/libexec/, ${c}%, load1 $l, after ${waited}s wait; busiest:"
  ps -axo pcpu=,comm= | sort -rn | head -6 | sed 's/^/     /'
}
for r in $(seq 1 $rounds); do
  for side in A B; do
    tree=$a; label=$la; [ $side = B ] && { tree=$b; label=$lb; }
    gate
    echo "== round $r $label $(git -C $tree rev-parse HEAD) $tool $(date +%H:%M:%S) load $(sysctl -n vm.loadavg)"
    (cd $tree && godot --path . --script res://world_view/$tool.gd < /dev/null 2>&1 | grep -E "p50|PASS|FAIL|setup|in view" | head -6)
  done
done
