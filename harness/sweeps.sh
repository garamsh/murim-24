#!/bin/sh
# Runs the sweep over seeds 0-190 and 192-199 for 100 years on main's tree and on head's, one at a
# time, each holding SIGSTOPped while /private/tmp/murim-quiet exists and resumed once it has been
# gone 15 s. Usage: sweeps.sh <main tree> <head tree> <out dir>
main=$1; head=$2; out=$3
guarded() {
  "$@" & pid=$!
  stopped=0; clear=0
  while kill -0 $pid 2>/dev/null; do
    if [ -e /private/tmp/murim-quiet ]; then
      [ $stopped = 0 ] && kill -STOP $pid && stopped=1 && echo "   stopped $(date +%H:%M:%S)" >&2
      clear=0
    elif [ $stopped = 1 ]; then
      clear=$((clear+5))
      [ $clear -ge 15 ] && kill -CONT $pid && stopped=0 && echo "   resumed $(date +%H:%M:%S)" >&2
    fi
    sleep 5
  done
  wait $pid
}
for side in main head; do
  tree=$main; [ $side = head ] && tree=$head
  for range in "0 190" "192 199"; do
    set -- $range
    echo "== $side $(git -C $tree rev-parse --short HEAD) seeds $1-$2 started $(date +%H:%M:%S)" >&2
    guarded $tree/build/bench/murim_core_sweep $1 $2 100 > $out/sweep-$side-$1-$2.md 2> $out/sweep-$side-$1-$2.err
    echo "   exit $? $(date +%H:%M:%S)" >&2
  done
done
