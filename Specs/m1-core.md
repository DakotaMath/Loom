---
type: spec
area: M1 core
tags: [m1, formats, validation, status, checks, ids, papers, cli, toolchain]
---

# Spec: M1, the core formats, rules and toolchain

> **Summary:** Everything `LoomCore` and the `loom` command do in M1, precisely enough to build without guessing: the folder `loom new-project` creates, `loom.toml`, the `+++` TOML front matter, statements, proofs, IDs, check histories and their fingerprints, the status rules with a worked example, paper entries, stubs, the key rule and the BibTeX it produces, every check `loom validate` makes with its message, every `loom` subcommand, and the toolchain (MSVC, CMake, Qt 6, toml++, Catch2) with the CI matrix. Departures from `Specs/loom.md` are listed in one place.

**Last verified:** 2026-11-03 · commit e5b68aa
**Status:** Approved by Dakota on 2026-11-03; amendments A1–A7 (from the M1 review) and A18 (from the final review) answered by Dakota on 2026-11-04; the others from A8 on (readings the fixes applied) approved by merging their PR
**Milestone:** M1

## Scope
- **Covers:** the project folder and `loom new-project`; `loom.toml`; front matter; statements, proofs and their verbose versions; IDs and aliases; check histories, fingerprints and merging them; status; paper entries, versions, readings, stubs, locators, keys and BibTeX; validation; the `loom` command; the toolchain and CI.
- **Does not cover:** tasks, threads, numerics, figures, records, reviews, meetings, spec sessions, notes and reading sessions (their formats come with their milestones); building LaTeX and `\paperref` expansion (M2); merging job branches and retagging (M4); fetching paper data and the verification window (M6); any UI.

## Departures from `Specs/loom.md`
The design is approved; these are the places this spec makes it more precise in a way the design didn't say, or changes it. Dakota approved all of them on 2026-11-03.

