---
type: log
tags: [m1, review, cli, dakota]
---

# 2026-11-04: M1 review fixes, batch 6a (Dakota's clicks and the CLI leftovers)

> **Summary:** Fixed batch 6a of the M1 review: `--outdate-checks` now outdates only the proofs Dakota confirmed, setting `level = "failed"` is his click (S7), `refute` checks its IDs before asking, `new-paper` says which dates it set to false, a git timeout is told apart from a missing git, `new-project` has its own `core.hooksPath` message, and `build-bib --out` resolves links and short names; each fix's test failed on `main` first, and all 225 `ctest` tests pass on Windows.

**Session model:** Opus 5.5
**Issues and PRs:** no issue (batch 6a of `Reviews/M1` on `m1/review`); PR to come

## Done
- **The `--outdate-checks` race:** `StatementEdit::outdateChecks` and `ProofEdit::outdateChecks` are now the list of proofs Dakota confirmed. `editStatement`/`editProof` refuse with `outdates-checks`, writing nothing, when the edit would outdate a different set. `loom` passes the list it showed in the prompt.
- **S7:** `NewProof::failedConfirmed` and `ProofEdit::failedConfirmed`. Setting `failed` without it is refused `dakota-only`, after the rules and before outdated checks. `loom new-proof` and `edit-proof` ask *"This is Dakota's decision: mark 4KX7.a failed."* (or *"create 4KX7.b as a failed proof"*), and refuse without a terminal.
- **`refute`:** new `checkRefutation()` runs before the prompt, so a mistyped ID, `refuted-self`, a stub or a statement already refuted fails as itself. A stub's refusal names its paper's key, in `refute` and `edit-statement`.
- **S16 notice:** `loom new-paper` prints *"Set to false, since verifying is Dakota's click: …"* on stderr, and `--json` has `"unclicked"`.
- **Git:** `GitRun::timedOut`, `GitTool::timeoutMs`, `gitDidntRunMessage()`; a timeout is `git-timeout` with its own message in install-hooks, new-project and validate. `new-project` with a global `core.hooksPath` has its own message.
- **`build-bib --out`:** the CLI resolves the project folder and `--out` with `std::filesystem::weakly_canonical()` before comparing.
- **Tests:** EditsTests, HooksTests, FakeGit (`slow`), StatementsCli, ChecksCli, StatusCli and PapersCli. One line each in ProofChecksTests and StatusTests, for the new type.
- **Wiki:** two rows of [how to fix validation problems](../../how-to/fix-validation-problems.md), and a new gotcha, [a `;` in a CMake argument splits it](../../gotchas/cmake/semicolon-splits-arguments.md).

## Learned
- Windows makes 8.3 short names on this machine's volume, so `STATEM~1` is a real way into `Statements/`. Before the fix, `build-bib --out <project>/STATEM~1/refs.bib` wrote `Statements/refs.bib`, and `--out TeX/papers/…` through a junction overwrote a `paper.toml` with BibTeX.
- `installHooks` ignoring `QFile::setPermissions`' result couldn't be made to fail in a test: on Windows it only toggles read-only, and I found no setup where the `QSaveFile` commit just before it succeeds and it then fails. Not changed (not reproduced).

## Next
- Dakota's answers to the flags in the PR.
