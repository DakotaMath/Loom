---
type: spec
area: M2 statements and review
tags: [m2, app, ui, latex, build, statements, review, git, markdown]
---

# Spec: M2, statements and review

> **Summary:** Everything `LoomApp`, `LoomCore` and `loom` do in M2, the milestone that replaces TexViewer's daily use: the app shell (start screen, the side bar of tabs, Settings, *Check my setup*), how the app calls `LoomCore` and turns Dakota's clicks into its confirmations, building LaTeX (the wrapper document, three modes, verbose, `\paperref`, errors mapped to source lines, SyncTeX, `loom build`, builds of `HEAD`), the Statements tab (list, editor, IDs, status and checks), never losing text, the Review tab with rendered Markdown, and the git panel, with a TexViewer parity table, the dependencies, a demo project and how the UI is tested.

**Last verified:** 2026-11-05 · commit 8506746
**Status:** Approved by Dakota on 2026-11-05
**Milestone:** M2

## Scope
- **Covers:** the app shell; the app's threads and its use of `LoomCore`; building LaTeX in `LoomCore` and `loom build`; building a revision; the Statements tab; IDs in the editor; status, checks and Dakota's clicks; recovery of unsaved text and files changed outside Loom; the Review tab for uncommitted edits, with rendered Markdown; the git panel; dependencies, CI and the UI tests.
- **Does not cover:** job branches in Review, merging and retagging, the job queue, tasks (M4); the Graph tab (M5); editing papers and stubs, the Library, Reading (M6); threads and Export (M8); editing Markdown and the Wiki and Home tabs (M12); GitHub pull requests, issues and sign-in (later); merges, rebases and conflicts (a terminal, as with TexViewer).

## The build issues
Each build issue reads the sections named here; they don't need the rest.

| Issue | Sections |
| --- | --- |
| #33 building LaTeX in `LoomCore` (`loom build`) | *Building LaTeX*, *Building another revision*, *The demo project* (#33 writes it), *Dependencies and toolchain* (TeX in CI) |
| #34 the app shell | *The app and `LoomCore`*, *The app shell*, *Testing the UI* |
| #35 the Statements tab: the list and the editor | *The Statements tab* (all but *The PDF pane*), *Never losing text*, the QScintilla row of *Dependencies* |
| #36 compiling, the PDF and the Problems panel | *The PDF pane*, *The Problems panel and the Output view*, the Qt PDF row of *Dependencies* |
| #37 IDs with their titles and the picker | *IDs in the editor* |
| #38 status, checks and Dakota's clicks | *Status and checks in the tab*, *Dakota's clicks* |
| #39 git in `LoomCore` and the git panel | *The git panel* |
| #40 the Review tab | *The Review tab*, *Rules: front-matter changes in plain words* |
| #41 rendered Markdown in Review | *Rendered Markdown*, the WebEngine, KaTeX and md4c rows of *Dependencies* |

## Departures from the design and M1
Places where this spec makes `Specs/loom.md` or `Specs/m1-core.md` more precise in a way they didn't say, or changes them. Dakota approved all of them on 2026-11-05.

