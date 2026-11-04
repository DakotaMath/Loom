---
type: log
tags: [m1, review, validate, status]
---

# 2026-11-04: M1 review fixes, batch 4 (validation and status)

> **Summary:** Fixed batch 4 of the M1 review: committed checks must keep their order (A7), `readCommit` reports git failures instead of skipping the commit checks, status explains a fingerprint it can't compute, `tagsInUse` counts every proof folder, and three new errors (`superseded_by` a stub, `\paperref` to a statement, refuting with one's own proof); each fix's test failed on `main` first, and all 196 `ctest` tests pass on Windows.

**Session model:** Opus 5.5
**Issues and PRs:** no issue (batch 4 of `Reviews/M1` on `m1/review`); PR #24

## Done
- `checkAgainstCommit`: the committed list must be a subsequence of the current one (greedy match in order); the message is the spec's new `check-changed` wording, and the line is the moved entry's, else where the entry should be.
- `readCommit`: `rev-parse --git-dir` (with `LC_ALL=C`) decides "no repository"; `rev-parse --verify --quiet HEAD^{commit}` exiting 1 silently decides "no commit yet"; anything else is `git-failed`. `loom-fake-git` gained `head-fails` and `dubious`. Wiki: [gotcha: `rev-parse` failures](../../gotchas/git/rev-parse-failures.md).
- `ProofChecking::fingerprintProblem` carries `proofFingerprint`'s reason; `lacks()` gives it.
- `ProjectContents::proofFolders` lists every tag-named folder in `Proofs/`; `tagsInUse` counts them and every checks file.
- New problems: `superseded-invalid` for a stub target; `paperref-not-stub` (error); `refuted-self` (error, and `refute` refuses it); `ref-dangling` has its own wording for `\paperref`.
- `StatementNode::paper` is the paper's key; `validateProject` makes its temporary folder only with `--staged`; doc comments for `Computation::isStub`, `ContentsChecker`'s constructor, `resolveProof`, and the struct comment's claim of linear checking corrected.
- EveryProblem: `SP3B`, `RF6S` (with `RF6S.a`) and a `\paperref` line in `RB0D`; the [how-to](../../how-to/fix-validation-problems.md) lists the new codes.

## Learned
- A plain `validate` making a temporary folder is visible only as a change to the temporary folder's modification time; the test points `TMP`/`TEMP`/`TMPDIR` at a folder of its own and checks `--staged` changes it, so the way of watching is itself tested.

## Next
- Batches 5 and 6 of the review.

## Waiting on Dakota
- The PR's ⚠️ Flags: the new codes and messages (S6, S22, S26), the explanation wording for an uncomputable fingerprint, and the spec amendments S19 and S20.
