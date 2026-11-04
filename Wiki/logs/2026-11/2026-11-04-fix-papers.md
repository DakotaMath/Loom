---
type: log
tags: [m1, review, papers, bibliography]
---

# 2026-11-04: M1 review fixes, batch 3 (papers and the bibliography)

> **Summary:** Fixed batch 3 of the M1 review: keys compare ignoring case (A3), step 4 of the key rule is checked and `rekey` repairs it, `arxiv_id` and `primary_class` have their form checked (A4), BibTeX names and braces follow A5 and A6, and `new-paper` no longer writes Dakota's clicks; each fix's test failed on `main` first, and all 209 `ctest` tests pass on Windows.

**Session model:** Opus 5.5
**Issues and PRs:** no issue (batch 3 of `Reviews/M1` on `m1/review`); PR #25

## Done
- **Keys (A3, step 4):** `assignKey` compares keys and old keys ignoring case, and a renamed paper keeps its own spelling of the base. New `keyFollowsRule()`: an unlettered base beside a lettered one breaks step 4. Validation's `paper-key-rule` and `loom rekey` both use it. `paper-key-duplicate` compares case-folded. `paper-key-rule` stays quiet when the paper has no authors or editors, since `authors-missing` already reports it.
- **Renames:** `new-paper` and `rekey` refuse with `paper-exists`, before renaming anything, when a folder is already where a rename goes (compared ignoring case). A rename that only changes the case of a folder's name now works on Windows.
- **Reading `paper.toml`:**
  - `arxiv_id` and `primary_class` are checked (`bad-value`).
  - Empty string values are `missing-field`, and the field is kept with its source.
  - An `http://` source gets its own `field-source` message.
  - The year 0 is `bad-value` in field values, `verified`, `checked`, `checked_against_pdf` and `loom.toml`'s `created`, through the new `FieldChecker::date()`.
  - A draft is named by its file in messages.
- **Bibliography:** `{Last}` for an empty `first`; every brace counts; `\\%` is escaped; problems name the paper's real `paper.toml` (`BibSource`); `arxivNumber` is now one function, `PaperVersion::arxivNumber()`.
- **`new-paper` (S16):** a draft's `verified` and `checked` dates are written as `false`, and the top-level `verified` is left out. They're listed in `PaperChange::unclicked`.
- **`loadPapers`:** a `paper.toml` refused as `bad-path` is no longer also "missing".
- **Wiki:** the paper rows of [how to fix validation problems](../../how-to/fix-validation-problems.md), and a new gotcha, [BibTeX names and braces](../../gotchas/bibtex/names-and-braces.md).

## Learned
- The other fixers share this session's scratchpad folder, so build logs there need names of their own.
- Windows refuses to rename a folder while a file in it is open through `QFile`. That's a portable-enough way to make the second of two renames fail in a test. Elsewhere, a folder without write permission makes the `paper.toml` rewrite fail instead.
- `QDir::rename` handles a rename that only changes case on Windows; Loom's own "already exists" check was what blocked it.

## Next
- `loom new-paper` should print `PaperChange::unclicked` (LoomCli is batch 5's).
- Dakota's answers to the flags in the PR.

## Waiting on Dakota
- The PR's ⚠️ Flags: S16's reading, the `primary_class` rule, the `http://` code, the `paper-key-rule` messages (S18), and the changed tests.
