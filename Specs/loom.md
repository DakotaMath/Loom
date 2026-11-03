---
type: spec
area: whole app
tags: [design, project-model, workflows, architecture]
---

# Spec: Loom, the design

> **Summary:** Loom is a Windows desktop app (C++, Qt 6) for doing mathematical research with AI agents. A research project is a folder Loom creates and owns, kept in local git. Its core is a graph: **statements** are nodes, **proofs** are edges ("from A₁…Aₙ we get B"), and a statement's status is **computed** from its proofs, never typed by hand. Around the graph sit the workflows: gathering and reading papers, spec sessions that propose what to prove next, per-proof agent jobs, threads that weave a path through the graph into a shareable paper, numerics with kept runs, scheduled reviewers and Lean. Agents run as Claude Code jobs on Dakota's subscription; reviewers use the Claude API. Everything an agent changes lands on its own branch and is reviewed as compiled math before Dakota merges it. This page is the design that the per-system specs will split out from.

**Last verified:** 2026-11-03 · commit c70b3e2
**Status:** Draft, written from the design chat of 2026-11-03; no open questions, awaiting approval
**Replaces:** TexViewer, the owner's earlier Electron/TypeScript editor, which is archived once Loom can do what it does

## Scope
- **Covers:** why Loom exists; its principles; the project folder and every file type in it; how status and staleness are computed; the tabs; each workflow (literature, reading sessions, spec sessions, proof jobs, threads, numerics, reviewers, Lean, tasks); agent jobs and local review; the technology; Loom's own repo layout; the build order; importing an existing project.
- **Does not cover:** screen-by-screen layouts, exact field lists beyond the examples here, and the agent prompts. Each gets its own spec under `Specs/` before its milestone is built.

## Why
Until now the math project came first (a research repo of LaTeX and Markdown files) and an editor was built for it afterwards (TexViewer). Two codebases parse the same file formats, so a list of "contract" files had to stop them drifting apart, and every format change risked breaking the other side. Loom turns this around: **a project is a document Loom owns**, the way a spreadsheet belongs to its spreadsheet program. Loom defines the formats, writes the files and checks them. Agents work through Loom's own command-line tool, so they can't write a file Loom wouldn't.

Loom is the one place for the whole human + AI workflow: the human's research idea, finding and reading papers, discussing them with an agent, proposing and proving results, checking every proof, writing papers, numerics, and formal verification.

## Principles
1. **The project is an object in Loom.** Loom defines every file format and is the only writer: the app, its command-line tool (`loom`) and every agent job go through one core library, and nothing that fails validation is committed.
2. **Statements are nodes, proofs are edges.** A statement holds only its statement; a proof holds only its proof and names its premises. A statement can have several proofs with different premises.
3. **Status is computed, not claimed.** No person or agent types "proved". It follows from the graph, the proofs' levels and their checks. The same goes for staleness of numerics and figures.
4. **Verify, don't recall.** Everything an agent extracts or fills in carries its source (a page in the PDF, a web link) and stays marked unchecked until Dakota checks it.
5. **Provenance is data.** Every note, check and record says who made it (Dakota, or which model on which date) and what kind it is. Colours in the app are drawn from that data.
6. **Nothing changes the project without a click.** Agents work on their own branches; Dakota merges. Keeping a run, accepting a check and approving a spec are clicks.
7. **Records attach to the object they are about.** Logs, sessions, checks and reviews belong to a statement, proof, paper or experiment, and Loom shows them there. Knowledge pages are the only things in the wiki, and every index is generated, never kept by hand.
8. **Read math, not code.** Every change is shown compiled or rendered first, with the raw diff one click away.
9. **Local first.** Everything is local git. GitHub (issues, PRs, sync) is a later addition.

## Decisions made in the design chat (2026-11-03)
| Decision | Choice |
| --- | --- |
| Language and toolkit | C++ with Qt 6 Widgets (not QML). Dakota reads C++ best |
| Tabs | A vertical icon bar on the far left, as in TexViewer |
| Repo | A fresh public repo for Loom; TexViewer is archived |
| Agent jobs | Claude Code in headless mode, on Dakota's subscription |
| Scheduled reviewers | The Claude API (pay per use), Loom's only API use |
| Paper text | Verbatim statements and proofs from papers are **local only**; the committed project holds stubs in our words |
| Paper versions | `paper.toml` separates the paper from its versions (each arXiv version, the published one) and holds full BibTeX data per version; locators, notes and readings name the version they come from |
| Links | Two kinds: **stated using** (on statements, e.g. definitions) and **premises** (on proofs). Only premises affect status |
| "Proved" | Needs an agent check **and** Dakota's click |
| Check history | Append-only, in a `<proof-id>.checks.toml` beside each proof; each check carries a fingerprint of what it read; the newest matching check of each kind wins |
| File format | TOML front matter on every file, **except threads**, which are plain LaTeX so they can be shared |
| Threads | Plain LaTeX plus `\statement{id}` and `\proof{id}`; **Export** writes a standalone `.tex` |
| Specs | Top-level `Specs/` in Loom's repo (specs for the software) and top-level `Specs/` in every project (spec sessions). No specs folder in any wiki |
| Tasks | A `Tasks/` folder in each project and a Tasks tab |
| Numerics | Scratch runs vs kept runs; kept runs tied to the exact code; large arrays in Git LFS |
| Python | A Python editor inside Loom; `.py` files with `# %%` cells, never Jupyter notebooks; numpy, scipy and matplotlib by default |
| Name | Loom |
| IDs | Short random permanent tags (4 characters), picked through a search picker, always shown with titles |
| Reviewers | Nightly, $2 a night; then a task for Dakota to triage the reviews, then an agent job to implement what he accepts |
| Discussions | Committed and released with the work |
| Editing checked statements | Needs Dakota's click |
| Collaborators who don't use Loom | Exports (threads, statement bundles) are enough for now |
| Platform | Windows only |
| Licences | GPL v3 for the code (QScintilla is GPL v3), CC BY 4.0 for `Wiki/` and `Specs/`, CC0 for `Templates/` so projects carry no licence from Loom |
| Run outputs | HDF5 |
| Tests | Catch2 for the core and the `loom` command, Qt Test for the UI |

