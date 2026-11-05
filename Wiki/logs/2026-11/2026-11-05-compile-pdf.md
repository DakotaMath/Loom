---
type: log
tags: [m2, app, ui, pdf, latex, synctex]
---

# 2026-11-05: Compiling, the PDF pane and the Problems panel

> **Summary:** Built #36: Ctrl+S saves and compiles on the Build lane, the PDF shows beside the editor (Qt PDF, loaded from memory) in the chosen mode, the last good PDF stays dimmed while an error is unfixed, errors list in a reworked Problems panel that opens the line, SyncTeX works both ways, Output has Build, Git and Loom channels, and a file changed on disk recompiles.

**Session model:** Opus 5.5
**Issues and PRs:** #36, PR #47

## Done
- `LoomCore/SyncTex`: a label's page from `main.aux`, `synctex view` and `synctex edit` run as processes and read, `main.tex` lines mapped to the body below their heading. Five Catch2 cases, one with TeX (`[tex]`).
- `LoomApp/PdfPane`: the mode switch, Verbose, Compile, zoom and build state; builds on the Build lane, a newer one stopping the running one; the PDF from a `QBuffer`; `last-good.pdf` dimmed under the spec's bar, or the errors alone with no good build; opening at the statement's page, keeping the scroll on a rebuild; Ctrl+J (forward SyncTeX, with a mark) and double-click (inverse); find in the PDF; each statement's mode and toggle and the zoom in the per-project state; recompiling 300 ms after a file of the build changes on disk (not after Loom's own saves).
- `BottomPanel`: Severity, File (`ID · title`), Line, Problem; the build's problems with the validation and refused-save ones; F8 and Shift+F8, keeping the selection across snapshots; Output's three channels with Copy and Clear, 5 MB each.
- `StatementsPage` and `MainWindow`: Ctrl+S compiles after a save (with *Compile on save*), Ctrl+B, Ctrl+J, F8; the status bar's build state; build problems marked in the editor's margin; Qt PDF in *About Loom*.
- Qt PDF linked by `LoomAppLib`; `LoomCore`'s link check refuses `Qt6::Pdf*` (a new `pdf` case); CI installs `qtpdf` ([decision 005](../../decisions/005-qt-pdf.md)).
- A reviewer (an Opus 5.5 subagent Dakota asked for) found bugs, fixed with tests: a build stopped by unreadable front matter dropped the last good PDF and never recompiled when fixed; the previous statement's build problems stayed after switching away; a change on disk during a build could be missed (inputs are now the snapshot's bytes at the build's start, or Loom's own saves); stale `main.aux` and a late double-click after a target change; SyncTeX while the last good PDF is dimmed (now the statement's page only, and double-click says why); reading the build's files after its lock was released (now `LoomCore`'s `readBuildOutput()`, under `build.lock`); a rebuild's scroll restored before the view's new layout allowed it. Also: preamble changes now recompile (the snapshot has their bytes), Compile shows a spinner, find in the PDF waits for its first result, forward search scrolls sideways too, reused builds don't repeat their log in Output.
- `CompileTests` (Qt Test, label `tex`): fourteen functions. All 343 `ctest` tests pass on Windows with TeX Live 2026, one skipped (the lock-age test, as before).

## Learned
- A Qt Test that "hung" for 300 s under `ctest -j8` wasn't a deadlock: another test's window had taken activation, so Ctrl+B never reached the shortcut and a two-minute wait ran into the watchdog. With no debugger installed, a watchdog thread in the test that walked the main thread's stack with dbghelp found it ([gotcha](../../gotchas/qt/shortcuts-need-an-active-window.md)). `StatementsTabTests::editsAndSaves` failed the same way once in five loaded runs.
- Qt Test programs on Windows print nothing to a redirected stdout here; `-o file.txt,txt` writes the log.
- `QPdfView` renders pages in the background: a grab just after loading shows a blank page. It also lays pages out again a moment after a zoom or a load, resetting a scroll position set too early; restoring one must wait for the scroll range.
- `synctex view` prints several records for a line; inverse search on a heading or a `\ref` lands in `main.tex`.
- Qt from a temporary folder had decayed again; reinstalled with aqt (`--modules qtpdf`).

## Next
- Dakota reviews the PR and runs its Windows check.
- #37 (IDs with their titles and the picker), #38 (status, checks and Dakota's clicks).
