---
name: Task
about: One task = one PR. See Wiki/start-here/workflow.md.
title: "<milestone> <area>: <task>"
labels: []
---

**Type:** <spec | build | tooling | docs> · **Milestone:** <e.g. M1> · **Size:** <S | M | L; S means minutes>
**Branch:** `<milestone>/<topic>`
**Depends on:** <#issue (title), … or "nothing">

## Goal
<One or two sentences.>

## Context
- Spec: <link, or "none: not a build task; this issue is the brief">
- Related decisions: <links>

## Allowed scope
- <Folders this task may change.>
- `Wiki/gotchas/`, `Wiki/logs/` and `STATUS.md` (always in scope)

## Out of scope
- <What it must not touch.>

## Done when
- [ ] <Check>
- [ ] Checks pass (indexes, tests, build)
- [ ] Windows checks listed in the PR, if anything user-visible changed
- [ ] Wiki updated for anything learned or changed
- [ ] Changed `STATUS.md` lines updated and session log written
