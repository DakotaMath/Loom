---
type: spec
area: M4 tasks and jobs
tags: [m4, tasks, jobs, claude-code, queue, records, review, merge, retag, markdown, kit]
---

# Spec: M4, tasks and jobs

> **Summary:** Everything `LoomCore`, `loom` and `LoomApp` do in M4, the milestone where agents start working on Dakota's projects: tasks (`Tasks/<task-id>.toml`, their computed status, the Tasks tab, Dakota's `needs-changes` becoming a task), Claude Code jobs run headless on their own branch in a worktree under `.loom/` (kinds, the exact command line, how tools and folders are enforced, how a job ends, fails, is cancelled or survives a crash), the queue and the Jobs tab (usage limits, what "cost" means on a subscription), records (`Records/<YYYY-MM>/<job-id>.md`), the new project kit, job branches in Review with merge, ask for changes and discard, merging with retagging, what stops an agent acting as Dakota, rendered Markdown (taken over from M2), a fake Claude Code for tests, and the build issues' sections.

**Last verified:** 2026-11-06 · commit cb0a446 · Claude Code 2.1.318
**Status:** Draft, waiting for Dakota's answers to the open questions
**Milestone:** M4

## Scope
- **Covers:** the project format 2 and kit 1; tasks and their `loom` commands; Dakota's and agents' `needs-changes` becoming tasks; job kinds and how later milestones add theirs; running a job (worktree, branch, command line, enforcement, ending, failing, cancelling, crashes); the queue, usage limits and cost; the Tasks and Jobs tabs; records, their validation, and records shown on objects and in a timeline; job branches in Review; merging a job branch, retagging and proof-letter collisions, `loom merge-job`; what stops a job acting as Dakota; rendered Markdown (`MarkdownView`); settings and *Check my setup*; dependencies, CI, the fake Claude Code; the demo project's additions; tests and Windows checks.
- **Does not cover:** the job kinds of later milestones (gather literature and paper jobs: M6; spec, first pass and per-proof jobs, agent checks: M7; thread: M8; numerics: M9; the reviewers' implementing job and the triage task: M10). The Graph tab's *Create task* (M5). Home (M12), so the offer to make a task from an agent's `needs-changes` lives in the Tasks tab for now. Editing Markdown files other than task briefs (M12). GitHub issues and pull requests (later). Running jobs on another machine or in the cloud.

## The build issues
Each build issue reads the sections named here; they don't need the rest. Every issue also reads *Departures*, *Decisions* and the open questions' answers.

| Issue | Sections |
| --- | --- |
| #53 tasks in `LoomCore` and `loom` | *Project format 2* (the `Tasks/` parts), *Tasks*, *Dakota's `needs-changes` becomes a task* (all but the Tasks tab's strip), *Rules: task status*, the `tasks` overlay of *The demo project* |
| #54 records in `LoomCore` | *Records* (all but *Records on objects and the timeline*), the record lines of *Rules: plain words for tasks and records*, the demo's record (*The demo project*) |
| #55 the project kit for jobs | *Project format 2*, *The project kit*, *Job kinds* (the kind files and prompts) |
| #56 merging a job branch in `LoomCore`, with retagging | *Merging a job branch*, *Rules: retagging*, *Rules: changes only Dakota can make*, *Agents can't act as Dakota*, the branch fixtures of *The demo project* |
| #57 the Tasks tab | *The Tasks tab*, *Dakota's `needs-changes` becomes a task* (the strip and the dialog), *Rules: task status* |
| #58 running a Claude Code job on its own branch | *Job kinds*, *Running a job* (all of it), *Agents can't act as Dakota*, *Settings and Check my setup*, *The fake Claude Code*, the Claude Code row of *Dependencies* |
| #59 the job queue and the Jobs tab | *The queue*, *The Jobs tab*, the badges in *The app in M4* |
| #60 job branches in Review | *Job branches in Review*, the merge, ask-for-changes and discard dialogs, *Rules: plain words for tasks and records* |
| #61 records on objects and the timeline | *Records on objects and the timeline* |
| #41 rendered Markdown in Review | *Rendered Markdown*, its rows of *Dependencies*, the WebEngine parts of *CI* |

**Order.** #53, #54, #55 and #41 depend on nothing in M4. #56 needs #54 (records name their task and touched objects). #58 needs #53–#55. #57 needs #53 (and #58 for its **Start job…** button, which it may leave disabled until #58 is merged). #59 needs #58. #60 needs #56, #58 and #41. #61 needs #54 and #41.

## Departures from the design, M1 and M2
Places where this spec makes `Specs/loom.md`, `Specs/m1-core.md` or `Specs/m2-statements-review.md` more precise in a way they didn't say, or changes them. Dakota approves them with the spec.

| # | Earlier spec says | This spec says | Why |
| --- | --- | --- | --- |
| F1 | Design: a task's file holds its status (idea, ready, running, waiting-on-dakota, done) | The five statuses stay, but only **done** is stored (`closed` and `outcome`); the others are computed from the task's type, its jobs and its branches (*Rules: task status*) | "Status is computed, never claimed." A stored `running` would have to be written into Dakota's working tree every time a job starts or ends, without a click, and would be wrong after a crash |
| F2 | Design: a task's file lists "the job records it produced" | Each record names its task (`task`); a task's records are found from them, never written into the task | The record is committed on the job's branch; writing its path into the task would mean a second write on `main` |
| F3 | Design: a task ends as done | A task is closed as **done** or **dropped** | Ideas and tasks are often abandoned; deleting the file would lose the brief |
| F4 | (silent) | Agents never write `Tasks/`. A record's *Noticed* items become tasks only by Dakota's click in Review | Tasks are Dakota's to-do list; jobs writing tasks on branches would also make task IDs collide at merge |
| F5 | Design: a job's commit fails if validation fails | Loom, not the agent, commits a job's work, in the job's worktree, through the pre-commit hook; the agent never runs `git` | One place decides what a job's commit holds, and an agent without `git` can't touch `main`, other branches or the remote |
| F6 | (silent) | Jobs run only while Loom is open. Closing Loom stops running jobs; they can be resumed when the project opens again | A job is a child of the app, so its progress, limits and ending are always seen by the app; nothing runs unwatched |
| F7 | M1: retagging happens at merge | Loom retags when a job ends (against `main` as it is then) and again at merge if `main` has moved on | Review then shows the tags the statements will have, and a retag at merge is rare |
| F8 | M1: agents must not act as Dakota (`CLAUDE.md` and the typed `yes`) | A branch with a change only Dakota can make can't be merged (*Rules: changes only Dakota can make*); `loom` refuses every *(Dakota)* subcommand inside a job, terminal or not | M1 called the typed `yes` "a guard against mistakes, not against an agent set on cheating; M4's review of job branches is the real gate". This is that gate |
| F9 | M1: a check's missing `record` is a warning "until M4 writes records" | For a check that isn't in `HEAD`, an agent check's `record` must exist and be in `Records/` or `Reviews/` (an error); committed checks keep the warning | New agent checks always come from jobs that write records; old projects mustn't stop committing |
| F10 | Issue #52: job prompt templates in `Templates/project/` | `CLAUDE.md` and the agent definitions are copied into projects; job kinds and prompts are in `Templates/jobs/`, compiled into Loom and never copied | A prompt is filled in by Loom for each job; a copy in every project would go stale and nothing would read it |
| F11 | Design: the queue shows each job's cost | On a subscription, "cost" is Claude Code's API-price estimate (`total_cost_usd`), shown as *≈ $1.20 at API prices*; what limits jobs is the usage windows, shown beside it | Dakota pays a flat subscription; the estimate is still the best measure of a job's size |
| F12 | M2: Review compares uncommitted edits with `HEAD` | For a job branch, Review's before and after are the merge base and the branch's tip; its status changes are from `main`'s tip to the merged result | Before and after must be commits to build; status changes must say what merging does to `main` now |
| F13 | M1: `loom.toml` has `format = 1` | M4 writes `format = 2` and a new field `kit` (*Project format 2*) | Tasks and records are new formats; the kit changes more often than formats and needs its own number |

## Decisions this spec makes
| Decision | Choice | Why |
| --- | --- | --- |
| Where job logic lives | In `LoomCore` (Qt Core: `QProcess`, files, git): the command line, the stream parser, the end of a job, the queue's order, records, merging. The app schedules and shows | Catch2 can test all of it against the fake Claude Code |
| How Claude Code is run | `claude -p` with `stream-json` output, one process per run, the prompt on stdin, in the job's worktree | The design's choice (headless); stdin avoids Windows' command-line length limit |
| How a job is confined | Three layers: Claude Code's permissions (`--restricted`, `dontAsk`, allowed tools and paths); `loom` refusing Dakota's commands and other projects inside a job; and Loom's check of the branch before it commits and before any merge | Each layer is a guard on its own; only the last is a gate, and it reads what actually changed |
| Which account pays | The subscription: Loom removes `ANTHROPIC_API_KEY` and `ANTHROPIC_AUTH_TOKEN` from a job's environment | Claude Code prefers an API key in the environment over the subscription sign-in; jobs must never bill an API key by accident |
| Job IDs | `job-` and a number of at least four digits, one more than the highest in `Records/`, `.loom/jobs/` and `loom/job-*` branches | Matches the design's `job-0142`; sequential is readable, and jobs are only made on one machine |
| Task IDs | `T-` and four characters from the tag alphabet, random | Readable, never mistaken for a statement's tag, and safe if tasks ever come from two places (GitHub, later) |
| Branches and worktrees | Branch `loom/job-0142` from `main`'s tip; worktree `.loom/jobs/job-0142/tree/`. A follow-up job reuses its branch and worktree | The design: own branch, own worktree under `.loom/` |
| Where queue and job state live | `.loom/jobs/` (gitignored), written atomically by `LoomCore` | It is this machine's state; records, the lasting part, are committed |
| Who writes a record | Loom writes its front matter and the *Asked* and *Run* sections; the agent writes *Done*, *Noticed* and *Flags* | Facts Loom knows (what was asked, cost, what changed) never depend on the agent's word |
| `MarkdownView` | The app's only use of Qt WebEngine, as M2's appendix designed | Unchanged (*Rendered Markdown*) |

## Behaviour

### Project format 2
M4 reads format 1 and 2 projects and writes format 2. `loom.toml` gains one field:

| Field | Required | Type and rule |
| --- | --- | --- |
| `kit` | format 2: yes | Integer ≥ 1: the version of Loom's project kit the project's `CLAUDE.md` and `.claude/agents/` were written from. M4's kit is `1`. A format 1 project has none (kit 0, M1's `CLAUDE.md`) |

