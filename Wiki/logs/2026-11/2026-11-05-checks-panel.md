---
type: log
tags: [m2, app, ui, checks, status]
---

# 2026-11-05: Status, checks and Dakota's clicks in the Statements tab

> **Summary:** Built #38: proof pages show what the proof gives and its check history, and Dakota records Pass, Needs changes… and Fail, marks a proof failed and refutes a statement. Each click goes through a dialog that names the status changes `LoomCore` computes, and nothing is written until he confirms.

**Session model:** Opus 5.5
**Issues and PRs:** #38, PR #49

## Done
- **Scope:** Dakota approved widening #38 to `LoomCore`, since the spec's *Status changes* function didn't exist yet.
- **`LoomCore`:**
  - `StatusChanges` (`statusDirection()`, `statusChanges()`).
  - `ChangeResult::statusChanges`, filled in by `commit()`.
  - `addCheck()` and `refuteStatement()` now ask for Dakota's confirmation last, so the refusal without it describes the click.
  - `IdField::RefutedBy`.
- **`LoomApp`:**
  - `ChecksPanel` (the line, the history, the buttons).
  - The clicks in `StatementsPage`: `recordCheck`, `markProofFailed`, `refute`, `openProjectText`.
  - IDs in the status line are links; the #35 save dialogs now name their status changes.
  - `ProjectSession::stale()`.
- **Tests:** 6 Catch2 cases (`StatusChangesTests`) and 7 Qt Test functions (`ClicksTests`). All 362 `ctest` tests pass on Windows, 1 skipped.

## Learned
- **Window activation:** activating the window re-reads the project as a precaution. Counting that as "the snapshot is older than the disk" disabled the check buttons for a moment after every dialog closed, so the tests' second click did nothing. Now only changes the folder watcher saw count.
- **A reference to a field:** `chooseFailed(open, open->proof().level)` passed a reference to a field that the callee overwrites before it reads it, so cancelling kept `failed` ([gotcha](../../gotchas/qt/getter-reference-changes-under-you.md)).
- **Asking first:** an unconfirmed `LoomCore` call works as the "what would this do?" question for every click, once Dakota's confirmation is the last refusal in `commit()`. The app then never works out a status itself.

## Next
- Dakota reviews PR #49 and runs its Windows check.
- #39 (git in `LoomCore` and the git panel), then #40 (Review), which can reuse `statusChanges()` between `HEAD` and now.
