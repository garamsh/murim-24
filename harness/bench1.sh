#!/bin/sh
# One head run at each quality: frame bench, then the night village at high and at low.
cd "$(dirname "$0")/../.."
L=build/issue209/logs/$1
echo "== $(git rev-parse --short HEAD) dirty $(git status --short grade | wc -l | tr -d ' ') $(date +%H:%M:%S) load $(sysctl -n vm.loadavg) $(pmset -g batt | head -1) others: $(ps -axo comm= | grep -cE '(^|/)(ctest|murim_core_test|murim_core_sweep|murim_core_bench|ninja|clang-tidy|cc1plus)$')" >> $L
for i in 1 2; do godot --path . --script res://world_view/frame_bench.gd 2>&1 | grep -E "p99" >> $L; TAG=probe godot --path . --script res://build/issue209/night-village.gd 2>&1 | grep -E "frame time" >> $L; done
