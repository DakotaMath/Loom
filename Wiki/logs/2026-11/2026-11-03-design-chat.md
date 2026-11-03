---
type: log
tags: [m0, design]
---

# 2026-11-03: The design chat

> **Summary:** Designed Loom with Dakota in one conversation, wrote the design as `Specs/loom.md` (draft, no open questions), and set up this repo's skeleton.

**Session model:** Opus 5.5
**Issues and PRs:** the setup PR (no issue)

## Done
- Settled why Loom exists: the project becomes an object Loom owns, instead of a repo an editor was built for afterwards (TexViewer), which had two codebases parsing one set of formats.
- Settled the design, recorded in the spec's decisions table: C++ with Qt 6 Widgets; statements as nodes and proofs as edges with computed status; agent check plus Dakota's click for "proved"; append-only check history with fingerprints in a sidecar file per proof; TOML front matter except for threads; papers with separate versions and full bibliographic data; short random permanent IDs with a picker; scratch and kept numerics runs tied to their code, HDF5 outputs in Git LFS; a Python editor; tasks folder and tab; local review of job branches; nightly reviewers on the Claude API; Claude Code jobs on Dakota's subscription; the vertical tab bar.
- Chose the licences: GPL v3 for the code (QScintilla is GPL v3), CC BY 4.0 for docs and specs, CC0 for the project kit.
- Set up the repo: `CLAUDE.md`, `STATUS.md`, README, `Specs/` with its template, the wiki sections with templates, generated indexes (`Tools/update_indexes.py`, with tests), issue and PR templates, CI.

## Learned
- Hand-kept indexes and flat folders of records are what make an agent-built wiki unreadable after a few hundred pages; this repo generates every index and files logs by month and gotchas by area from the start.
- The Claude API has no C++ SDK and the Agent SDK is Python and TypeScript only; running Claude Code headless as a separate process sidesteps both.

## Next
- Dakota approves `Specs/loom.md`.
- Write the M1 spec: exact formats, validation, status and check rules, fingerprints, IDs, the `loom` commands, and the CMake and Qt setup.

## Waiting on Dakota
- Approval of `Specs/loom.md`.