- **New folders** Loom reads: `Tasks/` (*Tasks*), `Records/` (*Records*) and `.claude/agents/` (*The project kit*). Validation stops skipping `Tasks` and `Records` as later names, and adds `.claude` to the names it knows at the top.
- **`loom new-project`** writes `format = 2`, `kit = 1` and the kit's files.
- **A format 1 project** opens as before. The Tasks and Jobs tabs show a bar: *"This project has Loom's first project kit. Tasks and jobs need the new one."* **Update the project…** (*Updating the kit*). Until then tasks can't be created and jobs can't start; checks work as in M2, and Dakota's `needs-changes` makes no task (it is offered later, *Dakota's `needs-changes` becomes a task*).

### Tasks
One plain TOML file per task (no front matter), `Tasks/<task-id>.toml`.

```toml
id = "T-4RTW"
title = "Fix step 3 of 4KX7.a"
type = "prove"
for = "agent"
about = ["4KX7.a", "4KX7"]
depends_on = ["T-9QZP"]
created = 2026-11-06
from_check = { proof = "4KX7.a", date = 2026-11-06T14:02:11-06:00 }
brief = """
Step 3 uses Lemma 7RT2 with k = 0, but 7RT2 needs k ≥ 1. Either prove the k = 0 case
as a separate lemma or start the induction at k = 1 and treat k = 0 by hand.
"""
```

| Field | Required | Type and rule |
| --- | --- | --- |
| `id` | yes | A task ID (below), equal to the file name without `.toml` |
| `title` | yes | Non-empty string, one line |
| `type` | yes | `read`, `spec`, `prove`, `check`, `numerics`, `thread`, `lean`, `chore` or `idea` |
| `for` | no (default `agent`) | `agent` (a job does it) or `dakota` (Dakota does it himself: reading a paper, triaging reviews in M10) |
| `about` | no | Array, no repeats, of statement tags, proof IDs, stub tags and paper keys (current keys; an old key or alias is an error naming the current one, as in front matter) |
| `depends_on` | no | Array of task IDs, no repeats, not itself, no cycles |
| `created` | yes | Date |
| `from_check` | no | `{ proof = "<proof-id>", date = <offset date-time> }`: the check this task was made from; it must match a check of that proof exactly by `date` |
| `closed` | with `outcome` | Date of Dakota's click closing the task |
| `outcome` | with `closed` | `done` or `dropped` |
| `brief` | no | Markdown: what to do and why, the task's whole instruction to a job. Written as a multi-line string when it has line breaks |

- **Canonical form,** the empty-value rules, unknown fields and atomic writes are M1's (*Front matter*), applied to the plain TOML file.
- **Task IDs:** `T-` then four characters of M1's tag alphabet, drawn with `QRandomGenerator::system()` until unused among tasks (all-digit ones allowed: `T-2024` can't be read as a number). On the command line, `t-4rtw`, `T4RTW` and the letter fixes of M1 (`O` → `0`, `I`/`L` → `1`) are accepted. Task IDs are a namespace of their own.
- **Tasks are never deleted** by Loom; closing is `closed` and `outcome`. A deleted file is M1's `deleted-since-commit` warning, worded for a task.

#### What a task's status is
Computed, never stored (F1), by *Rules: task status*: **idea**, **ready** (possibly *blocked*), **running**, **waiting on Dakota**, **done** (or *dropped*, shown with done).

| Change | How |
| --- | --- |
| Created as *ready* or *idea* | Dakota's click (New task, or the `needs-changes` dialog); *idea* when `type = "idea"` |
| *idea* → *ready*, and back | Dakota changes `type` (a click in the form). **An idea never starts a job**: Start job refuses it, `LoomCore` refuses it, and `loom` has no way round it |
| *ready* → *running* | Loom, when a job for it is queued (no write) |
| *running* → *waiting on Dakota* | Loom, when the job ends and its branch waits for review (no write) |
| *waiting on Dakota* → *running* | Dakota's **Ask for changes** queues a follow-up job |
| *waiting on Dakota* → *done* | Dakota's **Merge**, with *Mark the task done* ticked (written in the merge commit) |
| *waiting on Dakota* → *ready* | Dakota's **Discard**, or **Merge** with the tick cleared |
| any → *done* / *dropped* | Dakota's **Mark done** / **Drop** (writes `closed` today and `outcome`) |
| *done* / *dropped* → back | Dakota's **Reopen** (removes `closed` and `outcome`) |

**Dependencies.** A task is *blocked* while any task in `depends_on` isn't closed. It stays in *ready* (its card says *waits for T-9QZP*) and can't start a job. A dependency closed as *dropped* unblocks it, with a warning on the card: *"T-9QZP, which this task depends on, was dropped."*

#### Which tasks can start which jobs
A job kind lists the task types it serves (*Job kinds*). In M4, **chore** serves every type but `idea`, and **revise-proof** serves `prove` tasks whose `about` has exactly one proof. Start job is refused for an idea, a `for = "dakota"` task, a blocked task, a task with an empty brief (*"Write a brief first: it is the job's instructions."*), and a task with a job queued, running, or waiting for review (*"job-0142 for this task is waiting for your review. Merge it, discard it, or ask it for changes."*).

#### Validation of tasks
Plus M1's file and field codes (`toml-syntax`, `unknown-field`, `missing-field`, `wrong-type`, `bad-value`, `id-mismatch`, `stray-file` for anything in `Tasks/` that isn't `T-xxxx.toml`, `ref-alias`).

| Code | Sev. | Message |
| --- | --- | --- |
| `task-id-syntax` | error | `{id}` isn't a valid task ID: `T-` and four characters from 0–9 and A–Z without I, L, O and U. {Did you mean `{normalised}`?} |
| `task-about-unknown` | error | `{ref}` in `about` isn't a statement, proof, paper result or paper of this project. |
| `task-depends-unknown` | error | `{ref}` in `depends_on` isn't a task of this project. |
| `task-depends-self` | error | {id} can't depend on itself. |
| `task-depends-cycle` | error | Tasks depend on each other in a circle: {A} → {B} → {A}. |
| `task-closed-incomplete` | error | {id} has `{closed / outcome}` but not `{the other}`. A closed task needs both: the date and done or dropped. |
| `task-from-check-unknown` | error | {id} was made from a check of {proof} dated {date}, but {proof} has no such check. |
| `task-depends-dropped` | warning | {id} depends on {dep}, which was dropped. |

The how-to page of validation codes (`Wiki/how-to/`) gains these, and those of *Records*.

#### `loom` task commands
Every command that writes a task is *(Dakota)*: it asks for the typed `yes` (M1, *Dakota's clicks*), and refuses inside a job (*Agents can't act as Dakota*). All take the common options and print JSON with `--json`.

| Subcommand | Arguments | Does | JSON |
| --- | --- | --- | --- |
| `tasks` | `[--status idea\|ready\|running\|waiting\|done] [--type <type>] [--about <id>]` | Lists tasks: `ID  status  type  title`, then counts by status. Read-only | `{"tasks": [{"id", "title", "type", "for", "status", "blocked_by": [id…], "about": [id…], "jobs": [job-id…], "records": [path…]}…], "counts": {…}}` |
| `task` | `<task-id>` | One task: its fields, status sentence, jobs and records | One element of `tasks`, plus `"brief"`, `"from_check"`, `"explanation"` |
| `new-task` | `--title <t> --type <type> [--for agent\|dakota] [--about <id>]… [--depends-on <task>]… (--brief <text> \| --brief-file <path>)` *(Dakota)* | Creates `Tasks/<new id>.toml`, `created` today | `{"id", "path"}` |
| `new-task` | `--from-check <proof-id>` *(Dakota)* | Makes the task of *Dakota's `needs-changes` becomes a task* from the proof's counting `needs-changes` check (Dakota's or an agent's). Refused if there is none, or it already has a task | `{"id", "path"}` |
| `edit-task` | `<task-id> [--title …] [--type …] [--for …] [--add-about …] [--remove-about …] [--add-depends-on …] [--remove-depends-on …] [--brief-file <path>]` *(Dakota)* | Changes the task. Refuses a closed task (*"Reopen T-4RTW first."*) | `{"id", "changed": [field…]}` |
| `close-task` | `<task-id> --outcome done\|dropped` *(Dakota)* | Sets `closed` today and `outcome` | `{"id", "status"}` |
| `reopen-task` | `<task-id>` *(Dakota)* | Removes `closed` and `outcome` | `{"id", "status"}` |

