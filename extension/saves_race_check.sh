#!/bin/sh
# Runs the GDScript tests that write saves twice at once, as two worktrees' make ci would, ROUNDS
# times, and prints how many of each pair passed. Each run of run_tests.gd has a directory of its
# own (extension/test_saves.gd), so both should always pass. Given a directory, both runs of a pair
# share it instead, as they shared user:// before: the control, where they race on the same slots.
# The race depends on timing, so the control fails often but not every time. Each run is stopped
# after LIMIT seconds. A tool run by hand from the repository, not a test:
#   extension/saves_race_check.sh [shared directory]
set -u
ROUNDS=${ROUNDS:-2}
LIMIT=600
TESTS="res://extension/file_saves_test.gd res://play/play_test.gd"
shared=${1:-}
logs=$(mktemp -d)
passed=0
total=0
for round in $(seq 1 "$ROUNDS"); do
	for run in a b; do
		if [ -n "$shared" ]; then
			MURIM_TEST_SAVES="$shared" perl -e "alarm $LIMIT; exec @ARGV" godot --headless --path . --script res://run_tests.gd -- $TESTS > "$logs/$round$run.log" 2>&1 &
		else
			perl -e "alarm $LIMIT; exec @ARGV" godot --headless --path . --script res://run_tests.gd -- $TESTS > "$logs/$round$run.log" 2>&1 &
		fi
	done
	wait
	for run in a b; do
		total=$((total + 1))
		if grep -q "run_tests: 2 of 2 GDScript tests passed" "$logs/$round$run.log"; then
			passed=$((passed + 1))
		else
			echo "saves_race_check: round $round run $run failed:"
			grep -E "FAIL|SCRIPT ERROR" "$logs/$round$run.log" | head -5
		fi
	done
done
echo "saves_race_check: $passed of $total runs passed, two at a time, $([ -n "$shared" ] && echo "sharing $shared" || echo "each in its own directory")"
rm -rf "$logs"
[ "$passed" -eq "$total" ]
