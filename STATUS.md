<!--
FORMAT (keep this comment). Boot step 1: every session reads this file.
Updated at the end of every session: change only the lines that changed. Hard limit: 70 lines including
this comment. History goes in Wiki/logs/, not here.
Sections, in order: Updated / Current milestone lines, Done recently (last few items, newest first),
In progress, Next up, Waiting on Dakota (same as the needs-dakota label), Watch out for.
-->
# Status

**Updated:** 2026-11-04 · main at 0a9d224
**Current milestone:** M1, the core: the build, CI, `loom new-project` and statements and proofs are merged; #6 (checks) is in review

## Done recently
- #5 statements, proofs, IDs and their subcommands (2026-11-04): PR #15 merged ([log](Wiki/logs/2026-11/2026-11-04-statements-proofs.md)).
- #4 `loom new-project`, opening a project, path safety (2026-11-03): PR #14 merged ([log](Wiki/logs/2026-11/2026-11-03-new-project.md)).
- #3 CMake and CI (2026-11-03): PR #13 merged; `ctest` passes on Windows and Linux ([log](Wiki/logs/2026-11/2026-11-03-cmake-skeleton.md)).
- The M1 spec (2026-11-03): [`Specs/m1-core.md`](Specs/m1-core.md) written for #2, approved by Dakota and merged (PR #11; [log](Wiki/logs/2026-11/2026-11-03-m1-core-spec.md)).
- `Specs/loom.md` approved: PR #1 merged.
- The design chat (2026-11-03): Loom's design written as [`Specs/loom.md`](Specs/loom.md), and this repo's skeleton set up ([log](Wiki/logs/2026-11/2026-11-03-design-chat.md)).

## In progress
- #6 check histories, fingerprints, `loom fingerprint`/`add-check`/`merge-checks`, `--outdate-checks`: PR #16, all 100 `ctest` tests pass on Windows; waiting on Dakota's review and Windows check ([log](Wiki/logs/2026-11/2026-11-04-checks.md)).

## Next up
Milestones M0–M12 are on GitHub; M1's issues are #2–#9, linked by "blocked by".
- **In order:** #8 papers; #7 status after #6 and #8; #9 `loom validate` last. The issues were updated to match the approved spec on 2026-11-03.

## Waiting on Dakota
- PR #16 (#6): review, the Windows check in the PR, and the spec points under its ⚠️ Flags.

## Watch out for
- **Windows CI and Qt 6.11:** aqtinstall is pinned to a commit of its main branch until a release after 3.3.0 ([gotcha](Wiki/gotchas/ci/aqtinstall-qt-6-11-windows.md)).
- **Path checks on Windows:** `QFileInfo::canonicalFilePath()` doesn't resolve junctions; `LoomCore` uses `std::filesystem::canonical()` ([gotcha](Wiki/gotchas/qt/canonical-path-ignores-junctions.md)).
- **Terminal checks on Windows:** `_isatty()` is true for `NUL`; `loom` uses `GetConsoleMode` ([gotcha](Wiki/gotchas/windows/isatty-true-for-nul.md)).
- **Public repo:** never commit details of private research projects; examples use made-up papers.
