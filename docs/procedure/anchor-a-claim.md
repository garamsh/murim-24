# Anchor a claim

The acts that turn a claim in a document or a GitHub artifact into one a reader can check. The form each claim takes is `docs/convention/documentation.md` §Keep it current and §GitHub artifacts; this file cites them and states none of it.

## Find the subject

Each claim has one of three subjects, and `documentation.md` gives each its own form:

| Subject | Form |
|---|---|
| This repository | `documentation.md` §Keep it current, "Every claim about this repository must be verifiable in it" |
| Anything outside it — a tool, a library, a host, another repository | `documentation.md` §Keep it current, "A claim about anything outside it names what it holds for", and "A file may state once at the top what it was checked against" where one statement at the top of the file covers several claims |
| The project that will carry this document | `documentation.md` §Keep it current, "A claim about the project that will carry this document" |

A sentence holding claims about two subjects is two claims; anchor each.

## Anchor it

1. **Name an object in full.** Cite a commit as `documentation.md` §Keep it current, "A commit is cited by its full object name", states: `git rev-parse <ref>` prints that name. A short name read from `git log --format='%h'` is no basis for writing the full one: its output is a correct prefix, so a remainder written in after it looks resolved.
2. **Cite a rule by its words.** Where the rule is numbered outside this project, the citation takes the form `documentation.md` §Keep it current, "A citation of a rule numbered outside this project", states — quote its heading or opening words.
3. **Record what was read and when.** Name the release or commit, the command with its arguments, and the date, as `documentation.md` §Keep it current, "A claim about anything outside it names what it holds for", and §GitHub artifacts, "A claim names a basis that could have contradicted it", ask.
4. **Ask whether a measurement exists before asserting a behaviour.** Before writing that a tool does something, find the command, run, or source that showed it. Where none exists, either produce one or ask whoever made the claim what established it; where neither yields one, the claim comes out, per `documentation.md` §Keep it current, "Every claim about this repository must be verifiable in it", for this repository.
5. **Enumerate before quantifying over this repository.** A claim whose subject is a set of this repository's files, lines or rules — one using a word such as "every", "no", "nothing" or "only", or a count — takes one of two anchors, per `documentation.md` §Keep it current, "Every claim about this repository must be verifiable in it", and §GitHub artifacts, "A claim names a basis that could have contradicted it":
   - name the command that enumerated the set and the classification applied to its output; or
   - write the members as instances, with no quantifier over them.

   The members a writer happened to read are not an enumeration. Where a quantifier failed, fix it with one of the two anchors; a narrower quantifier drawn from the same reading is the same defect again. Example: at PR #559 head `c4919b45fc38aab0c761df7bdb06474291ed7ddf`, `README.md:5` read "Of the lines that name an agent, two reach more than agents", written in place of a failed quantifier; `docs/convention/ci.md:52` names an agent and made a third (review 5555442139). The paragraph that landed at `985afb2af4f6b450a26e0b341ae08404a81e5d28` names two such lines as instances and counts none.

## Check the anchor

1. Repeat the basis as written — the command, the file at the revision named — and confirm it returns what the claim says.
2. Where the basis is a search, run it where the claim is false. For each part of the claim the search is offered for, ask what it would have read had that part been false, construct an input in which that part is false, and run the same command over it. Where it reads the same as it did over the real input, it is no basis for that part, as `documentation.md` §GitHub artifacts, "A claim names a basis that could have contradicted it", states. An expression matching nothing is one case: had the absence been false, the input would hold what was searched for, so the expression must match an input that holds it. Matching some input is not the test; reading differently where the part is false is. Example: #566 counted citations of `README.md:5` with the expression below, its control a string known to contain one; the expression also matches `README.md:50`, so an input holding no citation reads the same (GNU grep 3.11):

   ```
   printf 'README.md:5`\nREADME.md:50\n' | grep -c 'README\.md:5'   -> 2
   printf 'README.md:50\nREADME.md:50\n' | grep -c 'README\.md:5'   -> 2
   ```

   Where the claim quantifies over a set, its members come from the enumeration §Anchor it, step 5 names; this step tests the expression that enumeration ran.
3. Where the claim quantifies over a set (§Anchor it, step 5), re-run the enumeration at the revision named and apply the classification to its output. Re-reading the claim shows only the members it names.
4. Run `make lint` before pushing; it prints the count of Markdown files each script read and the command that selected them.
