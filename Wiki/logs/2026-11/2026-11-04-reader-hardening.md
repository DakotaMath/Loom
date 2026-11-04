---
type: log
tags: [m1, review, validate]
---

# 2026-11-04: M1 review fixes, batch 2 (reader hardening)

> **Summary:** Fixed batch 2 of the M1 review: empty values are `missing-field` in every reader (A2), every `QRegularExpression` rule is anchored at the very end, `FieldReader::date` reports the year 0, and `loom.toml` gets the UTF-8 check; each fix's test failed on `main` first, and all 180 `ctest` tests pass on Windows.

**Session model:** Opus 5.5
**Issues and PRs:** no issue (batch 2 of `Reviews/M1` on `m1/review`); PR m1/fix-reader-hardening

## Done
- `TomlFields`: `Need` is now `Optional` (free text), `Naming` (optional, names something) and `Required`; `FieldChecker::reportEmpty()` is the one rule for empty values, used by `FieldChecker::stringField`, `FieldReader::string`, `FieldReader::date`, the check-history reader and `loom.toml`'s `created`.
- Readers swept against A2: statements, proofs, verbose proofs, stubs (and `locators.*.at`), `paper.toml` (`key`, `cite`, `[work]` `type` and `title`, a name's `last`, versions' `id`, `kind`, `pdf`, readings' `version`, `level`, `what`, `session`), check histories and `loom.toml`.
- Anchored with `anchoredPattern()`: `isCheckAuthor`, the origin and verbose-fingerprint rules, `tag-syntax`, the arXiv and edition version IDs, and the session rule. Wiki: [gotcha: Qt's `$`](../../gotchas/qt/regex-dollar-before-final-newline.md).
- `FieldReader::date`: a date QDate can't hold (toml++ accepts `0000-01-01`) is `bad-value` and makes the reading not lossless.
- `readProjectToml` parses through `parsePlainToml`, so a bad byte is `not-utf8`.
- Tests: new cases in the statement, check, paper, stub and `loom.toml` tests; `Statements/RF5E.tex` (`refuted_by = ""`) in EveryProblem. Three existing assertions changed with the fix (listed under Flags in the PR).

## Learned
- toml++ checks days per month but accepts the year 0000, which `QDate` can't hold.
- A trailing-`\n` case can't go into EveryProblem: its expected output is one problem per line, and the message would contain the line break.

## Next
- Batches 3 to 6 of the review.

## Waiting on Dakota
- The PR's ⚠️ Flags: the wording for an empty optional field, which fields count as naming, and the year-0 message.
