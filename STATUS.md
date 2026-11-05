<!--
FORMAT (keep this comment). Boot step 1: every session reads this file.
Updated at the end of every session: change only the lines that changed. Hard limit: 70 lines including
this comment. History goes in Wiki/logs/, not here.
Sections, in order: Updated / Current milestone lines, Done recently (last few items, newest first),
In progress, Next up, Waiting on Dakota (same as the needs-dakota label), Watch out for.
-->
# Status

**Updated:** 2026-11-05 · main at 8506746
**Current milestone:** M1, the core, is **done**: built, reviewed, fixed and re-reviewed; all 279 `ctest` tests pass on Windows. M2 (statements and review) has an approved spec, waiting to be merged

## Done recently
- The M1 review, fixes and final pass (2026-11-04 to 05): review on branch `m1/review` (`Reviews/M1/`, verdict in `FINAL.md`, no blockers); fixes in PRs #21–#27, #29, #30; spec amendments A1–A23 in PRs #20, #28, #30. A proof's `level` is now in its fingerprint (A18, `loom-fingerprint-v2`) ([log](Wiki/logs/2026-11/2026-11-05-m1-review.md)).
- #9 `loom validate`, `loom install-hooks` and the commit guard (2026-11-04): PR #19 merged ([log](Wiki/logs/2026-11/2026-11-04-validate.md)).
- #7 the graph and computed status, `loom status`/`graph`/`refute` (2026-11-04): PR #18 merged ([log](Wiki/logs/2026-11/2026-11-04-status.md)).
- #8 paper entries, stubs, the key rule, `refs.bib`, `loom new-paper`/`new-stub`/`rekey`/`build-bib` (2026-11-04): PR #17 merged ([log](Wiki/logs/2026-11/2026-11-04-papers.md)).
- #6 check histories, fingerprints, `fingerprint`/`add-check`/`merge-checks` (2026-11-04): PR #16 merged ([log](Wiki/logs/2026-11/2026-11-04-checks.md)).
- #5 statements, proofs, IDs and their subcommands (2026-11-04): PR #15 merged ([log](Wiki/logs/2026-11/2026-11-04-statements-proofs.md)).
- #4 `loom new-project`, opening a project, path safety (2026-11-03): PR #14 merged ([log](Wiki/logs/2026-11/2026-11-03-new-project.md)).
- #3 CMake and CI (2026-11-03): PR #13 merged ([log](Wiki/logs/2026-11/2026-11-03-cmake-skeleton.md)).
- The M1 spec (2026-11-03): [`Specs/m1-core.md`](Specs/m1-core.md), approved by Dakota (PR #11).

## In progress
- #32 the M2 spec, [`Specs/m2-statements-review.md`](Specs/m2-statements-review.md): PR #42, approved by Dakota (Q1–Q6 as recommended), waiting to be merged ([log](Wiki/logs/2026-11/2026-11-05-m2-spec.md)).

## Next up
Milestones M0–M12 are on GitHub. M1's issues (#2–#9) are all closed.
- M2's build issues #33–#41 are filed and can start once PR #42 is merged; #33 (`loom build`) and #34 (the app shell) start first.
- M1 follow-ups, for M2 or later: see "Follow-ups" in `Reviews/M1/FINAL.md` on `m1/review` (`\cite` of an old key is settled by the M2 spec, E3).

## Waiting on Dakota
- Merge PR #42 (the M2 spec, #32).
- Close the M1 milestone on GitHub.
- The Windows checks listed in PRs #21–#30.

## Watch out for
- **Windows CI and Qt 6.11:** aqtinstall is pinned to a commit of its main branch until a release after 3.3.0 ([gotcha](Wiki/gotchas/ci/aqtinstall-qt-6-11-windows.md)).
- **Qt on Windows:** install it with the Qt Online Installer; a Qt left in a temporary folder can lose files and break a fresh configure.
- **Fingerprints are v2** (A18): checks written before PR #29 no longer match.
- **Path checks on Windows:** `QFileInfo::canonicalFilePath()` doesn't resolve junctions; `LoomCore` uses `std::filesystem::canonical()` ([gotcha](Wiki/gotchas/qt/canonical-path-ignores-junctions.md)).
- **Terminal checks on Windows:** `_isatty()` is true for `NUL`; `loom` uses `GetConsoleMode` ([gotcha](Wiki/gotchas/windows/isatty-true-for-nul.md)).
- **Qt regexes:** `$` also matches before a final `\n`; anchor with `QRegularExpression::anchoredPattern` ([gotcha](Wiki/gotchas/qt/regex-dollar-before-final-newline.md)).
- **Public repo:** never commit details of private research projects; examples use made-up papers.
