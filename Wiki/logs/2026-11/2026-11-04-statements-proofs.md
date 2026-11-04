---
type: log
tags: [m1, build, statements, proofs, ids, cli]
---

# 2026-11-04: Statements, proofs and IDs

> **Summary:** Built #5: `LoomCore` reads and writes statement, proof and verbose proof files (TOML front matter around a body kept byte for byte), checks every field and the references between files, generates tags and proof letters, and creates and edits statements, proofs and tags behind the write lock; `loom` gains `new-tag`, `new-statement`, `edit-statement`, `new-proof` and `edit-proof`. All 82 `ctest` tests pass on Windows.

**Session model:** Opus 5.5
**Issues and PRs:** #5, PR #15

## Done
- `LoomCore`: `FrontMatter` (finding `+++` front matter, UTF-8, BOM, CRLF), `StatementsAndProofs` (the three file types, their own problems, the canonical writer), `Ids` (tags, Crockford reading on the command line, generation, proof letters, aliases), `ProjectContents` (reading `Statements/` and `Proofs/`, the problems that need several files, resolving typed IDs), `Cycles` (one circle per strongly connected group; #7 can reuse it for premises), `Edits` (the five changes, `.loom/write.lock`, refusing any change that adds an error), `TomlFields` (the field checker `ProjectToml` used, now shared).
- `loom`: the five subcommands, `--project`, finding the project from a folder inside it; `Cli` holds what subcommands share.
- Tests: Catch2 for front matter, the formats and every per-file problem with its line, the cross-file problems (one project with one of each), IDs, cycles and every change and refusal; a CMake script for the subcommands' exit codes and JSON.
- Deleted the placeholder `The version is 0.1.0` test, as Dakota chose this session.
- Wiki: [gotcha: `CMAKE_MATCH_<n>` doesn't leave a function](../../gotchas/cmake/cmake-match-inside-function.md).

## Learned
- Refusing a change exactly when it would add an error (comparing the project's errors before and after, by code, file and message) gives every subcommand the spec's own messages without a second set of rules, and still lets an edit fix one problem in a file that has others.
- Rewriting a file Loom couldn't fully read (an unknown field, a value of the wrong type) would drop that text, so edits are refused for such files; a missing field loses nothing and doesn't block.
- On this machine, PowerShell 5.1's `Get-Content`/`Set-Content` read UTF-8 sources as ANSI and write a BOM: an en dash in a comment came out as `â€“`. Edit files with the file tools or Python, not PowerShell.
- `CMAKE_MATCH_1` set inside a CMake `function()` is empty in the caller ([gotcha](../../gotchas/cmake/cmake-match-inside-function.md)).

## Next
- Dakota reviews the PR and does its Windows check, and decides the spec points under its ⚠️ Flags.
- Then #6 (checks and fingerprints) and #8 (papers), which build on `ProjectContents`.

## Waiting on Dakota
- PR #15 (#5): review, the Windows check, and the spec points it raises.
