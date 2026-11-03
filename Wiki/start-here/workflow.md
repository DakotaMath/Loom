---
type: detail
tags: [workflow, issues, prs, milestones]
---

# Workflow

> **Summary:** Work happens one GitHub issue at a time, one PR per issue, in milestones M1–M12 from `Specs/loom.md`. Each milestone starts with a spec task that writes its spec in `Specs/`; build tasks start only once Dakota has approved that spec. Agents open PRs; Dakota reviews and merges every one.

**Last verified:** 2026-11-03 · commit c70b3e2

## Task types
| Type | What it produces | Needs an approved spec? |
| --- | --- | --- |
| **spec** | A spec in `Specs/` for a milestone or system, with open questions for Dakota | No: the issue is the brief |
| **build** | Code and tests implementing an approved spec | Yes |
| **tooling** | Scripts in `Tools/`, CI, repo checks | No |
| **docs** | Wiki pages, README | No |

## A milestone, start to finish
1. **Spec task.** An agent writes `Specs/<milestone>-<topic>.md` from `Specs/loom.md` and the template, listing everything undecided under Open questions.
2. **Dakota answers** the open questions on the PR and approves; the spec's status becomes "Approved by Dakota on <date>".
3. **Planning.** The spec is split into build issues, each one session and one PR, vertical slices a user can try where possible, with "Done when" lists and dependencies.
4. **Build tasks**, one PR each, reviewed and merged by Dakota. Windows checks are listed in each PR for Dakota to try.
5. **Wrap-up.** `STATUS.md` moves to the next milestone.

## Branches and PRs
- Branch from `main` as `<milestone>/<topic>`; never base a branch on another unmerged branch.
- The PR template's ⚠️ Flags, Noticed and Windows check sections are required.
- Dakota merges. Follow-ups to a merged PR go on a fresh branch and a new PR.

## Labels
- `needs-dakota`: blocked on a decision or approval.
- Task types as labels: `spec`, `build`, `tooling`, `docs`.

## See also
- Up: [Start here](README.md)
- [The design](../../Specs/loom.md): milestones and build order