A change that would add a validation error is refused, naming it (M2's E6), as for statements. Exit codes are M1's.

### Dakota's `needs-changes` becomes a task
M1 (D10) decided that Dakota's `needs-changes` carries a `changes` message which, from M4, becomes a task.

**What the click creates.** When Dakota records `needs-changes` on a proof (the Statements tab's **Needs changes…**, or `loom add-check --by dakota --verdict needs-changes`), `LoomCore`'s `addCheck` writes the check and then, under the same write lock, a task:

| Field | Value |
| --- | --- |
| `title` | `Fix 4KX7.a: ` and the first line of `changes`, cut at 60 characters with `…` |
| `type` | `prove` |
| `about` | `[<proof-id>, <statement tag>]` |
| `created` | today |
| `from_check` | `{ proof = "<proof-id>", date = <the check's date> }` |
| `brief` | `changes`, verbatim, then a blank line and *"From your needs-changes check of {date, local time to the minute} on {proof-id}."* |

- The dialog says so: *"Record your needs-changes for 4KX7.a? It also creates a task to fix it, ready to start a job."* `addCheck`'s result (and `add-check --json`) gains `task`: the new task's ID, or `null`.
- **The check is written first.** If the task can't be written (a disk error, say), the check stays, the result carries the warning `task-not-created` (*"Loom recorded your check but couldn't create its task: {reason}. The Tasks tab offers it again."*), and the strip below offers it.
- **In a format 1 project** no task is made; the strip offers it after the kit is updated.

**Checks that wait for a task.** A **waiting check** is the *counting* `needs-changes` check of a proof (M1, rule 5: by Dakota or by an agent) that no task names in `from_check`. Checks written before M4 are waiting checks; so are agents' `needs-changes` (from M7, and in the demo). Nothing is made from them without a click:
- **The Tasks tab** shows a strip above the board while there are any: *"3 checks ask for changes and have no task yet."*, expanded to one line each: `4KX7.a · Dakota, 2026-11-04: "Step 3 uses …"` or `G8TV.a · agent, 2026-11-20 (record)`, each with **Make a task** and **Not now**. **Make tasks for all** makes one per line. *Not now* hides a line in this app (per-project app state, M2's `%LOCALAPPDATA%` file), keyed by proof and check date.
- **The Statements tab's check history** shows **Make a task** under a waiting check, and **Task T-4RTW** (a link) under a check that has one.
- A task from an agent's check has the same fields, with the brief being the check's `changes` if it has one, then *"From the agent's needs-changes check of {date} on {proof-id}. Its reasoning: {record path}."*
- A waiting check stops waiting when the proof changes (the check no longer counts), or when a newer check counts.
- `loom new-task --from-check <proof-id>` does the same at a terminal.

### Records
One Markdown file with TOML front matter per job, `Records/<YYYY-MM>/<job-id>.md`, where `<YYYY-MM>` is the month the job started (local time). Every job writes one, finished or not.

```markdown
+++
job = "job-0142"
kind = "revise-proof"
task = "T-4RTW"
branch = "loom/job-0142"
follows = "job-0139"
base = "cb0a446f3e2d9a1b7c4e5f60718293a4b5c6d7e8"
model = "<model>"
claude_code = "2.1.318"
started = 2026-11-06T10:12:44-06:00
ended = 2026-11-06T10:31:02-06:00
outcome = "finished"
touched = ["4KX7", "4KX7.a", "N3PV"]
turns = 34
cost_usd = 1.23
+++
## Asked
<the prompt the job was given, exactly>

## Done
<what the agent did, in its words>

## Noticed
- <something outside the task, one item each>

## Flags
- <something Dakota must look at, one item each; "None." when there is nothing>

## Run
<written by Loom: duration, usage-limit pauses, fix rounds, permission denials, validation>
```

| Field | Required | Type and rule |
| --- | --- | --- |
| `job` | yes | The job ID, equal to the file name |
| `kind` | yes | A job kind Loom knows |
| `task` | yes | The task ID (on a branch it may be a task only `main` has; on `main` it must exist) |
| `branch` | yes | The job's branch, `loom/job-<n>` |
| `follows` | no | The previous job on the same branch, for a follow-up |
| `base` | yes | Full hash of the commit the branch started from |
| `model` | yes | The model Claude Code reported (the `system` `init` event's `model`), as `[a-z0-9][a-z0-9.-]*` |
| `claude_code` | yes | Claude Code's version, from the same event |
| `started`, `ended` | yes | Offset date-times; `ended` ≥ `started` |
| `outcome` | yes | `finished`, `failed`, `cancelled` or `interrupted` (*How a job ends*) |
| `touched` | no | Statement tags, proof IDs, stub tags and paper keys whose files the job changed, computed by Loom from the branch's diff, in tag order |
| `turns` | yes | Integer: the agent's turns over every run of the job |
| `cost_usd` | yes | Number: the sum of Claude Code's `total_cost_usd` over every run (F11) |

- **Sections** are exactly the five headings above, in that order, each a line `## <name>`. The agent writes the text under **Done**, **Noticed** and **Flags**; Loom writes the front matter, **Asked** and **Run**. When Loom rewrites its parts at the end of a job, the agent's three sections are kept byte for byte; a missing heading is put back, empty, and *Run* says so.
- **Noticed** and **Flags** are Markdown lists: each top-level `- ` item is one notice or flag. Review shows each flag at the top of the branch, and each notice with **Make a task**.
- **Commits.** A job's record is committed on its branch with the rest of its work (*How a job ends*), so on `main` it appears only when the branch is merged.
- **Agents' checks** (M7) link their job's record in `record`, as M1 always required.

**Validation of records**, plus M1's front-matter codes:

| Code | Sev. | Message |
| --- | --- | --- |
| `record-path` | error | `{path}` must be `Records/{YYYY-MM}/{job}.md`, from its `job` and the month it started. |
| `record-section` | warning | {job}'s record has no `## {section}` section. Loom's records have Asked, Done, Noticed, Flags and Run, in that order. |
| `record-task-unknown` | warning | {job}'s record is for task {task}, which isn't in this project. |
| `record-touched-unknown` | warning | {job}'s record lists `{id}` in `touched`, which isn't in this project. |
| `record-follows-unknown` | warning | {job}'s record follows {other}, which has no record in this project. |

**A check's `record` (F9).** M1's `check-record-not-found` stays a warning for checks that are in `HEAD`. For a check that isn't in `HEAD` (new, as a job adds it), a `record` that doesn't exist is an error, `check-record-missing-file`, and one outside `Records/` and `Reviews/` is an error, `check-record-folder`: *"Check {n} links `{path}`; an agent's reasoning lives in `Records/` or `Reviews/`."*

### The project kit
Loom's kit, `Templates/` in Loom's repo (CC0), compiled into `loom` and the app as Qt resources:

```
Templates/project/                 copied into projects (kit 1)
  CLAUDE.md
  claude/agents/loom-chore.md      copied as .claude/agents/loom-chore.md
  claude/agents/loom-revise-proof.md
  TeX/, gitignore, gitattributes   as in M1
Templates/jobs/                    never copied (F10)
  chore.toml, chore.md             a kind's definition and its prompt
  revise-proof.toml, revise-proof.md
  fix-round.md, resume-limit.md, resume-interrupted.md, follow-up.md
```

`claude/` is stored without its dot and renamed when copied, as `gitignore` is (M1), so it doesn't act on Loom's own repo.

#### `CLAUDE.md` (kit 1)
Replaces M1's. It is the kit's text, not a template: the same for every project. #55 writes it; it must say, in this order:
1. **What this is:** a Loom project; Loom owns every file; statements are nodes, proofs are edges, status is computed. *"Run `loom help` for the commands and `loom status` for where things stand."*
2. **You are in a job:** on your own branch in a worktree; Dakota reviews everything before it reaches `main`. Work only on the task you were given; put anything else under *Noticed*.
3. **How to change files:** with `loom` commands (new statements and proofs, tags, edits of front matter) or by editing bodies; never invent a tag (use `loom status`, `loom graph --json`); `\ref` tags, never aliases; run `loom validate` before you finish and fix what it reports.
4. **What you must never do** (Dakota's clicks), M1's list plus: never run `git`; never write `Tasks/`, `CLAUDE.md` or `.claude/`; never edit a check history by hand; never edit another job's record.
5. **Your record:** its path is in your prompt; write *Done* (what you did and why), *Noticed* and *Flags* (anything Dakota must check, such as a statement you changed or a step you're unsure of; "None." if nothing). Don't touch the other sections.
6. **Mathematics:** LaTeX with the project's macros (`TeX/macros.tex`); a statement's body is the statement only; a proof's body is the proof only; a dead end is a proof Dakota marks failed, so describe it in *Flags* instead.
7. **Where to read more:** the project's `Wiki/decisions/` and `Wiki/glossary.md` when they exist (notation and conventions are Dakota's choices there).

#### Agent definitions
`.claude/agents/loom-<kind>.md`, one per M4 kind, in Claude Code's subagent format: front matter (`name`, `description`) and a short body saying the role (*"You revise one proof of a Loom project so that it answers the changes Dakota asked for."*). They carry no `tools` field: tools are set on the command line (*The command line*), so a copy that drifted couldn't widen them. Loom reads the definition from the job's worktree and passes it to Claude Code (`--agents`), so jobs use the kit on the branch they run on, and Dakota can use the same agents in an interactive `claude` session in the project.

#### Job kinds
A kind is a pair in `Templates/jobs/`: `<kind>.toml` and its prompt `<kind>.md`.

```toml
kind = "revise-proof"
title = "Revise a proof"
agent = "loom-revise-proof"
task_types = ["prove"]
about = "one-proof"        # any | one-proof | one-statement | one-paper
tools = ["Read", "Glob", "Grep", "Edit", "Write", "Bash(loom *)"]
change = ["Proofs/{statement}/"]
create = ["Statements/", "Proofs/"]
minutes = 60
```

| Field | Means |
| --- | --- |
| `task_types` | The task types it serves (`"*"` for all but `idea`) |
| `about` | What the task's `about` must hold for this kind; `{statement}`, `{proof}`, `{paper}` in `change` and in the prompt come from it |
| `tools` | Claude Code tools and rules it may use (*The command line*). Never `Bash` without a pattern; never web tools in M4 |
| `change` | Folders (or files) whose existing files it may change; its own record is always added |
| `create` | Folders where it may create files (by `loom` commands or plain writes) |
| `minutes` | Its default time limit (the setting overrides it) |

**M4's kinds:**

| Kind | Serves | Changes | Creates | The prompt asks it to |
| --- | --- | --- | --- | --- |
| `chore` | every type but `idea` | `Statements/`, `Proofs/`, `Wiki/`, `loom.toml` (tags only, through `loom new-tag`) | `Statements/`, `Proofs/`, `Wiki/` | Do what the brief says, within the project's rules |
| `revise-proof` | `prove`, `about` with one proof | `Proofs/{statement}/` (the proof, its verbose version) | `Statements/` and `Proofs/` (new lemmas the fix needs, as `open` or with proofs) | Revise `{proof}` so that it answers the brief; keep its premises honest (add or remove them with `loom edit-proof`); never touch its checks |

**The prompt** is Markdown with placeholders Loom fills: `{{task.id}}`, `{{task.title}}`, `{{task.brief}}`, `{{about}}` (each ID with its title, kind and status sentence, from `LoomCore`), `{{record}}` (the record's path), `{{job}}`, `{{branch}}`, `{{base}}`, `{{note}}` (Dakota's note from the Start job dialog, or empty), and for follow-ups `{{changes}}` (Dakota's message) and `{{previous_records}}` (the paths of the branch's earlier records). An unknown placeholder is a build error of Loom's kit test, not a runtime surprise.

**How later milestones add kinds:** a new pair of files in `Templates/jobs/`, an agent definition in `Templates/project/claude/agents/` (which raises `kit`), and, when the kind needs inputs the placeholders don't cover (a PDF for M6's paper job, a review for M10's), the code that supplies them; plus its row in this table in that milestone's spec. Nothing else in the job machinery changes.

#### Updating the kit
A project whose `kit` is lower than Loom's (or a format 1 project) is offered the update:
- **In the app**, the bar on the Tasks and Jobs tabs (above), and **Project → Update the project kit…**. The dialog lists every file written and what changes (*"CLAUDE.md: replaced by Loom's new instructions for agents"*, *".claude/agents/loom-chore.md: new"*, *"loom.toml: format 1 → 2, kit 1"*), with **Update and commit** (Cancel focused).
- **`loom update-kit`** *(Dakota)* does the same.
- It writes the files, then commits exactly those paths (`git commit -m "Update the project kit to 1" -- <paths>`) through the hook. It refuses when the project isn't on `main`, a merge or rebase is in progress, or any of those files has uncommitted changes (*"CLAUDE.md has uncommitted changes. Commit or remove them first: the kit replaces the whole file."*).
- **Only Loom writes the kit.** A kit file that differs from Loom's kit of the same number is the warning `kit-changed`: *"{file} isn't Loom's kit {n} any more. Loom replaces it on the next kit update; put your own instructions for agents in Wiki/decisions/."* On a job branch, a change to a kit file is a change only Dakota can make (*Rules*).

### Running a job
A **job** is one piece of work for one task: one or more runs of Claude Code (a first run, resumes after a usage limit or a crash, fix rounds) in one worktree, ending in one record.

#### Job IDs, branch and worktree
- **Job ID:** `job-` and a number, at least four digits (`job-0142`, `job-10000`): one more than the highest number among `Records/**/job-*.md` in the working tree, the folders `.loom/jobs/job-*`, and the branches `loom/job-*`. Taken when the job is queued.
- **Branch and worktree,** made when the job **starts** (not when it is queued): `git worktree add -b loom/job-0142 .loom/jobs/job-0142/tree <main's tip>`. `main`'s tip is read at that moment; Dakota's uncommitted edits aren't in it (the Start job dialog says so, below). The worktree shares the main repository's hooks and merge driver (M1, A10).
- **A follow-up job** (*Ask for changes*) gets its own ID and record, and runs on the branch and worktree of the job it follows.

#### What Loom keeps for a job
`.loom/jobs/` (gitignored), every file written atomically:

```
.loom/jobs/
  queue.toml                     the queue's order and state (The queue)
  job-0142/
    job.toml                     the job's state (below)
    prompt.md                    the prompt as given, per run: prompt-1.md, prompt-2.md, …
    agents.json                  the agent definition passed to Claude Code
    stream-1.jsonl, …            Claude Code's output for each run, as it came
    stderr-1.txt, …              its error output
    tree/                        the git worktree (branch loom/job-0142)
```

```toml
id = "job-0142"
kind = "revise-proof"
task = "T-4RTW"
branch = "loom/job-0142"
follows = ""
base = "cb0a446f3e2d9a1b7c4e5f60718293a4b5c6d7e8"
state = "running"            # queued | running | paused | finishing | finished | failed | cancelled | interrupted | merged | discarded
queued = 2026-11-06T10:10:02-06:00
started = 2026-11-06T10:12:44-06:00
ended = 2026-11-06T10:31:02-06:00
session = "6f1c2a9e-…"       # Claude Code's session ID, for resuming
runs = 1
fix_rounds = 0
limit_until = 2026-11-06T15:00:00-06:00   # while paused
turns = 34
cost_usd = 1.23
note = ""                    # Dakota's note from Start job
changes = ""                 # Dakota's message, for a follow-up
```

#### The command line
Run in the worktree folder, with the prompt on stdin:

```
claude -p --verbose --output-format stream-json
       --session-id <new UUID>                  (first run; --resume <session> on later runs)
       --restricted --strict-mcp-config
       --agents <.loom/jobs/job-0142/agents.json> --agent loom-revise-proof
       --tools Read,Glob,Grep,Edit,Write,Bash
       --allowedTools Read Glob Grep "Edit(./Proofs/4KX7/**)" "Write(./Proofs/4KX7/**)"
                      "Write(./Statements/**)" "Write(./Proofs/**)" "Edit(./Records/2026-11/job-0142.md)"
                      "Bash(loom *)"
       --disallowedTools "Bash(git *)" WebFetch WebSearch
       --permission-mode dontAsk --permission-prompts none
       --max-budget-usd <setting>
       [--model <setting>]
```

| Part | Why |
| --- | --- |
| `-p --verbose --output-format stream-json` | Headless, one JSON event per line, as it happens. `stream-json` with `-p` needs `--verbose` ([gotcha](../Wiki/gotchas/claude-code/stream-json-needs-verbose.md)) |
| `--session-id` / `--resume` | Loom chooses the session's ID, so it can resume the same conversation after a limit, a crash or for a fix round |
| `--restricted` | No settings files (Dakota's own hooks, permissions and MCP servers don't apply to jobs), no code-running tools unless `--tools` names them, file tools confined to the worktree, and writes to settings, git and tool configuration never approved |
| `--strict-mcp-config` | No MCP servers |
| `--agents`, `--agent` | The kind's agent definition, from the worktree (*Agent definitions*) |
| `--tools` | The tools that exist in the session at all |
| `--allowedTools` | The kind's `tools`, with `Edit` rules for `change` and `Write` rules for `change` and `create`, as paths relative to the worktree |
| `--disallowedTools` | Denied even if a rule above would allow them |
| `--permission-mode dontAsk`, `--permission-prompts none` | Anything not allowed is denied without asking anyone; Claude Code lists each denial in its result |
| `--max-budget-usd` | A runaway guard, at API prices (F11): the setting minus what the job's earlier runs cost, so a job's runs together stay within it |
| `--model` | Only when the setting *Job model* is set (Q2) |

**The environment:** the app's, plus `LOOM_JOB=job-0142` and `LOOM_JOB_ROOT=<the worktree's absolute path>`, Loom's own folder first on `PATH` (so `loom` is the app's own), `GIT_TERMINAL_PROMPT=0`; and without `ANTHROPIC_API_KEY` and `ANTHROPIC_AUTH_TOKEN` (*Decisions*).

**Checked at start.** The first event is `system`/`init`. Loom checks that its `agents` include the kind's agent, its `tools` are exactly those asked for, its `permissionMode` is `dontAsk`, and its `apiKeySource` isn't an API key; otherwise it stops the run at once, and the job fails with `job-setup-mismatch`: *"Claude Code didn't start the job as Loom asked ({what differs}). Loom stopped it before it did anything. Check my setup shows your Claude Code version."* The flags above were checked against Claude Code 2.1.318; #58's real-CLI test (*Verification*) confirms they confine a job as described, and the PR says what it found.

#### What Loom reads from the stream
Each line is one JSON object. Loom keeps every line in `stream-<n>.jsonl` and uses:

| Event | Loom uses |
| --- | --- |
| `system`, subtype `init` | `session_id`, `model`, `claude_code_version`, `tools`, `agents`, `permissionMode`, `apiKeySource` (the check above) |
| `assistant` | `message.content`: `text` blocks (the last one is shown as *what it's saying*) and `tool_use` blocks (`name` and the main input: a path, a command), for progress. Each `assistant` event with a `tool_use` or ending a turn counts toward turns shown while running |
| `rate_limit_event` | `rate_limit_info`: `status` (`allowed`, `allowed_warning`, `rejected`), `resetsAt` (Unix seconds), `rateLimitType`, `utilization`, and `unifiedWindows` (each window's `utilization`, `resetsAt`, `status`) |
| `result` | `subtype` (`success` or an error subtype), `is_error`, `num_turns`, `total_cost_usd`, `permission_denials` (each `tool_name` and `tool_input`), `terminal_reason`, `api_error_status`, `result` (the final text) |

Unknown event types and fields are ignored, so a newer Claude Code that adds events still works. A line that isn't JSON is kept and ignored.

#### How a job ends
When a run's process exits, Loom decides what happened, first match wins:

| # | When | Then |
| --- | --- | --- |
| 1 | Dakota cancelled it | **Cancelled**: *Finishing*, below |
| 2 | Loom stopped it: the time limit (minutes running, paused time excluded) | **Failed** (*"Stopped after {n} minutes, the job's time limit."*): *Finishing* |
| 3 | A `rate_limit_event` with `status = "rejected"`, or a `result` with `is_error` and `api_error_status = 429`, or a final text mentioning the usage limit | **Paused** until the limit resets (*The queue*); then a new run with `--resume` and `resume-limit.md` |
| 4 | A `result` with `subtype = "success"` | The agent finished: *Checking the work*, below |
| 5 | Anything else: an error `result` (the budget, an API error), no `result`, a crash | **Failed**, with the error's text or the last lines of stderr: *Finishing* |

**Checking the work** (after rule 4). Loom checks the worktree, in order: validation (`LoomCore`'s, as `loom validate` there); the paths changed against the kind's `change` and `create` (*Rules: changes only Dakota can make* also lists them); the changes only Dakota can make; the record's *Done* not being empty. If anything is wrong and fewer than two fix rounds have run, Loom starts a **fix round**: a new run with `--resume` and `fix-round.md`, which lists every problem with its file and line (*"`loom validate` reports: Proofs/4KX7/4KX7.a.tex:7: error: …"*, *"You changed TeX/preamble.tex, which this job may not change. Undo that change."*). After two fix rounds the job goes to *Finishing* as **failed** if anything is still wrong, **finished** otherwise.

**Finishing** (every outcome):
1. Loom writes the record's front matter, *Asked* and *Run*, keeping the agent's sections (*Records*). `touched` comes from the worktree's changes against `base`.
2. **Retag against `main`** (F7): tags and proof letters new on the branch that `main`'s tip already uses are renamed in the worktree (*Rules: retagging*); *Run* lists each.
3. If the worktree validates and has no change only Dakota can make: `git add -A`, then `git commit --author="Loom job-0142 <job-0142@loom.invalid>" -m "<job-id>: <task title>"` with the trailer `Loom-Job: job-0142`, through the pre-commit hook.
4. Otherwise (failed, cancelled or interrupted with invalid work): only the record is committed (`git add -- <record>`, then the same commit), and the other changes stay uncommitted in the worktree. The Jobs tab says *"{n} changed files weren't committed because {reason}."*, with **Show in Explorer**; Review shows the branch with its record.
5. If even the record's commit is refused, the job is **failed** with the hook's output, and nothing is committed.
6. The job's state becomes `finished`, `failed` or `cancelled`; its task is *waiting on Dakota*.

**Cancelling.** **Cancel…** on a running or paused job asks: *"Stop job-0142? Its work so far stays on its branch for you to review or discard."* **Stop job** stops Claude Code and everything it started (M2's process tree: on Windows a job object), then *Finishing* as cancelled. A queued job is removed from the queue without asking; it has no branch yet.

**When the app closes or crashes.** Closing a window with a running job asks: *"job-0142 is running. Closing Loom stops it; you can resume it when you open the project again."* **Stop and close** / **Cancel**. Stopping or a crash leaves (F6): `job.toml` with `state = "running"`, the worktree with whatever the agent wrote, and Claude Code's saved session. A crash of the app also ends Claude Code (the job object closes with the app). On the next open, such a job becomes **interrupted**; the Jobs tab offers **Resume** (a new run with `--resume` and `resume-interrupted.md`: *"Loom was closed while you worked. Check the files: some of your edits may be done. Then carry on."*), **Finish now** (*Finishing* as interrupted) and **Discard**.

**Paused jobs at closing** stay paused; the queue waits for the reset when the project opens again.

### The queue
- **One or two jobs at a time** (setting *Jobs at once*, default 1, Q1). The queue starts the next queued job whenever fewer are running, unless the queue is paused by Dakota or by a usage limit.
- **The order** is the order queued; Dakota can move a queued job up or down, or to the top. Follow-ups join the end, like any job. A job paused by a limit keeps its place and runs first when the limit resets.
- **Usage limits** are the subscription's, for the whole account. When a run hits one (rule 3), the **whole queue** waits: no job starts and every running job is let finish its run. The queue resumes at the latest `resetsAt` seen plus 60 seconds; without a `resetsAt`, after 15 minutes, then 30, then 60, and hourly after that. A limit reached again is another wait. The Jobs tab says *"Waiting for your usage limit to reset at 3:00 pm."*
- **Cost** (F11): each job shows *≈ $1.23 at API prices*, the sum of `total_cost_usd` over its runs. It is what the job would cost on the API, not what Dakota pays; on a subscription the limits are the usage windows, which the Jobs tab shows from the latest `rate_limit_event` (*"5-hour window 3% · weekly 94%, resets Fri 3:00 pm"*, with the time it was read).
- **Where it lives:** `.loom/jobs/queue.toml` (`paused = true|false`, `order = ["job-0143", …]`, `limit_until`), and each job's `job.toml`. The queue runs only while the project is open in the app (F6); the setting *Start the queue when the project opens* (on by default) decides whether it starts by itself.

### The app in M4
- **Side bar:** **Tasks**, **Statements**, **Review** at the top, in the design's order; **Jobs** at the bottom above Settings. Ctrl+1 to Ctrl+4 follow that order (M2: positional). Hovers: *"Tasks: the task board; start a job from a task"*, *"Jobs: the job queue: running, waiting, done"*; Review's becomes *"Review: job branches and your uncommitted changes, as compiled math"*.
- **Badges:** Tasks shows the number of tasks waiting on Dakota; Jobs the number running (a pause mark while the queue waits for a limit); Review the job branches waiting for review plus M2's count.
- **Lanes** (M2): each running job has a thread of its own (at most *Jobs at once*), for its process and its end; merging and discarding run on the Git lane, taking the project's write lock.
- **Window title** is unchanged; a job never changes the main working tree.

### The Tasks tab
Two parts: the **board** (left, most of the width) and the **task panel** (right, the selected task).

**The board** has five columns: **Ideas**, **Ready**, **Running**, **Waiting on you**, **Done**. Each **card** shows `T-4RTW`, the title (two lines at most), the type as a small chip, up to three `about` IDs with their status marks (`4KX7.a`, `+2`), and one line for its state: *waits for T-9QZP* (blocked), *job-0142 · 12 min · editing Proofs/4KX7/4KX7.a.tex* (running), *queued, 2nd*, *paused until 3:00 pm*, *branch ready to review* (a link to Review), *for you* (`for = "dakota"`), or the closing date (done; dropped ones struck through). Cards are in creation order, newest first; Running by start time. The **Done** column shows the last 30 days unless *Show all done* is ticked.

**Above the board:** **New task**, a search box (ID, title, brief, as you type), filters (type; *about*, with the ID picker; *for*), and the strip of waiting checks (*Dakota's `needs-changes` becomes a task*).

**The task panel:**
- The status line: *"Ready: no job has run for it yet."*, *"Blocked: waits for T-9QZP (ready)."*, *"Running: job-0142, 12 minutes."*, *"Waiting on you: job-0142's branch is ready to review."* (link), *"Done on 2026-11-08."*
- The form: title, type, for, about (chips with status marks, + opens the ID picker for statements, proofs, stubs and papers, × removes), depends on (chips; + opens a task picker: tasks by ID and title words), and the brief: a QScintilla editor with Markdown highlighting, and **Preview** (a `MarkdownView` of it). From a check: a read-only line *"Made from your needs-changes check of 2026-11-06 on 4KX7.a"*, a link to the check.
- **Save** (Ctrl+S in the panel): `LoomCore`'s `saveTask(path, task, base)`, with M2's rules: refused if the file changed since it was loaded (`file-changed`, then M2's bar), or if it would add a validation error; unsaved edits kept in a recovery file (`.loom/drafts/Tasks/<id>.toml`, M2's *Never losing text*); the panel's dot and the Tasks dot like the editor's.
- **Buttons:** **Start job…**, **Mark done**, **Drop**, **Reopen** (on a closed task). Mark done and Drop on a task with a job queued, running or waiting for review ask first: *"T-4RTW has job-0142 waiting for review. Mark it done anyway? The branch stays in Review."*
- **Jobs and records:** the task's jobs (from `.loom/jobs/`) with their states, and its records (from `Records/` on `main` and on its branches), each opening in a `MarkdownView` window.

**New task** opens the panel empty: title, type (default `chore`), for, about, depends on, brief. **Create** calls `createTask`.

**Start job…** opens a dialog:
- **Kind:** the kinds that serve the task (*Which tasks can start which jobs*), the first selected; each with its title and one line on what it may change.
- **A note** for this job only (optional): added to the prompt as `{{note}}` and kept in the record's *Asked*.
- What it starts from: *"Starts from main at cb0a446. Your uncommitted edits aren't in it: Proofs/4KX7/4KX7.a.tex."* when files in the task's `about` have uncommitted changes, with **Commit first** opening the git panel.
- **Queue job** (Cancel focused): queues it at the end; *Queue at the top* is a tick. The refusals are those of *Which tasks can start which jobs*, plus: the kit is out of date (the update bar), and Claude Code isn't set up (*"Check my setup says Claude Code isn't ready: {line}."*).

### The Jobs tab
A bar at the top: **Run the queue** / **Pause the queue** (Dakota's pause), *Jobs at once* (1 or 2), and the usage line (*The queue*). Under it, the banner while waiting for a limit.

**Views** (tabs inside the Jobs tab):

| View | Rows | Each row | Actions |
| --- | --- | --- | --- |
| **Running** | Running, finishing and paused jobs | Job ID, task title, kind, elapsed, turns, *≈ $* so far, the last action (*Edit Proofs/4KX7/4KX7.a.tex*, *loom validate*) | **Cancel…**, **Show log** |
| **Queued** | Queued jobs, in order | Job ID, task title, kind, queued time | Move up, move down, to the top, **Remove** |
| **Needs you** | Interrupted jobs, jobs whose setup failed, and failed jobs with nothing committed (*Finishing*, step 5) | What happened, in one line | **Resume**, **Finish now**, **Discard** (interrupted); **Retry** (setup); **Show in Explorer**, **Discard** (nothing committed) |
| **Done** | Finished, failed, cancelled, merged and discarded jobs, newest first, the last 100 | Outcome, ended, duration, turns, *≈ $*, and the branch's state (*waiting for review*, *merged*, *discarded*) | Open in Review; open the record |
| **Records** | The timeline (*Records on objects and the timeline*) | | |

**A job's details** (below the views, for the selected row): the prompt (rendered), the **log** (one line per tool use and text block, newest last, as the run goes), stderr, and the record once it exists. **Show log** opens the raw `stream-<n>.jsonl` in a plain text window.

### Job branches in Review
Review's list gains a group at the top, **Job branches**: one entry per branch `loom/job-*` with a commit beyond `main` and no job running on it: `job-0142 · Fix step 3 of 4KX7.a`, its outcome, the number of jobs on it, and a mark for flags (▲ blocking, ● to look at). M2's groups follow, unchanged, for Dakota's uncommitted edits.

**A branch's view** (right), top to bottom:
1. **The header:** the task (link), each job on the branch with its outcome and record (link), where it started (*from main at cb0a446; main has moved on 3 commits since*), and its cost.
2. **Flags,** first those that block merging (▲), then those to look at (●), then the record's own:
   - ▲ *Changes only you can make*: each one (*Rules: changes only Dakota can make*), with the file. Merge is disabled.
   - ▲ *Conflicts with main*: the files both changed.
   - ▲ *The merged project would have errors*: the validation problems of the merged contents.
   - ▲ *Uncommitted work*: a failed job's files left in the worktree.
   - ● *Files outside what this job may change*: each path.
   - ● *Checks that stop counting*: each proof, its counting checks and the status change (from the merge plan).
   - ● *Retags*: *"4KX7 on this branch becomes 9QZP when merged: main has a 4KX7 too."*
   - ● *Things the agent tried and wasn't allowed*: each permission denial (tool and input).
   - The record's *Flags* items, rendered.
3. **Noticed:** each item, rendered, with **Make a task** (a New task panel filled in: the item as title and brief, type `chore`, about from the IDs in it).
4. **Status changes** if merged now (from `main`'s tip to the merged contents, M2's rules).
5. **The branch's entries,** grouped as M2's list (statements and proofs, papers, Markdown, tasks, records, project files, other), for the changes from the merge base to the branch's tip. Selecting one shows M2's entry view with **before** at the merge base and **after** at the tip (F12): compiled side by side in the chosen mode, plain-word changes, raw diff. Records and other Markdown are rendered with their changes marked (*Rendered Markdown*).
6. **Buttons:** **Merge…**, **Ask for changes…**, **Discard…** (Cancel focused in each dialog).

**Merge…** shows the merge plan (*Merging a job branch*): the commits, the retags and renamed proof letters, the checks that stop counting with their status changes, every status change, and *Mark the task T-4RTW done* (ticked). **Merge into main** runs the merge. It is disabled while anything blocks, with the reason, and while the main working tree isn't on `main` (*"Switch to main first."*) or has a file the merge changes with uncommitted changes or unsaved edits (*"Commit or save Proofs/4KX7/4KX7.a.tex first: the merge changes it."*).

**Ask for changes…** asks for a message (required, a Markdown box) and queues a **follow-up job**: same kind, same task, same branch and worktree, its own ID and record, `follows` set. Its prompt (`follow-up.md`) gives the task's brief, the paths of the branch's earlier records, Dakota's message as `{{changes}}`, and says the branch already has its earlier work. When the branch conflicts with `main`, the dialog offers *Bring main into the branch first* (ticked): Loom runs `git merge main` in the worktree before the job starts, leaving conflict markers, and the prompt asks the agent to resolve them; Loom refuses to commit while `git diff --check` finds markers (Q7).

**Discard…** says: *"Discard job-0142? Its branch and worktree are deleted. Kept: the job's log in .loom/jobs/ for 30 days, and its last commit {hash}, which git keeps for a while."* With a tick, *Keep its record in Records/ (commits it to main)*, off by default (Q5). **Discard** runs `git worktree remove --force`, `git branch -D loom/job-0142`, and with the tick, writes the record into the main working tree and commits only it (*"Record of discarded job-0142"*). The task goes back to *ready*. `loom discard-job <job-id>` *(Dakota)* does the same (`--keep-record`).

### Merging a job branch
`LoomCore`'s `planMerge(root, branch)` computes a **merge plan** without changing anything; `mergeJobBranch(root, branch, confirmation)` carries it out. `loom merge-job <job-id|branch> [--plan] [--keep-task-open]` uses both: `--plan` prints the plan and exits (read-only, no `yes`); otherwise it prints the plan, asks *(Dakota)*, and merges.

**The plan.** With `B` the branch's tip, `M` `main`'s tip and `O` their merge base (`git merge-base`), each read as contents (M2's *Building another revision*):
1. **Refusals first:** a job still running or finishing on the branch; uncommitted work in its worktree; the main working tree not on `main`, or a merge, rebase, cherry-pick or revert in progress; any change only Dakota can make between `O` and `B`.
2. **Retags:** the tags and proof IDs that are new on the branch (in `B`, not in `O`) and also in `M` (as a tag or alias, or a proof of the same statement), with their new tags and letters (*Rules: retagging*). New tags are drawn then and kept in the plan.
3. **The merged contents,** in memory: `M`, plus every path changed between `O` and `B` after the retags. A path changed on both sides (`O` → `M` and `O` → `B`) is a **conflict**, except `*.checks.toml`, which M1's `mergeChecks` merges (a list that doesn't parse is a conflict). Any conflict blocks the merge.
4. **Validation** of the merged contents (M1's, all of it, with `M` as the `HEAD` it compares checks with). Errors block the merge.
5. **What it changes:** status changes from `M` to the merged contents (M2's *Status changes*), and the checks that stop counting (M1's `outdatedChecks` from `M` to merged: proofs whose counting checks no longer match, which includes proofs whose fingerprint a retag changed).
6. **The files it writes** in the main working tree; any of them with uncommitted changes blocks the merge, naming them (the app also refuses for unsaved edits).
7. **The plan's hash:** SHA-256 over its retags, conflicts, outdated checks and the commits `O`, `M`, `B`.

**The merge** (`mergeJobBranch`), given the hash Dakota confirmed and *mark the task done*:
1. Plan again; if the hash differs, refuse (`merge-plan-changed`: *"Loom didn't merge job-0142: main or the branch changed since you looked. Look at the new plan and merge again."*), as M1 does for outdated checks.
2. **Retag on the branch:** in the worktree, rewrite (*Rules: retagging*) and commit *"Retag for merge: 4KX7 → 9QZP"* through the hook.
3. **Merge:** take the project's write lock; `git merge --no-ff --no-commit loom/job-0142` in the main working tree (the merge driver merges check lists). Any conflict git reports: `git merge --abort`, refuse with `merge-conflict` and the files.
4. With *mark the task done*: set the task's `closed` and `outcome = "done"`, `git add` it.
5. `git commit -m "Merge job-0142: <task title>"` through the hook. Refused: `git merge --abort`, report the hook's problems (to the Problems panel, M2).
6. `git worktree remove`, `git branch -d loom/job-0142`; the job's state becomes `merged`. Its folder in `.loom/jobs/` is kept 30 days.

`loom merge-job --plan --json` prints `{"branch", "base", "main", "tip", "commits": [{hash, subject}…], "retags": [{"from", "to"}…], "conflicts": [path…], "problems": [{…}…], "dakota_only": [{"path", "what"}…], "outside": [path…], "outdated_checks": [proof…], "status_changes": [{"id", "from", "to"}…], "writes": [path…], "blocked": true|false, "hash"}`. Exit codes are M1's: 0 merged (or planned), 1 refused or blocked, 2 usage, 3 git failing.

### Agents can't act as Dakota
What stops a job, layer by layer, and what Dakota sees if one tries:

| A job trying to | Stopped by | Seen |
| --- | --- | --- |
| Merge, commit, push or switch branches | No `git` (`Bash(git *)` denied; `--restricted`); Loom makes the job's commits | A permission denial in the record's *Run* and Review's flags |
| Touch `main`'s working tree | File tools confined to the worktree (`--restricted`); `loom` refuses, inside a job, any project but `LOOM_JOB_ROOT` (`job-outside-worktree`: *"This is a job for {root}; loom won't change another project."*) | A denial, or `loom`'s refusal in the log |
| Run a *(Dakota)* `loom` command (a Dakota check, refuting, `failed`, outdating checks, any task command, `update-kit`, `merge-job`) | `loom` refuses whenever `LOOM_JOB` is set, terminal or not (`dakota-in-job`: *"Only Dakota can {do this}, and never from a job."*) | The refusal in the log |
| Write a Dakota check, `refuted_on`, `failed`, a verified date, or edit a task or the kit by hand | Nothing stops the edit itself; Loom's check finds it at the end (a fix round asks to undo it) and again in the merge plan | ▲ *Changes only you can make*: Merge disabled |
| Edit a checked statement or proof | Allowed (revising is the point); its checks stop counting | ● *Checks that stop counting*, in the plan and the merge dialog |
| Change files outside its kind's folders | Claude Code's path rules; Loom's check at the end (a fix round) | ● *Files outside what this job may change* |
| Use the web or MCP servers | Not in `--tools`; `--strict-mcp-config` | A denial |

### Rendered Markdown
The design kept in M2's appendix *Moved to M4: rendered Markdown* is taken over and approved as it stands (the view, its rendering with md4c and KaTeX, the block-by-block changes, the links, no internet), with these additions:
- **Where `MarkdownView` is used in M4:** Review's Markdown entries (uncommitted and on job branches), records (Review's branch view, the record window opened from a check, a task, an object or the timeline), task briefs (the panel's Preview), the job prompt in the Jobs tab, and *Noticed* and *Flags* items.
- **Front matter** of a record is shown as M2's small table; `touched`, `task` and `follows` are links.
- **Links** `T-xxxx` and `job-xxxx` written in Markdown as code or plain words become links too (to the task, to the job's record).
- **Versions:** #41 pins md4c to its latest release and KaTeX to its latest 0.16.x, by URL and SHA-256, and records them on its decision page.
- Notes (`notes.md`) stay M6's.

### Records on objects and the timeline
- **On statements and proofs:** the Statements tab's statement page and each proof page gain a **Records** list below the header: every record on `main` whose `touched` includes the statement or the proof (a statement's list includes its proofs'), newest first, each as the date, `job-0142`, the kind, the outcome and the first line of *Done*. A click opens the record in a `MarkdownView` window. The check history's `record` links open the same window (replacing M2's plain text).
- **On tasks:** the task panel's records (*The Tasks tab*).
- **The timeline:** the Jobs tab's **Records** view: every record on `main`, newest first, grouped by day; each row as above plus the task. Filters: kind, outcome, task, an object (the ID picker), and a date range. A click opens the record.
- `LoomCore` gains `recordsOf(contents, id)` and `recordTimeline(contents)`, Catch2-tested.

### Settings and Check my setup
**Settings** gains (M2's table):

| Setting | Default | Notes |
| --- | --- | --- |
| `claude` | Found on `PATH` | A full path overrides it |
| Jobs at once | 1 | 1 or 2 (Q1) |
| Job model | (empty: Claude Code's default) | Passed as `--model` when set (Q2) |
| Stop a job after | 60 minutes | 5–600, running time only (Q3) |
| Budget per job at API prices | $10 | 1–100, `--max-budget-usd` (Q3) |
| Start the queue when the project opens | On | |

**Check my setup** gains:

| Tool | Checked by | Missing says |
| --- | --- | --- |
| Claude Code | `claude --version`, 2.1.318 or newer | *"Install Claude Code (claude.ai/code), then sign in with `claude auth login`."* |
| Signed in to a subscription | `claude auth status --json`: `loggedIn` true and `authMethod` `"claude.ai"` | Not signed in: *"Run `claude auth login` and sign in with your Claude subscription."* Another method: *"Claude Code is signed in with {method}; Loom runs jobs on your subscription. Run `claude auth login` with your subscription account."* |
| `ANTHROPIC_API_KEY` set | The app's environment | A note, not a cross: *"An API key is set in your environment; Loom leaves it out of jobs, so they use your subscription."* |

No check starts a job: that would use the subscription.

### Dependencies and toolchain
| Dependency | Version | Licence | Used how | Fine with GPL v3? | Decision page |
| --- | --- | --- | --- | --- | --- |
| Claude Code CLI | 2.1.318 or newer (verified 2026-11-06) | Anthropic's commercial terms | A separate program, run as a process; never linked or shipped; installed by Dakota | Not linked or shipped | #58 (how-to: installing and signing in) |
| Qt WebEngine Widgets | Qt 6.11.1 | LGPL v3 / GPL v3 | Linked in `LoomApp`, used only by `MarkdownView` | Yes | #41 |
| md4c | Latest release, pinned | MIT | Compiled in (`FetchContent`) | Yes | #41 |
| KaTeX | Latest 0.16.x, pinned | MIT | Release archive by `FetchContent`; compiled into `LoomApp` as Qt resources | Yes | #41 |

`LoomCore` gains no dependency: jobs, the stream parser, records, tasks and merges use Qt Core and toml++ only. The configure check that keeps Gui, Widgets, PDF and WebEngine out of `LoomCore` stays.

**CI** (`.github/workflows/checks.yml`):
- Qt gains the modules `qtwebengine qtwebchannel qtpositioning` on both runners, and the Linux runner the system libraries WebEngine needs.
- **WebEngine tests** carry the label `webengine` and skip when WebEngine can't start (M2's appendix).
- **No Claude Code in CI.** Every job test runs the fake Claude Code. Tests that need the real one carry the label `claude` and run only with `LOOM_REAL_CLAUDE=1`, on Dakota's machine.

### The fake Claude Code
`Tests/Support/FakeClaude/`, a small Qt Core program built as `fake-claude` with the tests. Tests point the `claude` setting (or `LoomCore`'s job options) at it.

- **`fake-claude --version`** prints `2.1.318 (Claude Code)`; **`fake-claude auth status --json`** prints the `[auth]` table of its script.
- **`fake-claude -p …`** reads its script from the file named by `LOOM_FAKE_CLAUDE_SCRIPT`: a TOML file with one `[[run]]` per invocation, taken in order (a counter file beside the script says which). It appends its arguments, environment (`LOOM_*`, `PATH`, whether `ANTHROPIC_API_KEY` is set) and stdin to `calls.jsonl` beside the script, so tests can check the command line and the prompt.
- **Steps** in a run, done in order in the current folder:

| Step | Does |
| --- | --- |
| `{ emit = "init" }` | Prints a `system`/`init` event with the session ID it was given, the `tools` and `agents` it was given (or `agents_override`, to test the setup check), `model = "fake-model"` |
| `{ say = "…" }`, `{ tool = "Edit", input = "…" }` | Prints an `assistant` event with a text or `tool_use` block |
| `{ write = "<path>", text = "…" }`, `{ append = "<path>", text = "…" }` | Changes a file, as an agent's Edit or Write would |
| `{ run = "loom new-statement …" }` | Runs a command (with the environment it was given) |
| `{ record = { done = "…", noticed = ["…"], flags = ["…"] } }` | Fills the sections of the record named in the prompt |
| `{ limit = "rejected", resets_in = 2 }` | Prints a `rate_limit_event`, seconds from now |
| `{ sleep = 1.5 }`, `{ hang = true }` | Waits; or never ends (for cancelling and time limits) |
| `{ result = "success", turns = 3, cost = 0.42, denials = [{ tool = "Bash", input = "git push" }] }` | Prints a `result` event |
| `{ exit = 1, stderr = "…" }` | Ends with that code (without a `result`, a crash) |

Every scenario in *Verification* is a script in `Tests/Support/FakeClaude/scripts/`.

### The demo project
`Tests/Projects/Demo/` gains (M4's build issues write them):
- `format = 2`, `kit = 1`, and the kit's `CLAUDE.md` and `.claude/agents/` (#55).
- Its record becomes `Records/2026-11/job-0001.md` in the format of *Records* (task `T-1DEM`, kind `chore`, outcome `finished`), and the checks that linked `demo-0001.md` link it (#54). Fingerprints don't change: `record` isn't in them.
- `Tasks/T-1DEM.toml`: a done chore, the task of that record (#53).

**Overlays** (M2's `Demo-edits/`):
- `tasks`: one task of each status: an idea (`T-2DEM`), a ready chore about E6PQ (`T-3DEM`), a ready task blocked by it (`T-4DEM`), a task for Dakota (`T-5DEM`, read), a dropped one (`T-6DEM`), and a task made from a check (`T-7DEM`, `from_check` on a Dakota `needs-changes` the overlay adds to G8TV.a).
- `waiting-checks`: a Dakota `needs-changes` on G8TV.a with no task (a message from before M4), and an agent `needs-changes` on D5MN.a with its record.

**Branch fixtures** (#56): `Tests/Projects/Demo-branches/<name>/`, each a `branch/` overlay committed on `loom/job-0007` from the demo's first commit, and optionally a `main/` overlay committed on `main` after it. `Tests/Support/DemoProject` gains `addJobBranch(name)`, which also writes the job's record.

| Fixture | Branch | Main | The plan says |
| --- | --- | --- | --- |
| `collision` | New lemma `M4ST` with proof `M4ST.a`, `\ref{M4ST}` in a new proof `C4JK.b`; a record | A different new statement `M4ST`, and a new proof `C4JK.b` | Retag `M4ST` → a new tag (files, ids, `\ref`, premises, the record's `touched`); `C4JK.b` → `C4JK.c`; the checks of the branch's proofs that mention `M4ST` stop counting |
| `revise` | G8TV.a's body revised (a `revise-proof` job) | | G8TV.a's agent pass stops counting; status unchanged (unchecked) |
| `dakota-only` | A check by `dakota` added to G8TV.a | | Blocked: a change only Dakota can make |
| `outside` | `TeX/preamble.tex` changed | | Flag: outside what a chore may change |
| `conflict` | K3AB's body changed | K3AB's body changed differently | Blocked: conflict |
| `checks-both` | An agent check added to E6PQ.a | A Dakota check added to E6PQ.a | Merged by `mergeChecks`, no conflict |

## Rules

### Task status
For a task T, first match wins:

| # | When | Status |
| --- | --- | --- |
| 1 | `closed` and `outcome` are set | **done** (shown *dropped* when `outcome = "dropped"`) |
| 2 | `type = "idea"` | **idea** |
| 3 | A job for T is queued, running, paused or finishing | **running** |
| 4 | A job branch for T waits for review (a job for T has state `finished`, `failed`, `cancelled` or `interrupted`, and its branch still exists) | **waiting on Dakota** |
| 5 | `for = "dakota"` | **waiting on Dakota** |
| 6 | Otherwise | **ready**; *blocked* when a task in `depends_on` isn't closed |

Rules 3 and 4 read `.loom/jobs/` and the branches; without them (`loom tasks` on a fresh clone) they don't apply.

### Retagging
For each tag `X` new on the branch and taken on `main` (a tag or alias there), Loom draws a fresh tag `Y` (M1's generator, unused on `main` and on the branch). Then, on the branch:
1. **Files:** `Statements/X.tex` → `Statements/Y.tex`; `Proofs/X/` → `Proofs/Y/`, and in it `X.<letters>.tex`, `.verbose.tex` and `.checks.toml` → `Y.<letters>…`; a stub `Papers/<Key>/Statements/X.toml` → `Y.toml`.
2. **Front matter and TOML:** `id`, `proves`, a proof's `id`, a verbose version's `proof`, and every `premises`, `stated_using`, `superseded_by`, `refuted_by`, `paper_premises` entry that names `X` or a proof `X.<letters>`.
3. **Bodies** of every statement, proof, verbose proof on the branch: the argument of `\ref`, `\paperref` (and, from M8, `\statement`, `\proof`) when it is exactly `X` or `X.<letters>`. Nothing else in a body changes (no plain-text replacement).
4. **Records** on the branch: `touched`. **Checks** keep their `fingerprint`, so those of proofs whose fingerprint changed stop counting; the plan lists them.

For each proof `S.p` new on the branch where `S` exists at the merge base and `main` has its own `S.p`, Loom renames the branch's to the next free letter after the highest on either side, by the same steps 1–4. An alias new on the branch that `main` uses can't be renamed (aliases are old names): the plan is blocked, *"The alias {alias} of {id} on this branch is also used on main ({other}). Ask for changes to drop or change it."*

At the end of a job (F7) the same rules run against `main`'s tip in the worktree, and the record's *Run* lists the renames.

### Changes only Dakota can make
Between the merge base and the branch's tip (or the worktree, at the end of a job):

| Change | Says |
| --- | --- |
| A check with `by = "dakota"` added | *a check by you on {proof}* |
| Any committed check changed or removed (also M1's `check-changed`) | *a check of {proof} changed* |
| `refuted_by` or `refuted_on` set, changed or removed | *{id}'s refutation* |
| `level = "failed"` set | *{proof} marked failed* |
| A paper field's `verified`, a locator's `checked_against_pdf` or a renumbering's `checked` set to a date | *{Key}: {field} verified* |
| Anything in `Tasks/` | *the task {id}* |
| `CLAUDE.md`, anything in `.claude/`, `.gitignore`, `.gitattributes`, or `loom.toml` beyond new `[[tags]]` entries | *Loom's file {path}* |
| A statement, proof or stub deleted | *{id} deleted* |

Each blocks merging and is listed with its file. Outdating checks isn't one of them: a job may revise a checked proof, and Dakota sees the checks that stop counting before he merges.

### Plain words for tasks and records
Added to M2's *Front-matter changes in plain words*, for Review's Tasks and Records entries:

| Change | Line |
| --- | --- |
| A new task | `new task T-4RTW · Fix step 3 of 4KX7.a (prove)` |
| `title`, `type`, `for` | `title: "Old" → "New"`, `type: idea → prove`, `for: agent → dakota` |
| `about`, `depends_on` | `about: +4KX7.a · …, −…`, `depends on: +T-9QZP · …` |
| `closed`, `outcome` | `closed: done (2026-11-08)` / `reopened` |
| `brief` | `brief changed` (and the rendered diff below) |
| A new record | `new record job-0142 (revise-proof, finished; touched 4KX7, 4KX7.a)` |
| A changed record | `record changed` (records aren't changed after their job; also flagged ● on a branch) |

## Edge cases
- **The app closes while a job is finishing** (validating or committing): `finishing` is redone from the start on the next open; every step is safe to repeat (the record is rewritten from `job.toml`, the commit is skipped if `HEAD` already has the trailer `Loom-Job: job-0142`).
- **`.loom/` deleted while jobs exist:** queue and job state are gone; the branches and worktree registrations remain. On open, Loom runs `git worktree prune` and lists each `loom/job-*` branch without a job folder in Review as a job branch whose record is read from the branch.
- **`main` doesn't exist or isn't the branch Dakota uses:** jobs start from `main`; a project with no `main` can't start jobs (*"Jobs start from main, and this project has no main branch."*).
- **The worktree's parent `CLAUDE.md`:** Claude Code also finds the main working tree's `CLAUDE.md` above the worktree. Both are Loom's kit; after a kit update on `main`, a branch made before it may see two kits. The job's prompt names the kit on its branch as the one to follow.
- **Two jobs for different tasks change the same proof:** both branches can't both merge cleanly; the second shows a conflict, and *Ask for changes* with *Bring main into the branch* resolves it.
- **A follow-up job on a branch whose worktree was removed by hand:** Loom re-adds the worktree for the existing branch (`git worktree add <path> loom/job-0142`) before the job starts.
- **A task edited while its job runs:** the job keeps the brief it was given (in `prompt.md`); the task panel says *"The brief changed after job-0142 started."*
- **A task closed while its job runs:** allowed after the question (*The Tasks tab*); the branch still comes to Review.
- **A retag at the end of a job collides again at merge** (`main` gained the new tag too): the merge plan retags again.
- **A branch whose job created a statement that `main` meanwhile superseded or refuted its premise:** the merged contents' validation shows the warnings; errors block.
- **A job edits its own record's front matter:** Loom rewrites the front matter at *Finishing*; the agent's change is lost by design, and *Run* says so.
- **A record for a job whose task was later dropped:** fine; the record stays, the task is closed.
- **A usage limit reached during a fix round:** paused as any run; the fix round resumes after the reset.
- **Claude Code updates itself between runs of one job:** each record keeps the version of its last run; the setup check runs at each run's start.
- **Paths with spaces or non-ASCII letters** in the project: the worktree path is passed as one argument and `LOOM_JOB_ROOT`; Claude Code's rules use paths relative to the worktree.
- **Windows path length:** the worktree adds about 25 characters (`.loom\jobs\job-0142\tree\`); long paths are M1's concern as for any file.
- **A `needs-changes` from Dakota on a proof with a task already open from an earlier check:** a second task is made (each check its own); the strip doesn't list either.
- **A job left `running` in `job.toml` with a process still alive** (the app was killed but Claude Code survived, outside a job object): on open Loom finds the PID it recorded still running a `claude` with the job's session and stops it, then treats the job as interrupted.

## Verification
| Test | Kind | Checks |
| --- | --- | --- |
| Task format | Catch2 | Round trip in canonical form; every field rule; IDs (generation, command-line normalisation); every task validation code |
| Task status | Catch2 | Every row of *Rules: task status*, with job states and branches supplied |
| Task commands | CTest scripts | Each subcommand's text and JSON; *(Dakota)* refused without a terminal and inside a job; an idea can't be made ready to run without changing its type |
| `needs-changes` makes a task | Catch2, CTest | Exactly the fields of the table; the check written first; `task-not-created`; no task in a format 1 project; `--from-check` for a Dakota and an agent check; waiting checks found and not found |
| Record format | Catch2 | Round trip; agent sections kept byte for byte when Loom rewrites; missing headings put back; every record validation code; F9's two errors and the kept warning |
| Kit | Catch2 | `new-project` writes format 2 and kit 1; every placeholder of every prompt filled; `update-kit` writes, commits and refuses as listed; `kit-changed` |
| Command line | Catch2, fake | The exact arguments and environment for each kind, first run and resume; API key variables removed |
| Job runs | Catch2, fake | A finished job commits on its branch with its record; a fix round fixes validation; two failed fix rounds commit the record only; time limit; crash without `result`; setup mismatch stops at once |
| Limits | Catch2, fake | `rejected` pauses the queue until `resetsAt` + 60 s and resumes the session; 429 without `resetsAt` backs off 15, 30, 60 min (with a fake clock) |
| Cancel and crash | Catch2, fake | Cancel stops the process tree and commits what's valid; a killed app leaves `running`, which opens as interrupted; Resume, Finish now |
| Confinement | Catch2, fake | Writes outside the kind's folders found; each row of *Rules: changes only Dakota can make*; `loom` refuses *(Dakota)* commands and other projects with `LOOM_JOB` set |
| Retagging | Catch2 | The `collision` fixture: every rewritten place; nothing else in bodies; aliases blocked; proof letters |
| Merge plan and merge | Catch2, on the branch fixtures | Each fixture's row; `merge-plan-changed`; uncommitted files in the way; the merge commit through the hook, and its refusal aborting cleanly; the task marked done |
| `loom merge-job`, `discard-job` | CTest scripts | `--plan` JSON; exit codes; *(Dakota)* |
| Records on objects | Catch2 | `recordsOf` and the timeline on the demo |
| Rendered Markdown | Catch2 or Qt Test without WebEngine; screenshot test labelled `webengine` | M2's appendix's tests, plus a record with front matter and task and job links |
| Tasks tab | Qt Test | Board columns from the `tasks` overlay; create, edit, save refusals keep text; Start job refusals; Mark done, Drop, Reopen; the strip (`waiting-checks`) making tasks |
| Jobs tab | Qt Test, fake | Running progress lines; queue order changes; pause and resume; the limit banner; interrupted jobs offered |
| Review job branches | Qt Test, fixtures | The group; flags in order; merge, ask for changes and discard, each confirmed and cancelled; nothing written on Cancel |
| A real job | Labelled `claude`, `LOOM_REAL_CLAUDE=1`, on Dakota's machine | On a copy of the demo, a `chore` job asked to add a sentence to `Wiki/notes.md`, to try writing `loom.toml` directly and to try `git status`: the agent loads, both tries are denied and listed, the record's sections are filled, the branch has one commit |
| On Windows | manual | Each build issue's Windows check (below) |

**Each build issue's Windows check** covers what CI can't:
- **#53:** `loom new-task` in a test project, then `loom validate`; `loom add-check --by dakota --verdict needs-changes` creates a task file; a *(Dakota)* task command refused from a pipe.
- **#54:** the demo's record validates; a record with a missing section warns.
- **#55:** `loom new-project` writes the kit; `loom update-kit` on a format 1 project commits it.
- **#56:** `loom merge-job --plan` on the `collision` fixture; the merge through the real hook on Windows.
- **#57:** the board at high DPI and in dark mode; the brief's Preview.
- **#58:** one real Claude Code job from the app on a copy of the demo (the `claude` test, then the same by hand): it starts, shows progress, commits on its branch; cancelling one mid-run; ending Loom in Task Manager mid-job, then Resume.
- **#59:** two jobs at once; the usage line; pausing the queue.
- **#60:** merging, asking for changes and discarding a real job's branch; Review's before and after.
- **#61:** records on a statement page; the timeline's filters.
- **#41:** rendered Markdown with math in Review; WebEngine's process starting from the installed app.

## Open questions
Each has a recommendation; the spec above is written as if it were accepted.

- **Q1, jobs at once:** one at a time by default, with a setting allowing two (recommended), or two by default. One keeps the usage windows easy to read and the merge order simple; two finishes the queue faster.
- **Q2, the model jobs use:** Claude Code's own default unless the *Job model* setting names one (recommended), or a model fixed by Loom for each kind. The default follows Dakota's subscription as it changes; a fixed model is reproducible but goes stale.
- **Q3, a job's limits:** 60 minutes of running time, $10 at API prices, and two fix rounds (recommended), or other numbers.
- **Q4, a branch with a change only Dakota can make:** can't be merged; Dakota asks for changes or discards (recommended, F8). Or merge after a dialog listing them, Dakota taking responsibility.
- **Q5, discarding:** the record is deleted with the branch unless Dakota ticks *Keep its record* (recommended), or every discarded job's record is committed to `main`, so dead ends are kept.
- **Q6, jobs while Loom is closed:** jobs run only while Loom is open, and resume when it opens again (recommended, F6), or jobs keep running after the window closes (a background process with no one watching its limits or end).
- **Q7, a branch that conflicts with `main`:** *Ask for changes* can bring `main` into the branch and have the agent resolve the conflict markers (recommended), or conflicts are left to a terminal, as in M2.
- **Q8, the first tasks of a new project:** the design has Loom create *write the topic pages* and *gather literature* tasks with a new project. Recommended: not in M4 (topic pages are M12's format and gathering literature is M6's job), so a new project still starts with no tasks; or M4 creates a *write the topic pages* chore from the research idea.
- **Q9, Dakota's own instructions for agents:** `CLAUDE.md` is Loom's and is replaced on kit updates, so agents read Dakota's conventions from `Wiki/decisions/` and `Wiki/glossary.md` (recommended), or a project file Dakota owns (say `Wiki/agents.md`) is imported by the kit's `CLAUDE.md`.

## See also
- Up: [Specs](README.md)
- [The design](loom.md): *Tasks*, *Records, reviews and meetings*, *Local review*, *Agents and jobs*, *Technology*
- [The M1 spec](m1-core.md): IDs and collisions between branches, proof letters, `merge-checks`, D10, a check's `record`, the typed `yes`, the M1 kit
- [The M2 spec](m2-statements-review.md): the shell and badges, Dakota's clicks, the lanes, Review and the git panel, *Rules*, *Moved to M4: rendered Markdown*
- The M4 build issues: #53–#61 and #41
