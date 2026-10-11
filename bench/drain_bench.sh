#!/bin/zsh
# Holds /private/tmp/murim-quiet for the whole #249 series: waits, at most 30 minutes, until no
# ctest, murim_core_sweep or make ci process is left and the 1-minute load is under 4, then runs
# series236b.sh, and removes the file after its last run whatever happens.
S=$1
[ -e /private/tmp/murim-quiet ] && [[ "$(cat /private/tmp/murim-quiet)" != art ]] && { echo "held by $(cat /private/tmp/murim-quiet)"; exit 1; }
echo art > /private/tmp/murim-quiet
trap 'rm -f /private/tmp/murim-quiet' EXIT
for i in $(seq 1 180); do
  # By process name, so a shell waiting to run one of them is not counted.
  left=$(ps -Ao comm=,args= | awk '{n = split($1, p, "/"); c = p[n]} c == "ctest" || c == "murim_core_test" || c == "murim_core_sweep" || (c == "make" && $3 == "ci") {k++} END {print k + 0}')
  l=$(sysctl -n vm.loadavg | awk '{print $2}')
  if [[ $left == 0 ]] && (( l < 4.0 )); then echo "drained at $(date +%T), load $l"; break; fi
  sleep 10
done
[[ $left == 0 ]] || { echo "not drained after 30 minutes: $left left"; exit 1; }
rm -f $S/series236.log
perl -e 'alarm 600; exec @ARGV' $S/series236b.sh $S
