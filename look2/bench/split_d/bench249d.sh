#!/bin/zsh
# #249's counted series under the quiet file: takes /private/tmp/murim-quiet (or keeps it when it
# already says art), waits at most 30 minutes until no ctest, murim_core_test, murim_core_sweep or
# make ci process is left, then runs 3 rounds of the head's frame bench, with blood off and with the hit stop off,
# each started on AC at a 1-minute load under 3, or, after 20 minutes waiting, under 4, logging the
# pmset line, the load and the gate, and removes the file after the last run.
S=$1; C=/Users/sauce/orca/workspaces/murim-24/m2-art
MAIN=/private/tmp/claude-501/-Users-sauce-orca-workspaces-murim-24-m2-art/fd317a05-d3a7-45fd-973d-0a7a27cb92ba/scratchpad/wt-main
LOG=$S/bench249d.log
[ -e /private/tmp/murim-quiet ] && [[ "$(cat /private/tmp/murim-quiet)" != art ]] && { echo "held by $(cat /private/tmp/murim-quiet)" >> $LOG; exit 1; }
# One series at a time: a second copy started while one runs leaves at once, touching nothing.
if [ -e $S/bench249d.pid ] && kill -0 $(cat $S/bench249d.pid) 2>/dev/null; then echo "already running" >> $LOG; exit 1; fi
echo $$ > $S/bench249d.pid
echo art > /private/tmp/murim-quiet
# Stopped, it removes the file and its pid and leaves; a trap that does not exit lets zsh go on.
trap 'rm -f /private/tmp/murim-quiet $S/bench249d.pid' EXIT
trap 'rm -f /private/tmp/murim-quiet $S/bench249d.pid; exit 1' INT TERM
others() { ps -Ao comm=,args= | awk '{n = split($1, p, "/"); c = p[n]} c == "ctest" || c == "murim_core_test" || c == "murim_core_sweep" || (c == "make" && $3 == "ci") {k++} END {print k + 0}'; }
for i in $(seq 1 180); do
  if [[ $(others) == 0 ]] && (( $(sysctl -n vm.loadavg | awk '{print $2}') < 4.0 )); then break; fi
  sleep 10
done
[[ $(others) == 0 ]] || { echo "not drained after 30 minutes" >> $LOG; exit 1; }
quiet() {
  local waited=0 gate=3.0
  while true; do
    l=$(sysctl -n vm.loadavg | awk '{print $2}')
    (( waited >= 120 )) && gate=4.0
    if pmset -g batt | head -1 | grep -q "AC Power" && (( l < gate )) && [[ $(others) == 0 ]]; then
      echo "load $l under $gate, $(pmset -g batt | head -1 | tr -d "'")"; return
    fi
    sleep 10; waited=$((waited + 1))
  done
}
run() {
  echo "round $1 $2 start $(quiet) $(date +%T)" >> $LOG
  (cd $3 && perl -e 'alarm 120; exec @ARGV' godot --path . --script $4 -- $5) > $S/b249d_$2_$1.log 2>&1
  echo "round $1 $2 exit $? $(date +%T)" >> $LOG
}
for r in 1 2 3; do
  run $r head $C res://world_view/frame_bench.gd ""
  run $r noblood $C $S/town/frame_bench_parts.gd noblood
  run $r nohitstop $C $S/town/frame_bench_parts.gd nohitstop
done
echo "series done $(date +%T)" >> $LOG
