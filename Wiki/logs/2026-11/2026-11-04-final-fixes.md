---
type: log
tags: [m1, review, spec]
---

# 2026-11-04: M1 final review, the last fixes

> **Summary:** Fixed the M1 final review's last list: a year-0 check date is reported and never counts, a whitespace-only value is empty (A21), a lettered key that differs from the rule's only in case keeps its letter, every git command that hangs is `git-timeout` and the message names the whole command, the kit's `CLAUDE.md` lists all of Dakota's clicks, S4 and S5 are answered (A19, A20), amendments A18–A23 are in the table, and the wording drifts are fixed; each code fix's test failed on `main` first, and all 278 `ctest` tests pass on Windows.

**Session model:** Opus 5.5
**Issues and PRs:** no issue (the list "Proposed before calling M1 done" in `Reviews/M1/FINAL.md` on `m1/review`); PR on branch `m1/final-fixes`

## Done
- **Year-0 check date (A9, A22):** `EntryReader::date` reports it as `bad-value` through `FieldChecker::date()`, now shared for date-times, and marks the history lossy. `standingOf` skips any check without a valid date, so it never counts.
- **Whitespace-only values (A21):** `FieldChecker::reportEmpty` treats a value of only whitespace as empty for required and naming fields. Paper fields that don't go through it do the same: `work.type`, `work.title`, version values, a stub locator's `at`. A whitespace-only `work.title` is no longer a `wrong-type` that says "empty". The locator's `at` messages use A8's wording.
- **Case-only lettered keys:** new `ruleKey()` in PaperKeys. `rekey` and the `paper-key-rule` warning both use it. A key equal to the base plus a letter, ignoring case, keeps the letter (`smithjones24a` → `SmithJones24a`, a case-only rename).
- **`git-timeout` (A16):** install-hooks' `--git-path`, `--git-common-dir` and `config` calls, new-project's steps 4–5 (install-hooks' own timeouts included), and validate's `ls-tree` and both `cat-file` calls. `gitCommand()` names the whole command, quoting arguments that contain spaces. `readObjects` no longer hard-codes 120 s, so `GitTool::timeoutMs` applies to it too.
- **Kit `CLAUDE.md`:** also forbids marking a proof `failed` and outdating checks. The spec's quote, the template and the new-project test agree.
- **Spec:** S4 is answered (A19) and S5's steps are reordered (A20). Added rows A18–A23 and updated the status line. Also: `ref-not-listed`'s backticks, `--version` with an argument, new-paper's message for a top-level `verified`, and the form of JSON paths (A23).
- **Drifts:** `loom --version extra` is now its own usage error. `build-bib --json` gives the project path. new-paper says "Left out `verified`" for the entry's own `verified`. Also fixed: the stale comments in `main.cpp` and `Cycles.h`, the missing space in `StatementsAndProofs.cpp`, the JSON-coverage comment in ValidationTests, and the renamed test "new checks may sit between committed ones, which must not change or go".
- **Tests:** FakeGit gets a `slow-at` mode: the command named in `LOOM_FAKE_GIT_SLOW` hangs, and every other command runs the real git. There is a new linked-worktree test for A10.
- **Wiki:** [how to fix validation problems](../../how-to/fix-validation-problems.md) (`missing-field`, `bad-value`, `alias-invalid`, `paper-key-rule`); [`core.hooksPath` gotcha](../../gotchas/git/hookspath-moves-hooks.md): the linked worktree is tried and tested.

## Learned
- The shared Qt install in the coordinator's scratchpad had lost files (`syncqt.exe`, `Qt6Gui.dll` and more) by midday, so a fresh `cmake --preset windows` failed. Reinstalling `qtbase` with the same `aqt` into a scratchpad of my own fixed it in 25 s.
- To make a test fail at one particular git step, the fake git has to pass every other command through to the real git; a fake that answers everything itself can't get past `rev-parse --show-toplevel`.

## Next
- Dakota's review of the PR and its ⚠️ Flags; then M1 is done once fix-level's PR (A18, `level` in the fingerprint) is merged too.
