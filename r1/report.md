# Evidence for #324, review round 1

Head `300f278f` (fog thickness and cloud shade drawn once an art pixel in the light field). Round 0's evidence is the parent commit.

- `r1/logs/bench3.log`: the PM-cleared clean bench, p1 `a44ac4e7` against p2 `300f278f`, M1 frame and morning village, four rounds, every run started below load 4 with no other builders. `r1/logs/bench3-52abddef.log`: the same against the pre-fix head `52abddef`, which showed +1.65 ms and +3.2 ms.
- `r1/logs/mut-*.log`, `r1/logs/rmut-*.log`: headless and renderer mutations at the fix, the last two renderer mutations aimed at the new field reads.
- `r1/logs/check-*.log`, `r1/logs/gdtests.log`: grade_check 9/9 and the GDScript suite.
- `r1/harness/`: bench3.sh, bench3-extra.sh, mutf.sh, rmutf.sh.