## The project folder
Top-level folders are capitalised.

```
MyProject/
  loom.toml                 the project: name, tags (equations, techniques), preamble files, settings
  CLAUDE.md                 written by Loom from its project template; agents read it
  STATUS.md                 generated by Loom: what's in progress, what waits on Dakota
  TeX/                      preamble.tex, macros.tex
  Statements/<id>.tex       Dakota's nodes: one definition, lemma, theorem, conjecture or question each
  Proofs/<statement-id>/    edges: <proof-id>.tex (front matter lists the premises) and, beside it,
                            <proof-id>.verbose.tex (the step-by-step version) and
                            <proof-id>.checks.toml (the proof's check history)
  Papers/<Key>/
    paper.toml              the database entry (fields with sources, see Papers)
    Statements/<id>.toml    committed stubs of the paper's results (locator + gist in our words)
    notes.md                reading notes; every note tagged with author and kind
    Sessions/<YYYY-MM-DD>/  one reading session: session.md (committed), discussion.md (committed)
    local/                  gitignored: one PDF per version, annotated PDFs, transcripts, verbatim statements
  Specs/<spec-id>/spec.md   spec sessions: the target, the explored neighbourhood, the proposal, approval
  Threads/<name>.tex        plain LaTeX: a path through the graph plus prose
  Numerics/<experiment>/    experiment.toml, the scripts, Runs/<run-id>/ for kept runs
  Numerics/lib/             shared Python modules used by experiments
  Figures/<figure-id>/      figure.toml and the output (PDF or PNG)
  Tasks/<task-id>.toml      the task board
  Reviews/<YYYY-MM>/<review-id>.md   referee files, each naming what it reviews
  Records/<YYYY-MM>/<job-id>.md      one record per agent job, linking what it touched
  Meetings/<YYYY-MM-DD>.md  meetings (e.g. with an advisor or collaborators)
  Lean/                     the Lean project
  Wiki/
    topics/                 one page per equation or technique: prose plus generated sections
    pitfalls/               general traps (math dead ends live on failed proofs instead)
    decisions/              notation and convention choices
    glossary.md
  .loom/                    gitignored: search index, build output, scratch runs, job worktrees
  pyproject.toml, uv.lock   the project's Python environment
  .gitattributes            Git LFS patterns for run outputs
```

### File format
Every file except threads starts with TOML front matter between `+++` lines. The body below it is LaTeX (statements, proofs) or Markdown (notes, specs, records). Loom strips the front matter when it builds LaTeX.

### Statements
```toml
+++
id = "4KX7"
kind = "theorem"        # definition | lemma | proposition | theorem | corollary | conjecture | question | remark | example
title = "Energy decay for the model system"
tags = ["model-system"]
stated_using = ["2M9Q"]          # shown in the app as "2M9Q · Weak solution of the model system"
origin = "spec:S-004"   # new | spec:<id> | import:<path>
created = 2026-11-03
verified = 2026-11-03
+++
Let $u_0\in L^2(\R^n)$ and let $u$ be a weak solution with data $u_0$ ...
```
- The body is the statement only. Loom adds the environment from `kind` and the `\label` from `id`, so a file has no boilerplate.
- **No `status` field.** Status is computed (below). The only hand-set state is `refuted_by = "<proof-id or experiment-id>"` for a false statement, which needs Dakota's click.
- `question` is an informal open question: a node in the graph with no status, for things not yet precise enough to be a conjecture.
- **Editing a statement whose proof is checked needs Dakota's click.** The statement's text is part of every proof's fingerprint (see Checks), so the edit outdates the checks of every proof of it; Loom says which before it saves.

