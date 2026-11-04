---
type: log
tags: [m1, review, papers, hooks, cli]
---

# 2026-11-04: M1 review fixes, batch 1 (write safety)

> **Summary:** Fixed the M1 review's write-safety batch (F1, F2/F8, M1, M2, M7) against amendments A1 and A3: `new-paper` never replaces a folder, `install-hooks` and `new-project` never write hooks outside the project's own git directory, failed paper renames are undone, and `build-bib` never writes over a project file; each fix's test failed on `main` first, and all 177 `ctest` tests pass on Windows.

**Session model:** Opus 5.5
**Issues and PRs:** no issue (batch 1 of `Reviews/M1` on `m1/review`), PR #PR_NUMBER

## Done
- F1: `createPaper` refuses `paper-exists` when anything named `Papers/<Key>` is there, compared ignoring case, by listing `Papers/` (so Linux behaves as Windows).
- F2/F8, M7: `installHooks` refuses `not-a-project`, `hooks-not-top-level` and `hooks-path-outside` before writing anything; `createProject` stops on a global `core.hooksPath`; `findProject` checks that `--project` holds `loom.toml`; `runInstallHooks` refuses every project `openProject` can't open.
- M1: `commitPapers` undoes every rename already done, the last first, when a later rename or the new `paper.toml` fails.
- M2: `buildBibliography` refuses `bib-out-project-file`; the `PaperEdits.h` header now says `build-bib` has no "adds no error" check.
- Wiki: [gotcha: `core.hooksPath`](../../gotchas/git/hookspath-moves-hooks.md).

## Learned
- Reproduced on Windows: with a global `core.hooksPath`, `new-project` put Loom's hook and Git LFS's four hooks into the shared folder and renamed the shared `pre-commit`. A case-only folder (`smithjones24`) was overwritten by `new-paper` of `SmithJones24`.
- The test stand-in `loom-fake-git` answered every `git config --get` with a value, so it looked as if `core.hooksPath` were set; it now answers that key as unset.

## Next
- Dakota reviews the PR and its ⚠️ Flags, and does its Windows check.
