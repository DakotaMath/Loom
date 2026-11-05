<!--
FORMAT (keep this comment). Boot step 1: every session reads this file.
Updated at the end of every session: change only the lines that changed. Hard limit: 70 lines including
this comment. History goes in Wiki/logs/, not here.
Sections, in order: Updated / Current milestone lines, Done recently (last few items, newest first),
In progress, Next up, Waiting on Dakota (same as the needs-dakota label), Watch out for.
-->
# Status

**Updated:** 2026-11-05 · main at b31e71e
**Current milestone:** M1, the core, is **done**: built, reviewed, fixed and re-reviewed; all 279 `ctest` tests pass on Windows. **M2** (statements and review) is being built from its approved spec

## Done recently
- #37 IDs with their titles and the ID picker: PR #48 merged (2026-11-05) ([log](Wiki/logs/2026-11/2026-11-05-id-picker.md)).
- #36 compiling, the PDF pane, SyncTeX, the Problems panel and Output: PR #47 merged (2026-11-05) ([log](Wiki/logs/2026-11/2026-11-05-compile-pdf.md)).
- #35 the Statements tab: PR #46 merged (2026-11-05) ([log](Wiki/logs/2026-11/2026-11-05-statements-editor.md)).
- #34 the app shell: PR #45 merged (2026-11-05) ([log](Wiki/logs/2026-11/2026-11-05-app-shell.md)).
- #33 building LaTeX in `LoomCore` and `loom build`, with the demo project: PR #44 merged (2026-11-05) ([log](Wiki/logs/2026-11/2026-11-05-build-latex.md)).
- The M2 spec (#32, PR #42) and dropping M3 (PR #43), merged by Dakota (2026-11-05) ([log](Wiki/logs/2026-11/2026-11-05-m2-spec.md)).
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
- #38 status, checks and Dakota's clicks (status changes in `LoomCore`, approved beyond the issue's scope; the check history, Pass/Needs changes/Fail, Mark failed, Refute): PR #49, waiting for Dakota's review; 362 tests pass on Windows, 1 skipped ([log](Wiki/logs/2026-11/2026-11-05-checks-panel.md)).

## Next up
Milestones M0–M12 are on GitHub; M3 is dropped and its milestone closed. M1's issues (#2–#9) are all closed.
- M2's build issues #39–#40 (#41 is now M4's): #39 (git in `LoomCore` and the git panel) next, then #40 (Review, which can reuse `statusChanges()`); both use the demo project and `Tests/Support/DemoProject` from PR #44.
- M1 follow-ups, for M2 or later: see "Follow-ups" in `Reviews/M1/FINAL.md` on `m1/review` (`\cite` of an old key is settled by the M2 spec, E3).

## Waiting on Dakota
- Review PR #49 (#38, checks and clicks) and run its Windows check; the Windows checks of PRs #46–#48, if not done.
- Close the M1 milestone on GitHub.
- The Windows checks listed in PRs #21–#30.

## Watch out for
- **Windows CI and Qt 6.11:** aqtinstall is pinned to a commit of its main branch until a release after 3.3.0 ([gotcha](Wiki/gotchas/ci/aqtinstall-qt-6-11-windows.md)).
- **Qt on Windows:** install it with the Qt Online Installer, with the *Qt PDF* component (M2 needs it); a Qt left in a temporary folder can lose files and break a fresh configure.
- **Shortcuts in Qt Tests:** make the window active right before each one; parallel tests take activation away ([gotcha](Wiki/gotchas/qt/shortcuts-need-an-active-window.md)).
- **Fingerprints are v2** (A18): checks written before PR #29 no longer match.
- **Path checks on Windows:** `QFileInfo::canonicalFilePath()` doesn't resolve junctions; `LoomCore` uses `std::filesystem::canonical()` ([gotcha](Wiki/gotchas/qt/canonical-path-ignores-junctions.md)).
- **Terminal checks on Windows:** `_isatty()` is true for `NUL`; `loom` uses `GetConsoleMode` ([gotcha](Wiki/gotchas/windows/isatty-true-for-nul.md)).
- **QPdfView at Fit width** hangs at some sizes unless its vertical scroll bar is always on ([gotcha](Wiki/gotchas/qt/qpdfview-fit-width-scrollbar-loop.md)).
- **QScintilla annotations:** `annotation(line)` returns garbage in 2.14.1; read them with `SCI_ANNOTATIONGETTEXT` ([gotcha](Wiki/gotchas/qt/qscintilla-annotation-returns-garbage.md)).
- **QScintilla takes shortcuts** its keymap binds while an editor has focus ([gotcha](Wiki/gotchas/qt/qscintilla-takes-bound-shortcuts.md)).
- **Qt regexes:** `$` also matches before a final `\n`; anchor with `QRegularExpression::anchoredPattern` ([gotcha](Wiki/gotchas/qt/regex-dollar-before-final-newline.md)).
- **TeX tests:** labelled `tex` (`ctest -L tex`); they skip without `latexmk`, and fail instead under `LOOM_REQUIRE_TEX=1`, as on Linux CI. BibTeX refuses a plain `max_print_line` ([gotcha](Wiki/gotchas/latex/bibtex-rejects-long-max-print-line.md)).
- **Public repo:** never commit details of private research projects; examples use made-up papers.
