---
type: log
tags: [m2, app, ui, editor, recovery]
---

# 2026-11-05: The Statements tab: the list and the editor

> **Summary:** Built #35: the Statements tab lists, opens, edits, creates and saves statements and proofs through `LoomCore`, in a QScintilla editor with the front matter as a form or raw text, asking Dakota before a save outdates checks or marks a proof failed; unsaved text survives tab switches, crashes (recovery files) and changes on disk.

**Session model:** Opus 5.5
**Issues and PRs:** #35, PR #46

## Done
- `LoomCore`: `EditorSaves` (`saveStatementText()`, `saveProofText()`, `saveVerboseBody()`, `markVerboseUpToDate()`, `fileBase()`), `Drafts` (recovery files under `.loom/drafts/`). `commit()` moved from `Edits.cpp` to `EditSession` so the saves share it; `ChangeResult` gains `written`, and a `dakota-only` refusal carries `outdatedChecks`.
- `LoomApp`: `StatementsPage`, `FilePage`, `EditBuffer`, `StatementList`, `HeaderForms` (chips, the statement and proof forms), `LatexEditor`, `FindBar`, `NewItemDialogs`, `StatusMarks`, `TextDiff`; snapshots carry each file's bytes and time; the bottom panel lists a refused save's problems and opens a clicked problem's line; the window asks about unsaved edits before closing or opening another project; a Statement menu with the tab's shortcuts.
- QScintilla 2.14.1 built by `Source/CMake/QScintilla.cmake` ([decision 004](../../decisions/004-qscintilla.md)).
- Tests: 11 Catch2 cases (saves, drafts) and 22 Qt Test functions (`StatementsTabTests`); all 335 `ctest` tests pass on Windows, one skipped (the lock-age test, as before).

## Learned
- QScintilla accepts `ShortcutOverride` for every key in its keymap, so Ctrl+- zoomed the editor instead of reaching the window's action ([gotcha](../../gotchas/qt/qscintilla-takes-bound-shortcuts.md)).
- A widget added to a layout that is already on screen is shown only when the event loop next runs: screenshots taken straight after showed empty chip rows and a blank bar. `show()` it at once.
- Rebuilding bars, chips or pages from a click on one of their own buttons must `deleteLater()` the old widgets, not `delete` them.
- A `MainWindow` keeps the project's `app.lock` until it is destroyed, not when it closes; tests that reopen a project in a new window must destroy the old one first (the app itself deletes its window on close).
- A child widget of a hidden parent isn't `isHidden()`; test `isVisible()`.

## Next
- Dakota reviews PR #46 and runs its Windows check.
- #36 (compiling and the PDF pane) fills the right-hand pane and Output's channels; #37 replaces the typed IDs in chips and the list's search with the ID picker and `LoomCore`'s ID search; #38 adds the check history, Dakota's buttons and the status changes in the dialogs.
