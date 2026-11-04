<!--
FORMAT (keep this comment). Boot step 1: every session reads this file.
Updated at the end of every session: change only the lines that changed. Hard limit: 70 lines including
this comment. History goes in Wiki/logs/, not here.
Sections, in order: Updated / Current milestone lines, Done recently (last few items, newest first),
In progress, Next up, Waiting on Dakota (same as the needs-dakota label), Watch out for.
-->
# Status

**Updated:** 2026-11-04 · main at a49670c
**Current milestone:** M1, the core: the build, CI, `loom new-project`, statements and proofs, checks and papers are merged; #7 (status) is in review

## Done recently
- #8 paper entries, stubs, the key rule, `refs.bib`, `loom new-paper`/`new-stub`/`rekey`/`build-bib` (2026-11-04): PR #17 merged ([log](Wiki/logs/2026-11/2026-11-04-papers.md)).
- #6 check histories, fingerprints, `fingerprint`/`add-check`/`merge-checks` (2026-11-04): PR #16 merged ([log](Wiki/logs/2026-11/2026-11-04-checks.md)).
- #5 statements, proofs, IDs and their subcommands (2026-11-04): PR #15 merged ([log](Wiki/logs/2026-11/2026-11-04-statements-proofs.md)).
- #4 `loom new-project`, opening a project, path safety (2026-11-03): PR #14 merged ([log](Wiki/logs/2026-11/2026-11-03-new-project.md)).
- #3 CMake and CI (2026-11-03): PR #13 merged; `ctest` passes on Windows and Linux ([log](Wiki/logs/2026-11/2026-11-03-cmake-skeleton.md)).
- The M1 spec (2026-11-03): [`Specs/m1-core.md`](Specs/m1-core.md) written for #2, approved by Dakota and merged (PR #11; [log](Wiki/logs/2026-11/2026-11-03-m1-core-spec.md)).
- `Specs/loom.md` approved: PR #1 merged.
- The design chat (2026-11-03): Loom's design written as [`Specs/loom.md`](Specs/loom.md), and this repo's skeleton set up ([log](Wiki/logs/2026-11/2026-11-03-design-chat.md)).

## In progress
- #7 the graph and computed status, `loom status`/`graph`/`refute`: PR open on `m1/status`, all 157 `ctest` tests pass on Windows; waiting on Dakota's review, the Windows check and the spec points under its ⚠️ Flags ([log](Wiki/logs/2026-11/2026-11-04-status.md)).

## Next up
Milestones M0–M12 are on GitHub; M1's issues are #2–#9, linked by "blocked by".
- **In order:** #9 `loom validate` after #7, last in M1. The issues were updated to match the approved spec on 2026-11-03.

## Waiting on Dakota
- The #7 PR: review, the Windows check in the PR, and the spec points under its ⚠️ Flags.

## Watch out for
- **Windows CI and Qt 6.11:** aqtinstall is pinned to a commit of its main branch until a release after 3.3.0 ([gotcha](Wiki/gotchas/ci/aqtinstall-qt-6-11-windows.md)).
- **Path checks on Windows:** `QFileInfo::canonicalFilePath()` doesn't resolve junctions; `LoomCore` uses `std::filesystem::canonical()` ([gotcha](Wiki/gotchas/qt/canonical-path-ignores-junctions.md)).
- **Terminal checks on Windows:** `_isatty()` is true for `NUL`; `loom` uses `GetConsoleMode` ([gotcha](Wiki/gotchas/windows/isatty-true-for-nul.md)).
- **Public repo:** never commit details of private research projects; examples use made-up papers.
