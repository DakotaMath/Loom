# Loom: instructions for agents

Loom is a Windows desktop app (C++20, Qt 6 Widgets) for doing mathematical research with AI agents. A research project is a folder Loom creates and owns; statements are nodes, proofs are edges, and status is computed. The owner, Dakota, directs and reviews; agents do the building. **We are in the design phase:** there is no code yet, and the design is [`Specs/loom.md`](Specs/loom.md).

## Before you start: read these, in order
1. `STATUS.md`: the current milestone, what's in progress, what waits on Dakota.
2. `Wiki/INDEX.md`: the wiki's sections. Each section's `README.md` lists its pages.
3. Your task: the GitHub issue, then the spec it links to in `Specs/`.

Read only as deep as your task needs. Every page opens with a summary; stop there if it isn't what you need.

## Philosophy
- **The spec is the contract.** Build what the approved spec says. If the spec is wrong or silent, say so in the PR; don't decide quietly.
- **The project folder belongs to Loom.** Every file format is defined in a spec and written only through `LoomCore`. Nothing else may write project files.
- **Status is computed, never claimed.** Code that lets anyone set "proved" directly is a bug.
- **Never lose user text, never write without a click.** Unsaved edits survive failures and outside changes; nothing changes a project, git or GitHub unless the user clicked.
- **Knowledge lives in the wiki, not in your head.** What you learn, write down, so the next session doesn't relearn it.
- **Small steps.** One task = one PR.

## Directory layout
| Path | What |
| --- | --- |
| `Specs/` | Specs for the software, one per system or milestone. A Build task needs an approved one |
| `Source/` | C++: `LoomCore` (the project model, formats, validation, status; no UI code), `LoomCli` (the `loom` command), `LoomApp` (the Qt UI). From M1 |
| `Tests/` | Catch2 tests for `LoomCore` and `loom`; Qt Test for the UI. From M1 |
| `Templates/` | The project kit Loom copies into new projects (CC0) |
| `Tools/` | Small Python scripts for working on this repo, each with a test in `Tools/tests/` |
| `Wiki/` | Documentation: start-here, decisions, how-to, gotchas (by area), logs (by month), glossary |

## Hard rules
1. **Never start a Build task without an approved spec.** If it's missing, unapproved or has open questions, comment on the issue, label it `needs-dakota`, and stop.
2. **`LoomCore` has no UI code.** No Qt Widgets, no WebEngine; it must be testable on its own.
3. **Every change comes with tests,** and the checks pass before you open a PR.
4. **Never edit a generated index.** Section `README.md` tables are written by `python Tools/update_indexes.py`.
5. **Windows is tested by Dakota.** Cloud sessions and CI run Linux. Never claim something works on Windows that you could only test on Linux; list what needs checking under **Windows check** in the PR.
6. **New dependencies need a decision page** in `Wiki/decisions/` saying why, and must be well known and maintained. Mind the licence: Loom is GPL v3.
7. **Don't change tests in the same PR as the code they check** unless it's listed under ⚠️ Flags in the PR.
8. **Stuck after three failed attempts at the same problem? Stop.** Push the branch, open a draft PR with the write-up at the top (what you tried, what you learned, where the problem is), label the issue `needs-dakota`, and end the task.
9. **Never merge your own PR.** Dakota reviews and merges every PR.
10. **This repo is public.** Never commit secrets, personal paths or details of Dakota's private research projects. Examples use made-up papers and statements.

## Code documentation
- Every class and function gets a doc comment: purpose, parameters, returns, side effects.
- Comments explain why, not what.
- Code that computes status, staleness or fingerprints states the rule it implements and cites the spec section.

## Doing a task
1. Branch from `main`: `<milestone>/<topic>`, e.g. `m1/status-rules`. Right after cloning, run `git config remote.origin.fetch '+refs/heads/*:refs/remotes/origin/*'` so pushed branches track their GitHub copies.
2. Do what the issue asks, within its allowed scope. Anything more goes under **Noticed** in the PR.
3. Run the checks: `python Tools/update_indexes.py --check` and `python -m pytest Tools/tests` (and the C++ build and tests once they exist).
4. Update the wiki: new gotchas, decisions, changed behaviour.
5. Open a PR from the template, closing the issue. In cloud sessions use `gh api` REST paths; `gh pr` and `gh issue` need GraphQL, which is blocked.
6. Write your session log in `Wiki/logs/<YYYY-MM>/` and update the lines of `STATUS.md` that changed.

**Follow-ups on a merged PR:** check first (`gh api repos/DakotaMath/Loom/pulls/<n> --jq .merged`). If merged, branch fresh from `main` and open a new PR; never push to the merged branch.

## Keeping work proportionate
- **Do exactly what the issue asks.** Anything more goes under Noticed in the PR, not into the change.
- **New issues:** at most one per session, and only for something wrong or blocking.
- **Subagents:** none unless Dakota asks.
- **Size S tasks:** a short log (Summary and Done), only the changed `STATUS.md` lines. If one runs long, stop and say why.

## Formats
Never invent a format. Every page type has a `_TEMPLATE.md` in the folder where it's used; GitHub offers the issue and PR templates. Page rules: `Wiki/start-here/writing-wiki-pages.md`.

**Dates are US Central time** (America/Chicago), never UTC: log names and titles, `Last verified`, `STATUS.md`'s Updated line, decisions. Cloud sessions run in UTC, so convert first: `TZ=America/Chicago date +%F`.

## Learn more
- The design: `Specs/loom.md`
- How work is organised: `Wiki/start-here/workflow.md`
- Writing wiki pages: `Wiki/start-here/writing-wiki-pages.md`
