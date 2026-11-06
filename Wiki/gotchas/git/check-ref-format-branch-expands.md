---
type: gotcha
tags: [git, branches, git-panel]
---

# `git check-ref-format --branch` accepts `@{-1}` and prints another name

> **Summary:** `git check-ref-format --branch <name>` doesn't only check a name: it expands `@{-1}` (and `@{-N}`) to the branch checked out before, prints that, and exits 0. Exit 0 alone doesn't mean `<name>` is a valid new branch name. Also check that what it prints is exactly the name given.

**Last verified:** 2026-11-05 · commit 6b59d6e

## Symptom
**New branch…** in the git panel (#39) would accept `@{-1}` as a name, because the check passed. `git switch -c @{-1}` then fails, or makes a branch with an odd name.

## Cause
`--branch` is meant for "what does this branch name mean": git runs the name through the same expansion as `git switch <name>`, so `@{-1}` becomes the previous branch. Only after that does it check the format.

## Fix
Compare the output with the input: `createBranch()` in `Source/LoomCore/GitOperations.cpp` refuses with `bad-branch-name` unless git exits 0 *and* prints the name unchanged. `GitOperationsTests` checks `a..b`, `@{-1}` and an empty name.

## See also
- Up: [Gotchas](../README.md)
- [`git check-ref-format`](https://git-scm.com/docs/git-check-ref-format): the `--branch` option