### IDs
Descriptive IDs go stale as results are generalised and superseded, so every statement, paper statement and figure gets a **short permanent tag** instead, in the spirit of the Stacks Project's tags:
- **Four characters** from Crockford's base 32 (digits and capitals without I, L, O and U, so nothing is misread): `4KX7`, `2M9Q`. That is about a million tags.
- **Random, not sequential,** so two job branches creating statements at the same time almost never pick the same tag. Loom checks at merge time and, if a branch's new tag is already taken on `main`, gives it a fresh one before merging (tags only become permanent once merged).
- **Permanent:** never renamed or reused. Superseding is a field (`superseded_by`). Statements are filed flat in `Statements/`; grouping is by `tags` and `title`, which can change freely.
- **Proofs** are named after what they prove: `4KX7.a`, `4KX7.b`, ….
- **Never typed from memory.** Wherever an ID goes (`\ref`, premises, `stated_using`, `\statement`, task links), Loom offers a picker: start typing words from a title, tag or the statement itself and pick from a list showing the compiled statement. Typing `\ref{` in the editor opens it.
- **Shown with their titles.** The editor, graph and Review show every ID with its title beside it ("4KX7 · Energy decay for the model system"), and hovering shows the compiled statement. In exported threads readers see ordinary numbering (Theorem 2.3), never tags.
- Paper keys stay author–year (they don't go stale); paper statements get tags like any statement.

### Proofs
```toml
+++
id = "4KX7.a"
proves = "4KX7"
premises = ["7RT2", "9D3F"]
level = "complete"      # sketch | heuristic | complete | failed
created = 2026-11-03
verified = 2026-11-03
+++
By Fact \ref{7RT2} with $k=0$, ...
```
- The body is the proof only. Its verbose version, if any, is `<proof-id>.verbose.tex` beside it, shown by a toggle.
- A `failed` proof records a dead end: what was tried and why it breaks, written at the top of the body. It stays on the graph as a dashed edge, so the attempt is kept where it happened.
- The proof file holds only what the proof says. What people and agents have said about it lives in its checks file.

### Checks
Each proof's checks are in `<proof-id>.checks.toml`, beside the proof. The file is plain TOML (no front matter, no body).

```toml
[[checks]]
by = "agent:claude-opus-5-5"
date = 2026-11-04
fingerprint = "a3f9c2"
verdict = "pass"            # agents: pass | needs-changes | fail
record = "Records/2026-11/job-0142.md"

[[checks]]
by = "dakota"
date = 2026-11-04
fingerprint = "a3f9c2"
verdict = "accept"          # Dakota: accept | reject

[[checks]]
by = "agent:claude-opus-5-5"
date = 2026-11-20
fingerprint = "a3f9c2"
verdict = "needs-changes"
record = "Reviews/2026-11/review-0031.md"
```

- **Append-only.** A check is never edited or deleted. A later session that disagrees, whether an agent or Dakota, adds a new entry. The file is the proof's verification history; git holds the rest.
- **Every check records what it read.** `fingerprint` is a short hash of the proof's body, its premises and the text of the statement it proves, when the check was made. Editing any of them changes the proof's current fingerprint.
- **Newest matching check wins.** For each kind of checker (agent, Dakota), only the newest check whose fingerprint matches the proof's current fingerprint counts. Checks of older versions stay in the history but don't count.
- **Dakota's accept must follow the agent's pass.** An `accept` counts only if it is newer than the agent `pass` that currently counts. If an agent later says `needs-changes` or `fail`, the earlier accept stops counting too, since it accepted something that has since been questioned. In the example, the 2026-11-20 entry overrides both earlier ones and the statement drops from proved to unchecked.
- **Every agent check links its reasoning** (`record` or a review file); Dakota's checks may link a note.

| Verdict | Effect |
| --- | --- |
| `pass` (agent) | Counts toward proved once Dakota accepts |
| `needs-changes` (agent) | The proof is probably fixable; the linked file says what's wrong. Loom flags it on Home and offers a task to fix it |
| `fail` (agent) | The argument is wrong; the proof stops supporting its statement. Loom does not set `level = "failed"` itself: that is Dakota's click, since he may repair it instead |
| `accept` (Dakota) | Together with a counting agent `pass`, makes the proof checked |
| `reject` (Dakota) | Works like `needs-changes` from Dakota |

- **Merging.** Fixing a proof and recording a check touch different files, so they never collide. When two branches both add checks to the same file, Loom merges the two lists itself (append-only, ordered by date) without asking.

### How status is computed
For a statement B, from its proofs and their premises:

| Status | When |
| --- | --- |
| **proved** | Some proof of B is `complete`, is checked (a counting agent `pass` and a newer `accept` from Dakota, under the rules in Checks), and every premise is **proved** or **cited** |
| **conditional** | Some proof is `complete` and checked, but a premise isn't proved or cited. Loom names which premises |
| **unchecked** | A `complete` proof exists but lacks one of the two checks |
| **sketch** / **heuristic** | The best proof is at that level |
| **open** | No proof that isn't `failed`. Gaps appear automatically |
| **cited** | A paper statement checked against its PDF (see Papers) |
| **extracted** | A paper statement not yet checked against its PDF; counts as unproved |
| **false** | `refuted_by` is set and accepted |
| *(none)* | Definitions, questions, remarks, examples |

- A check is tied to the proof's content by its fingerprint: editing a proof's body or premises after a check leaves that check behind, and the statement drops to **unchecked** until it is checked again. A newer `needs-changes`, `fail` or `reject` does the same.
- Cycles of premises are reported as errors; a statement can't help prove itself.
- When a status changes, everything downstream is recomputed, and the Graph and Review tabs show what moved.

### Papers
A paper's facts live in `paper.toml`: plain TOML (no front matter), filled in by agents and verified by Dakota. It holds enough to build a complete BibTeX entry, and it separates the **paper** (the work) from its **versions** (each arXiv version, the published version), because numbering and content change between versions and every locator, note and reading has to say which version it was taken from.

```toml
key = "SmithJones24"                # from the key rule (below); equals the folder name
aliases = ["SmithJones23"]          # old keys after a rename
cite = "published"                  # the version the bibliography entry and printed locators use
relevance = 1                       # rank within the project
relevance_why = "Proves the estimate the project wants to extend"
tags = ["energy-estimates", "model-system"]
summary = "Proves energy decay for weak solutions of the model system with L^2 data, ..."
created = 2026-10-18
verified = 2026-11-03

[work]                              # true of the paper whichever version
type = { value = "article", source = "https://doi.org/10.0000/jea.2024.17", verified = 2026-11-03 }
title = { value = "Energy decay for a model parabolic system", source = "https://doi.org/10.0000/jea.2024.17", verified = 2026-11-03 }
authors = { value = [ { last = "Smith", first = "Alice" }, { last = "Jones", first = "Bob" } ], source = "https://doi.org/10.0000/jea.2024.17", verified = 2026-11-03 }

[[versions]]
id = "arxiv-v1"
kind = "arxiv"
arxiv_id = { value = "2401.99999", source = "https://arxiv.org/abs/2401.99999v1", verified = 2026-11-03 }
arxiv_version = { value = 1, source = "https://arxiv.org/abs/2401.99999v1", verified = 2026-11-03 }
posted = { value = 2023-01-15, source = "https://arxiv.org/abs/2401.99999v1", verified = 2026-11-03 }
page_count = { value = 18, source = "local/arxiv-v1.pdf", verified = false }
pdf = "local/arxiv-v1.pdf"
differences = ""                    # how this version differs from the others, in our words

[[versions]]
id = "published"
kind = "published"
journal = { value = "Journal of Example Analysis", source = "https://doi.org/10.0000/jea.2024.17", verified = 2026-11-03 }
volume = { value = "12", source = "https://doi.org/10.0000/jea.2024.17", verified = 2026-11-03 }
number = { value = "3", source = "https://doi.org/10.0000/jea.2024.17", verified = 2026-11-03 }
pages = { value = "101--130", source = "https://doi.org/10.0000/jea.2024.17", verified = 2026-11-03 }
year = { value = 2024, source = "https://doi.org/10.0000/jea.2024.17", verified = 2026-11-03 }
doi = { value = "10.0000/jea.2024.17", source = "https://doi.org/10.0000/jea.2024.17", verified = 2026-11-03 }
pdf = "local/published.pdf"
differences = "Equations renumbered; adds Remark 2.5; Lemma 3.1 restricted to bounded times (reported, unchecked)"

[[reading]]                         # what was read, always of one version
version = "arxiv-v1"
level = "studied"                   # skimmed | read | studied
what = "§§1–3, every proof line by line"
date = 2026-10-18
session = "Sessions/2026-10-18"
```

**Fields.** Every bibliographic field is an inline table `{ value, source, verified }`: the value, the link it came from, and the date Dakota verified it (or `false`). The app shows them as plain fields; the verification window puts each beside its source link and the version's PDF, and Dakota ticks them off. Which fields a version needs depends on the type:

| Type | Fields (besides title and authors) |
| --- | --- |
| article | journal, volume, number, pages, year, doi; optionally journal abbreviation, month, publisher, issn |
| book | publisher, address, year, edition, series, volume in series, isbn, doi; optionally editors |
| incollection, inproceedings | booktitle, editors, publisher, address, pages, year, series, volume, doi |
| phdthesis, mastersthesis | school, address, year |
| techreport | institution, number, year |
| talk, lecture notes, other | howpublished, venue, date, url |
| arXiv version (any type) | arxiv_id, arxiv_version, posted (that version's date), primary class, page count |

Every version may also have `mr` (MathSciNet number), `zbl` (zbMATH number) and `url`: these are the best sources for verifying the others. Agents can read arXiv, DOI landing pages and zbMATH Open themselves; MathSciNet needs Dakota's login, so those links are for him to click.

**Versions.**
- Each arXiv version Loom knows of is its own entry (`arxiv-v1`, `arxiv-v3`, …), with its own posting date and PDF, since authors replace arXiv versions over time. The published version is `published`; a book's second edition is `ed2`, and so on.
- `cite` picks the version the bibliography uses: by default `published` when there is one, otherwise the newest arXiv version. A gather-literature or paper job that finds a newer arXiv version or a publication adds a version entry and flags it; it never changes `cite` itself.
- `differences` says, in our words, how a version differs from the others (renumbering, added or changed results). Agents fill it when they compare versions; Dakota verifies it like any field.

**Readings.** `[[reading]]` entries record what was read, at which level, of **which version**, and from which session. The paper's overall reading level (shown in the Library) is the highest level read of any version. A reading session records the version its annotated PDF is.

**Locators are per version.** A paper statement's stub (`Statements/<id>.toml`) gives where it is in each version it has been located in:

```toml
id = "7RT2"
kind = "theorem"
gist = "Weak solutions with L^2 data lose energy at least at rate t^{-1/2}"
stated_using = ["5HN8"]
paper_premises = ["3WQ6"]          # the paper's own proof structure
extracted_from = "arxiv-v1"
extracted_by = "agent:claude-opus-5-5"
created = 2026-10-18

[locators]
arxiv-v1 = { at = "Theorem 1.2, p. 4", checked_against_pdf = 2026-10-18 }
published = { at = "Theorem 1.2", checked_against_pdf = false }
```
- A stub is **cited** once its locator in the version it was extracted from is checked against that version's PDF; **extracted** before.
- Equations and other numbered items that aren't stubs get a renumbering table in `paper.toml` when versions differ (`[[renumbering]]` entries like `{ arxiv-v1 = "(2.18)", published = "(2.8)", checked = false }`).
- In statements, proofs and threads, `\paperref{<stub-id>}` cites a paper's result. Loom expands it at build time to `\cite[Theorem 1.2]{SmithJones24}` using the locator **for the version in `cite`**, and warns when that version's locator is missing or unchecked. Switching `cite` from an arXiv version to the published one updates every printed theorem number at once.
- The verbatim statement and proof are in `local/`, per version, and shown when present.

**Notes** say which version they refer to. `notes.md` holds every note on the paper, from reading sessions and from agents, each a block with its author, kind, date, version and place:
```
::: note {author="dakota" kind="observation" date=2026-11-03 version="arxiv-v1" at="Lemma 2.4"}
The constant here depends on T, which the abstract hides.
:::
```
Kinds: summary, observation, question, inconsistency-in-paper, disagrees-with-dakota, idea. Loom colours by author and kind and filters on them ("show where the agent thinks I'm wrong").

**Keys** follow a fixed rule, built into Loom: one to three authors give every surname then the year's last two digits (`SmithJones24`); four or more give the first surname and two letters of each other (`SmithJoBrGa20` for Smith, Jones, Brown and Garcia); surnames in ASCII CamelCase with accents dropped and particles joined (`van der Berg` → `VanDerBerg`); the year is the published version's, else the first arXiv version's; papers sharing a key all get a letter in posting order (`SmithJones24a`, `SmithJones24b`). When a preprint is published in a different year its key changes; Loom renames it everywhere and keeps the old key in `aliases`.

**In the graph,** paper statements are drawn as not Dakota's own, and each paper's nodes can be shown or hidden. A paper statement is proved by an implicit "cited from" edge.

**The bibliography** (`refs.bib`) is generated from the paper entries using the `cite` version, never edited by hand. A published version gives its usual entry plus the arXiv identifier as `eprint`, `archiveprefix` and `primaryclass`; an arXiv-only paper gives the arXiv identifier **with its version** (`2401.99999v1`). For bibliography styles that ignore `eprint` (such as `amsplain`), Loom also writes the identifier into `note`. Only verified fields are written without a warning; Export lists any unverified field it used.

### Threads
- A thread is plain LaTeX: a paper's prose, with `\statement{id}` and `\proof{id}` where a node or edge of the graph goes, and `\figure{id}` for figures.
- Loom reads those commands to show which part of the graph the thread covers, and warns when a thread uses a statement that isn't proved or a figure that's stale.
- **Export** writes a standalone `.tex` with every statement, proof and figure inlined, plus a `.bib`: an ordinary LaTeX paper for people who don't use Loom. Paper statements are cited, never inlined.

### Numerics and figures
- **An experiment** is a folder: `experiment.toml` (purpose, scripts, parameters with defaults, which statements it informs) and its `.py` scripts with `# %%` cells. Shared code goes in `Numerics/lib/`.
- **Scratch runs** (most runs) stay in `.loom/`, are listed in the run history for a while, and never enter git.
- **Kept runs:** clicking **Keep** makes one commit with the script exactly as it ran, the parameters and the output, in `Runs/<run-id>/`. `run.toml` records the commit, a fingerprint of every project file the script imported (Loom runs scripts through a wrapper that records them), the Python and package versions (from `uv.lock`), the random seed and the parameters.
- **Large outputs** (3D arrays of a function on a grid) are saved through `loom.save` as HDF5, written to a temporary file and renamed when complete so a crashed run never leaves a corrupt output, and stored with Git LFS. Small outputs (tables, scalars) are plain files.
- **Staleness is computed:** when the script or anything it imported has changed since a kept run, the run is **stale**: made with older code, not wrong. Loom offers *Show changes* (the code diff since the run) and *Re-run with the current version* (old and new outputs side by side). Figures from a stale run, and threads using those figures, inherit the warning.
- **Evidence:** experiments attach to statements as evidence. Evidence is shown in the graph but never counts toward proved. A counterexample run can back `refuted_by`, with Dakota's click.
- **A figure** is `figure.toml` (which experiment and kept run or script, parameters, caption) plus its output, committed, since exported threads need it.
- **The field viewer** shows a kept run's 3D array directly: slice planes dragged through the grid, a colour scale, later isosurfaces.

### Tasks
- One TOML file per task in `Tasks/`: title, type (read, spec, prove, check, numerics, thread, lean, chore, idea), the objects it's about, status (idea, ready, running, waiting-on-dakota, done), dependencies, and the job records it produced.
- The Tasks tab shows them as a board. Agent jobs start from a task. Ideas are tasks of type `idea` and never start a job until Dakota changes the type.
- They will sync to GitHub issues once GitHub is added.

### Records, reviews and meetings
- **Records:** every agent job writes one record (what it was asked, what it changed, what it noticed, what it cost) linking the objects it touched. Loom shows records on those objects and in a timeline.
- **Reviews:** referee files from the scheduled reviewer, each naming the statement or proof it reviews and the content version it read.
- **Meetings:** one dated page each, linkable from anything they touched.

### The wiki
Only knowledge pages:
- **`topics/`**: one page per equation or technique. Prose by Dakota and agents, plus sections Loom generates: the topic's statements by status, its open nodes, its papers, its threads.
- **`pitfalls/`**: general traps. Math dead ends are `failed` proofs instead.
- **`decisions/`**: notation and convention choices.
- **`glossary.md`**: terms and notation.

Process documents (workflow, formats, rules, how-tos for agents) belong to Loom, not the project: Loom writes the project's `CLAUDE.md` and agent definitions from its own templates.

## The app

### Tabs
A narrow vertical bar on the far left, as in TexViewer and VS Code's activity bar: one line icon per tab, out of the way of the work.
- **Hovering** an icon shows its name, its shortcut (Ctrl+1 to Ctrl+9 for the first nine) and one line on what it's for.
- **The open tab** is highlighted; the rest of the window is that tab's own layout.
- **Small badges** on an icon show what's waiting there: changes to review, tasks waiting on Dakota, running jobs.
- **Order:** the everyday tabs at the top (Home to Review in the table below); Wiki and Jobs at the bottom, with Settings below them.


| Tab | What it does |
| --- | --- |
| **Home** | `STATUS.md`, counts by status, what waits on Dakota (merges, checks to accept, entries to verify), recent activity |
| **Tasks** | The task board; start a job from a task |
| **Library** | Every paper by relevance and reading status; the verification window; open the PDF; "suggest papers" |
| **Reading** | Reading sessions: annotated PDF, transcript, notes in colour, the post-session discussion |
| **Specs** | Spec sessions: the explored neighbourhood, the proposal, approve |
| **Statements** | Edit statements and proofs with the compiled PDF beside them; compile alone, with direct premises, or with the full chain; verbose toggle; checks and records shown |
| **Graph** | The dependency graph (below) |
| **Threads** | Edit a thread with its compiled PDF and the part of the graph it covers; Export |
| **Numerics** | Python editor with cells, plots inline, run history, Keep, staleness, field viewer |
| **Lean** | Lean editor on one half, the PDF on the other, a small graph explorer |
| **Review** | Every change waiting to be merged (job branches and uncommitted edits), as compiled math and rendered Markdown, with the raw diff a click away |
| **Wiki** | Markdown editor with live preview (math rendered), backlinks, search |
| **Jobs** | The job queue: running, waiting, done; cost and limits |

### The graph
- Statements as nodes coloured by computed status; proofs as edges (a proof with several premises is drawn as one junction feeding its target); `stated using` links in a lighter style; failed proofs dashed.
- Filters: tags, status, kind, Dakota's own vs each paper's, show or hide papers. Search by ID or title.
- **Local view** (a statement, its premises and what it supports, depth slider) and **whole-project view**.
- "What is this blocking?" on any node; "Create task" on a gap; click a node to open it in Statements.

### Local review
- **Every agent job runs on its own branch** in its own git worktree under `.loom/`, so jobs never touch the files Dakota is editing and several can run at once.
- The Review tab shows a job's branch like a pull request: changed statements and proofs compiled before and after, front-matter changes in plain words ("premises: +a, −b", "check: agent pass"), status changes the computation would produce, rendered Markdown with additions highlighted, and the record's flags and notices at the top.
- Dakota merges, asks for changes (which queues a follow-up job on the same branch) or discards.
- The same view shows Dakota's own uncommitted edits against `HEAD`. All the v1 git panel features stay: branch, changed files, commit, run checks, push.

## Workflows

### Starting a project
New project: name, the research idea in Dakota's words, any seed papers (PDFs or arXiv IDs) and notes. Loom creates the folder from its template, initialises git and Git LFS, sets up the Python environment, writes `CLAUDE.md`, and creates the first tasks: write the topic pages, and gather literature.

### Literature
A **gather literature** job searches for papers, creates entries for the most relevant, ranks them with a reason for each rank, and fills the fields with sources. Dakota verifies entries in the verification window and picks what to read.

### Reading sessions
1. Dakota reads a paper on paper, annotates it in a PDF annotation app and records himself talking it through.
2. **Add reading session:** the paper, **which version was read**, the annotated PDF, the transcript, the date, comments. Loom files them under `Papers/<Key>/local/` and `Sessions/<date>/`.
3. **The paper's job** reads the PDF, the annotations and the transcript and:
   - breaks the paper into result stubs with locators and the paper's dependency graph (verbatim text into `local/`);
   - adds Dakota's notes to `notes.md` as his blocks;
   - adds its own blocks: summary, inconsistencies in the paper, places it thinks Dakota's reasoning is wrong, its own ideas;
   - fills or updates the database entry.
4. **Discussion:** when the job is done, Loom opens a chat with the agent in the paper's context. Dakota and the agent go through the notes; the full discussion is saved in `Sessions/<date>/discussion.md`, committed and released with the work for transparency, and its outcome at the top of `session.md`.
5. The changes arrive in Review as one branch.

### Spec sessions
1. Dakota names a target: a statement he wants, or a gap in the graph.
2. A **spec job** explores the neighbourhood: related statements in the project and its papers, variants (weaker, stronger, special cases), and for each one how provable it looks and what it would need. It proposes one.
3. Dakota approves (possibly edited). The approved statement becomes a statement file, and a task for a first pass is created.

### First pass and proof jobs
- **First pass:** a job writes the approved statement's proof as a draft paper broken into statements and proofs, every intermediate step marked sketch, heuristic or complete.
- **Per-proof jobs:** each proof gets its own job, which:
  1. checks the proof is valid and records an agent check;
  2. writes the verbose version that skips no steps;
  3. confirms every premise, and creates `open` statements for dependencies the proof uses that aren't in the project yet;
  4. updates any out-of-date front matter (tags, `stated_using`, dates).
- Dakota accepts checks in Review; status follows.

### Threads
Dakota (or a job) picks a target and a path through the graph that proves it from first principles. The thread adds motivation, history from the literature and connecting prose. Export makes the shareable paper.

### Scheduled reviewers
1. **Once a night,** Loom sends statements and proofs to the Claude API for review, unreviewed and recently changed ones first, until the night's budget of **$2** is spent (both settings). It stops cleanly at the budget and picks up where it left off the next night.
2. Each review becomes a file in `Reviews/` naming errors and poor explanations, with the fingerprint of what it read. Reviews never change a status or a check themselves; they show on the reviewed object.
3. **At the end of the night's session** Loom creates one task for Dakota: *review the reviews*. It lists each point raised; Dakota marks each one **act on it**, **not an issue** or **later**, with an optional note.
4. When he's done, Loom queues an agent job to implement the points marked *act on it*. Its changes arrive in Review like any job's, and any proof it changes is checked again under the usual rules.

### Lean
Lean editor on one half (completion, errors and goals through Lean's language server), the PDF of the statement on the other, a small graph explorer. A statement records the Lean declaration that formalises it (`lean = "Foo.bar"`), and the graph shows formalisation status as in Lean blueprint. Loom tries to stay compatible with leanblueprint's conventions (`\uses`, `\lean`, `\leanok`) when it exports, so blueprint's own tools can run on a Loom project. The goal-state panel (Lean's "infoview") is the expensive part and is built last; until then the Lean tab can open the file in VS Code.

## Agents and jobs
- A **job** is one run of Claude Code in headless mode, in the job's worktree, on Dakota's subscription. Its kind (gather literature, paper, spec, first pass, proof check, verbose proof, dependency audit, thread, numerics, chore) sets its prompt template, allowed tools and allowed folders.
- "The paper's agent" or "the proof's agent" is a job kind run with that object's files as its memory; nothing persists between jobs except files.
- Agents change the project only through `loom` commands (create statement, add proof, add check, add note, …) and plain edits that `loom validate` accepts. The commit of a job's branch fails if validation fails.
- **The queue** runs one or two jobs at a time (setting), waits and retries when the subscription's usage limit is reached, and shows each job's progress, record and cost.
- **The Claude API** is used only by scheduled reviewers, with the key stored in Windows' credential store.

## Technology
| Piece | Choice |
| --- | --- |
| Language, UI | C++20, Qt 6 Widgets, CMake |
| PDF view | Qt PDF |
| Editors (LaTeX, Python, Lean, TOML, Markdown) | QScintilla |
| Completion, errors, hover | One language-server client, used by the Python and Lean editors |
| Markdown with math | Qt WebEngine with KaTeX, contained in one view component |
| TOML | toml++ |
| Git | The `git` command line (as Dakota uses it), Git LFS |
| LaTeX | Local TeX Live, `latexmk`, `-synctex=1` |
| Graph layout | Graphviz |
| Python | uv-managed environment per project; numpy, scipy, matplotlib by default |
| Agents | Claude Code CLI, headless |
| Reviewers | Claude API over HTTPS |
| Tests | Catch2 for `LoomCore` and `loom` (most tests; they read like the spec); Qt Test for the UI (simulated clicks, screenshots) |
| Run outputs | HDF5 (chunked, so slices read without loading the whole array; metadata stored as attributes); scripts save through `loom.save(name, array, **attrs)` |
| Platform | Windows only; Linux only for CI |

**Code layout:** `Source/LoomCore` (the project model, validation, status and staleness computation, the formats: no UI code, so it's fully testable), `Source/LoomCli` (the `loom` command), `Source/LoomApp` (the UI). Agents and the app share `LoomCore`, which is what makes Loom the only writer.

## Loom's own repo
Organised like the owner's other agent-built repos, with two fixes learned from them:

```
Loom/
  CLAUDE.md, STATUS.md, README.md
  Specs/            specs for the software (this page first); a Build task needs an approved spec
  Source/           LoomCore, LoomCli, LoomApp
  Tests/
  Templates/        the project kit: project CLAUDE.md, agent definitions, job prompts, starter files (CC0)
  Tools/            small Python scripts for working on the repo (e.g. the index generator), with tests
  LICENSE, LICENSE-docs
  Wiki/
    start-here/     overview, architecture, build and test, workflow, writing wiki pages
    decisions/      one page per decision
    how-to/
    gotchas/        grouped by area
    logs/<YYYY-MM>/ session logs filed by month
    glossary.md
  .github/
```
- **Fix 1:** every index (the wiki's section lists, the gotchas list, the logs list) is generated by a check, never edited by hand.
- **Fix 2:** logs are filed by month, so no folder grows without bound.

## Build order
| Milestone | Delivers |
| --- | --- |
| **M1 Core** | `LoomCore` and `loom`: the project folder, every format, validation, status computation, `loom new-project` and `loom validate`, tests |
| **M2 Statements and Review** | The app shell and the side bar of tabs; Statements tab (edit, compile in three modes, PDF beside); Review tab and the git panel for Dakota's own edits. Replaces TexViewer's daily use |
| **M3 Move in** | Importer for existing research repos (below); Dakota switches to Loom |
| **M4 Tasks and Jobs** | Tasks tab, the job queue, Claude Code jobs on branches, records, job branches in Review |
| **M5 Graph** | The Graph tab |
| **M6 Library and Reading** | Paper entries, the verification window, gather literature, reading sessions and the discussion |
| **M7 Specs and proof jobs** | Spec sessions, first pass, per-proof jobs |
| **M8 Threads** | Threads tab and Export |
| **M9 Numerics** | Python editor, environment, runs, Keep, staleness, figures, field viewer |
| **M10 Reviewers** | Nightly reviews over the Claude API, the triage task, the implementing job |
| **M11 Lean** | Lean tab |
| **M12 Wiki and Home** | Wiki tab, generated topic sections, Home dashboard |

Each milestone gets its own spec in `Specs/` before it's built.

## Importing an existing project
An importer (M3) turns an existing LaTeX research repo into a Loom project, so work done before Loom isn't left behind. It handles repos organised as one result per file with a comment header (status, uses, cites) plus one Markdown page per paper:
- Each result file (statement and proof together) becomes a statement file plus a proof file. Its list of used results becomes the proof's premises, or `stated_using` for definitions. The old hand-set status becomes the proof's `level`; recorded referee passes become agent checks, which Dakota re-accepts.
- Every result gets a new tag; its old ID is kept as an alias, so search and old notes still find it.
- Paper pages become `Papers/<Key>/` entries with every field marked unverified.
- Wiki pages are sorted by kind: per-equation pages become `Wiki/topics/`, narrative pages become `Threads/`, open-problem pages become open statements or questions, pitfalls split between failed proofs and `Wiki/pitfalls/`, math decisions stay in `Wiki/decisions/`, logs and meetings become records and meetings, and process pages are dropped (Loom owns the process now).
- Frozen source drafts come in as a read-only archive folder; numerics, Lean and transcripts move to their new homes.
- The importer writes a report of everything it couldn't map, for Dakota to settle.

## Open questions
- None. Ready for Dakota's approval.

## See also
- Up: [Specs](README.md)
- TexViewer's roadmap and spec: the features Loom carries over.
- Lean blueprint (Patrick Massot's leanblueprint): the closest existing tool to Threads plus Lean.
- The Stacks Project's tags: the model for Loom's IDs.
