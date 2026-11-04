---
type: gotcha
tags: [git, validate, rev-parse, dubious-ownership, locale]
---

# A failing `git rev-parse` doesn't mean "no repository" or "no commit yet"

> **Summary:** `git rev-parse` fails the same way (exit 128) for "not a git repository", for a repository git refuses to use ("dubious ownership") and for a damaged one. Treating every failure as "nothing to compare with" silently skips checks. Tell the cases apart: `rev-parse --git-dir`'s message, read with `LC_ALL=C`, for "no repository"; `rev-parse --verify --quiet HEAD^{commit}` exiting 1 with nothing on stderr for "no commit yet"; anything else is a failure to report.

**Last verified:** 2026-11-04 · commit be0d3a7

## Symptom
Before the fix, `loom validate` treated any failure of `git rev-parse --verify --quiet HEAD^{commit}` as "no commit yet". In a repository git won't use, `check-changed` and `deleted-since-commit` were skipped without a word, and validation reported a clean project.

## Cause
- `rev-parse` exits 128 for every fatal error: no repository, dubious ownership, a broken `.git`. Only the message differs.
- With `--verify --quiet`, a `HEAD` that names no commit (a fresh repository, before its first commit) exits 1 and prints nothing.
- git translates its messages, so matching "not a git repository" works only in English.

## Fix
`readCommit()` in `Source/LoomCore/Validation.cpp`:
1. `git rev-parse --git-dir` with `LC_ALL=C` in its environment. If it fails and stderr says `not a git repository`, there's nothing to compare with. Any other failure is `git-failed` (exit 3).
2. `git rev-parse --verify --quiet HEAD^{commit}`. Exit 1 with empty stderr: no commit yet. Any other failure: `git-failed`.

Tests use `loom-fake-git` (`Tests/LoomCoreTests/FakeGit.cpp`, modes `head-fails` and `dubious`), since a real git can't be made to fail like this on demand.

## Dead ends
- `git rev-parse --is-inside-work-tree` alone: it fails the same way for dubious ownership as for no repository.
- Telling the cases apart by exit code: both are 128.

## See also
- Up: [Gotchas](../README.md)
- [`Specs/m1-core.md`](../../../Specs/m1-core.md), "Checks", rule 1
- [How to fix what `loom validate` reports](../../how-to/fix-validation-problems.md)
