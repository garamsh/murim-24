# Review a pull request

The order of the acts in a review of a pull request in this repository, the commands for each, and what the host does. What a review must carry is `docs/convention/review.md`; this file cites it and states none of it.

## Before the diff

1. List the changed paths without reading the diff: `gh pr diff <n> --name-only`.
2. Open `docs/convention/README.md` and, from its §Stack-neutral and §Stack-specific tables, pick every file whose Governs column covers a changed path. §Precedence there orders them where two apply.
3. Read those files, and `docs/convention/review.md` in full, from the files at the current `main` — `AGENTS.md` §Start here, every time gives the reason a remembered version is not read instead.
4. Where a changed path touches code or a decision, open `docs/architecture/README.md` too; its §Rules feed the Architecture row.
5. Read the task the pull request states — its issue and that issue's comments, or its Summary where it names no issue. `review.md` §What each check verifies, "Scope is measured against the task", names it as the measure.

## Reading the diff

1. Fetch first, so `main` is the one GitHub holds: `git fetch origin`.
2. Measure scope from the merge base: `git diff --stat origin/main...origin/<branch>`, three dots. In git 2.47.3, `git diff A...B` compares B against the merge base of A and B, while the two-dot `git diff A..B` compares the two tips, so every change merged to `main` after the branch was cut shows up as if the branch reverted it. `gh pr diff <n>` shows the same set the three-dot form does.
3. Check each changed line against the conventions read above. Each violation is cited as `review.md` §Citing violations states, and tagged as §Blockers and nits states.

## Confirming the run

The two checks this section serves are `review.md` §Submitting a review, "Confirm the checks belong to the head commit" and "A conclusion is not a verdict unless the run did the work".

1. Read the head: `gh pr view <n> --json headRefOid,headRefName`.
2. List the runs on the branch with the commit each ran on: `gh run list --branch <headRefName> --json databaseId,headSha,conclusion,event`. Take the run whose `headSha` equals `headRefOid`; a run on any other commit is not a run on the head.
3. Count the steps that ran: `gh run view <databaseId> --json jobs --jq '.jobs[] | {name, conclusion, steps: [.steps[] | {name, conclusion}]}'`.
4. Read what the steps selected: `gh run view <databaseId> --log`. In this repository the `make ci` step runs `make lint`, whose scripts each print a `checked <count> ... selected by: <command>` line; the counts are the work selected.

## Posting the verdict

1. Write the body in a file, holding what `review.md` §Submitting a review names.
2. Where the reviewer opened the pull request from the same account, the state to submit is the one `review.md` §Single-account setups names. Where the reviewer also wrote the change, `review.md` §Reviewing your own change applies.
3. Submit it: `gh api --method POST repos/garamsh/convention-driven-project/pulls/<n>/reviews -f event=<APPROVE|REQUEST_CHANGES|COMMENT> -F body=@<file>`. `-F` reads a value starting with `@` from the named file; `-f` sends it as the literal string, per `gh api --help` in gh 2.46.0.
4. To merge, the method and the commit it leaves are `docs/convention/git.md` §PRs and §Commits, the second paragraph of §Commits, beginning "An author is a person", naming what the subject must carry: `gh pr merge <n> --squash --subject '<pr title> (#<n>)' --body-file <file>`, then read the commit on `main` against the squashed commits. The subject passed to `--subject` is the subject that lands, so the number has to be in it. #507's title ends `abbreviated sha` while its merge commit `8c4c3b8bc5a85b422f2c615eac977d5badd80875` ends `abbreviated SHA (#507)`, though `gh api repos/garamsh/convention-driven-project --jq .squash_merge_commit_title` reads `PR_TITLE`; a subject passed without the number lands without it. `f92f1f50d9419eb324acfdb56b0ade67d17c8b07`, the merge of #513, reads exactly #513's title and no number; `gh pr view 513 --json title,mergeCommit` and `git show -s --format='%H %s' f92f1f50d9419eb324acfdb56b0ade67d17c8b07` show both.

## Host facts

Observed on this host on 2026-10-16, with gh 2.46.0 (Debian 2.46.0-3).

- `gh pr view <n>` and `gh issue view <n>` without `--json` exit with `GraphQL: Projects (classic) is being deprecated ... (repository.pullRequest.projectCards)` and print nothing else. Every `--json` form works, and so does `gh api repos/garamsh/convention-driven-project/pulls/<n>`.
- `gh pr edit` is reported failing the same way in #480, case 2. Setting a body through REST works: `gh api --method PATCH repos/garamsh/convention-driven-project/pulls/<n> -F body=@<file>`.
- `gh pr diff <n>` and `gh pr list` work without `--json`.
- `gh run list --commit <sha>` given an abbreviated SHA prints nothing and exits 0, which reads the same as a head with no run — the case `review.md` §Submitting a review, "Confirm the checks belong to the head commit", treats as an unverified change. With `H=2a24e5c46d54fb1261d34b8d19f9ab56d3e0afb9`, `gh run list --commit "$H"` listed one run and `gh run list --commit "$(echo "$H" | cut -c1-8)"` printed nothing. Passing the full 40-character `headRefOid` works. Reported by the PM of `garamsh/gitops`, whose first pass over six merges read no run on all six, and received here in #504.