| # | Design says | This spec says | Why |
| --- | --- | --- | --- |
| D1 | A check's `date` is a date (`2026-11-04`) | It is a date and time with offset (`2026-11-04T15:32:10-06:00`) | "Newer" must be exact: Dakota's pass counts only if it is newer than the agent's, and two checks on the same day are common |
| D2 | The fingerprint is a short hash (example: 6 characters) | 12 hex characters of SHA-256 | 6 characters is 1 in 16 million per edit; 12 costs nothing and removes the doubt |
| D3 | The fingerprint covers the proof, "its premises" and the statement it proves | It covers the proof's body, the proved statement's body and **each premise's statement body**, and the proof's `level` (A18) | If a premise is weakened after the check, the old check shouldn't still count |
| D4 | `refuted_by` is "set and accepted" | `refuted_by` plus `refuted_on` (the date of Dakota's click); both or neither | The file needs to record the acceptance, or "accepted" can't be computed |
| D5 | Proofs prove statements | Only lemmas, propositions, theorems, corollaries and conjectures have proofs or appear as premises; paper stubs have no proof files in M1 | The other kinds have no status, so a proof of one, or a premise on one, means nothing |
| D6 | Top-level folders listed | Folders are created when their first file is written; `new-project` creates none of the empty ones | Git doesn't keep empty folders, so a clone would differ from the original |
| D7 | (silent) | `loom.toml` has `format = 1`; Loom refuses a newer format and migrates older ones | Formats will change across milestones |
| D8 | (silent) | Verbose proofs have front matter (`proof`, `fingerprint`, `created`) | So Loom can tell when the verbose version is older than the proof |
| D9 | Issue #8: renaming a key "updates every reference" | Renaming a key rewrites the paper's own files and keeps the old key as an alias; it does **not** rewrite `\cite{Old}` in statement and proof bodies, which keep working through the alias | Rewriting a body changes its fingerprint and would quietly outdate every check of every proof that cites the paper |
| D10 | Agents give `pass`, `needs-changes`, `fail`; Dakota gives `accept`, `reject` | Both give `pass`, `needs-changes`, `fail`. Dakota's `needs-changes` carries a `changes` message, which becomes a task (from M4) | Dakota's decision on the PR: one set of verdicts, and the changes he asks for go straight onto the task list |

## Amendments from the M1 review
The M1 code review (`Reviews/M1/` on branch `m1/review`) found places where the spec was silent or the code needed a rule to be fixed against. Dakota answered A1–A7 on 2026-11-04, before the fixes, and A18 after the final review (`Reviews/M1/FINAL.md`). A8–A17 and A19–A23 are the readings the fixes (PRs #21–#25, batch 6 and the final fixes) applied where the spec was still silent, or answers that make the spec say what the code does (A19, A20); Dakota approves them by merging the PR that adds them. Each is marked in the text where it applies.

| # | Question | Answer |
| --- | --- | --- |
| A1 | Where the commit guard goes when git runs hooks from elsewhere (`core.hooksPath`), or the folder isn't a Loom project at the top of its repository | Refuse and write nothing; never write hooks outside the repository's own git directory (The project folder) |
| A2 | Is an empty string in a required or naming field an error? | Yes: `missing-field`, "`{field}` is empty" (Front matter) |
| A3 | Do keys that differ only in case collide? | Yes, everywhere keys are compared; `new-paper` never replaces an existing folder (Papers, the key rule) |
| A4 | Which `arxiv_id` forms are valid? | `YYMM.NNNN`, `YYMM.NNNNN` and old-style, never with `vN`; else `bad-value` (Papers) |
| A5 | How a name with an empty `first` is written in BibTeX | The surname in braces, `{Last}` (The bibliography) |
| A6 | Do `\{` and `\}` count toward brace balance? | Yes, as in BibTeX (The bibliography) |
| A7 | Must committed checks keep their order? | Yes, relative to each other; `check-changed` says "moved" (Checks, rule 1) |
| A8 | What an empty optional field says, and which fields name something | "`{field}` is empty; give it a value or leave it out." Every bibliographic string value of a version counts as naming (Front matter) |
| A9 | A date Loom can't hold (`0000-01-01`; toml++ reads it, Qt has no year 0) | `bad-value`, and the file isn't rewritten (Front matter) |
| A10 | `new-project` under a global `core.hooksPath`; which git directory is "the repository's own"; `build-bib --out` onto a project file | Its own message; `git rev-parse --git-common-dir`; refused, `bib-out-project-file` (The project folder; The bibliography) |
| A11 | What `loom help` does, and what an unknown subcommand says | Usage on stdout, exit 0; an unknown or missing subcommand is a usage error that names the problem (The `loom` command) |
| A12 | May `superseded_by` name a paper stub? May `\paperref` name a statement? May a statement be refuted by one of its own proofs? | No to all three: `superseded-invalid`, `paperref-not-stub`, `refuted-self` (Statements; Validation) |
| A13 | The deciding proof of a proved statement; "What a proof gives"; a fingerprint Loom can't compute; git failing to read the last commit | The proof that brought it into the proved set; either counting check's `fail`; the explanation says why; exit 3, `git-failed` (How status is computed; Checks) |
| A14 | `paper-key-rule` when no key can be made; `primary_class`'s form; an `http://` source | Skipped when `authors-missing` covers it, else the key rule's own message; an arXiv category; `field-source` with its own message (Papers; Validation) |
| A15 | The JSON every subcommand prints | As listed under JSON shapes (The `loom` command) |
| A16 | Which proofs Dakota's `--outdate-checks` covers; whether `level = "failed"` is his click; what `new-paper` does with a draft's `verified` dates; a git command that hangs | Exactly the proofs he was shown; yes, when setting it, in `new-proof` and `edit-proof`; written as `false`, and it says so; stopped, code `git-timeout` (Statements; Proofs; Papers; The `loom` command) |
| A17 | Do lookups by key, old keys and `\cite` follow A3? Which entries `build-bib` leaves out | Yes: commands find a paper in any case, a case-only rename keeps no old key, an old key equal to the key ignoring case is `alias-duplicate`, and `\cite` in another case is `cite-alias`; an entry is left out (`bib-entry-skipped`, error) for unbalanced braces, a value ending in an unpaired `\`, or a `paper.toml` with errors (Papers; Validation) |
| A18 | Is a proof's `level` in its fingerprint? | Yes: a level change outdates the proof's checks; a checked proof's level changes only with `--outdate-checks` *(Dakota)* (Checks, the fingerprint; Proofs) |
| A19 | S4: what "shaped like a tag" means for an alias | What the command line would read as a tag: four characters, all in the tag alphabet once lower case is raised and `O` is read as `0`, `I` and `L` as `1` (`thm1`, `eq12`); such an alias is `alias-invalid` (IDs) |
| A20 | S5: does `new-project` check git before creating the folder? | Yes: steps 1 and 2 write nothing, and step 3 creates the folder (The project folder) |
| A21 | Is a value of only whitespace empty? | Yes, for required and naming fields: `missing-field`, "is empty"; free text is unaffected (Front matter) |
| A22 | Does a check whose `date` Loom can't read (missing, of the wrong type, or in the year 0) count? | No: it is reported, and it never counts, since it has no place in the date order (Checks, rules 2 and 5) |
| A23 | Which form of `path` the JSON gives | Project-relative with `/`, as in files, for every subcommand, `build-bib` too; `new-project` gives the new folder's absolute path (The `loom` command) |

## Decisions this spec makes
| Decision | Choice | Why |
| --- | --- | --- |
| What `LoomCore` links | Qt 6 **Core** only (never Gui, Widgets, WebEngine) | Gives SHA-256 (`QCryptographicHash`), Unicode normalisation for the key rule (`QString::normalized`), atomic writes (`QSaveFile`), processes for git (`QProcess`), a system random source (`QRandomGenerator::system()`), all without a new dependency. Still no UI code |
| The hash library | None: `QCryptographicHash::Sha256` | Qt is already a dependency |
| How toml++ and Catch2 arrive | CMake `FetchContent`, pinned by URL and SHA-256 | Two small CMake-native libraries; vcpkg would add a toolchain file and a manifest for no gain, and Qt is too big to build through it |
| Who writes front matter | Loom's own TOML writer, fields in the order this spec lists | toml++ keeps tables in a `std::map`, so it writes keys alphabetically; Loom's order keeps diffs small and files readable |
| Dakota's clicks in the `loom` command | Commands that act as Dakota ask for a typed `yes` at a terminal, and refuse when input isn't a terminal | Agents run `loom` without a terminal, so they can't act as Dakota by accident. It's a guard against mistakes, not against an agent set on cheating; M4's review of job branches is the real gate |
| The owner's name in files | Always `dakota`, as in the design | Loom is Dakota's tool; a configurable owner is a later change if ever needed |

## Behaviour

### The project folder
A project is a folder with `loom.toml` at its top. Every `loom` command finds the project by looking for `loom.toml` in the current folder and then each parent, as git does, unless `--project <folder>` is given.

**What `loom new-project <folder> --name <name> [--idea <text>]` does, in order:**
1. Refuses if `<folder>` exists and isn't empty: *"`C:\Temp\Demo` isn't empty. Loom creates projects only in a new or empty folder."* If it doesn't exist, its parent must.
2. Checks, before writing anything (the folder isn't created yet; A20), that `git` and `git lfs` run and that git knows the user's name and email. If not, it stops with one of: *"Loom needs git, and `git` didn't run. Install Git for Windows and try again."*, *"Loom needs Git LFS, and `git lfs` didn't run. Git for Windows includes it; reinstall Git with LFS ticked."*, *"Git doesn't know your name and email yet, so Loom can't make the first commit. Run `git config --global user.name \"…\"` and `git config --global user.email …`, then try again."* It also stops (code `hooks-path-outside`) if the user's global or system git config sets `core.hooksPath`, since both `git lfs install --local` and the commit guard would then write their hooks into that shared folder (A1). Its message is its own, since `install-hooks`' remedy needs a repository that doesn't exist yet (A10): *"Git runs hooks from `{folder}` (`core.hooksPath` in your global git config), so a new project's commit guard and Git LFS hooks would go there. Unset it with `git config --global --unset core.hooksPath`, then try again."*
3. Creates `<folder>` if it doesn't exist, and writes the files below from the project kit (`Templates/project/`, compiled into `loom` as a Qt resource so `loom` is one self-contained program).
4. Runs `git init -b main`, then `git lfs install --local` (hooks for this repo only; the user's global git config is never touched), then `loom install-hooks` (below).
5. Stages everything and commits with the message `Create project <name>`, through the pre-commit hook like any commit.
6. Prints the folder's path. If any step after 2 fails, it says which, and leaves what it made in place for the user to inspect (it never deletes files).

**Files it creates:**

| Path | Content |
| --- | --- |
| `loom.toml` | As below, with the name, the idea and today's date |
| `CLAUDE.md` | The M1 project kit's agent instructions (below). M4 replaces it with the full template |
| `TeX/preamble.tex` | `% Packages for this project. Loom adds the theorem environments itself.` then `\usepackage{amsmath,amssymb,amsthm}` |
| `TeX/macros.tex` | `% Macros for this project, e.g. \newcommand{\R}{\mathbb{R}}` |
| `.gitignore` | As below |
| `.gitattributes` | As below |

No other folder is created. `Statements/`, `Proofs/`, `Papers/` and `.loom/` appear when their first file is written; a missing folder means the same as an empty one. In the kit, `.gitignore` and `.gitattributes` are stored as `gitignore` and `gitattributes` and renamed when copied, so they don't act on Loom's own repo.

**`CLAUDE.md` (M1):**
```markdown
# This is a Loom project

Loom owns the files in this folder. Change them only with `loom` commands, or with plain edits
that `loom validate` accepts. Run `loom validate` before every commit; the pre-commit hook runs it too.
Never write a check by `dakota`, never set `refuted_on` or `checked_against_pdf`, never mark a
field verified, never mark a proof `failed`, and never outdate checks (`--outdate-checks`): those
are Dakota's clicks.
```

**`.gitignore`:**
```
# Written by Loom.
.loom/
Papers/*/local/
```

**`.gitattributes`:**
```
# Written by Loom.
* text=auto eol=lf
*.pdf binary
*.png binary
*.h5 filter=lfs diff=lfs merge=lfs -text
*.checks.toml merge=loom-checks
```
`eol=lf` makes every checkout LF on Windows too, so the bytes Loom hashes are the same on every machine. `*.h5` is the Git LFS pattern for run outputs (M9 uses it). `merge=loom-checks` names the merge driver for check histories (see Checks).

**`loom install-hooks`** (run by `new-project`; run it by hand after cloning a project, since git doesn't clone hooks or local config):
- writes `.git/hooks/pre-commit`: a `sh` script (Git for Windows runs hooks with its own `sh`) that runs `loom validate --staged`, trying `loom` on `PATH` first and then the absolute path of the `loom` that installed the hook. If neither runs, the commit is refused: *"Loom's checks couldn't run because `loom` wasn't found. Put Loom on PATH, or run `loom install-hooks` again."*
- sets, in the repo's own `.git/config`: `merge.loom-checks.name = Loom check history` and `merge.loom-checks.driver = loom merge-checks %O %A %B`.
- An existing `pre-commit` hook that Loom didn't write is renamed `pre-commit.before-loom`, and Loom says so.
- **Only for a Loom project at the top of its own repository** (A1). It refuses, writing nothing, when the folder has no `loom.toml` (exit 3, as for any command outside a project), or when it isn't the repository's top level (`git rev-parse --show-toplevel`): *"`{folder}` isn't the top of its git repository, so its hooks belong to another project. Run `loom install-hooks` in a Loom project."* (exit 3, code `hooks-not-top-level`).
- **Only inside the repository's own git directory** (A1). If git runs hooks from somewhere else (`core.hooksPath` is set, locally or globally), it refuses, writing nothing: *"Git runs hooks from `{folder}` (`core.hooksPath`), outside this project, so Loom can't install its commit guard there. Run `git config --local core.hooksPath .git/hooks` here, then `loom install-hooks` again."* (exit 3, code `hooks-path-outside`). A hook in `.git/hooks` that git never runs would look like a guard and not be one. "Outside" means outside `git rev-parse --git-common-dir`, so a linked worktree, whose hooks git keeps in the main repository's git directory, is accepted (A10).

### `loom.toml`
Plain TOML (no front matter).

```toml
format = 1
name = "Energy decay"
idea = """
Find the sharp decay rate for weak solutions of the model system with L^2 data.
"""
created = 2026-11-03

[tex]
preamble = ["TeX/preamble.tex", "TeX/macros.tex"]

[[tags]]
id = "model-system"
kind = "equation"
title = "The model system"

[[tags]]
id = "energy-estimates"
kind = "technique"
title = "Energy estimates"
```

| Field | Required | Type and rule |
| --- | --- | --- |
| `format` | yes | Integer. M1 writes and reads `1`. A larger number: Loom stops with *"This project was made by a newer Loom (format N). Update Loom to open it."* |
| `name` | yes | Non-empty string |
| `idea` | no | String: the research idea in Dakota's words |
| `created` | yes | Date |
| `tex.preamble` | yes | Array of project-relative paths to existing `.tex` files, read in order |
| `tags` | no | Array of tables, each `id` (lowercase words joined by `-`: `[a-z0-9]+(-[a-z0-9]+)*`), `kind` (`equation`, `technique` or `other`), `title` (non-empty). IDs unique |

Every tag used on a statement, stub or paper must be declared here; `loom new-tag` adds one. Later milestones add tables (`[settings]`, …) and raise `format`.

### Front matter
Every statement, proof and verbose proof is a text file with TOML front matter, then a LaTeX body:
```
+++
<TOML>
+++
<body>
```
- **Encoding:** UTF-8. A byte-order mark is skipped when reading (with a warning) and never written.
- **Line endings:** Loom writes LF. When reading, every CRLF counts as LF, so a file saved by a Windows editor reads, validates and fingerprints the same. When Loom rewrites such a file, the whole file comes out LF.
- **Where it starts and ends:** the first line must be exactly `+++` (after a byte-order mark, if any). The front matter ends at the next line that is exactly `+++`. The body is everything after that line's line ending. A `+++` line in the body (in a verbatim block, say) is just body text, since only the first closing line counts.
- **The TOML** is TOML 1.0, parsed by toml++. Error line numbers are file lines (the TOML's line plus 1).
- **The body is kept byte for byte.** When Loom changes front matter, it re-parses the file, changes the data and writes `+++`, the front matter in canonical form, `+++`, then the body bytes it read, untouched (apart from CRLF becoming LF, above). When Loom creates a file from a `--body` it adds a final newline if the text has none.
- **Canonical form:** the fields in the order of this spec's tables, one per line as `key = value`; strings as basic strings (`"…"`, escaping `"`, `\` and control characters, so no string ever spans lines); arrays inline (`["7RT2", "9D3F"]`); dates as `2026-11-03`; optional fields left out when empty or absent. Comments in front matter are dropped on rewrite, since front matter is data: prose belongs in the body.
- **Unknown fields are errors,** so a misspelt field is caught rather than ignored.
- **Empty values are missing values** (A2). In every file Loom reads (front matter, `loom.toml`, `paper.toml`, stubs, check histories), a required field, or an optional field whose value names something (an ID, a tag, a key, a path, a date, a fingerprint), that is present but empty (`""`) is `missing-field`, worded "`{field}` is empty". A value of only whitespace (`"   "`) is empty too (A21). An optional field with no value is left out, never written empty. Free-text optional fields (such as a version's `differences`), and fields the tables say may be empty (an author's `first`), are unaffected.
  - **Wording** (A8): a required field says "`{field}` is empty; every {thing} needs one."; an optional one says "`{field}` is empty; give it a value or leave it out."
  - **Which optional fields name something** (A8): `origin`, `superseded_by`, `refuted_by`, every date, a version's `pdf`, a reading's `session`, a check's `record` and `note`, a draft's `key` and `cite`, and every bibliographic string value of a version (`doi`, `journal`, `primary_class`, …). Free text that may be empty: `idea`, `relevance_why`, `summary`, `differences`, a stub's `title`, an author's `first`, renumbering labels. An empty `gist` keeps its own code, `stub-gist-empty`; a check's `changes` keeps `wrong-type`, since the Checks table already requires it non-empty.
- **Dates Loom can't hold** (A9): TOML allows the year 0 (`0000-01-01`), Qt's dates don't. Such a date, in any file, is `bad-value`: "`{field}` can't be `0000-01-01`; it must be one of the dates from 0001-01-01 on." Loom then won't rewrite the file, since the rewrite would drop the date.
- **Writes are atomic:** through `QSaveFile` (write a temporary file, then rename), so a crash never leaves half a file. A `loom` command that writes takes the lock `.loom/write.lock` first and waits up to 10 seconds for it.

### Statements
`Statements/<id>.tex`. The body is the statement only; Loom adds the environment from `kind` and the `\label` from `id` when it builds LaTeX (M2).

```
+++
id = "4KX7"
kind = "theorem"
title = "Energy decay for the model system"
tags = ["model-system"]
stated_using = ["2M9Q"]
aliases = ["thm:energy-decay"]
origin = "spec:S-004"
created = 2026-11-03
verified = 2026-11-03
+++
Let $u_0\in L^2(\R^n)$ and let $u$ be a weak solution with data $u_0$ ...
```

| Field | Required | Type and rule |
| --- | --- | --- |
| `id` | yes | A tag (see IDs), equal to the file name |
| `kind` | yes | `definition`, `lemma`, `proposition`, `theorem`, `corollary`, `conjecture`, `question`, `remark` or `example` |
| `title` | yes | Non-empty string, one line |
| `tags` | no | Array of tag IDs declared in `loom.toml`, no repeats |
| `stated_using` | no | Array of statement or stub tags, no repeats, not itself; no cycles through `stated_using` |
| `aliases` | no | Array of old names (see IDs) |
| `origin` | no (default `new`) | `new`, `spec:<spec-id>` or `import:<path>`, where the part after `:` is non-empty |
| `superseded_by` | no | The tag of another statement of this project, not a paper stub (A12); no cycles |
| `refuted_by` | with `refuted_on` | A proof ID in the project, not a proof of this statement (A12; M9 adds experiment IDs) |
| `refuted_on` | with `refuted_by` | Date of Dakota's click accepting the refutation. Dakota-only |
| `created` | yes | Date |
| `verified` | no | Date the front matter (title, tags, `stated_using`) was last checked against the body |

- **Statuses by kind:** `definition`, `question`, `remark` and `example` have no status, no proofs, and can't be premises or refuted. The other five kinds (the **provable kinds**) can.
- **The body** must not be empty (only whitespace counts as empty).
- **Editing a statement whose text is in a checked proof's fingerprint** (a proof of it, or one using it as a premise) is refused by `LoomCore` unless the caller passes the confirmation that stands for Dakota's click; the refusal names the proofs whose checks it would outdate. Title, tags and other front matter aren't in any fingerprint and edit freely.
- **The confirmation names the proofs Dakota was shown** (A16), and covers exactly those. If, by the time the edit runs, it would outdate a different set (the project changed in between), it is refused as `outdates-checks` and nothing is written: *"Loom didn't change {id}, because it would now outdate the checks of {proofs}, not those of {confirmed} that Dakota confirmed: the project changed in between. Run the command again to see what it outdates now."* The same holds for `edit-proof`.

### Proofs
`Proofs/<statement-id>/<proof-id>.tex`.

```
+++
id = "4KX7.a"
proves = "4KX7"
premises = ["7RT2", "9D3F"]
level = "complete"
created = 2026-11-03
verified = 2026-11-03
+++
By Fact \ref{7RT2} with $k=0$, ...
```

| Field | Required | Type and rule |
| --- | --- | --- |
| `id` | yes | `<statement-id>.<letters>`, equal to the file name without `.tex` |
| `proves` | yes | The statement's tag, equal to the folder name. Must be a provable kind and not a stub |
| `premises` | yes (may be `[]`) | Array of tags of provable statements or stubs; no repeats; not the proved statement itself |
| `level` | yes | `sketch`, `heuristic`, `complete` or `failed`. Setting `failed` (in `new-proof` or `edit-proof`) is Dakota's click (A16); without his confirmation it is refused, `dakota-only`: *"Only Dakota can mark a proof failed, and only at a terminal."* Changing a failed proof back to another level is not a click. The level is in the proof's fingerprint (A18), so changing the level of a checked proof outdates its checks and is refused unless Dakota confirms it, as a change to its body or premises is (Statements); setting `failed` on a checked proof needs both of his confirmations |
| `created` | yes | Date |
| `verified` | no | Date the premises and level were last checked against the body |

- The body must not be empty. A `failed` proof's body starts with what was tried and why it breaks.
- Beside the proof, and only there, may sit `<proof-id>.verbose.tex` and `<proof-id>.checks.toml`. Nothing else goes in a proof folder.

**Verbose proofs** (`<proof-id>.verbose.tex`):

```
+++
proof = "4KX7.a"
fingerprint = "3c8e1f0a92d4"
created = 2026-11-05
+++
<the step-by-step proof>
```
`proof` names its proof; `fingerprint` is the proof's fingerprint when the verbose version was written from it. When the proof's current fingerprint differs, the verbose version is **stale** (a warning). The verbose body is not part of any fingerprint.

### IDs
**Tags** name statements and paper stubs (M9 adds figures). One namespace: no tag is used twice in a project.
- **Alphabet:** Crockford's base 32, `0123456789ABCDEFGHJKMNPQRSTVWXYZ` (digits, then capitals without I, L, O, U).
- **Length:** exactly four characters, so 32⁴ = 1,048,576 tags, of which the 10,000 made only of digits are never generated (they read as numbers: "Theorem 2024").
- **Generation:** each character drawn uniformly from the alphabet with `QRandomGenerator::system()`; drawn again if the tag is all digits or already used in the project as a tag or alias. After 100 failed draws, Loom stops with *"Loom couldn't find a free tag after 100 tries; the project may be nearly full."* (It won't be: at 10,000 statements a draw fails 1% of the time.)
- **In files** tags are written exactly, in capitals. **On the command line** they're forgiving, as Crockford intends: lower case is accepted, `O`/`o` is read as `0`, and `I`, `i`, `L`, `l` as `1`.
- **Collisions between branches:** two branches can pick the same new tag. Tags become permanent only when merged into `main`; at merge time (M4), Loom gives the incoming statement a fresh tag and rewrites every reference to it on the branch (file names, front matter, `\ref`, `\paperref` and the rest in bodies). Since bodies change, the fingerprints of proofs that mention it change and their checks on the branch stop counting; the merge view says so. M1 only detects a duplicate tag (an error).
- **Permanent:** tags are never renamed after merging or reused. `loom` has no command that deletes a statement or proof; superseding is `superseded_by`, a dead end is `level = "failed"`. Deleting a file by hand is a warning.

**Proof IDs** are `<tag>.<letters>`: `4KX7.a`, …, `4KX7.z`, then `4KX7.aa`, `4KX7.ab`, … (lowercase letters counting in bijective base 26). A new proof takes the letter after the highest one in its folder, so a deleted proof's letter isn't reused. Collisions between branches are handled like tags, at merge (M4).

**Aliases** are a statement's or stub's old names: from an import (`lem:energy-decay`) or anything else it used to be called. An alias is 1–100 characters with no whitespace and none of `{}[],"`, isn't shaped like a tag, and is unique across every tag and alias in the project. **Shaped like a tag** means the command line would read it as one (A19): four characters that are all in the alphabet once lower case is raised and `O` is read as `0`, `I` and `L` as `1`. So `thm1`, `eq12` and `4kx7` can't be aliases, while `thm:1` and `lem-2` can. Loom resolves an alias wherever a person types an ID (the picker, the command line), but **files must use tags**: an alias in front matter is an error that names the tag to write instead.

**Paper keys** follow the key rule (Papers); a paper's old keys are its `aliases`, unique across every key and key alias.

### Checks
`Proofs/<statement-id>/<proof-id>.checks.toml`: plain TOML, one array of tables `checks`, nothing else.

```toml
[[checks]]
by = "agent:claude-opus-5-5"
date = 2026-11-04T10:12:44-06:00
fingerprint = "3c8e1f0a92d4"
verdict = "pass"
record = "Records/2026-11/job-0142.md"

[[checks]]
by = "dakota"
date = 2026-11-04T16:03:09-06:00
fingerprint = "3c8e1f0a92d4"
verdict = "needs-changes"
changes = """
Step 3 uses Lemma 7RT2 with k = 0, but 7RT2 needs k ≥ 1. Either prove the k = 0 case
as a separate lemma or start the induction at k = 1 and treat k = 0 by hand.
"""
```

| Field | Required | Type and rule |
| --- | --- | --- |
| `by` | yes | `dakota`, or `agent:<model>` with `<model>` matching `[a-z0-9][a-z0-9.-]*` |
| `date` | yes | Offset date-time to the second |
| `fingerprint` | yes | 12 lowercase hex characters |
| `verdict` | yes | `pass`, `needs-changes` or `fail`: the same three for agents and Dakota |
| `changes` | Dakota's `needs-changes`: yes. An agent's `needs-changes`: optional. Any other verdict: not allowed | Non-empty text saying exactly what needs to change. Loom writes it as a multi-line string when it has line breaks |
| `record` | agents: yes | Project-relative path to the job record or review that holds the reasoning (`Records/…` or `Reviews/…`). A missing file is a warning until M4 writes records |
| `note` | no | Project-relative path to a note, for anything Dakota wants to say beyond a verdict |

**Verdicts.** Agents and Dakota give the same three:

| Verdict | Means | Effect |
| --- | --- | --- |
| `pass` | The proof is right as it stands | Counts toward checked: a proof needs a counting `pass` from an agent and a newer counting `pass` from Dakota |
| `needs-changes` | Probably fixable; `changes` (Dakota) or the record (agents) says what to fix | The proof isn't checked. **Dakota's `needs-changes` becomes a task:** from M4, the same click creates a task in `Tasks/` with `changes` as its brief, linking the proof and the check, ready to start a job. M1 has no task list yet, so the message waits in the check and `loom status` shows it. An agent's `needs-changes` is flagged on Home with an offer to make a task, as the design says |
| `fail` | The argument is wrong | The proof stops supporting its statement. Loom doesn't set `level = "failed"` itself; that's Dakota's click, since he may repair the proof instead |

**Rules.**
1. **Append-only.** No entry is ever edited, removed or reordered. Validation compares the file with its version in `HEAD`: every committed entry must still be there, unchanged and in the same order relative to the other committed entries (A7); new entries may sit between them, as a merge interleaves by date. With no commit yet, or outside a git repository (`git rev-parse --git-dir` fails with "not a git repository", read with `LC_ALL=C` so it isn't translated), there is nothing to compare with. If git fails in any other way (a repository git refuses as of dubious ownership, say), `validate` stops with exit 3, code `git-failed` (A13): *"Loom couldn't read the last commit to compare the checks with it: `{git command}` failed ({git's message})."*
2. **Date order.** Entries are in non-decreasing `date` order. Ties are allowed; then the later entry in the file is the newer.
3. **Who.** Every agent check has a `record`. Every `needs-changes` by Dakota has `changes`.
4. **Matching.** A check *matches* when its `fingerprint` equals the proof's current fingerprint. Checks that don't match stay in the history and count for nothing.
5. **The counting checks.** The **counting agent check** is the newest matching check by an agent (any model). The **counting Dakota check** is the newest matching check by Dakota. A check without a `date` Loom can read (missing, of the wrong type, or in the year 0, A9) is an error and never counts (A22).
6. **Checked.** A proof is **checked** when the counting agent check is `pass`, the counting Dakota check is `pass`, and Dakota's pass is newer than the agent's. (Dakota's pass confirms the agent's; if an agent checks again afterwards, Dakota passes it again.)
7. **Fail.** When either counting check is `fail`, the proof **doesn't support** its statement: for status it is treated like a `failed` proof.

**Verdict table** (every row is a Catch2 test; `A:` is an agent's check, `D:` is Dakota's; "now" means the fingerprint matches, "old" that it doesn't; entries oldest first):

| Checks | Checked? | Supports? | Why |
| --- | --- | --- | --- |
| (none) | no | yes | No checks yet |
| A:pass now | no | yes | Waits for Dakota's pass |
| A:pass now, D:pass now | **yes** | yes | |
| D:pass now, A:pass now | no | yes | Dakota's pass is older than the agent's |
| A:pass now, D:pass now, A:pass now | no | yes | A re-check by an agent needs Dakota's pass again |
| A:pass now, D:pass now, A:needs-changes now | no | yes | The counting agent check is needs-changes |
| A:pass now, D:pass now, A:fail now | no | **no** | The counting agent check is fail |
| A:pass now, D:needs-changes now | no | yes | The counting Dakota check is needs-changes (and it becomes a task) |
| A:pass now, D:pass now, D:needs-changes now | no | yes | Dakota's newest check is needs-changes |
| A:pass now, D:fail now | no | **no** | The counting Dakota check is fail |
| A:pass now, D:fail now, D:pass now | **yes** | yes | Dakota's newest check, a pass, is the one that counts |
| A:pass old, D:pass old | no | yes | Neither matches: the proof was edited since |
| A:pass old, D:pass old, A:pass now | no | yes | Waits for Dakota's pass of the new version |
| A:fail old, A:pass now, D:pass now | **yes** | yes | The old fail doesn't match |
| A:pass now (agent X), A:needs-changes now (agent Y), A:pass now (agent X) | no | yes | Counting is the newest agent check: X's second pass, which no pass from Dakota follows |
| A:pass now, A:needs-changes now, D:pass now | no | yes | The counting agent check is needs-changes |
| D:needs-changes now, without `changes` | | | Invalid: `check-changes-missing` |

**The fingerprint** of a proof is computed as follows.
1. **Normalise each text** (a body): CRLF becomes LF; spaces and tabs at the end of every line are removed; newlines at the very end are removed. Nothing else changes. So an editor trimming whitespace doesn't outdate a check, and any other edit does.
2. **Build one byte string** from sections, each written as a label line, the length of the text in bytes as a decimal line, the text, and `\n`:
   ```
   loom-fingerprint-v2\n
   level\n<length>\n<level>\n
   statement <tag>\n<length>\n<normalised body of the proved statement>\n
   premise <tag>\n<length>\n<normalised body of the premise>\n      (one per premise, by tag in byte order)
   proof\n<length>\n<normalised body of the proof>\n
   ```
   A premise that is a paper stub is written `premise <tag>\n0\n\n`: its content is the paper, which doesn't change under us, and its `gist` is our wording, free to improve. `<level>` is the proof's `level` as its front matter gives it (`sketch`, `heuristic`, `complete` or `failed`), not normalised (A18).
3. **Hash** the bytes with SHA-256 and keep the first 12 lowercase hex characters.

What isn't in it: titles, kinds, tags, `stated_using`, dates, the order of `premises`, the verbose version, and the bodies of `stated_using` statements (definitions change rarely, and the cascade of re-checks would be large). Changing which premises a proof has changes it (a section appears or goes).

The proof's `level` is in it (A18): a level is a claim about the proof, so checks of the proof at its old level don't carry over to the new one, and a checked sketch raised to `complete` isn't checked until it is checked again.

**Adding a check** (`loom add-check`) computes nothing on the caller's word: Loom computes the current fingerprint itself. An agent passes the fingerprint of the version it read (`--fingerprint`, from `loom fingerprint` before reading); if the proof has changed since, the check is refused: *"4KX7.a changed after you read it (you read 3c8e1f0a92d4, it is now 77b2e9c0d415). Read it again before checking it."* The date is the current local time.

**Merging two check lists** (`loom merge-checks`, the git merge driver, and the function M4's merges call):
1. Parse the base, ours and theirs. If any fails to parse, exit 1 and leave git's conflict for a person.
2. The result is every entry of ours, plus every entry of theirs that isn't already in ours (entries are equal when all their fields are equal), sorted stably by `date`; for equal dates, ours' entries come first.
3. An entry in the base that's missing from ours or theirs was deleted, which is never allowed: it is put back, and the driver prints *"A check was deleted on one side of the merge; Loom kept it, since checks are never removed."*
4. Write the result in canonical form into the "ours" file and exit 0.

### How status is computed
Statuses: `proved`, `conditional`, `unchecked`, `sketch`, `heuristic`, `open`, `cited`, `extracted`, `false`, and no status.

**What a proof gives.** Every proof P of statement S gives one of:
- **nothing**, if `level = "failed"` or either counting check (agent's or Dakota's) is `fail` (P doesn't support S; rule 7 of Checks, A13);
- **complete-checked**, if `level = "complete"` and P is checked;
- **complete-unchecked**, if `level = "complete"` and P isn't checked;
- **sketch** or **heuristic**, from its level (checks on these levels change nothing except a `fail`).

**The proved set** is the smallest set of statements such that S is in it when S is a provable kind, not refuted, and some proof of S gives complete-checked with every premise in the proved set or **cited**. Loom computes it by starting from the empty set and adding statements until nothing changes. Because it is the *smallest* such set, a statement can never help prove itself: in a cycle of premises, nobody is proved unless some proof enters the cycle from outside.

**The rules,** first match wins:

| # | When | Status |
| --- | --- | --- |
| 1 | `kind` is `definition`, `question`, `remark` or `example` | no status |
| 2 | A paper stub whose locator in its `extracted_from` version has `checked_against_pdf` set to a date | **cited** |
| 3 | A paper stub otherwise | **extracted** |
| 4 | `refuted_by` and `refuted_on` are both set | **false** |
| 5 | In the proved set | **proved** |
| 6 | Some proof gives complete-checked | **conditional** |
| 7 | Some proof gives complete-unchecked | **unchecked** |
| 8 | Some proof gives sketch | **sketch** |
| 9 | Some proof gives heuristic | **heuristic** |
| 10 | Otherwise | **open** |

**Why sketch outranks heuristic:** a sketch is a proof with routine gaps, meant to become complete; a heuristic is a non-rigorous argument that suggests the statement is true. The levels rank `complete` > `sketch` > `heuristic` everywhere Loom compares them.

**Tie-breaks and explanations.** The **deciding proof** is the proof the matching rule rests on; when several qualify, the one with the fewest premises that are not proved or cited, then the lowest letter (`a` before `b`, `z` before `aa`). For a **proved** statement it is the proof that brought the statement into the proved set (A13): the set grows in rounds, each judged against the set as the round began, and the deciding proof is the lowest-lettered proof that qualified in the round the statement entered. Its premises were all proved or cited before it, so the explanation never goes round in a circle (the worked example's `H8KD proved (via J3RW.b)`); the plain rule could pick a proof whose premise is proved only through the statement itself.
- A **conditional** statement **rests on** the premises of its deciding proof that are neither proved nor cited, in tag order. Loom names them, with their statuses.
- An **unchecked** statement says what its deciding proof lacks: an agent pass, Dakota's pass, a pass from Dakota newer than the agent's, or a fix (a counting check is `needs-changes`; Dakota's `changes` text is quoted). When Loom can't compute the proof's fingerprint (a premise's front matter doesn't parse, say), no check can count, and the explanation says why in a second sentence (A13): `4KX7 unchecked: 4KX7.a is complete, but no check of it can count. Loom can't compute the fingerprint of 4KX7.a: Loom can't read the front matter of its premise 5HN8 (Statements/5HN8.tex).`
- An **open** statement says whether it has no proofs or only proofs that don't support it.

**Superseded statements** keep the status their proofs give them (a superseded result is usually still true). Using one as a premise is a warning.

**Cycles.** A cycle of premises (S has a proof with premise T, T has a proof with premise S, possibly through more statements) is reported as an error naming the statements in order, as the design says. The status rules don't need the cycle to be removed: the proved set handles it.

**Recomputing.** Status is a pure function of the files: `LoomCore` reads the project, builds the graph and computes every status from scratch. At M1's sizes (thousands of statements) this takes milliseconds, so "recompute everything downstream" is "recompute everything"; an incremental version waits until it's needed.

**`loom status <id>` explains in one sentence** (two when a fingerprint can't be computed, above):
- `4KX7 proved: 4KX7.a is complete and checked, and its premises 5HN8, 9D3F are proved.`
- `6BWT conditional: 6BWT.a is complete and checked, but rests on 3WQ6, which is extracted (not yet checked against the paper's PDF).`
- `8CJA unchecked: 8CJA.a is complete and has an agent pass, but no pass from Dakota.`
- `B7MP unchecked: Dakota asked for changes to B7MP.a: "Step 3 uses Lemma 7RT2 with k = 0, but …"`
- `D9XF open: no proof supports it (D9XF.a failed; an agent check says D9XF.b is wrong).`
- `F2NS false: refuted by G5TZ.a, accepted 2026-11-06.`

### The worked example
This graph is a test project in `Tests/` (with the record files its agent checks link, and a complete paper entry, so nothing else warns): the test computes every status and compares with the table, and runs validation and compares with the expected problems. "pass, pass" means an agent pass then a newer pass from Dakota, both matching the current fingerprint, unless said otherwise.

Paper `SmithJones24` has two stubs: **7RT2** (theorem, locator for `extracted_from` checked against the PDF) and **3WQ6** (lemma, not checked).

| Statement | Kind | Proofs: level, premises, checks | Expected status |
| --- | --- | --- | --- |
| 7RT2 | theorem (stub) | none | **cited** |
| 3WQ6 | lemma (stub) | none | **extracted** |
| 2M9Q | definition | none | no status |
| R2HW | question | none | no status |
| S6KT | theorem | none | **open** (no proofs) |
| 5HN8 | lemma | a: complete, [], pass, pass | **proved** |
| 9D3F | lemma | a: complete, [7RT2], pass, pass | **proved** (a cited premise counts) |
| 4KX7 | theorem, `stated_using = ["2M9Q"]` | a: complete, [5HN8, 9D3F], pass, pass | **proved** |
| 6BWT | lemma | a: complete, [3WQ6], pass, pass | **conditional**, rests on 3WQ6 |
| 8CJA | proposition | a: complete, [5HN8], pass only | **unchecked** (no pass from Dakota) |
| AH3K | lemma | a: complete, [], pass, pass, both at an old fingerprint (body edited since) | **unchecked** |
| B7MP | lemma | a: complete, [], pass, pass, then Dakota's needs-changes with `changes` | **unchecked** |
| C5RE | lemma | a: complete, [], Dakota's pass, then an agent's pass | **unchecked** (Dakota's pass is older) |
| CQ2R | lemma | a: heuristic, [] | **heuristic** |
| W5HC | lemma | a: heuristic, []; b: sketch, [] | **sketch** (sketch outranks heuristic) |
| D4VS | lemma | a: sketch, [5HN8] | **sketch** |
| D9XF | conjecture | a: failed, []; b: complete, [], pass, then fail | **open** (nothing supports it) |
| E4GV | theorem | a: complete, [8CJA], pass, pass; b: sketch, [] | **conditional**, rests on 8CJA |
| E8TB | theorem | a: complete, [5HN8, 3WQ6, 8CJA], pass, pass; b: complete, [8CJA], pass, pass | **conditional**, rests on 8CJA (deciding proof b: fewer unmet premises) |
| G5TZ | proposition | a: complete, [], pass, pass | **proved** |
| F2NS | lemma, `refuted_by = "G5TZ.a"`, `refuted_on` set | a: complete, [], pass, pass | **false** |
| T9GB | lemma | a: complete, [F2NS], pass, pass | **conditional**, rests on F2NS |
| H8KD | lemma | a: complete, [J3RW], pass, pass | **proved** (via J3RW.b) |
| J3RW | lemma | a: complete, [H8KD], pass, pass; b: complete, [], pass, pass | **proved** |
| K6YM | lemma | a: complete, [M1PQ], pass, pass | **conditional**, rests on M1PQ |
| M1PQ | lemma | a: complete, [K6YM], pass, pass | **conditional**, rests on K6YM |
| N7ZE | lemma, `superseded_by = "4KX7"` | a: complete, [], pass, pass | **proved** |
| P3QA | lemma | a: complete, [N7ZE], pass, pass | **proved** |

**Expected validation problems** (and no others): `premise-cycle` H8KD → J3RW → H8KD; `premise-cycle` K6YM → M1PQ → K6YM; `refuted-but-proved` F2NS; `premise-false` T9GB.a; `premise-superseded` P3QA.a.

### Papers
`Papers/<Key>/` holds `paper.toml`, `Statements/<tag>.toml` (stubs), and three things M1 doesn't read: `notes.md` and `Sessions/` (M6) and `local/` (gitignored).

**`paper.toml`** is plain TOML, as in the design's example (`Specs/loom.md`, *Papers*), with these rules.

Top level:

| Field | Required | Type and rule |
| --- | --- | --- |
| `key` | yes | Equals the folder name; see the key rule |
| `aliases` | no | Old keys, unique across all keys and key aliases |
| `cite` | yes | The `id` of one of `versions` |
| `relevance` | yes | Integer ≥ 1; 1 is most relevant; ties allowed |
| `relevance_why` | no | String |
| `tags` | no | Declared tag IDs |
| `summary` | no | String, in our words |
| `created` | yes | Date |
| `verified` | no | Date |
| `[work]` | yes | `type`, `title`, `authors` (all fields, below); optionally `editors` |
| `[[versions]]` | yes, at least one | Below |
| `[[reading]]` | no | Below |
| `[[renumbering]]` | no | Below |

**Fields** (every bibliographic value) are inline tables `{ value = …, source = "…", verified = … }`: `source` is a non-empty URL (`https://…`; an `http://` one is `field-source`, A14) or a path inside the paper's `local/`; `verified` is the date of Dakota's click, or `false`. Setting `verified` to a date is Dakota's click (M6's verification window); agents write `false`.

**`[work]`:**
- `type`: one of `article`, `book`, `incollection`, `inproceedings`, `phdthesis`, `mastersthesis`, `techreport`, `talk`, `lecture-notes`, `other`.
- `title`: string; LaTeX math in `$…$` allowed.
- `authors` and `editors`: arrays of `{ last = "…", first = "…" }`, `last` including any particle as printed (`"van der Berg"`, `"de Giorgi"`). `first` may be empty. At least one author, or for a book with no authors, at least one editor.

**`[[versions]]`:** each has `id`, `kind`, `pdf` (optional path inside `local/`), `differences` (optional string), and fields from the table for its kind and the work's type.
- `kind = "arxiv"`: `id` is `arxiv-v<N>`, and `arxiv_version` must equal N. Fields: `arxiv_id` (A4: `YYMM.NNNN` for April 2007 to December 2014, `YYMM.NNNNN` from January 2015, or old-style `archive/YYMMNNN`, optionally with a subject class, such as `math/0601001` or `math.AP/0601001`; never with a version suffix `vN`, which belongs in `arxiv_version`; anything else is `bad-value`, worded "`arxiv_id` can't be `{value}`; it must be an arXiv identifier such as `2503.01234` or `math/0601001`, without `vN`."; old-style identifiers are checked for a month 01–12, not for their years), `arxiv_version`, `posted` (date), `primary_class` (an arXiv category, A14: an archive of lowercase words joined by `-`, optionally followed by `.` and either two capitals or lowercase words joined by `-`, such as `math.AP`, `hep-th` or `cond-mat.mes-hall`; else `bad-value`, "…must be an arXiv category such as `math.AP` or `hep-th`"), `page_count`.
- `kind = "published"`: `id` is `published` (at most one), or `ed<N>` (N ≥ 2) for a book's later editions. Fields by the work's type:

| Type | Expected fields | Optional fields |
| --- | --- | --- |
| article | `journal`, `volume`, `number`, `pages`, `year`, `doi` | `journal_abbrev`, `month`, `publisher`, `issn` |
| book | `publisher`, `address`, `year`, `edition`, `series`, `series_volume`, `isbn`, `doi` | |
| incollection, inproceedings | `booktitle`, `publisher`, `address`, `pages`, `year`, `series`, `volume`, `doi` | |
| phdthesis, mastersthesis | `school`, `address`, `year` | |
| techreport | `institution`, `number`, `year` | |
| talk, lecture-notes, other | `howpublished`, `venue`, `date`, `url` | |

- Any version may also have `mr`, `zbl`, `url` and `page_count`.
- A field not in its version's lists is an error. An expected field that's missing is a warning (series and numbers often don't exist), named in plain words: *"SmithJones24's published version has no `volume`; articles usually have one."*
- Values: `year`, `arxiv_version`, `page_count` are integers; `posted` and `date` are dates; the rest are strings.

**`cite`** defaults, when Loom creates an entry, to `published` if there is a published version, else the newest arXiv version. Agents never change it; adding a newer version to an entry whose `cite` stays put is flagged by the job (M6).

**`[[reading]]`:** `version` (a version `id`), `level` (`skimmed`, `read` or `studied`), `what` (string), `date` (date), `session` (optional, `Sessions/<YYYY-MM-DD>`; a missing folder is a warning). The paper's reading level is the highest `level` over all readings.

**`[[renumbering]]`:** each entry has two or more version IDs as keys with the item's label in that version as value (`{ arxiv-v1 = "(2.18)", published = "(2.8)", checked = false }`), plus `checked` (a date or `false`).

**Stubs** (`Papers/<Key>/Statements/<tag>.toml`, plain TOML):

| Field | Required | Type and rule |
| --- | --- | --- |
| `id` | yes | A tag, equal to the file name |
| `kind` | yes | A statement kind |
| `title` | no | A short title; without one, Loom shows the gist |
| `gist` | yes | The result in our words (never the paper's text) |
| `stated_using` | no | Tags, as for statements |
| `paper_premises` | no | Tags of stubs **of the same paper** (the paper's own proof structure). Not used for status |
| `aliases` | no | As for statements |
| `extracted_from` | yes | A version `id` of this paper |
| `extracted_by` | yes | `dakota` or `agent:<model>` |
| `created` | yes | Date |
| `[locators]` | yes | One entry per version it has been found in: `<version-id> = { at = "Theorem 1.2, p. 4", checked_against_pdf = <date or false> }`. Must include `extracted_from`. Setting `checked_against_pdf` to a date is Dakota's click |

A stub is **cited** when its locator for `extracted_from` has `checked_against_pdf` set to a date, **extracted** otherwise. A stub has no proof files.

**The key rule.** Built from `[work]`'s authors (editors when there are none) and a year:
1. **Each surname** (`last`) becomes ASCII CamelCase: decompose it (Unicode NFKD) and drop the combining marks (`é` → `e`, `ő` → `o`); replace the letters that don't decompose (`ß` → `ss`, `æ` → `ae`, `Æ` → `Ae`, `ø` → `o`, `Ø` → `O`, `œ` → `oe`, `Œ` → `Oe`, `ł` → `l`, `Ł` → `L`, `đ`/`ð` → `d`, `Đ`/`Ð` → `D`, `þ` → `th`, `Þ` → `Th`, `ı` → `i`); split at spaces, hyphens and apostrophes; capitalise the first letter of each part and leave the rest as written; join; drop everything that isn't an ASCII letter. If nothing is left, the key can't be made: *"Can't make a key from the surname `…`: write it in Latin letters, as the paper prints it."*
2. **One to three authors:** every CamelCase surname, in order. **Four or more:** the first CamelCase surname, then the first two letters of each other's CamelCase surname (one letter if the surname has only one).
3. **The year:** the `year` of the `published` version if there is one; else the year of `posted` of the arXiv version with the lowest `arxiv_version`; else (talks, lecture notes, other) the year of `date`. Its last two digits are appended. With no year anywhere: *"Can't make a key for this paper: it has no year. Add a published year, an arXiv posting date, or a date."*
4. **Shared keys:** when a new paper's key equals an existing paper's key (or alias), both get letters. Keys are compared **ignoring case** here and everywhere else (A3), since a key is a folder name and Windows folder names ignore case: `MacDonald20` and `Macdonald20` are the same key. The first time, the letters go by posting order (the earliest of the arXiv `posted` dates, else the published year; then title): `SmithJones24a`, `SmithJones24b`, and the existing paper is renamed (below). After that, a lettered key never changes: a later paper with the same base takes the next unused letter, whatever its date. A lettered key that differs from the base and its letter only in case (the surname `MacDonald` corrected to `Macdonald`) keeps its letter; `rekey` only fixes the spelling (`MacDonald24a` → `Macdonald24a`), and `paper-key-rule` names that key.
5. **Renaming** (`loom rekey`, and when the year changes because a preprint was published in a later year): the folder is renamed, `key` changes, the old key is added to `aliases`, unless it differs from the new key only in case: then it is the same key (A3), so no old key is added and any old key equal to the new one is dropped (A17). Commands that take a key (`rekey`, `new-stub`) find the paper in any case, an exact match first (A17). Nothing else in the project names a paper by key except `\cite{…}` in bodies, which isn't rewritten (D9); old keys keep working through the alias, and validation warns that they could be updated.

| Authors (last names) | Year source | Key |
| --- | --- | --- |
| Smith, Jones | published 2024 | `SmithJones24` |
| Smith | arXiv v1 posted 2023-01-15, no published version | `Smith23` |
| Smith, Jones, Brown | published 2020 | `SmithJonesBrown20` |
| Smith, Jones, Brown, Garcia | published 2020 | `SmithJoBrGa20` |
| Smith, van der Berg, Brown, Garcia | published 2020 | `SmithVaBrGa20` |
| Erdős, Rényi | published 1960 | `ErdosRenyi60` |
| van der Berg, Müller | published 2019 | `VanDerBergMuller19` |
| O'Neil | published 2021 | `ONeil21` |
| de Giorgi | published 1957 | `DeGiorgi57` |
| Łojasiewicz | published 1963 | `Lojasiewicz63` |
| Weiß | published 2015 | `Weiss15` |
| McKean | published 1969 | `McKean69` |
| Smith-Jones | published 2022 | `SmithJones22` (the same as Smith and Jones: rule 4 letters them if both exist) |
| Smith, Jones (preprint 2023, published 2024) | | `SmithJones23` while a preprint; renamed `SmithJones24` with alias `SmithJones23` |
| Smith, Jones: two papers, posted 2023-05 and 2024-01, both published 2024 | | `SmithJones24a` (posted 2023-05), `SmithJones24b` |

**The bibliography** (`loom build-bib`) writes one BibTeX entry per paper, for the version in `cite`, in key order, UTF-8. Fields come out in the order shown, two-space indented, `field = {value},`, and only when they have a value. Authors and editors are `Last, First` joined by ` and `; a name whose `first` is empty is written as its surname in braces, `{Garcia Lopez}`, so BibTeX keeps a surname of several words whole (A5). In values, `&`, `%` and `#` that aren't already escaped and aren't inside `$…$` are escaped with `\`; a value with unbalanced braces is an error and its entry is skipped. `$$` is one math delimiter, not two. The entry is also skipped, as an error with code `bib-entry-skipped`, when a value ends in an odd run of backslashes (the last would escape the closing brace) or when the paper's `paper.toml` has errors of its own (A17): *"{Key}'s paper.toml has errors (`loom validate` lists them), so its BibTeX entry was left out of refs.bib."* Braces are counted as BibTeX counts them: every `{` and `}`, whether or not a `\` comes before it (A6). Every field written whose `verified` is `false` is listed in a warning (*"refs.bib uses unverified fields: SmithJones24 journal, volume"*); the file is still written. Output goes to `.loom/build/refs.bib` unless `--out` says otherwise.

Published article (`cite = "published"`), with arXiv versions:
```bibtex
@article{SmithJones24,
  author = {Smith, Alice and Jones, Bob},
  title = {Energy decay for a model parabolic system},
  journal = {Journal of Example Analysis},
  volume = {12},
  number = {3},
  pages = {101--130},
  year = {2024},
  doi = {10.0000/jea.2024.17},
  mrnumber = {MR0000001},
  eprint = {2401.99999},
  archiveprefix = {arXiv},
  primaryclass = {math.AP},
  note = {arXiv:2401.99999},
}
```
The `eprint` of a published version is the arXiv identifier **without** a version (taken from the newest arXiv version's `arxiv_id` and `primary_class`); `mrnumber` and `zbl` come from the cited version when present; `month`, `publisher` and `issn` follow `doi` when present.

arXiv-only paper (`cite = "arxiv-v2"`), whatever its type:
```bibtex
@misc{Garcia25,
  author = {García, Ana},
  title = {A counterexample to energy decay in two dimensions},
  year = {2025},
  eprint = {2503.00001v2},
  archiveprefix = {arXiv},
  primaryclass = {math.AP},
  note = {arXiv:2503.00001v2},
}
```
The identifier carries the cited version; `year` is that version's `posted` year.

Other types, cited version `published`:

| Type | Entry | Fields, in order |
| --- | --- | --- |
| book | `@book` | author (or editor), title, edition, series, volume (from `series_volume`), publisher, address, year, isbn, doi, mrnumber, zbl, url, then eprint fields and note if there's an arXiv version |
| incollection, inproceedings | `@incollection`, `@inproceedings` | author, title, booktitle, editor, series, volume, publisher, address, pages, year, doi, mrnumber, zbl, url, eprint fields, note |
| phdthesis, mastersthesis | `@phdthesis`, `@mastersthesis` | author, title, school, address, year, url |
| techreport | `@techreport` | author, title, institution, number, year, url |
| talk, lecture-notes, other | `@misc` | author, title, howpublished, note (`<venue>, <date as YYYY-MM-DD>`), year (from `date`), url |

### Validation
`loom validate` checks the whole project (or, with `--staged`, the state in git's index, read with `git ls-files --stage` and `git cat-file`) and prints each problem as `<file>:<line>: <severity>: <message>`, errors first, then warnings, each group in file order. `<line>` is the line of the offending value, else of its table header, else 1. Exit code 1 if there is any error, 0 otherwise (warnings don't block). Each problem has a stable `code` for `--json` and for the how-to page #9 writes. Messages below use `{…}` for what's filled in.

**Files and syntax**

| Code | Sev. | Message |
| --- | --- | --- |
| `not-utf8` | error | This file isn't valid UTF-8 (the first bad byte is at offset {n}). Save it as UTF-8. |
| `bom` | warning | This file starts with a byte-order mark. Loom will drop it the next time it saves the file. |
| `toml-syntax` | error | This isn't valid TOML: {parser message}. |
| `front-matter-missing` | error | The first line should be `+++`: Loom files start with front matter between two `+++` lines. |
| `front-matter-unclosed` | error | The front matter starting on line 1 is never closed by a `+++` line. |
| `body-empty` | error | {id} has no text below its front matter. |
| `unknown-field` | error | `{field}` isn't a field of {a statement / a proof / …}. Is it misspelt? The fields are: {list}. |
| `missing-field` | error | `{field}` is missing; every {thing} needs one. When present but empty (A2): `{field}` is empty; every {thing} needs one. |
| `wrong-type` | error | `{field}` should be {a date / a list of tags / …}, not {what it is}. |
| `bad-value` | error | `{field}` can't be `{value}`; it must be one of {allowed values}. |
| `bad-path` | error | `{path}` must be a path inside the project, written with `/`, without `..`. |
| `stray-file` | error | `{path}` doesn't belong in {folder}: it holds only {what it holds}. |
| `unknown-top-level` | warning | Loom doesn't know `{name}` at the top of the project. Move it into a folder Loom knows, or remove it. (Names later milestones use, such as `Threads/`, are skipped.) |
| `deleted-since-commit` | warning | {id} was in the last commit but its file is gone. Tags are permanent: supersede a statement, or mark a proof `failed`, instead of deleting it. |

**The project**

| Code | Sev. | Message |
| --- | --- | --- |
| `format-too-new` | (stops) | This project was made by a newer Loom (format {n}). Update Loom to open it. |
| `preamble-missing` | error | The preamble file `{path}` listed in loom.toml doesn't exist. |
| `tag-syntax` | error | `{tag}` isn't a valid tag name: use lowercase letters, digits and single hyphens, like `energy-estimates`. |
| `tag-duplicate` | error | The tag `{tag}` is declared twice in loom.toml. |
| `tag-unknown` | error | `{tag}` isn't one of the project's tags. Add it with `loom new-tag`, or fix the spelling. |

**IDs and references**

| Code | Sev. | Message |
| --- | --- | --- |
| `id-syntax` | error | `{id}` isn't a valid tag: four characters from 0–9 and A–Z without I, L, O and U. {Did you mean `{normalised}`?} |
| `id-mismatch` | error | The id is `{id}` but the file is named `{name}`; they must match. |
| `id-duplicate` | error | The tag `{id}` is used by both {path} and {path}. |
| `alias-invalid` | error | `{alias}` can't be an alias: {it contains whitespace or one of {}[],"  / it is shaped like a tag / it's over 100 characters}. |
| `alias-duplicate` | error | `{alias}` is an alias of both {id} and {id} (or is also a tag). In `paper.toml` (A17): an old key that is the paper's own key, or another of its old keys, in another case. |
| `ref-unknown` | error | `{ref}` in `{field}` isn't a statement or paper stub in this project. |
| `ref-alias` | error in front matter, warning in a body | `{ref}` is an old name of {id}. Write `{id}` here. |
| `ref-not-listed` | warning | The text refers to {id} (`\ref` or `\paperref`) but `{premises / stated_using}` doesn't list it. (A proof's refs to the statement it proves, and to statements without a status, such as definitions, don't count.) |
| `ref-dangling` | warning | `\ref{{label}}` points to nothing: it isn't a tag, an alias, or a `\label` in any body. For `\paperref` (A12): `\paperref{{id}}` points to nothing: it isn't the tag or old name of a paper stub. |
| `paperref-not-stub` | error | `\paperref{{ref}}` names {id}, a statement of this project, not a paper's result. Use `\ref{{id}}`. (A12) |
| `cite-alias` | warning | `\cite{{old}}` uses an old key of {Key}. It still works; `{Key}` is its current key. For a key or old key in another case (A17): `\cite{{key}}` spells the key of {Key} in another case. BibTeX matches keys exactly, so write `{Key}`. (For an old key: "spells an old key of {Key} in another case", then the same.) |

**Statements**

| Code | Sev. | Message |
| --- | --- | --- |
| `stated-using-self` | error | {id} can't be stated using itself. |
| `stated-using-cycle` | error | `stated_using` goes round in a circle: {A} → {B} → {A}. |
| `superseded-invalid` | error | {id} is superseded by {target}, which {doesn't exist / is itself / leads round in a circle: …}. For a stub (A12): {id} is superseded by {target}, which is a result from {Key}, not a statement of this project. |
| `refuted-incomplete` | error | {id} has `{refuted_by / refuted_on}` but not `{the other}`. Refuting a statement needs both: the proof and the date Dakota accepted it. |
| `refuted-unknown` | error | {id} is refuted by `{proof}`, which isn't a proof in this project. |
| `refuted-self` | error | {id} is refuted by `{proof}`, which is one of its own proofs. A statement is refuted by a proof of another statement, such as its negation. (A12) |
| `refuted-no-status` | error | {id} is a {kind}; only lemmas, propositions, theorems, corollaries and conjectures can be refuted. |
| `refuted-but-proved` | warning | {id} is marked false, but {proof} would prove it. One of them is wrong. |
| `conjecture-proved` | warning | Conjecture {id} is now proved. Change its kind to theorem (or lemma, …) when you're ready. |

**Proofs**

| Code | Sev. | Message |
| --- | --- | --- |
| `proof-folder-unknown` | error | `Proofs/{name}/` is for proofs of {name}, but there is no statement {name}. |
| `proof-id-mismatch` | error | The proof's id `{id}` doesn't match its file name `{name}`, or doesn't start with its folder's tag. |
| `proves-mismatch` | error | {proof} says it proves {x}, but it's filed under {y}. |
| `proof-of-no-status` | error | {id} is a {kind}; only lemmas, propositions, theorems, corollaries and conjectures have proofs. |
| `proof-of-stub` | error | {id} is a result from {Key}; it's cited from the paper, not proved here. |
| `premise-self` | error | {proof} uses {id}, the statement it proves, as a premise. |
| `premise-duplicate` | error | {id} is listed twice in the premises of {proof}. |
| `premise-no-status` | error | {id} is a {kind}, so it can't be a premise. Definitions belong in the statement's `stated_using`. |
| `premise-cycle` | error | Premises go round in a circle: {A} → {B} → {A}. A statement can't help prove itself. |
| `premise-false` | warning | {proof} uses {id}, which is false. |
| `premise-superseded` | warning | {proof} uses {id}, which is superseded by {target}. |
| `verbose-orphan` | error | `{file}` is the verbose version of {proof}, which doesn't exist. |
| `verbose-stale` | warning | The verbose version of {proof} was written from an older version of the proof. |
| `checks-orphan` | error | `{file}` holds checks of {proof}, which doesn't exist. |

**Checks**

| Code | Sev. | Message |
| --- | --- | --- |
| `check-by` | error | Check {n}: `by` must be `dakota` or `agent:<model>`, not `{value}`. |
| `check-verdict` | error | Check {n}: `{verdict}` isn't a verdict. Use pass, needs-changes or fail. |
| `check-changes-missing` | error | Check {n} is Dakota's needs-changes but doesn't say what to change. Add `changes`. |
| `check-changes-unexpected` | error | Check {n} has `changes`, but only a needs-changes check says what to change. |
| `check-fingerprint` | error | Check {n}: the fingerprint must be 12 characters from 0–9 and a–f. |
| `check-record-missing` | error | Check {n} is by an agent and has no `record`: every agent check links its reasoning. |
| `check-record-not-found` | warning | Check {n} links `{path}`, which doesn't exist. |
| `check-order` | error | Check {n} is dated before check {n−1}. Checks are kept in date order. |
| `check-duplicate` | error | Checks {n} and {m} are the same entry twice. |
| `check-changed` | error | A check committed earlier ({by}, {date}) was changed, moved or removed. Checks are never edited or reordered; add a new one instead. |

**Papers and stubs**

| Code | Sev. | Message |
| --- | --- | --- |
| `paper-key-mismatch` | error | The key is `{key}` but the folder is `{folder}`; they must match. |
| `paper-key-rule` | warning | By the key rule this paper's key is `{expected}`, not `{key}`. Run `loom rekey {key}`. When no key can be made (A14): the key rule's own message (steps 1 and 3). Not reported for a paper with no authors or editors, which `authors-missing` already reports. |
| `paper-key-duplicate` | error | `{key}` is the key or an old key of more than one paper. (Compared ignoring case, A3.) |
| `field-shape` | error | `{field}` must be `{ value = …, source = "…", verified = … }`. |
| `field-source` | error | `{field}` has no source. Every field says where it came from. For an `http://` source (A14): `{field}`'s source `{url}` must be an `https://` link or a path inside this paper's `local/` folder. |
| `field-verified` | error | `{field}`'s `verified` must be a date or `false`. |
| `field-not-for-type` | error | `{field}` isn't a field of {an article's published version / an arXiv version / …}. |
| `field-expected` | warning | {Key}'s {version} has no `{field}`; {type}s usually have one. |
| `version-id` | error | `{id}` isn't a valid version id for a {kind} version: use {arxiv-v<N> / published or ed<N>}. |
| `version-duplicate` | error | Two versions are called `{id}`. |
| `arxiv-version-mismatch` | error | Version `{id}` has `arxiv_version = {n}`; they must agree. |
| `cite-unknown` | error | `cite` names `{id}`, which isn't one of this paper's versions. |
| `version-unknown` | error | `{where}` names version `{id}`, which this paper doesn't have. |
| `session-missing` | warning | Reading of {date} links `{session}`, which doesn't exist. |
| `pdf-path` | error | `{path}` must be inside this paper's `local/` folder. |
| `authors-missing` | error | {Key} has no authors (and no editors). |
| `stub-locator-missing` | error | Stub {id} was extracted from `{version}` but has no locator for it. |
| `stub-paper-premise` | error | `{id}` in `paper_premises` isn't a result of the same paper. |
| `stub-gist-empty` | error | Stub {id} has no gist. |

### The `loom` command
`loom <subcommand> [arguments]`. Common options: `--project <folder>` (default: found from the current folder) and `--json` (one JSON object on stdout instead of text). Messages and problems go to stderr; results to stdout. With `--json`, a failure is `{"error": {"code": "…", "message": "…"}}` on stdout as well.

**Usage** (A11). The common options are taken only where they stand as options, never as another option's value (`--title --json` is a title); every valueless option (`--json`, `--version`, `--staged`, `--verified`, `--outdate-checks`) is known by name, so the word after it is never taken as its value. Giving `--project` twice, or giving it to `new-project`, is a usage error. A usage error (exit 2, code `usage`) prints the problem and then the usage on stderr; with `--json` the error's `message` is the problem alone. With no subcommand: *"`loom` needs a subcommand."*; with an unknown one: *"`{word}` isn't a loom subcommand. `loom help` lists them."*

**Exit codes** (every subcommand):

| Code | Meaning |
| --- | --- |
| 0 | Done (for `validate`: no errors) |
| 1 | Refused by a rule, or `validate` found errors. Nothing was written |
| 2 | Wrong usage: unknown subcommand, missing or bad argument |
| 3 | Can't work here: no project found, a newer project format, a file Loom can't read or write, `git` missing or failing |

**Git that doesn't finish.** Loom stops a git command that runs too long and exits 3 with code `git-timeout` (A16): *"`{git command}` didn't finish in time, so Loom stopped it. Git may be waiting for something, such as another git's lock or a slow network drive; try again."*

**Dakota's clicks.** Subcommands marked *(Dakota)* ask *"This is Dakota's decision: {what it does}. Type yes to go ahead:"* and go ahead only on `yes`. If input isn't a terminal they refuse (exit 1): *"Only Dakota can {do this}, and only at a terminal."* `LoomCore` itself takes an explicit confirmation argument; the terminal prompt is how `loom` supplies it.

| Subcommand | Arguments | Does | Prints (text; `--json`) |
| --- | --- | --- | --- |
| `--version` | | A word after it is a usage error: *"`loom --version` takes no arguments."* | `loom 0.1.0`; `{"version": "0.1.0"}` |
| `help` | `[subcommand]` | Usage, on stdout, exit 0 (A11). With a subcommand, only its usage lines and the common options; an unknown subcommand, or more than one word, is a usage error | Usage text; `{"usage": "…"}` |
| `new-project` | `<folder> --name <name> [--idea <text>]` | Creates a project (above) | The path; `{"path": …}` |
| `install-hooks` | | Pre-commit hook and merge driver (above) | What it did |
| `new-tag` | `<tag> --kind equation\|technique\|other --title <title>` | Adds a tag to `loom.toml` | The tag |
| `new-statement` | `--kind <kind> --title <title> (--body <text> \| --body-file <path>) [--tag <tag>]… [--stated-using <id>]… [--origin <origin>]` | Creates `Statements/<new tag>.tex`, `created` today | The new tag; `{"id", "path"}` |
| `edit-statement` | `<id> [--title …] [--kind …] [--body-file <path>] [--add-tag …] [--remove-tag …] [--add-stated-using …] [--remove-stated-using …] [--superseded-by <id>] [--verified] [--outdate-checks]` | Changes the statement. A change to the body that would outdate checks (see Statements) is refused unless `--outdate-checks` *(Dakota)*. `--verified` sets `verified` to today | The changed fields; with `--json`, also `outdated_checks` |
| `new-proof` | `<statement-id> --level <level> (--body <text> \| --body-file <path>) [--premise <id>]…` | Creates the next proof of the statement. `--level failed` is *(Dakota)* (A16) | The new proof ID; `{"id", "path"}` |
| `edit-proof` | `<proof-id> [--level …] [--body-file <path>] [--add-premise …] [--remove-premise …] [--verified] [--outdate-checks]` | Changes the proof. Changing the body, premises or level of a checked proof is refused unless `--outdate-checks` *(Dakota)* (A18: the level is in the fingerprint). `--level failed` is *(Dakota)* (A16), a click of its own: on a checked proof both are asked for | The changed fields |
| `fingerprint` | `<proof-id>` | | The current fingerprint; `{"proof", "fingerprint"}` |
| `add-check` | `<proof-id> --by <dakota\|agent:model> --verdict <pass\|needs-changes\|fail> [--changes <text> \| --changes-file <path>] [--fingerprint <fp>] [--record <path>] [--note <path>]` | Appends a check with the current fingerprint and time. Agents must give `--fingerprint` (refused if the proof has changed) and `--record`. `--by dakota` is *(Dakota)*; with `needs-changes` and no `--changes`, it asks for the message at the terminal | The entry and whether the proof is now checked |
| `refute` | `<id> --by <proof-id>` *(Dakota)* | Sets `refuted_by` and `refuted_on` (today). Refuses a proof of `<id>` itself (A12, code `refuted-self`): *{proof} is a proof of {id} itself, so it can't refute it. A statement is refuted by a proof of another statement, such as its negation.* | The new status |
| `status` | `[<id>]` | Computes status | One line per statement (`ID  status  title`) and the counts by status; with an ID, the explanation sentence. JSON below |
| `graph` | `--json` | | The graph (below) |
| `new-paper` | `<draft.toml>` | Reads a draft `paper.toml` without `key` (or with a `key` that is checked against the rule), computes the key, creates `Papers/<Key>/paper.toml` in canonical form (`cite` defaulted if absent, `created` today), renaming a paper that now shares its key. Never replaces a file: if `Papers/<Key>/` already exists (compared ignoring case), whatever is in it, it refuses and writes nothing: *"`Papers/{Key}/` already exists. Fix or remove what's there first; `loom validate` shows its problems."* (A3, code `paper-exists`; `{Key}` is the folder as it is on disk, which may differ in case). Setting `verified` and `checked` dates is Dakota's click, so a draft's are written as `false` (a top-level `verified` is left out), and `new-paper` says so on stderr (A16): *"Set to false, since verifying is Dakota's click: {fields}."*, and for a top-level `verified`, *"Left out `verified`, since verifying is Dakota's click."* | The key; `{"key", "path", "renamed": [{from, to}]}` |
| `new-stub` | `<key> --kind <kind> --gist <text> --version <version-id> --at <locator> --by <dakota\|agent:model> [--title …] [--stated-using <id>]… [--paper-premise <id>]…` | Creates a stub with a new tag, its locator `checked_against_pdf = false` | The new tag |
| `rekey` | `<key>` | Renames the paper to the key the rule gives (see the key rule) | `old → new`, or "already right" |
| `build-bib` | `[--out <path>]` | Writes the bibliography. Refuses (exit 1, code `bib-out-project-file`, A10) an `--out` that is one of the project's own files or in a folder Loom keeps them in (`loom.toml`, `CLAUDE.md`, `.gitignore`, `.gitattributes`, the preambles, `.loom/write.lock`, anything in `Statements/`, `Proofs/`, `Papers/` or `.git/`; compared ignoring case): *"`{path}` is one of the project's own files, or in a folder Loom keeps them in, so Loom won't write the bibliography there. Give `--out` a path of its own, such as `TeX/refs.bib`."* | The path and any warnings |
| `validate` | `[--staged]` | Checks everything (above) | The problems and a count line; JSON below |
| `merge-checks` | `<base> <ours> <theirs>` | The git merge driver (see Checks) | Nothing on success |

**JSON shapes** (keys always present; absent values are `null` or `[]`):

`loom status --json`:
```json
{"statements": [
  {"id": "6BWT", "kind": "lemma", "title": "…", "paper": null, "status": "conditional",
   "deciding_proof": "6BWT.a", "rests_on": ["3WQ6"], "superseded_by": null,
   "explanation": "6BWT conditional: 6BWT.a is complete and checked, but rests on 3WQ6, …"}
 ],
 "counts": {"proved": 8, "conditional": 6, "unchecked": 4, "sketch": 2, "heuristic": 1, "open": 2, "cited": 1, "extracted": 1, "false": 1, "none": 2}}
```
`counts` here are the worked example's. `loom status <id> --json` is one element of `statements`. `status` is `null` for kinds without one; `paper` is the key for stubs.

`loom graph --json`:
```json
{"format": 1,
 "nodes": [{"id": "4KX7", "kind": "theorem", "title": "…", "status": "proved", "paper": null,
            "superseded_by": null, "tags": ["model-system"]}],
 "proofs": [{"id": "4KX7.a", "proves": "4KX7", "premises": ["5HN8", "9D3F"], "level": "complete",
             "fingerprint": "3c8e1f0a92d4", "checked": true, "supports": true,
             "gives": "complete-checked", "counting_agent_check": 0, "counting_dakota_check": 1}],
 "stated_using": [{"from": "4KX7", "to": "2M9Q"}],
 "paper_premises": [{"from": "7RT2", "to": "3WQ6"}],
 "cycles": [["H8KD", "J3RW"]]}
```
`counting_*_check` is the index of that entry in the checks file, or `null`. `gives` is `nothing`, `complete-checked`, `complete-unchecked`, `sketch` or `heuristic`.

`loom validate --json`:
```json
{"errors": 1, "warnings": 1,
 "problems": [{"severity": "error", "code": "ref-unknown", "file": "Proofs/4KX7/4KX7.a.tex",
               "line": 4, "message": "…"}]}
```

**The other subcommands' JSON** (A15). Each prints one object on success; `index` and `counting_*_check` count checks from 0, as in `graph`. A `path` inside the project is project-relative with `/`, as in files, `build-bib`'s too (A23); `new-project`'s is the new folder's absolute path.

| Subcommand | JSON |
| --- | --- |
| `new-project` | `{"path"}` |
| `install-hooks` | `{"hook", "moved_hook", "merge_driver": "loom-checks"}`; `moved_hook` is the renamed old hook, or `null` |
| `new-tag` | `{"id"}` |
| `new-statement`, `new-proof`, `new-stub` | `{"id", "path"}` |
| `edit-statement`, `edit-proof` | `{"id", "changed": [field…], "outdated_checks": [proof-id…]}` |
| `fingerprint` | `{"proof", "fingerprint"}` |
| `add-check` | `{"proof", "path", "index", "check": {"by", "date", "fingerprint", "verdict", "changes", "record", "note"}, "checked", "supports", "counting_agent_check", "counting_dakota_check"}` |
| `refute` | `{"id", "path", "status", "explanation"}` |
| `new-paper` | `{"key", "path", "renamed": [{"from", "to"}…], "unclicked": [field…]}` |
| `rekey` | `{"from", "to", "renamed": [{"from", "to"}…]}`; `from` equals `to` when the key was already right |
| `build-bib` | `{"path", "problems": [{"severity", "code", "file", "line", "message"}…]}` |
| `merge-checks` | Nothing; it is run by git |

### Path safety
Every path `LoomCore` reads or writes is resolved against the project folder and must stay inside it: no absolute paths, no `..` that leaves it, and no symlink or junction that resolves outside it (checked on the canonical path). Paths stored in files are project-relative with `/`. A path that fails is refused (`bad-path`, or exit 3 for a command).

### Toolchain
| Piece | Choice |
| --- | --- |
| Language | C++20 |
| Windows compiler | MSVC from Visual Studio 2026 (Community is enough), x64. Flags `/W4 /permissive- /utf-8 /EHsc`; `/utf-8` is required, or MSVC reads the sources in the system code page and mangles non-ASCII string literals |
| Linux compiler (CI only) | GCC 13 (Ubuntu 24.04's), `-Wall -Wextra -Wpedantic` |
| Warnings as errors | In CI only |
| CMake | `cmake_minimum_required(VERSION 3.28)`; on Windows, 4.2 or newer, the first to know the `Visual Studio 18 2026` generator (Visual Studio 2026's own CMake qualifies) |
| Presets | `CMakePresets.json`: `windows` (generator `Visual Studio 18 2026`, multi-config, so `--config Debug` or `Release` at build time) and `linux` (Ninja, Debug); build folders `build/<preset>`, gitignored |
| Qt | 6.11.1 (6.12.0 is out, but is a .0 release; move up at 6.12.1), the same in CI and on Dakota's machine. `find_package(Qt6 6.11 REQUIRED COMPONENTS Core)` for `LoomCore` and `loom`; `Gui Widgets Test` for `LoomApp` and its tests; PDF and WebEngine when M2 needs them. Windows: the Qt Online Installer, its MSVC 64-bit component; CMake finds it through `CMAKE_PREFIX_PATH`. CI: `jurplel/install-qt-action@v4` (aqtinstall), Qt cached between runs |
| toml++ | `FetchContent`, pinned to one commit by URL (GitHub's archive of that commit) and SHA-256. A commit, not the `v3.4.0` tag, because that tag is from 2023 and fixes have landed since; the decision page records which commit and why |
| Catch2 | v3.16.1 by `FetchContent`, URL and SHA-256; `catch_discover_tests` registers each test with CTest |
| Qt Test | From Qt; UI tests run on Linux CI with `QT_QPA_PLATFORM=offscreen` |
| SHA-256 | `QCryptographicHash` (Qt Core) |
| Git, Git LFS | Run as programs (`QProcess`), never linked. Git for Windows includes Git LFS |
| Targets | `LoomCore` (static library; links `Qt6::Core` and toml++ only, and CMake fails the configure if anything adds Gui, Widgets or WebEngine to it), `loom` (executable, `LoomCore`), `LoomApp` (Qt Widgets executable), `LoomCoreTests` (Catch2), `LoomAppTests` (Qt Test) |

**CI** (`.github/workflows/checks.yml`): the existing `repo` job (indexes, tool tests), plus a `build` job with a matrix of `windows-2025-vs2026` and `ubuntu-24.04` (named exactly, not `-latest`, so the toolchain changes only when we change it). Each: check out, install Qt with the action, configure with the preset, build, `ctest --output-on-failure`. Tests that make git repos set a git identity first.

**Licences, against GPL v3:**

| Dependency | Licence | Used how | Fine with GPL v3? |
| --- | --- | --- | --- |
| Qt 6 (Core, Gui, Widgets, Test, later PDF, WebEngine) | LGPL v3 (and GPL v3) | Linked | Yes |
| toml++ | MIT | Compiled in | Yes |
| Catch2 | BSL-1.0 | Tests only, never shipped | Yes |
| install-qt-action, aqtinstall | MIT | CI only | Not linked or shipped |
| Git, Git LFS | GPL v2, MIT | Separate programs | Not linked |

Each linked or compiled-in dependency gets its decision page in `Wiki/decisions/` with the build issue that adds it (#3).

## Edge cases
- **A `+++` line inside the body** (a verbatim block): body text; only the first `+++` after line 1 closes the front matter.
- **A multi-line string in front matter** containing a `+++` line would close the front matter early. Loom's writer never writes multi-line strings in front matter; a hand-written one produces a TOML error.
- **CRLF files:** read as LF, fingerprint the same as LF, rewritten as LF.
- **An empty folder** (`Statements/` with nothing in it, or missing): the same as no statements.
- **A tag typed with `O` or `l`** on the command line: read as `0` or `1`; in a file: `id-syntax`, with the corrected tag suggested.
- **A proof whose premise is a stub of an uncited paper result:** the stub is extracted, so the proof is at best conditional, resting on the stub.
- **A refuted statement with a checked proof:** false, plus `refuted-but-proved`.
- **A premise that is false:** the proof can't prove anything (false isn't proved or cited); `premise-false`.
- **An equivalence (A ⇔ B, or a TFAE theorem):** a proof of A from B and a proof of B from A form a cycle, which is an error. Record the equivalence as one statement whose proof covers every direction, or prove one side without the other.
- **Two agents' checks with the same fingerprint at the same second:** the later in the file is newer.
- **A check list where one branch's entry predates the other's:** merge sorts by date, so the result may interleave; validation then checks the committed entries are still all there, which they are.
- **A check whose `record` doesn't exist yet** (before M4): a warning, not an error.
- **Editing a definition used in `stated_using` of a checked statement:** doesn't outdate any check: `stated_using` bodies aren't in fingerprints.
- **A retag at merge (M4)** changes the bodies that mention the tag, so checks of those proofs on the branch stop counting; the merge view lists them.
- **A paper whose surname has no Latin letters** (`last = "张"`): no key; the message asks for the surname as the paper prints it in Latin letters.
- **A book with editors and no authors:** the key and `author` field use the editors (`editor` in BibTeX).
- **Shared keys, third paper:** takes the next unused letter; existing lettered keys never change.
- **Keys that differ only in case** (`MacDonald20`, `Macdonald20`): the same key (A3), so they get letters, and two such folders are `paper-key-duplicate`.
- **`new-paper` onto a folder that exists** (an entry that doesn't parse, or whose `key` disagrees with its folder): refused, `paper-exists`; the old file is never replaced.
- **`core.hooksPath` set:** `install-hooks` and `new-project` refuse rather than write hooks outside the project (A1).
- **`\cite` of an old key:** works through the alias; `cite-alias` warns.
- **`loom new-project` without git identity:** stops before writing anything.
- **Two `loom` commands at once in one checkout:** the second waits up to 10 seconds for `.loom/write.lock`, then exits 3: *"Another loom command is changing this project. Try again when it has finished."*
- **A project from a newer Loom:** every command exits 3 without writing.
- **A file in `Statements/` that isn't `<tag>.tex`** (a stray `notes.txt`, an editor backup `4KX7.tex~`): `stray-file`.

## Verification
| Test | Kind | Checks |
| --- | --- | --- |
| New project layout | Catch2 | `new-project` writes exactly the files listed, with the listed contents; one commit; hook and merge driver installed; refuses a non-empty folder; stops cleanly without git identity |
| Front matter round trip | Catch2 | Parse then write leaves the body byte for byte, including a `+++` line in a verbatim block, Unicode, a missing final newline; CRLF in, LF out; BOM dropped |
| Canonical writer | Catch2 | Field order as in the tables; strings escaped; optional fields omitted |
| Field validation | Catch2 | Each `unknown-field`, `missing-field`, `wrong-type`, `bad-value` case per file type, with line numbers |
| IDs | Catch2 | Alphabet, length, no all-digit tags, no collision with tags or aliases (seeded generator), command-line normalisation, proof letters through `z` → `aa` |
| Fingerprint | Catch2 | Known byte string and hash for a fixed example; each input changes it; whitespace at line ends and CRLF don't; premise order doesn't |
| Verdict table | Catch2 | Every row of the verdict table |
| Check merge | Catch2 | Interleaving by date, duplicates once, ours-first ties, a deleted entry restored |
| Worked example | Catch2 | Every status and every expected validation problem in the worked example |
| Status explanations | Catch2 | The sentences for each status |
| Key rule | Catch2 | Every row of the key table, plus shared-key lettering and renaming with aliases |
| BibTeX | Catch2 | Exact output for the published article, the arXiv-only paper and one entry per other type; escaping; unverified-field warnings |
| Validation messages | Catch2 | A test project with one of every problem gives exactly the expected list, text and JSON |
| CLI | CTest scripts (`Tests/CMake/`) | Exit codes 0–3; `--json` shapes; *(Dakota)* commands refuse without a terminal |
| Path safety | Catch2 | `..`, absolute paths and symlinks leaving the project are refused |
| Build and tests on Windows | manual on Windows | The build how-to works on a clean machine; `ctest` passes |
| Commit guard on Windows | manual on Windows | Break a statement's front matter; `git commit` is refused with the message; fix it and the commit goes through |

## Open questions
- None. Dakota answered them on 2026-11-03, and the spec above follows his answers:
  - **Q1, what a fingerprint covers:** the recommendation. It covers the bodies of the proof, the proved statement and each premise statement, but not the bodies of `stated_using` statements.
  - **Q2, heuristic or sketch:** `sketch` ranks above `heuristic`.
  - **Q3, cycles of premises:** an error, as the design says.
  - **Q4, the departures:** D1–D9 approved; D10 is his own decision.

## See also
- Up: [Specs](README.md)
- [The design](loom.md): *The project folder*, *Checks*, *How status is computed*, *IDs*, *Papers*, *Technology*
- The M1 build issues: #3 (CMake and CI), #4 (`new-project`), #5 (statements, proofs, IDs), #6 (checks), #7 (status), #8 (papers), #9 (`validate`)
