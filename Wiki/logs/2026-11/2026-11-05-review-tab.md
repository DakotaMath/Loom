---
type: log
tags: [m2]
---

# 2026-11-05: The Review tab

> **Summary:** Built #40: Review's list and the front-matter changes in plain words in `LoomCore` (`ReviewChanges`), Catch2-tested on the `review` overlay and on edits of the demo, and the Review tab itself (the list, plain words, before and after, raw diff, Open in Statements), Qt-tested.

**Session model:** Opus 5.5
**Issues and PRs:** #40

## Done
- `Source/LoomCore/ReviewChanges.{h,cpp}`: `reviewGroupOf()` (the spec's table of groups), `fileChanges()` (every row of *Rules: Front-matter changes in plain words*, for statements, proofs, check histories, verbose versions, `paper.toml` and stubs), `reviewList()` (the status changes from `statusChanges()`, and one entry per statement, paper or other file, with a summary such as *"G8TV.a premises, check"*). A body change names the checks it outdates by comparing `HEAD` with `HEAD` plus that one file (`outdatedChecks()`). Committed checks are matched as M1's `check-changed` matches them.
- `Source/LoomApp/ReviewPage.{h,cpp}`: the tab. The list is read on the Git lane (`git status --porcelain=v2 -z --untracked-files=all`, then `HEAD`'s tree through `readRevision()`, its contents cached per commit), 300 ms after each snapshot and when the tab is shown. A statement entry shows its plain words, both builds on the Build lane (Alone, + Premises, + Chain, Verbose; Link scrolling), a side's errors in place of its PDF, the raw diff (coloured; a binary file gives its sizes) and Open in Statements (the selected proof's page, or the statement's). Other entries show the raw diff only.
- `MainWindow`: Review replaces its placeholder; the badge counts the entries; Review's git commands go to Output's Git channel and its builds to the Build channel.
- Tests: `ReviewChangesTests` (7 cases) and `ReviewTests` (8 cases, label `tex`; the build cases skip without `latexmk`).
- Gotcha: [QPdfView draws blank pages in a grab taken as soon as the PDF is ready](../../gotchas/qt/qpdfview-grab-before-render.md).

## Learned
- In the `review` overlay, G8TV stays unchecked: Dakota's pass is of the new version, but the agent's pass is of the old one, so the proof isn't checked. The statuses that change are C4JK, D5MN and E6PQ (drops) and M7QR (new, open).
- Clearing a stub's `checked_against_pdf` drops the stub from cited to extracted and also drops every statement resting on it (E6PQ, through 7RT2).
- Files sort `Proofs/` before `Statements/`, and `.checks.toml` before `.tex`, so an entry's files are reordered to put the statement first and each proof before its checks; otherwise the summary reads "check, premises".

## Next
- M2's build issues are done once #40 is merged; then M2's review.