| # | Design or M1 says | This spec says | Why |
| --- | --- | --- | --- |
| E1 | `\label` from `id`; readers see ordinary numbering in exported threads | In builds of statements (M2), a statement's number **is its tag**: "Theorem 4KX7 (Energy decay …)", and `\ref{4KX7}` prints `4KX7`. Ordinary numbering stays for threads (M8) | A build shows one statement and part of its neighbourhood; numbers 1, 2, 3 would change with the mode and match nothing else in the app, while tags are what Dakota sees everywhere (Q2) |
| E2 | Hovering an ID shows "the compiled statement" | The compiled preview is built in the background and cached; until it's ready the hover shows the statement's source | Compiling takes a second or more; a hover must answer at once |
| E3 | Statement bodies are kept byte for byte (M1, D9: `\cite{Old}` isn't rewritten) | Bodies still aren't rewritten; the **build's copies** of them have old and wrong-case keys in `\cite` replaced by the current key | BibTeX matches keys exactly and `refs.bib` has entries only under current keys; the M1 review's follow-up asked M2 to decide |
| E4 | The Tabs table lists thirteen tabs | M2 shows only its own tabs (Statements, Review) and Settings; each later tab appears with its milestone (Q4) | A tab that does nothing is noise |
| E5 | Design: "All the v1 git panel features stay" | Discarding uncommitted changes isn't in M2 (Q5) | It is the one git operation that loses text, which the principles forbid without a very deliberate click |
| E6 | M1: a change that would add a validation error is refused | The app's saves follow the same rule; the text stays in the editor and in its recovery file (Q1) | One rule for every writer; the commit guard would refuse the file anyway |

## Decisions this spec makes
| Decision | Choice | Why |
| --- | --- | --- |
| Who the app acts as | Dakota. Every check, refutation and `failed` the app writes is `by = "dakota"`, each after a confirmation dialog | M1's decision: Loom is Dakota's tool. Agents never run the app |
| Where builds run | In `LoomCore` (Qt Core's `QProcess`), shared by the app and `loom build` | So agents, tests and the app get the same build |
| Which LaTeX the build reads | Saved files only; the editor saves before it builds (Ctrl+S saves and compiles) | One source of truth; a build never differs from what's on disk |
| How bodies get into the build | Line-preserving copies under the build folder, front matter replaced by `%` lines | Errors and SyncTeX positions map back to source lines with no arithmetic, and paths in the build never contain the project path (which may have spaces) |
| Where the PDF is read from | Loaded into memory, never held open | On Windows an open PDF can't be overwritten, and the next build would fail |
| SyncTeX | The `synctex` program TeX Live installs, run as a process; no SyncTeX library | No new dependency; the page of the open statement comes from the `.aux` file when `synctex` is missing |
| Settings and app state | Per user, outside the project: `QSettings` INI files under `%APPDATA%\Loom\` and `%LOCALAPPDATA%\Loom\` | Settings are Dakota's, not the project's; the project folder holds only what Loom's formats define |
| Recovery files | `LoomCore` writes them under the project's `.loom/drafts/` (gitignored) | They are about project files, and `LoomCore` is the only writer in a project |
| One window, one project | A project opens in one Loom window at a time (`.loom/app.lock`); several projects may be open in several windows | Two windows editing one file would race each other's unsaved text |

## Behaviour

### The app and `LoomCore`
The app links `LoomCore` and calls it in-process; it never runs `loom`. Everything that reads or writes the project goes through `LoomCore`, exactly as `loom`'s subcommands do, so the app is bound by every rule M1 defines.

**Threads.** The UI thread never waits on a file, git or TeX. Work runs in three lanes, each a worker thread with a queue:

| Lane | Runs | At once |
| --- | --- | --- |
| **Project** | Reading the project, every write (saves, new statements and proofs, checks, refutations, recovery files), status, validation, search | One task at a time, in order |
| **Build** | LaTeX builds (`LoomCore`'s build, which runs `latexmk`) | Two: the Statements tab's and Review's, or Review's before and after |
| **Git** | Every git command of the git panel and Review | One at a time |

- **Snapshots.** After every change it makes or sees, the project lane re-reads what changed, recomputes every status (milliseconds at M2's sizes, as M1 says) and publishes an immutable **snapshot**: the project's contents, graph, statuses and validation problems. Every tab draws from the latest snapshot, so status updates everywhere at once after any save or click.
- **Locks.** Writes take `.loom/write.lock` as `loom` does, so the app and an agent's `loom` never write at once. Git operations that change the working tree (switch, pull) take it too, so a save waits for them and they wait for a save. If the lock isn't free within M1's 10 seconds, the save fails with M1's message and the text stays in the editor.
- **Busy controls.** While a task runs, the control that started it shows a spinner and doesn't start a second one; everything else stays usable.

**Watching the folder.** The app watches the project's folders (`QFileSystemWatcher` on `Statements/`, `Proofs/*`, `Papers/*`, `TeX/`, the top folder) and re-checks every open file's state when its window is activated, since watchers miss changes on some drives. Changes are gathered for 300 ms, then the project lane re-reads and publishes a snapshot. What happens to an open file that changed is in *Never losing text*.

#### Dakota's clicks
`loom` asks for a typed `yes`; the app asks with a dialog and passes `LoomCore` the same explicit confirmation. Every such dialog names what will be written and what it changes, has buttons named for the action (never "OK"), and starts with **Cancel** focused, so Enter alone never confirms. Cancelling writes nothing.

| Click | The dialog says | `LoomCore` gets |
| --- | --- | --- |
| A check (`pass`, `needs-changes`, `fail`) | "Record your pass of 4KX7.a as it is now? With the agent's pass of 2026-11-04 this makes 4KX7.a checked: 4KX7 becomes proved; P3QA becomes proved." For `needs-changes`, the text box for `changes` (required) | `addCheck` with `by = "dakota"`, the fingerprint of the version shown (refused if the file changed since), and `dakotaConfirmed` |
| A save that outdates checks | "Saving changes text that the checks of 4KX7.a and P3QA.a read. Their checks stop counting: 4KX7 proved → unchecked, P3QA proved → conditional." **Save and outdate checks** | The edit with `outdateChecks` set to exactly the proofs listed (M1, A16); if the set changed meanwhile, `LoomCore` refuses and the app asks again with the new set |
| Marking a proof `failed` | "Mark 4KX7.b failed? It stops supporting 4KX7; …" (and, on a checked proof, the outdated checks too, in the same dialog) | `failedConfirmed`, and `outdateChecks` when needed |
| Refuting a statement | "Mark F2NS false, refuted by G5TZ.a? Today's date is written as the date you accepted it." | `refuteStatement` with `dakotaConfirmed` |

The status changes in each dialog come from `LoomCore` (below), never from the app's own reasoning.

#### What `LoomCore` gains in M2
The build issues add these, each Catch2-tested, none with UI code:

| Function | Used by | Does |
| --- | --- | --- |
| The build (*Building LaTeX*) | #33, `loom build`, the app | Writes the build folder, runs `latexmk`, returns the PDF path and the mapped problems |
| Reading a revision (*Building another revision*) | #33, #40 | A project's contents as they are at a commit, without touching the working tree |
| Status changes | #38, #40, the dialogs | The statements whose status differs between two contents (before → after), raises and drops marked. Every `ChangeResult` (done, or refused for `outdates-checks`) carries the status changes the edit makes or would make |
| Front-matter changes in plain words | #40 | The lines of *Rules: front-matter changes in plain words* for one file between two contents |
| Saving editor text | #35 | `saveStatementText` and `saveProofText` (below): the editor's whole file, checked against the file it was loaded from |
| Verbose proofs | #35 | Writing `<proof-id>.verbose.tex` (create, change the body, mark up to date) |
| Recovery files | #35 | Writing, listing and removing `.loom/drafts/` (*Never losing text*) |
| ID search | #37 | Ranking statements and stubs for a query (*IDs in the editor*) |
| Git operations | #39, #40 | The git panel's operations and the changed-file list (*The git panel*) |
| *Check my setup* | #34 | Finds and runs each tool and reports its version or what's missing |

### The app shell

#### Starting and opening
- **At start** the app reopens the last project on the tab last used (setting *Reopen the last project*, on by default). Otherwise, or if that project can't be opened, it shows the **start screen**.
- **The start screen** has **New project**, **Open project…** and **Recent projects** (the last 10, newest first, each its name and path; one that no longer exists is greyed, with **Remove from list**).
- **New project** is a form: the parent folder, the folder name, the project's name, and the research idea (several lines, optional). **Create** runs `LoomCore`'s `newProject`, which does exactly what `loom new-project` does, with M1's checks and messages (git, Git LFS, git identity, `core.hooksPath`) shown in the form; then the project opens on the Statements tab. Seed papers and first tasks come with M4 and M6.
- **Open project…** picks a folder. One project per window: opening another project in a window that has one asks first if any file has unsaved edits (**Save all**, **Keep them for next time**, which leaves the recovery files, or **Cancel**). **File → New window** opens another window.

**Opening a folder:**

| The folder | What happens |
| --- | --- |
| Has `loom.toml` that reads | Opens. Any validation problems go to the Problems panel, and if there are errors a bar says *"This project has {n} errors; commits are refused until they're fixed."* with **Show problems**. Files whose front matter doesn't parse open in raw mode |
| Is inside a project (a parent has `loom.toml`) | *"This folder is inside the project {name} at {path}."* **Open that project** |
| Is empty | *"This folder is empty."* **Create a project here** (the New project form, filled in) |
| Has no `loom.toml` and isn't empty | *"This folder isn't a Loom project: it has no loom.toml. Loom creates projects only in new or empty folders; bringing in an existing research repo comes with the importer (M3)."* |
| Has a `loom.toml` that doesn't parse | *"Loom can't read loom.toml: {parser message} (line {n}). Fix it in a text editor, then open the project again."* **Show in Explorer**. Not opened |
| Was made by a newer Loom | M1's message: *"This project was made by a newer Loom (format {n}). Update Loom to open it."* Not opened |
| Is open in another Loom window | *"This project is open in another Loom window."* Not opened. A window that crashed leaves a stale lock, which `QLockFile` detects and takes over |
| Isn't in a git repository, or git isn't found | Opens; Review and the git panel say *"This project isn't in a git repository"* or *"Loom can't find git; Check my setup says what to install."* |

#### The window
- **Title:** `{project name} · {branch} · Loom`.
- **The side bar:** a narrow vertical bar on the far left, one line icon per tab. M2 shows **Statements** and **Review** at the top and **Settings** (a gear) at the bottom (E4). Hovering an icon shows its name, its shortcut and one line: *"Statements: edit statements and proofs, with the compiled PDF beside them"*, *"Review: your uncommitted changes, as compiled math"*. The open tab is highlighted. Each tab keeps its state (open statement, scroll positions, selection) when you switch away and back.
- **Badges** (M2): Review shows the number of entries with uncommitted changes (*The Review tab*), hidden at 0; Statements shows a dot while any statement or proof has unsaved edits.
- **The status bar:** the branch (click: the git panel), the build state (*"Built 14:02 in 1.8 s"*, *"2 errors"*, *"Building…"*), the problem counts (click: the Problems panel), and a spinner while any lane is busy.
- **The bottom panel**, shared by every tab: **Problems** and **Output** (*The Problems panel and the Output view*). Hidden until there is something to show, or toggled with Ctrl+\`.
- **The git panel** opens on the right from every tab (*The git panel*).
- **Help → About Loom** shows the version and every dependency with its licence (the table in *Dependencies and toolchain*).

**Shortcuts:**

| Keys | Does |
| --- | --- |
| Ctrl+1, Ctrl+2, … | The tabs, in side-bar order (M2: Statements, Review). Positional, as the design says, so a number moves when a later milestone adds a tab before it |
| Ctrl+, | Settings |
| Ctrl+O | Open project |
| Ctrl+P | Go to a statement (the ID picker, *IDs in the editor*) |
| Ctrl+N / Ctrl+Shift+N | New statement / New proof of the open statement |
| Ctrl+S | Save the open file and compile |
| Ctrl+B | Compile (the saved files) |
| Ctrl+J | Show the cursor's place in the PDF |
| Ctrl+F, Ctrl+H, F3, Shift+F3 | Find, replace, next, previous |
| Ctrl+Shift+F | Find in the project |
| Ctrl+Space | The ID picker, inside `\ref{…}`, `\paperref{…}` or an ID field |
| Alt+← / Alt+→ (and the mouse's back and forward buttons) | Back and forward |
| F8 / Shift+F8 | Next / previous problem |
| Ctrl+Shift+G | The git panel |
| Ctrl+\` | The bottom panel |

#### Settings
Stored per user with `QSettings` in INI format, `%APPDATA%\Loom\Loom.ini`; never in the project. Settings is a page of the window (the gear), with these:

| Setting | Default | Notes |
| --- | --- | --- |
| Theme | Follow Windows | Light, dark, or follow Windows (Qt's colour scheme) |
| Editor font and size | Cascadia Mono, 11 pt (Consolas if it's missing) | Ctrl+= and Ctrl+- change the size in the editor too |
| Interface font size | Windows' | |
| Compile on save | On | Ctrl+S saves and compiles; off, it only saves |
| Compile timeout | 120 seconds | 10–1800 |
| Default compile mode | Alone | The mode a statement opens with the first time |
| Show titles beside IDs | On | *IDs in the editor* |
| Reopen the last project | On | |
| `latexmk`, `synctex` and `git` | Found on `PATH` | A full path overrides `PATH`, for unusual installs |

**Check my setup** (a button in Settings, and on the start screen) checks, in order, and shows one line each: a tick with the version and path, or a cross with what to do.

| Tool | Checked by | Missing says |
| --- | --- | --- |
| git | `git --version`, 2.40 or newer | *"Install Git for Windows."* |
| git identity | `git config user.name`, `user.email` | M1's message |
| Git LFS | `git lfs version` | M1's message |
| `loom` on `PATH` | Running `loom --version` from `PATH` | *"The commit guard runs `loom` from PATH. Add Loom's folder to PATH."* |
| `latexmk` | `latexmk -v` (which also proves Perl runs) | *"Install TeX Live (it includes latexmk and Perl). With MiKTeX, also install Perl."* |
| pdfLaTeX | `pdflatex --version` | *"Install TeX Live."* |
| `synctex` | `synctex help` | *"Without synctex, clicking in the PDF can't find the source; everything else works."* (a warning, not a cross) |
| A test build | Builds a two-line document in a temporary folder, within the compile timeout | The first lines of the log |

`LoomCore` does the checking (it runs only processes); the app shows it.

#### What the app remembers
Per user, never in the project:
- `%APPDATA%\Loom\Loom.ini`: the settings, the recent projects, the last project and tab, window geometry.
- `%LOCALAPPDATA%\Loom\projects\<12 hex characters of the SHA-256 of the project's canonical path>.ini`: per project, the open statement and page, the back and forward history (up to 100 places), the cursor and scroll position of the last 100 files opened, the compile mode and verbose toggle, splitter sizes, PDF zoom, Review's selection, and the git panel's unfinished commit message.

### Building LaTeX
A **build** makes a PDF of one **target** (a statement, or one proof of it) in one **mode**, optionally **verbose**, from the working tree or from a commit. It is `LoomCore`'s, run by the app and by `loom build`.

#### The build folder
`.loom/build/<target>/<mode>[-verbose]/` for the working tree (for example `.loom/build/4KX7/chain-verbose/`, `.loom/build/4KX7.a/alone/`), and `.loom/rev/<commit>/build/…` for a revision. In it:

```
main.tex            the wrapper document (below)
src/                line-preserving copies of every file the build reads, at their project paths:
  TeX/preamble.tex, TeX/macros.tex
  Statements/4KX7.tex, Proofs/4KX7/4KX7.a.tex, …
refs.bib            the bibliography, when the build cites anything
loom-build.toml     what was built: target, mode, verbose, the inputs' hash, the time, the result
main.pdf, main.log, main.aux, main.synctex.gz, …   latexmk's output
last-good.pdf       a copy of the last main.pdf from a build without errors
```

- **Copies.** Each copy is its source file with every front-matter line, the two `+++` lines included, replaced by a line holding only `%`, so line *n* of the copy is line *n* of the source. Bodies are otherwise copied byte for byte, except in `\cite` keys (E3, below). Preamble files are copied whole.
- **Paths in the build** are relative to the build folder (`src/Statements/4KX7.tex`), and `latexmk` runs with the build folder as its working folder, so a project path with spaces or non-ASCII letters never reaches TeX.
- **Skipping work.** The inputs' hash is SHA-256 over the copies, `refs.bib`, `main.tex` and the build options. If it equals the one in `loom-build.toml` and that build ended without errors, the build returns its result without running `latexmk`.
- **Cleaning.** Working-tree build folders of statements that no longer exist are removed when the project opens. `.loom/` is gitignored and safe to delete.

#### The wrapper document
`main.tex` for `loom build 4KX7 --mode premises` (4KX7 is a theorem stated using the definition 2M9Q, with one complete proof `4KX7.a` from premises 5HN8 and 9D3F, and 9D3F's text cites the paper result 7RT2):

```latex
% Written by Loom for 4KX7, mode premises. Don't edit: Loom rewrites it on every build.
\documentclass[11pt]{article}
\input{src/TeX/preamble.tex}
\input{src/TeX/macros.tex}
\usepackage{loom}                % Loom's environments and commands (below)
\loompaperref{7RT2}{SmithJones24}{Theorem 1.2}
\loomoutside{7RT2}{SmithJones24, Theorem 1.2}
\loomoutside{6BWT}{A conditional decay estimate}   % … one line for every ID not in the build
\begin{document}
\loomheader{4KX7 · Energy decay for the model system}{with its direct premises}

\loomsection{Stated using}
\begin{loomstatement}{definition}{2M9Q}{Weak solution of the model system}
\input{src/Statements/2M9Q.tex}
\end{loomstatement}

\loomsection{Premises}
\begin{loomstatement}{lemma}{5HN8}{A uniform bound for the energy}
\input{src/Statements/5HN8.tex}
\end{loomstatement}
\begin{loomstatement}{lemma}{9D3F}{Decay of the tail}
\input{src/Statements/9D3F.tex}
\end{loomstatement}

\loomsection{The statement}
\begin{loomstatement}{theorem}{4KX7}{Energy decay for the model system}
\input{src/Statements/4KX7.tex}
\end{loomstatement}
\begin{loomproof}{4KX7.a}{complete}{5HN8, 9D3F}
\input{src/Proofs/4KX7/4KX7.a.tex}
\end{loomproof}

\bibliographystyle{amsplain}
\bibliography{refs}
\end{document}
```

- **`loom.sty`** is part of Loom (compiled in as a Qt resource, written into the build folder as `loom.sty` on every build), loaded with `\usepackage{loom}` after the project's preambles so it can see what they loaded. It needs only `amsthm`, which it loads if the preambles didn't. Its names all start with `loom`, so a preamble's own `theorem` environment doesn't clash.
- **`loomstatement`**`{kind}{tag}{title}` prints the kind's name, the tag as its number and the title in parentheses (E1): **Theorem 4KX7** (Energy decay for the model system). The body is in amsthm's `plain` style for lemma, proposition, theorem, corollary and conjecture, `definition` style for definition, question and example, and `remark` style for remark. It sets the current label to the tag, then writes `\label{<tag>}` and `\label{<alias>}` for each alias, so `\ref` to a tag or an old name prints the tag. A paper stub uses the same environment with the paper's citation in the heading: **Theorem 7RT2** (SmithJones24, Theorem 1.2), its gist as the body.
- **`loomproof`**`{proof-id}{level}{premises}` is a `proof` environment headed *Proof 4KX7.a (complete; from 5HN8, 9D3F).* A `sketch` or `heuristic` proof says so in the heading; a `failed` one is headed *Failed attempt 4KX7.b* and set in grey. It writes `\label{<proof-id>}`, so `\ref{4KX7.a}` prints `4KX7.a`.
- **`\loomheader`** prints one line at the top: the target with its title and the mode (*alone*, *with its direct premises*, *with the full chain*, and *verbose* when on).
- **`\loomsection`** prints a small heading between groups: *Stated using*, *Premises*, *The chain*, *The statement*.
- The document class is `article` at 11 pt; the preambles may load anything that works with it.

#### What each mode contains
Within each group, statements are in **dependency order** (a statement after everything in the build it is stated using or proved from), ties by tag; members of a cycle of premises are in tag order, and the build warns (`build-cycle`).

| Mode | Contains |
| --- | --- |
| **alone** | The target. For a statement: the statement and **all** its proofs, in letter order, failed attempts last. For a proof: its statement and that proof only |
| **premises** (*+ direct premises*) | As *alone*, after a *Stated using* group (the target statement's `stated_using`) and a *Premises* group (every premise of the proofs shown), as statements without their proofs. Paper stubs as stubs |
| **chain** (*+ full chain*) | As *alone*, after *The chain*: everything reachable from the proofs shown through premises and `stated_using`, transitively. Each statement in the chain comes with its **deciding proof** (M1, *Tie-breaks*: the proof its status rests on), and the chain follows only those proofs; a statement with no deciding proof (open, or no status) comes without one. Paper stubs end the chain: their `paper_premises` aren't followed |

**Verbose** replaces each proof shown by its verbose version, where it has one; a verbose version that is stale (M1: its `fingerprint` isn't the proof's) is headed *Verbose 4KX7.a: written from an older version of the proof* and the build warns `verbose-stale`. Proofs without a verbose version are shown as they are.

**References outside the build.** For every tag, alias and proof ID in the project that isn't in the build, `loom.sty`'s `\loomoutside{<id>}{<title>}` defines the label so `\ref{<id>}` prints the ID in sans serif with a dotted underline, never `??`, and LaTeX gives no undefined-reference warning. A `\ref` to something that is in neither the build nor the project prints `??` and is the warning `tex-ref-undefined`.

#### `\paperref` and `\cite`
- **`\paperref{<stub>}`** is defined by `loom.sty` from a table the wrapper writes: `\loompaperref{<stub tag>}{<Key>}{<locator>}` for each stub cited in the build, using the locator of the paper's `cite` version. It prints `\cite[<locator>]{<Key>}`: `\paperref{7RT2}` → [1, Theorem 1.2]. Old names of the stub work too.
  - The `cite` version has no locator for the stub: `\cite{<Key>}` without one, and the warning `paperref-no-locator`.
  - The locator isn't checked against the PDF (`checked_against_pdf = false`): printed, and the warning `paperref-unchecked`.
  - The ID isn't a stub (a statement, or nothing): bold **[7RT9?]**, and the warning `paperref-unknown` (`loom validate` already reports `paperref-not-stub` or `ref-dangling`).
- **`\cite`** (and `\cite[…]`, `\nocite`): in the copies, every key that is an old key of a paper (its `aliases`), or a key or old key in another case, is replaced by the paper's current key (E3). The source isn't touched.
- **`refs.bib`** is written into the build folder by M1's bibliography function when the build has a `\cite`, `\nocite` or `\paperref`; otherwise the wrapper has no `\bibliography`. Its warnings (unverified fields, `bib-entry-skipped`) become the build's problems. The style is `amsplain` (Q3).

#### Running `latexmk`
From the build folder:

```
latexmk -norc -pdf -interaction=nonstopmode -file-line-error -synctex=1 main.tex
```

with the environment variables `max_print_line=100000`, `error_line=254` and `half_error_line=238`, so TeX never wraps log lines (wrapped lines break `file:line` parsing). `-norc` ignores the user's own `latexmkrc`, so the app, `loom build` and the tests build the same way. The engine is pdfLaTeX (Q3).

- **Timeout:** the setting in the app (default 120 s), `--timeout` for `loom build` (same default). When it runs out, Loom stops `latexmk` **and everything it started** (on Windows, through a job object, since killing `latexmk` alone leaves `pdflatex` running and holding the files), and reports `tex-timeout`.
- **A newer build of the same folder** stops the running one the same way first.
- **`latexmk` missing:** `tex-missing`, nothing else is tried.

**The result:** succeeded (no errors; a PDF), failed (errors; `last-good.pdf` is kept from before), or couldn't run (`tex-missing`, `tex-timeout`, a file Loom can't write). An unchanged build (the inputs' hash) returns the previous result.

#### Problems from a build
The log is read for errors (`<file>:<line>: <message>` lines, from `-file-line-error`, with the lines after them up to the context line `l.<n>`) and warnings (`LaTeX Warning: … on input line <n>.`, `Package <name> Warning: …`, where the file is the one open at that point of the log, tracked through the log's `(file` … `)` nesting). Each problem's file is mapped back: `src/<path>` is `<path>` in the project, at the same line; a line of `main.tex` is the generated part around the statement it belongs to (the wrapper records which). Overfull and underfull boxes aren't reported.

| Code | Severity | Message |
| --- | --- | --- |
| `tex-error` | error | LaTeX's message, cleaned of `!` and the context lines: *"Undefined control sequence `\fracc`."* At the source `file:line` |
| `tex-error-generated` | error | *"LaTeX stopped in the part Loom writes around {id}: {message}. This is usually a problem in the preamble or a macro used in the title."* At the statement's file, line 1 |
| `tex-ref-undefined` | warning | *"`\ref{{label}}` points to nothing in this build or the project."* |
| `tex-cite-undefined` | warning | *"`\cite{{key}}` isn't a paper of this project, or its entry was left out of refs.bib."* |
| `tex-warning` | warning | Any other LaTeX or package warning, as LaTeX wrote it |
| `paperref-no-locator` | warning | *"{Key}'s cited version `{version}` has no locator for {stub}, so the citation has no theorem number."* |
| `paperref-unchecked` | warning | *"The locator \"{at}\" of {stub} in {Key}'s `{version}` isn't checked against its PDF yet."* |
| `paperref-unknown` | warning | *"`\paperref{{id}}` doesn't name a paper's result."* |
| `verbose-stale` | warning | M1's message |
| `build-cycle` | warning | *"Premises go round in a circle ({A} → {B} → {A}); the build lists them in tag order."* |
| `build-unreadable` | error | *"{file} can't be built: Loom can't read its front matter (`loom validate` says why)."* The build stops |
| `tex-timeout` | error | *"The build didn't finish in {n} seconds, so Loom stopped it. Look in Output for where it got stuck, or raise the compile timeout in Settings."* |
| `tex-missing` | (stops) | *"Loom couldn't run `latexmk`. Install TeX Live, then use Check my setup."* |
| `tex-no-pdf` | error | *"latexmk finished without an error Loom could read, but made no PDF. The full log is in Output."* |

Plus the bibliography's problems (M1: `bib-entry-skipped`, the unverified-fields warning). Problems are listed errors first, each group in file and line order, de-duplicated (LaTeX repeats warnings across runs).

#### SyncTeX and the open statement's page
- **The open statement's page** comes from `main.aux` (`\newlabel{<tag>}{{…}{<page>}…}`): always available.
- **From the editor to the PDF** (forward): `synctex view -i <line>:<column>:src/<path> -o main.pdf`, run in the build folder, gives the page and the box to show.
- **From the PDF to the source** (inverse): `synctex edit -o <page>:<x>:<y>:main.pdf` gives `src/<path>` and a line, which map back as problems do.
- Without `synctex`, forward search falls back to the statement's page, and inverse search is unavailable (Check my setup says so).

#### `loom build`
`loom build <id> [--mode alone|premises|chain] [--verbose] [--rev <commit>] [--timeout <seconds>] [--json]`

- `<id>` is a statement's tag (or alias) or a proof ID, read as M1's command line reads IDs. A paper stub can't be a target (exit 1, `build-stub`: *"{id} is a result from {Key}; it's built only as part of a statement's chain."*).
- `--mode` defaults to `alone`. `--rev` builds the target as it is at that commit (*Building another revision*); `HEAD` and any commit git can resolve are accepted.
- Prints the PDF's path (project-relative with `/`, M1's A23) and the problems in `validate`'s form, `<file>:<line>: <severity>: <message>`.
- **Exit codes:** 0 built (warnings allowed); 1 LaTeX errors (the problems say where), or a refusal (`build-stub`, `ref-unknown`); 2 usage; 3 can't work (`tex-missing`, `tex-timeout`, no project, git failing for `--rev`, a file Loom can't write).
- **JSON:** `{"target", "mode", "verbose", "rev", "pdf", "built": true|false, "reused": true|false, "seconds", "problems": [{"severity", "code", "file", "line", "message"}…]}`; `pdf` is `null` when no PDF was made, `rev` is the full commit hash or `null`.
- It reads and writes only `.loom/`, and takes no write lock: two builds of different folders run at once; two of the same folder are serialised by a lock in that folder.

### Building another revision
Review shows a statement as it was at `HEAD` beside how it is now. `LoomCore` reads a commit without touching the working tree or the index:

1. Resolve the commit to its full hash (`git rev-parse --verify <rev>^{commit}`).
2. If `.loom/rev/<hash>/tree/` doesn't exist, write the files Loom reads in full there (`loom.toml`, the preambles named in it, `Statements/`, `Proofs/`, `Papers/*/paper.toml`, `Papers/*/Statements/`), read from git with `git ls-tree -r -z <hash>` and `git cat-file --batch`, as `validate --staged` reads the index (M1). It is written to a temporary folder and renamed when complete, so a half-written tree is never used.
3. Read that folder as a project, and build in `.loom/rev/<hash>/build/…` as above.

- **Caching.** A commit never changes, so its tree and builds are reused as they are. When the project opens, Loom keeps the trees of the 5 most recently used commits and removes the others.
- **A target that doesn't exist at the commit** (a new statement): no build; Review shows *"New: not in the last commit."*
- **No commits yet:** everything is new.
- `loom build --rev` uses the same code.

### The Statements tab
Three columns, with movable splitters: the **list** on the left, the **editor** in the middle, the **PDF pane** on the right.

#### The list
- **Rows:** every statement of the project, each as a status mark, `ID · title`, and the kind in small letters (`thm`, `lem`, `prop`, `cor`, `conj`, `def`, `q`, `rem`, `ex`). A statement's proofs are child rows (`4KX7.a · complete`), with the failed ones greyed. A row with unsaved edits has a dot; a file with errors a red mark, with warnings a yellow one.
- **Status marks** are a coloured disc plus the status word on hover; colour is never the only signal (the editor's header spells the status out).

| Status | Colour |
| --- | --- |
| proved | green |
| conditional | teal |
| unchecked | amber |
| sketch | orange |
| heuristic | light purple |
| open | grey, hollow disc |
| cited | blue |
| extracted | light blue, hollow disc |
| false | red, struck through |
| no status | no disc |

  Each colour has a light-theme and a dark-theme value with at least 3:1 contrast against the list's background. The Graph tab (M5) uses the same table.
- **Search** (the box at the top) filters as you type, by the same search as the ID picker: words of the title, ID, aliases, tags and body text.
- **Filters** (a menu by the search box): kind, status, tag, and *Show paper results* (off by default; paper stubs then appear grouped under their paper, read-only, since editing them is M6).
- **Sort:** by ID, title, status, or recently changed (file time).
- Clicking a row opens it in the editor; leaving a file with unsaved edits keeps the edits (*Never losing text*).

#### The editor
One statement is open at a time, as **pages**: the **Statement**, then one page per proof (**a**, **b**, …), then **+ Proof**. Each page has a header and a text editor (QScintilla with LaTeX highlighting, line numbers, matching braces, and a margin mark on lines with problems).

**The statement page header** (the strip above the text):
- `4KX7 · Theorem · Energy decay for the model system`, the status mark and word, and the explanation sentence (*Status and checks in the tab*).
- The front matter as a **form**: kind (a list), title, tags (chips, with + from the project's tags), stated using (chips `2M9Q · Weak solution …`, each clickable, + opens the ID picker), superseded by (an ID field with the picker), and read-only lines for aliases, origin, created, verified (with **Mark verified**, which sets `verified` to today: M1's `--verified`), and refuted by and on. Refuting is a click (*Dakota's clicks*), not a field.
- **Raw**: a toggle that shows the whole file, front matter included, in the text editor instead of the form and body. Saving raw text goes through the same rules as the form. Raw mode is how a file whose front matter doesn't parse is opened and fixed.

**A proof page header:** `4KX7.a · proof of 4KX7`, the level (a list: sketch, heuristic, complete, failed), premises (chips with each premise's status mark, + opens the picker, × removes), **Mark verified**, the verbose toggle (**Proof** | **Verbose**), and the check history (*Status and checks in the tab*).

Form changes and text changes are one set of unsaved edits for the file; Ctrl+S saves them together.

#### Saving
`saveStatementText(path, text, base, confirmations)` and `saveProofText(…)` take the editor's whole file text (the form composes canonical front matter through `LoomCore`'s writer; raw mode passes the text as typed) and `base`, the SHA-256 of the file's bytes when the editor loaded it. They:
1. Refuse with `file-changed` if the file on disk isn't `base` any more (*Never losing text* says what the app does then).
2. Parse the text and compare it with the file, field by field, and apply it under the same rules as `editStatement` and `editProof`: an `id` or `proves` that changes is refused (`id-changed`: *"A statement's tag is permanent; Loom can't change 4KX7 to 4KX8."*); setting `refuted_by`, `refuted_on`, `failed`, or a change that outdates checks needs the same confirmations; a change that adds a validation error is refused, naming the errors (E6, Q1).
3. Write the file atomically (M1), front matter in canonical form, the body as typed.

**When a save is refused**, nothing is written, the text stays in the editor (and its recovery file), and the problems are listed in the Problems panel with their lines; for `outdates-checks` and `failed`, the app shows the confirmation dialog instead and saves again if Dakota confirms.

**When a save succeeds**, the editor's `base` becomes the new file, its recovery file is removed, the snapshot updates, and (with *Compile on save*) the build starts.

#### Creating statements and proofs
- **New statement** (Ctrl+N, or + at the top of the list): a form with kind, title, tags, stated using (picker), and the body (an editor with the picker). **Create** calls `createStatement`; the new statement opens. A body is required (M1). The form's text is kept in a recovery file while it's open.
- **New proof** (Ctrl+Shift+N, or the **+ Proof** page): level (not `failed`: a dead end is recorded by marking a proof failed later), premises (picker), body. **Create** calls `createProof`; its page opens.

#### Verbose versions
The **Verbose** toggle on a proof page edits `<proof-id>.verbose.tex` in the same editor:
- If there is none, the page offers **Write a verbose version**, which opens an empty editor; the first save creates the file with `proof`, the proof's current `fingerprint` and `created` today.
- A stale verbose version shows a bar: *"Written from an older version of this proof."* **Mark up to date** sets its `fingerprint` to the proof's current one (a click: it says the verbose text now matches the proof).
- Saving its text doesn't change its `fingerprint`; only **Mark up to date** or creating it does.
- The compile modes' **Verbose** toggle (*The PDF pane*) is separate: it chooses what the PDF shows.

#### Back and forward
Every time a statement or page is opened (from the list, a chip, Ctrl+click, the picker, a problem), the place left (file, page, cursor, scroll) goes on the back history. Alt+← and Alt+→ (and the mouse buttons) move through it; up to 100 places, kept per project between runs.

#### Find and replace
- **Ctrl+F** finds in the open file (match case, whole word, regular expression); **Ctrl+H** adds replace and replace all, in the open file only.
- **Ctrl+Shift+F** finds in every statement and proof (bodies only) and lists the hits as `ID · title` with the line; a click opens it. There is no project-wide replace: it would change many files at once and could outdate many checks in one click.

#### The PDF pane
- **Above the PDF:** the mode switch (**Alone** | **+ Premises** | **+ Chain**), the **Verbose** toggle, **Compile** (Ctrl+B), zoom, and the build state.
- **What it shows:** the build of the open target (the statement on the statement page, the proof on a proof page) in the chosen mode, remembered per statement. Ctrl+S saves and compiles; a file changed on disk that's in the shown build compiles it again (once, after 300 ms of quiet).
- **Where it scrolls:** a new document (another target, mode or verbose) opens at the open statement's page; a rebuild of the same document keeps its scroll position; Ctrl+J shows the cursor's place (forward SyncTeX) and marks it briefly. Double-clicking in the PDF opens the source at that line (inverse SyncTeX), in any statement or proof of the build.
- **While an error is unfixed** the pane shows `last-good.pdf`, dimmed, under a bar: *"Showing the last good build ({time}). {n} errors: fix them to update it."* With no good build yet: the errors only.
- The PDF is read into memory (`QPdfDocument` loaded from a buffer), so the file is never held open.
- **Find in the PDF** (Ctrl+F while the PDF has focus) uses Qt PDF's search.

#### The Problems panel and the Output view
- **Problems** lists, in one table: the open project's validation problems (from the latest snapshot), the latest build's problems for the open statement, and a refused save's or commit's problems. Columns: severity, file (as `ID · title` for statements and proofs), line, message. Clicking a row opens the file at the line (in raw mode for a front-matter line); F8 moves to the next.
- **Output** keeps, for the session, one channel each: **Build** (each build's command, time and full `main.log`), **Git** (each git command, *The git panel*) and **Loom** (each `LoomCore` change: what was written, refused and why). **Copy** and **Clear** per channel.

### IDs in the editor
**The picker** opens when you type `\ref{`, `\paperref{` or `\cite{` (for paper keys), on Ctrl+Space inside one, on + in a premises, stated-using or superseded-by field, and as **Go to statement** (Ctrl+P). It is a popup at the cursor (in the form: under the field) with a search box and a list.
- **What it searches** (`LoomCore`'s ID search, over the latest snapshot): tags, aliases, proof IDs (for `\ref` and Go to), title words, tags, and body text. The query is split into words; a result must match every word (as a prefix of a word, ignoring case and accents). Ranking: an exact tag or alias first, then matches in the title, then tags, then body text; ties by recently opened, then tag.
- **What it offers:** `\ref` and Go to: statements, proofs and stubs; `\paperref`: stubs only; premises: provable statements and stubs, not the statement itself; stated using: statements and stubs; superseded by: statements; `\cite`: paper keys (with the first author and title).
- **What it shows:** each result as the status mark, `ID · title` (a stub: `7RT2 · SmithJones24 Theorem 1.2 · {title or gist}`), and the kind; beside the list, the selected result's preview (below).
- **What it inserts:** always the tag (or proof ID, or key), never an alias; Enter inserts and closes the brace. Typing an ID that names nothing can't be inserted.

**IDs shown with titles.** With *Show titles beside IDs* on, every line of the editor that mentions IDs (in `\ref`, `\paperref`, `\cite`) gets a grey annotation under it listing `4KX7 · Energy decay for the model system` for each (a QScintilla annotation). An ID that names nothing is underlined in red.

**Hovering** an ID (in the text, a chip or a list row) shows a popup: the status mark, `ID · title`, the kind, the status sentence, and the compiled preview.

**Previews.** A preview is the statement built alone (without proofs) at a small page size, cut to its content and drawn from the PDF as an image. Previews are built in the background on the build lane at low priority, cached by the hash of the statement's copy and the preambles (`.loom/build/preview/<tag>-<hash>.pdf`), and rebuilt when either changes. Until one is ready, the popup shows the body's source (E2). A stub's preview is its gist and locators, as text.

**Ctrl+click** on an ID opens it: a statement on its statement page, a proof on its page, a stub in a read-only view (gist, locators, paper). Ctrl+click on an ID that names nothing does nothing but flash the red underline.

### Status and checks in the tab
- **The status line** under the statement's title is the status mark, the status word and `LoomCore`'s explanation sentence, the same as `loom status <id>` (M1): *"4KX7 conditional: 4KX7.a is complete and checked, but rests on 3WQ6, which is extracted (not yet checked against the paper's PDF)."* IDs in it are links.
- **Each proof page's check history** is a table, oldest first, with every entry of `<proof-id>.checks.toml`: date and time, who (*Dakota*, or *agent claude-opus-5-5*), the verdict, and **this version** or **an older version** (whether the fingerprint matches the proof's current one). The two counting checks (M1, rule 5) are marked **counts**. A `changes` text shows in full under its entry. `record` and `note` are links: a Markdown file opens read-only in the Markdown view (*Rendered Markdown*); a missing one says it's missing.
- **What the proof gives** is one line above the table, from the snapshot: *"Checked: the agent's pass of 2026-11-04 and your pass of 2026-11-04."*, *"Waiting for your pass."*, *"Doesn't support 4KX7: the agent says it's wrong (fail, 2026-11-20)."*

**Dakota's buttons** on a proof page: **Pass**, **Needs changes…**, **Fail**, each through its confirmation dialog (*Dakota's clicks*). They are disabled, with the reason on hover, while the proof's page has unsaved edits (*"Save first: a check records the version on disk."*) and while the snapshot is older than the file on disk. A refusal because the proof changed under the dialog (`proof-changed`) says so and offers to show the new version.

- **Fail** doesn't mark the proof `failed` (M1). After a `fail` is recorded, the page offers **Mark this proof failed…**, a separate click.
- **Marking a proof failed:** choosing *failed* in the level list asks first (*Dakota's clicks*), then saves.
- **Refute…** (on the statement page's menu) opens the picker for proofs of other statements, then the dialog. Undoing a refutation isn't in M1 or M2; it is an edit in raw mode, saved under the usual rules.
- After any click, the snapshot updates, and the list, header, Review and badges show the new status.

### Never losing text
**Unsaved edits** live in the editor's memory per file, across leaving the statement, switching tabs and opening other statements. Nothing saves them but Ctrl+S (or **Save all** when asked). Closing the window with unsaved edits asks: **Save all**, **Keep them for next time** (the recovery files stay), or **Cancel**.

**Recovery files** guard against crashes. One second after typing stops, and when the editor loses focus, `LoomCore` writes the file's unsaved text under `.loom/drafts/`, mirroring the project path:

```
.loom/drafts/Statements/4KX7.tex          the editor's text, byte for byte
.loom/drafts/Statements/4KX7.tex.toml     what it is
```

```toml
path = "Statements/4KX7.tex"
base = "9f2c…"                       # SHA-256 of the file when the editor loaded it; "" for a new file
written = 2026-11-05T14:02:11-06:00
```

The text is written first and the `.toml` second, each atomically; a text without its `.toml` is ignored. They are removed after a successful save or when Dakota discards the edits. The New statement and New proof forms keep theirs as `.loom/drafts/new/<form>.txt` and `.toml`. The git panel's commit message is kept in the per-project app state.

**On opening a project**, every recovery file is offered back:

| The file on disk | What the app does |
| --- | --- |
| Is still `base` | Opens the text as unsaved edits, with a bar: *"Recovered unsaved edits from {time}: Loom closed without saving them."* **Keep** / **Discard** |
| Changed since `base` | A bar: *"Recovered unsaved edits from {time}, but the file has changed since."* **Compare** (a diff of the recovered text against the file), **Keep mine** (the text stays as unsaved edits, `base` becomes the file now, and saving goes through the usual checks), **Discard mine** |
| Is gone | A bar: *"Recovered unsaved edits from {time}, but the file has been deleted."* **Restore it with my text** (written through the usual save rules), **Discard** |

**A file changed outside Loom** (by an agent, git or another editor) while open:
- **No unsaved edits:** it reloads silently, keeping the cursor and scroll, and the Output's Loom channel notes it.
- **Unsaved edits:** a bar: *"4KX7.tex changed on disk while you were editing it."* **Compare**, **Keep mine** (as above), **Take the file's** (the unsaved text is discarded, after a confirmation). Until one is chosen, saving is refused with `file-changed` and says why.
- **Deleted:** a bar: *"4KX7.tex was deleted on disk."* The text stays; **Restore it** (with the unsaved or loaded text, through the usual save rules) or **Close**.

**A save that would fail validation** is refused (E6); the text stays as unsaved edits and in its recovery file, and the problems are listed. Nothing is lost, and nothing invalid reaches the disk through Loom.

### The Review tab
Review shows every uncommitted change in the project against `HEAD`, as math. **Review never changes a file or git**: every button in it opens something or builds into `.loom/`.

**The list** (left), from `git status --porcelain=v2 -z --untracked-files=all` (ignored files excluded), with each entry marked **new**, **changed** or **deleted**, grouped:

| Group | Entries |
| --- | --- |
| **Status changes** | One line per statement whose status differs between `HEAD` and now (*Rules*), raises ▲ and drops ▼; at the top, always shown when not empty |
| **Statements and proofs** | One entry per statement with any change to its statement file, its proofs, their checks or verbose versions: `4KX7 · title`, with what changed in a few words (*"text, 4KX7.a premises, check"*) |
| **Papers** | One entry per paper with any change to `paper.toml` or its stubs |
| **Markdown** | Every other changed `.md` file |
| **Project files** | `loom.toml`, `TeX/`, `CLAUDE.md`, `.gitignore`, `.gitattributes` |
| **Other files** | Anything else |

The list updates when files change (from the snapshot and a new `git status`, after 300 ms of quiet). The Review badge counts the entries of every group but Status changes.

**A statement entry** (right) shows, from top to bottom:
1. **Changes in plain words**, one line each, for the statement file and each proof, check and verbose file (*Rules: front-matter changes in plain words*).
2. **Before and after**, side by side: the statement built at `HEAD` (left) and now (right), in the mode chosen above them (**Alone** | **+ Premises** | **+ Chain**, and **Verbose**), each with its own PDF view. A side that doesn't compile shows its errors in place of the PDF while the other still shows. A new statement has no left side (*"New: not in the last commit."*); a deleted one no right side. The two views scroll together (toggle **Link scrolling**, on by default).
3. **Raw diff**: a toggle that replaces the PDFs with `git diff HEAD -- <files>` for the entry's files, coloured.
4. **Open in Statements**: opens the statement (or the selected proof) in the Statements tab.

**A paper entry** shows the plain-word changes of `paper.toml` and its stubs (fields as `journal: "…" → "…"`, `verified` set or cleared, locators added, `checked_against_pdf` set) and the raw diff. **Markdown** entries show the rendered Markdown (below) with **Raw diff** a click away. **Project files** and **Other files** show the raw diff only; a binary file says so and shows its size before and after.

### Rendered Markdown
A changed Markdown file is shown rendered, with its changes marked, in one view component of `LoomApp` (`MarkdownView`), which is the app's only use of Qt WebEngine. It is also used read-only for records and notes opened from the check history.

- **Rendering.** md4c turns Markdown (CommonMark, with tables, strikethrough and task lists) into HTML, with `$…$` and `$$…$$` kept as math (md4c's `MD_FLAG_LATEXMATHSPANS`, so `_` and `*` inside math are never emphasis); KaTeX typesets the math. The project's `\newcommand`s without optional arguments from its preamble files are passed to KaTeX as macros; math that KaTeX can't typeset shows its source in red. TOML front matter (`+++`) is shown as a small two-column table at the top.
- **Changes.** The old and new files are split into top-level blocks (paragraphs, headings, list items, table rows, code blocks), which are compared in order (longest common subsequence). An added block has a green bar in the margin; a removed one is shown, struck through, with a red bar, where it was; a changed block is shown as the removed old one followed by the added new one, with the changed words in the new one highlighted. **Next change** and **Previous change** jump between them. A new file is all added; a deleted one all removed.
- **Links.** Links to project files open them in Loom (a statement in Statements; another Markdown file in the view); `http` and `https` links open in the default browser. The page loads nothing from the internet: KaTeX, its fonts and the page's script and styles are compiled into `LoomApp` as Qt resources, and every request for a URL outside them is blocked.
- **Raw diff** is a click away, as everywhere in Review.

### The git panel
A panel on the right, opened from every tab (the branch in the status bar, or Ctrl+Shift+G). It runs git only on a click, through `LoomCore`'s git operations (the `git` program, as M1 runs it: `GIT_TERMINAL_PROMPT=0` so git never waits for a terminal, M1's timeout and `git-timeout`). Merges, rebases and conflicts are left to a terminal.

**At the top:** the branch, and, when it has an upstream, how many commits it is ahead and behind (from the last fetch). A detached `HEAD` says *"Not on a branch ({short hash})"*. While a merge, rebase, cherry-pick or revert is in progress, the panel says *"A {merge} is in progress. Finish it in a terminal."* and offers only **Run checks** and **Fetch**.

**Changed files:** every changed file (as Review lists them, but as paths), each with a tick box, all ticked by default; a click shows its diff (`git diff HEAD -- <path>`, or the file for a new one).

| Operation | Does | Refuses |
| --- | --- | --- |
| **Commit** | Needs a message (a box; a first line over 72 characters is a warning). Runs `git add -- <ticked new files>`, then `git commit -m <message> -- <ticked paths>`, so only the ticked files go in, through the pre-commit hook | Nothing ticked; an empty message; not on a branch (*"Make a branch first: a commit here would be left on no branch."*); a ticked file with unsaved edits asks first: **Save and commit**, **Commit the saved version**, **Cancel** |
| **Run checks** | `LoomCore`'s validation (as `loom validate`), on the project lane; its problems in the Problems panel | |
| **New branch…** | A name; `git switch -c <name>`. Uncommitted changes come along, as git does | A name `git check-ref-format --branch` rejects, or that exists |
| **Switch branch…** | A list of local branches; `git switch <name>` | Uncommitted changes to tracked files (*"Commit your changes first, or they would come along to {branch}."*); unsaved edits in the app (*"Save or discard your unsaved edits first."*) |
| **Fetch** | `git fetch --prune` | No remote (the button is hidden) |
| **Pull** | `git pull --ff-only` | Unsaved edits in the app; no upstream; when git can't fast-forward: *"{branch} and its remote have both moved on, so Loom can't fast-forward. Merge them in a terminal."* |
| **Push…** | First lists the commits that will go up (`git log --oneline @{upstream}..HEAD`, or, with no upstream, `git log --oneline HEAD --not --remotes`), then on **Push {n} commits**, `git push` (or `git push -u <remote> <branch>` with no upstream, choosing the remote when there are several). Never forces | No remote (the button is hidden); nothing to push |

- **A refused commit.** When the pre-commit hook refuses, git's output is shown in the panel; each line in `loom validate`'s form (`<file>:<line>: <severity>: <message>`) also goes to the Problems panel, clickable. The commit message is kept.
- **Credentials.** Git Credential Manager may open its own sign-in window; Loom never asks for or stores a password.
- **Every git command** the app runs (from the panel, Review or *Check my setup*) is logged in Output's Git channel: the time, the folder, the command line, the exit code, how long it took, and its output (cut at 1 MB).
- After an operation that changes the working tree (switch, pull), the project lane re-reads the project and the open editors follow *Never losing text*.

### TexViewer parity
| TexViewer's daily feature | In Loom |
| --- | --- |
| The result editor with a header strip (ID, kind, status, what it uses, clickable) | M2: the editor's page headers (*The editor*); uses are the stated-using and premise chips |
| Back and forward between results | M2: Alt+←/→ (*Back and forward*) |
| Ctrl+click on `\ref` to open it | M2: Ctrl+click on any ID (*IDs in the editor*) |
| Find and replace | M2: in the file, and find (not replace) in the project (*Find and replace*) |
| Ctrl+S saves and compiles | M2: the same (*Saving*, *The PDF pane*) |
| Three compile modes (this result, + direct uses, + full chain) with the PDF scrolled to the open result | M2: alone, + premises, + chain, plus verbose (*What each mode contains*, *The PDF pane*) |
| Problems panel that jumps to the line; the last good PDF shown dimmed while an error is unfixed | M2 (*The Problems panel*, *The PDF pane*) |
| The full LaTeX log and every git command in an Output view | M2: Output's Build and Git channels |
| Reloading files changed elsewhere, asking if there are unsaved edits | M2, plus crash recovery (*Never losing text*) |
| New result from a form | M2: New statement and New proof |
| Settings: theme, font size, compile timeout, *Check my setup* | M2 (*Settings*) |
| Review: before at `HEAD` and after, compiled side by side | M2 (*The Review tab*) |
| Review: header changes in plain words | M2 (*Rules*), plus the status changes they cause |
| Review: Markdown rendered with changes marked | M2 (*Rendered Markdown*, #41; Q6) |
| Review: raw diff; *Open in editor* | M2 |
| Git panel: new branch, switch branch, tick files and commit, hook refusals shown, *Run checks*, push listing the commits first, fetch, pull fast-forward only | M2 (*The git panel*) |
| Git panel: discarding changes (Dakota never used it) | Dropped (E5, Q5) |
| Pull requests, GitHub sign-in | Later (GitHub milestone) |
| Reading sessions | M6 |

### Dependencies and toolchain
| Dependency | Version | Licence | Used how | Fine with GPL v3? | Decision page |
| --- | --- | --- | --- | --- | --- |
| QScintilla | 2.14.1 | GPL v3 (or Riverbank's commercial licence) | Compiled in: the source archive by `FetchContent` (URL and SHA-256), built as a static library by Loom's own `Source/CMake/QScintilla.cmake`, since QScintilla ships only qmake files | Yes: GPL v3 is Loom's licence | #35 |
| Qt PDF, Qt PDF Widgets | Qt 6.11.1 | LGPL v3 / GPL v3 (it contains PDFium, BSD-3-Clause) | Linked (`Qt6::Pdf`, `Qt6::PdfWidgets`) in `LoomApp` | Yes | #36 |
| Qt WebEngine Widgets | Qt 6.11.1 | LGPL v3 / GPL v3 (Chromium's licences are compatible) | Linked in `LoomApp`, used only by `MarkdownView` | Yes | #41 |
| md4c | its latest release, pinned by URL and SHA-256 | MIT | Compiled in by `FetchContent` (it has CMake) | Yes | #41 |
| KaTeX | 0.16.x, pinned | MIT | Its release archive by `FetchContent`; the script, CSS and fonts compiled into `LoomApp` as Qt resources | Yes | #41 |
| TeX Live (`latexmk`, pdfLaTeX, `synctex`, BibTeX) | 2025 or newer | LPPL, GPL and others | Separate programs, never linked; installed by Dakota | Not linked or shipped | how-to page (#34) |

`LoomCore` gains no dependency: the build, git and setup checks run programs through Qt Core. The configure check that keeps Gui, Widgets and WebEngine out of `LoomCore` (M1) is extended to Pdf and PdfWidgets.

**CI** (`.github/workflows/checks.yml`):
- Qt from `install-qt-action` with the modules `qtpdf qtwebengine qtwebchannel qtpositioning` on both runners.
- **Linux:** `apt-get install latexmk texlive-latex-base texlive-latex-recommended texlive-fonts-recommended`, so the TeX tests run; and the system libraries QScintilla and WebEngine need.
- **Windows:** no TeX (a TeX Live install is too large for each run), so the TeX tests skip there; Dakota's machine runs them.
- **TeX tests** are Catch2 and Qt Tests that need `latexmk`. They carry the CTest label `tex`; when `latexmk` isn't found they skip (Catch2's `SKIP`, Qt's `QSKIP`) with *"latexmk not found"*. The Linux job sets `LOOM_REQUIRE_TEX=1`, under which a missing `latexmk` fails the test instead, so CI can't skip them silently.
- **WebEngine tests** carry the label `webengine` and skip when WebEngine can't start (*"Qt WebEngine couldn't start here"*); the Markdown HTML (md4c, the block diff) is tested without WebEngine.

### The demo project
`Tests/Projects/Demo/`, written by #33 and used by every M2 test that needs a project: a small project of made-up mathematics that compiles, with one of most statuses. Its checks' fingerprints are computed with `loom fingerprint` when the files are written.

| ID | Kind | Title | Proofs | Status |
| --- | --- | --- | --- | --- |
| A2DF | definition | Tame sequence | | none |
| B3GH | definition | The tail of a sequence | | none |
| C4JK | lemma | Tails of tame sequences vanish | a: complete, [], pass, pass | proved |
| D5MN | lemma | Tame sequences are bounded | a: complete, [C4JK], pass, pass | proved |
| E6PQ | theorem | Products with bounded sequences stay tame | a: complete, [D5MN, 7RT2], pass, pass; a verbose version; uses `\paperref{7RT2}` | proved |
| F7RS | proposition | Rearrangements of tame sequences are tame | a: complete, [3WQ6], pass, pass | conditional |
| G8TV | lemma | Tame sums commute with limits | a: complete, [C4JK], agent pass only | unchecked |
| H9WX | conjecture | A sharp bound for the tail | a: sketch, [E6PQ] | sketch |
| J2YZ | question | Which tail bounds are sharp? | | none |
| K3AB | lemma | A tail bound for sums | none | open |
| 7RT2, 3WQ6 | stubs of the made-up paper `SmithJones24` | | | cited, extracted |

With records for its agent checks (`Records/…`), `TeX/macros.tex` defining `\R` and `\N`, and every body using only `amsmath` and `amssymb`.

**Edits for tests** are overlays in `Tests/Projects/Demo-edits/<name>/`, copied over a committed copy of the demo:
- `review`: a body edit to C4JK (outdating the checks of C4JK.a and D5MN.a), a premise added to G8TV.a, a new statement, a Dakota pass added to G8TV.a's history, and a changed Markdown note.
- `latex-error`: an unclosed brace in K3AB's body at a known line.

**`Tests/Support/DemoProject`** (C++, used by Catch2 and Qt Tests) copies the demo to a temporary folder, sets a git identity, runs `git init -b main`, `loom install-hooks` and one commit, and optionally applies an overlay.

### Testing the UI
- **Qt Test** drives the real widgets: clicks and keys (`QTest::mouseClick`, `QTest::keyClicks`, shortcuts), on the demo project, with `QT_QPA_PLATFORM=offscreen` on Linux CI and the normal platform on Windows. Every confirmation dialog is tested both confirmed and cancelled; every refusal is tested to leave the text in the editor.
- **Screenshots:** UI tests save the main screens (`QWidget::grab()`) to `build/<preset>/screenshots/<test>.png`; CI uploads them as an artifact per runner, and each UI build PR names the ones worth looking at. They are never compared pixel by pixel (fonts differ between machines).
- **Logic stays testable without the UI:** everything the build issues list for `LoomCore` (builds, revisions, plain-word changes, status changes, search, git operations, recovery files, setup checks) is Catch2-tested; the Qt Tests check wiring and behaviour, not the rules again.
- **Each build issue's Windows check** covers what CI can't: the real Windows platform (fonts, high DPI, dark mode), TeX Live's Windows paths, PDF and file locking, Git Credential Manager, file watching when another editor saves, and a crash (ending the process in Task Manager) for recovery. The build issues already list their steps.

## Rules

### Front-matter changes in plain words
One line per change, in the field order of M1's tables, IDs always with their titles. `LoomCore` computes them between two contents (Review: `HEAD` and now).

| Change | Line |
| --- | --- |
| A new statement or proof | `new theorem 4KX7 · Energy decay …` / `new proof 4KX7.b (sketch, from 5HN8)` |
| A deleted file | `deleted` (and M1's `deleted-since-commit` warning) |
| `title`, `kind`, `origin` | `title: "Old" → "New"`, `kind: lemma → theorem` |
| `tags`, `aliases` | `tags: +energy-estimates, −model-system` |
| `stated_using` | `stated using: +2M9Q · Weak solution …, −…` |
| `premises` | `premises: +7RT2 · …, −9D3F · …` |
| `level` | `level: sketch → complete` |
| `superseded_by` | `superseded by 4KX7 · …` / `no longer superseded` |
| `refuted_by`, `refuted_on` | `refuted by G5TZ.a (accepted 2026-11-06)` / `no longer refuted` |
| `verified` | `front matter verified 2026-11-05` |
| The body | `text changed`; when fingerprints change, `text changed; outdates the checks of 4KX7.a, P3QA.a` |
| A check added | `check: Dakota pass`, `check: agent claude-opus-5-5 needs-changes`, `check: Dakota needs-changes: "Step 3 uses …"` (the first line), each with `(this version)` or `(an older version)` |
| A check changed or removed | `check changed: {by}, {date}` (M1's `check-changed` error) |
| A verbose version | `verbose version added` / `changed` / `marked up to date` / `removed` |

### Status changes
For every statement or stub whose status differs: `4KX7 · title: unchecked → proved ▲`. **Raises** go up the order proved > conditional > unchecked > sketch > heuristic > open, and cited > extracted; a change to or from false, or between no status and a status, is neither and gets no mark. Sorted drops first, then raises, then the rest, each by tag.

## Edge cases
- **A body with a `+++` line in it** (M1): the copy replaces only the front matter's lines, found as M1 finds them.
- **A project path with spaces or non-ASCII letters:** the build uses relative paths only; TeX never sees the project path.
- **`main.pdf` open in another PDF viewer on Windows:** `pdflatex` can't write it; the build fails with LaTeX's *"I can't write on file"*, shown as `tex-error`, and Loom's own view never holds it open.
- **A preamble that defines `theorem` itself:** no clash; Loom's environments are `loom…`.
- **A preamble that loads `hyperref`:** `\ref` becomes a link; labels from `\loomoutside` aren't links.
- **A statement whose kind changes from lemma to definition** while a proof of it exists: refused (M1's `proof-of-no-status`), as any change that adds an error.
- **A save in raw mode that changes `id`:** refused, `id-changed`.
- **Two Loom windows on one project:** the second can't open it (`.loom/app.lock`).
- **The app and an agent's `loom` writing at once:** the write lock serialises them; the app's open file then changed on disk, and *Never losing text* applies.
- **A switch or pull that changes an open file:** the same as any outside change.
- **A crash during a save:** the write is atomic (M1), and the recovery file is still there.
- **A build of a statement in a premise cycle, chain mode:** every member once, in tag order; `build-cycle`.
- **A chain through a superseded statement:** included as any other, with M1's `premise-superseded` among the validation problems.
- **A `\paperref` to a stub of a paper whose `paper.toml` has errors:** the entry is left out of `refs.bib` (`bib-entry-skipped`), so the citation is undefined (`tex-cite-undefined`).
- **A revision build after a key rename:** the revision's own `paper.toml` and aliases are used, so its citations resolve as they did then.
- **No `synctex`:** forward search goes to the statement's page; double-clicking the PDF does nothing and says why once.
- **A very long build log:** Output keeps the last 5 MB per channel.
- **A Markdown file with invalid math:** KaTeX shows the source in red; the rest renders.
- **A detached `HEAD`:** the panel says so; Commit is refused; Review compares with `HEAD` as usual.
- **A repository with no commits:** Review lists everything as new; before sides are empty; Push is hidden.

## Verification
| Test | Kind | Checks |
| --- | --- | --- |
| Wrapper per kind and mode | Catch2 (no TeX) | `main.tex` and the copies for each kind, each mode, verbose, a stub in the chain, a proof target, the exact example above |
| Line-preserving copies | Catch2 | Line *n* of each copy is line *n* of its source; CRLF sources; a `+++` line in a body |
| `\paperref` and `\cite` | Catch2 | Locator present, missing, unchecked, unknown; old keys and wrong-case keys rewritten in copies and not in sources |
| Log parsing | Catch2, on saved logs in `Tests/LoomCoreTests/TexLogs/` | Errors and warnings mapped to the right file and line, in a statement, a proof, a preamble and the generated part; repeated warnings once |
| Real builds | Catch2, label `tex` | The demo builds in every mode; `latex-error` gives K3AB's file and line; a timeout stops `latexmk` and its children; an unchanged build is reused |
| Revision builds | Catch2 | A statement at `HEAD` built while the working tree differs, the working tree untouched; the cache reused; new statements |
| `loom build` | CTest scripts | Exit codes 0–3, the JSON shape, `--rev` |
| Plain-word changes and status changes | Catch2 | Every row of both rules tables, on the `review` overlay |
| Saving editor text | Catch2 | `file-changed`, `id-changed`, outdated checks with and without confirmation, a raw save fixing a broken file, canonical front matter, the body byte for byte |
| Recovery files | Catch2 | Written, listed and removed; the three cases on opening |
| ID search | Catch2 | Ranking, prefix and accent matching, what each kind of field offers, aliases resolved to tags |
| Git operations | Catch2, on temporary repositories | Each operation and each refusal; commit through the hook, refused and passed; push to a bare repository |
| Setup checks | Catch2 | Found and missing tools (with a `PATH` of stand-ins) |
| The shell | Qt Test | Start screen, each row of *Opening a folder*, tab switching keeps state, shortcuts, Settings saved per user, reopening the last project |
| The Statements tab | Qt Test | Open, edit, save, refusals keep text, outdate-checks dialog confirmed and cancelled, new statement and proof, verbose, back and forward, find |
| Compile and PDF | Qt Test, label `tex` | Ctrl+S builds; the last good PDF dimmed on an error; a problem click lands on the line |
| IDs | Qt Test | The picker from `\ref{`, a field and Ctrl+P; insertion; Ctrl+click |
| Checks and clicks | Qt Test | Each click confirmed and cancelled; status updates everywhere |
| Never losing text | Qt Test | A file changed on disk with and without edits; recovery after the app is killed (a child process ended mid-edit) |
| Review | Qt Test | The `review` overlay listed and grouped; before and after; plain words; status changes; raw diff; nothing written |
| Markdown | Catch2 or Qt Test without WebEngine; screenshot test, label `webengine` | HTML and block diff for added, removed and changed blocks; math kept out of emphasis; no external loads |
| Git panel | Qt Test | Commit, refused commit with clickable problems, switch refused, push listing |
| On Windows | manual | Each build issue's Windows check |

## Open questions
- None. Dakota answered them on 2026-11-05, each as recommended, and approved the departures and decisions; the spec above follows his answers:
  - **Q1, a save that would add a validation error:** refused; the text stays in the editor and its recovery file, and the errors are listed (E6).
  - **Q2, numbers in statement builds:** the tag is the number, "Theorem 4KX7 (title)", and `\ref` prints the tag (E1).
  - **Q3, TeX engine and bibliography style:** pdfLaTeX and `amsplain`, fixed for M2, with no change to `loom.toml`. A `[tex]` table can come later if a project needs another engine.
  - **Q4, tabs of later milestones:** hidden until their milestone (E4).
  - **Q5, discarding uncommitted changes:** left out; Dakota has never used it (E5).
  - **Q6, rendered Markdown (#41):** stays in M2, as its last issue.

## See also
- Up: [Specs](README.md)
- [The design](loom.md): *The app* (*Tabs*, *Local review*), *IDs*, *Papers* (`\paperref`), *Technology*, *Build order*
- [The M1 spec](m1-core.md): the formats, the `loom` command, Dakota's clicks, validation, the toolchain
- The M2 build issues: #33–#41
