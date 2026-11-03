<!--
FORMAT (keep this comment). Boot step 1: every session reads this file.
Updated at the end of every session: change only the lines that changed. Hard limit: 70 lines including
this comment. History goes in Wiki/logs/, not here.
Sections, in order: Updated / Current milestone lines, Done recently (last few items, newest first),
In progress, Next up, Waiting on Dakota (same as the needs-dakota label), Watch out for.
-->
# Status

**Updated:** 2026-11-03 · main at c70b3e2
**Current milestone:** M0, the design: approve `Specs/loom.md`, then write the M1 spec

## Done recently
- The design chat (2026-11-03): Loom's design written as [`Specs/loom.md`](Specs/loom.md), and this repo's skeleton set up ([log](Wiki/logs/2026-11/2026-11-03-design-chat.md)).

## In progress
- None.

## Next up
Milestones M0–M12 are on GitHub; M1's issues are #2–#9, linked by "blocked by".
- **#2, the M1 spec** (`Specs/m1-core.md`): the exact formats, validation, status and check rules, fingerprints, IDs, the `loom` commands, the toolchain. Starts once PR #1 is merged.
- **Then in order:** #3 CMake and CI → #4 `loom new-project` → #5 statements, proofs, IDs → #6 checks → #7 status; #8 papers after #5; #9 `loom validate` last.

## Waiting on Dakota
- Approve [`Specs/loom.md`](Specs/loom.md) (status: draft, no open questions) by merging PR #1.

## Watch out for
- **Public repo:** never commit details of private research projects; examples use made-up papers.
