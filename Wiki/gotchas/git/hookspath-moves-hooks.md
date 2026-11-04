---
type: gotcha
tags: [git, hooks, core-hookspath, install-hooks, new-project]
---

# `git rev-parse --git-path hooks/…` follows `core.hooksPath` out of the repository

> **Summary:** Asking git where a hook goes (`git rev-parse --git-path hooks/pre-commit`) gives `core.hooksPath` when it is set, locally or globally, which can be a folder every repository on the machine shares. Writing a hook there changes every one of those repositories. Check that the answer is inside the repository's own git directory (`git rev-parse --git-common-dir`), and refuse if it isn't; `loom install-hooks` and `loom new-project` do (A1).

**Last verified:** 2026-11-04 · commit 106722b

## Symptom
With `core.hooksPath = C:/SharedHooks` in the global config, Loom before the fix:
- `loom install-hooks` renamed the shared `pre-commit` to `pre-commit.before-loom`, so that hook stopped running for every repository, and wrote Loom's hook in its place. Every commit in any other repository then ran `loom validate --staged`, found no project and was refused.
- `loom new-project` did the same, and `git lfs install --local` added `post-checkout`, `post-commit`, `post-merge` and `pre-push` to the shared folder.

## Cause
`git rev-parse --git-path hooks/<name>` answers "where does git look for this hook", and git looks in `core.hooksPath` when it is set. `--local` on `git lfs install` limits only which *config file* it writes; it still puts its hooks where git looks for them.

## Fix
- Ask for both `--git-path hooks/pre-commit` and `--git-common-dir`, make both absolute against the project folder, resolve links (`std::filesystem::weakly_canonical`), and accept the hook only inside the git directory. `--git-common-dir`, not `--absolute-git-dir`: a linked worktree's git directory is `.git/worktrees/<name>`, but git's documentation (`githooks`) puts its hooks in the common directory's `hooks/`. Not yet tried with a linked worktree.
- Before `git init`, the repository's own config doesn't exist yet, so ask the global and system config directly: `git config --get --type=path core.hooksPath` with `GIT_DIR` pointed at the future `.git` (so a repository the folder sits in isn't read). `--type=path` expands `~`.
- The way out the refusal gives: `git config --local core.hooksPath .git/hooks`. A relative `core.hooksPath` is relative to the top of the working tree, so this points git back at the repository's own hooks.

## Dead ends
- Writing to `.git/hooks` anyway: git never runs a hook there while `core.hooksPath` points elsewhere, so it would look like a commit guard and not be one.
- Checking only whether `core.hooksPath` is set: the way out above sets it, to a folder inside the repository, which is fine.

## See also
- Up: [Gotchas](../README.md)
- [`Specs/m1-core.md`](../../../Specs/m1-core.md), "`loom install-hooks`" and amendment A1
- `installHooks()` in `Source/LoomCore/Hooks.cpp`; `createProject()` in `Source/LoomCore/NewProject.cpp`
