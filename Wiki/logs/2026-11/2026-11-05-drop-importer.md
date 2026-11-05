---
type: log
tags: [design, m2, m4]
---

# 2026-11-05: Dropping the importer (M3); rendered Markdown moves to M4

> **Summary:** Dakota dropped M3 (the importer), since research that predates Loom will start over as new projects, and moved rendered Markdown in Review (#41) from M2 to M4; the design, the M2 spec and the GitHub milestones and issues follow.

**Session model:** Opus 5.5
**Issues and PRs:** #41, PR #43 (the design), PR #42 (the M2 spec)

## Done
- PR #43, `Specs/loom.md`: M3 struck from the build order, with Dakota switching to Loom at the end of M2. M4 gains rendered Markdown in Review. *Importing an existing project* became a short *No importer* section. M4–M12 keep their numbers. `Specs/m1-core.md`'s alias example no longer mentions an import.
- PR #42, the M2 spec: Q6's answer changed to "moved to M4". Review shows Markdown as a raw diff in M2, and notes and records open as plain text. The rendered-Markdown design, with its WebEngine, md4c and KaTeX rows, CI, edge case and tests, moved to an appendix, *Moved to M4*, for the M4 spec to take over. M2 no longer needs Qt WebEngine. The *Opening a folder* message no longer promises an importer.
- GitHub: the M3 milestone is closed, not deleted. #41 moved to M4 and retitled. #40's out-of-scope line was updated.
- Kept M1's `origin = "import:<path>"` value and aliases. Nothing writes the import value, but removing it would change code and the format for no gain.

## Next
- Dakota merges PRs #42 and #43 (either order: they touch different files).
- Then #33 and #34. #41 waits for the M4 spec.
