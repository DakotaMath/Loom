# M1 review: consolidated

**Reviewed:** `main` at ee04f71 (all of M1, through PR #19), 2026-11-04
**Reviewers:** five Opus 5.5 agents, one per slice (see `README.md`); the per-block logs are the other files in this folder.
**Method:** read-only. Nobody built or ran the C++ tests (the reviewers shared one `build/` folder); `python -m pytest Tools/tests` was run and passes. Findings marked *unverified* were traced by reading but not run, and several need Windows to confirm.

## Verdict

| Reviewer | Entries | PASS | NEEDS-CHANGES | FAIL |
| --- | ---: | ---: | ---: | ---: |
| foundations | 98 | 71 | 24 | 3 |
| statements-proofs | 63 | 45 | 14 | 4 |
| papers | 91 | 62 | 24 | 5 |
| status-validation | 101 | 83 | 17 | 1 |
| cli-build | 86 | 59 | 24 | 3 |
| **Total** | **439** | **320** | **103** | **16** |

**The core is sound.** status-validation traced the spec's worked example through `buildGraph()` by hand, and every status, deciding proof, "rests on" list, count and sentence matches. Every validation code in the spec is implemented and appears in EveryProblem. Every row of the verdict table is a test. Nothing lets anyone set a status directly. Dakota's clicks are explicit arguments, and the check history is appended as text. Path safety, front matter splitting, UTF-8 checks and fingerprints are solid; foundations recomputed the fingerprint test hashes independently. Hard rule 2 holds, and the configure-time link check enforces it.

**The problems sit at the edges:** odd input the readers accept, a few writes that can replace or escape what they should, and CLI polish. Of the 16 FAILs:
- 1 is high (`new-paper` can overwrite an existing `paper.toml`).
- 6 are medium.
- 9 are low, and most of those are two repeated patterns: empty strings, and regexes ending in `$`.

## Patterns that cross slices (fix once, everywhere)

| # | Pattern | Where | Verdicts |
| --- | --- | --- | --- |
| P1 | **Empty strings accepted in required and reference fields.** `Need::Required` keeps `""` without a problem, and every later check is guarded by `!isEmpty()`. Worst case: `refuted_by = ""` with `refuted_on` set passes `loom validate` but status ignores the refutation. | `readStatement` (StatementsAndProofs.cpp:111-173), `readProof` (:175-220), `readVerboseProof` (:222-250), `readStub` (Papers.cpp:756-865); also lets it through: `ContentsChecker::statements` (ProjectContents.cpp:183-217). Note that `FieldChecker::stringField` (TomlFields.cpp:62-77) uses a different rule for the same thing. | 4 FAIL, 2 NC |
| P2 | **Qt regex `$` matches before a final `\n`.** `by = "agent:x\n"` is a valid author; `id = "energy\n"` is a valid tag. Fix: `QRegularExpression::anchoredPattern()` (or `\z`), and sweep every `QRegularExpression` in the code base. | Checks.cpp:159, StatementsAndProofs.cpp:156 and 243, ProjectToml.cpp:100, Papers.cpp:352-353 and 451 | 4 FAIL, 1 NC |
| P3 | **Chained `QString::arg()` rescans text it has already substituted.** `Smith%2C%20Jones.pdf` prints as `Smith1C%20Jones.pdf`. Fix: one multi-argument `.arg()` call. | Problem.cpp:8-13 (every `loom validate` line), Cli.cpp:241-259, StatusCommands.cpp:136-181 (`graph --json` could produce invalid JSON) | 1 FAIL, 2 NC |
| P4 | **Writes that can replace or escape what they should leave alone.** | `createPaper` overwrite; `build-bib --out` onto any project file; `installHooks` without a project check and following `core.hooksPath`; `carryOut` renames not rolled back | 2 FAIL, 3 NC |
| P5 | **Paper keys compared case-sensitively,** though keys are folder names and Windows folders aren't case-sensitive (unverified on Windows). | `assignKey` (PaperKeys.cpp:189-243), `paper-key-duplicate` (ProjectContents.cpp:280-290), `createPaper` | NC, and part of the high FAIL |
| P6 | **Missing doc comments** (CLAUDE.md requires one on every function). | `inAlphabet`/`isLowerLetters`, `ProjectFixture`, `Writer` methods, `ReadingEntry::lines`, `publishedFields`, the `ContentsChecker` constructor, `Computation::isStub`, a stray line at EditSession.cpp:31 | ~9 NC, low |

## FAIL (16)

| ID | Sev | Location | Problem | Change |
| --- | --- | --- | --- | --- |
| F1 | **high** | `createPaper`, PaperEdits.cpp:191-250 | Nothing checks that `Papers/<Key>/paper.toml` is free. An existing entry that doesn't parse (`othersThan` skips it), whose `key` disagrees with its folder, or that differs only in case (Windows) is overwritten, and the "adds no error" guard misses it. Loses user text. | Refuse if the target folder or `paper.toml` already exists (compare ignoring case). Add tests for all three cases. |
| F2 | medium | `installHooks`, Hooks.cpp:63-124 + `runInstallHooks`, ValidateCommands.cpp:69-106 + `findProject`, Cli.cpp:161-176 | (a) Never checks that `root` holds `loom.toml` or is the git top level. On any repo, or a subfolder of a bigger repo, it takes over the pre-commit hook and every later commit is refused (exit 3). (b) `rev-parse --git-path` follows `core.hooksPath`; verified with git 2.55, it renames the user's shared hook, and commits in every other repo are refused. Affects `new-project` too. | Guard: `loom.toml` exists, `show-toplevel` equals root, and the hook path is inside the repo's own git dir; otherwise refuse with a message. Needs a spec decision (S1). Make `findProject` check for `loom.toml`. |
| F3 | medium | `readStatement`, StatementsAndProofs.cpp:111-173 | P1: empty `kind`, `id`, `superseded_by` and `refuted_by` are accepted. Also `created = 0000-01-01` becomes an invalid QDate with no problem (FieldReader.cpp:106-124), and the next edit is then refused with a confusing `missing-field`. | Report empty required/reference values (`missing-field` or `bad-value`); have `FieldReader::date` report an invalid date and set `lost`. |
| F4 | medium | `PaperReader::version`, Papers.cpp:333-418 | `arxiv_id` form is never checked; `"2503.00001v2"` validates and the bibliography writes `eprint = {2503.00001v2v2}`. | Check the form (both `YYMM.NNNN` and `YYMM.NNNNN`, no `vN`), `bad-value`. Needs a spec note (S10). |
| F5 | medium | `PaperField::text`, Papers.cpp:613-625 | With an empty `first`, the name is written without the comma, so BibTeX reads `{Garcia Lopez}` as First=Garcia, Last=Lopez (BibTeX parse unverified). | Write `{Last}` braced or `Last,`, per the spec's answer (S12). |
| F6 | medium | `printUsage`/`main`, main.cpp:34-38, 173 | An unknown subcommand prints only the usage: no line naming the bad word, and nothing on stdout with `--json`, though the spec requires an error object. | Print "`x` isn't a subcommand", emit the JSON error object, exit 2. |
| F7 | medium | `main`, main.cpp:110-174 | `loom help [subcommand]` (in the spec) doesn't exist; `loom help` exits 2. Also `--json`/`--project` are stripped from anywhere in the argument list, even when they are another option's value. | Implement `help` (stdout, exit 0); strip global flags only in option position. |
| F8 | medium | `install-hooks` (same as F2, CLI side) | Logged separately by cli-build; one fix covers both. | See F2. |
| F9 | low | `readProof`, StatementsAndProofs.cpp:175-220 | P1: empty `level`, `id` and `proves`. | As F3. |
| F10 | low | `readVerboseProof`, StatementsAndProofs.cpp:222-250 | P1 + P2: an empty `proof`/`fingerprint`, or a fingerprint with a trailing `\n`, passes, and that verbose file is then never reported stale. | As F3, plus an anchored regex. |
| F11 | low | `isCheckAuthor`, Checks.cpp:157-161 | P2: `by = "agent:claude\n"` is accepted. | Anchored regex. |
| F12 | low | `readTags`, ProjectToml.cpp:89-138 | P2: a tag ending in `\n` passes `tag-syntax`. | Anchored regex. |
| F13 | low | `PaperReader::readings` and version regexes, Papers.cpp:352-353, 451 | P2. | Anchored regex. |
| F14 | low | `readStub`, Papers.cpp:756-865 | P1: empty `id`, `kind` and `extracted_by`. | As F3. |
| F15 | low | `problemLine`, Problem.cpp:8-13 | P3: file names with `%1`–`%9` are garbled in `loom validate` output. Confirmed by reading. | Single `.arg(a, b, c, d)` call; add a CLI test. |
| F16 | low | `checkAgainstCommit`, Validation.cpp:520-570 | Committed checks are matched as a multiset, so order isn't enforced. Swapping two committed same-date checks (a pass and a needs-changes) passes `check-order` and `check-changed`, and changes which one counts: a proof becomes checked without a new click from Dakota. | Require committed entries to appear in the same relative order (a correct merge always keeps it). Needs a spec wording fix (S21). |

F2 (core side, foundations) and F8 (CLI side, cli-build) are one problem logged by two reviewers. So there are 15 distinct FAILs, and one fix covers both.

## NEEDS-CHANGES, medium (9 entries)

| ID | Location | Problem | Change |
| --- | --- | --- | --- |
| M1 | `carryOut`/`commitPapers`, PaperEdits.cpp:121-167 | Earlier renames aren't undone when a later one, or the new `paper.toml` write, fails (e.g. a PDF open in `local/` on Windows). The letters end up permanently out of posting order. | Undo every rename already done, in reverse, on any failure. |
| M2 | `buildBibliography`, PaperEdits.cpp:357-392 | `build-bib --out loom.toml` (or any `paper.toml` or `.tex`) replaces a Loom-owned file with BibTeX. It also skips the "adds no error" check its header promises. | Refuse `--out` onto any file Loom owns; allow only `.bib` outside the model's files. |
| M3 | `assignKey`, PaperKeys.cpp:189-243 | P5: key comparisons are case-sensitive. | Compare ignoring case (S9). |
| M4 | `bracesBalanced`, Bibliography.cpp:129-141 | `\{` is skipped when counting braces, but BibTeX counts it, so `On the set \{x > 0` passes and breaks the `.bib`. A test pins the wrong behaviour. | Count every brace (S13). The test change must be flagged (hard rule 7). |
| M5 | PaperEditsTests.cpp | No test for the overwrite (F1) or for the partial-rename failure (M1). | Add them. |
| M6 | `readCommit`, Validation.cpp:354-384 | Any `git rev-parse` failure counts as "no commit yet", so `check-changed` and `deleted-since-commit` are skipped silently and validate is clean. Likely trigger: git's "dubious ownership" on Windows (unverified). | Treat only "no HEAD yet" as no commit; report other failures. |
| M7 | `findProject`, Cli.cpp:161-176 | `--project <folder>` isn't checked for `loom.toml`. | Check it here (part of F2). |
| M8 | `runAddCheck`, CheckCommands.cpp:63-135 | For Dakota's `needs-changes`, Dakota types the message and `yes` before the core validates anything. A mistyped proof ID or a held lock then loses that text (the "never lose user text" rule). | Validate first, prompt second; or save the typed text to `.loom/` on refusal. |

M1 covers two log entries (`carryOut` and `commitPapers`), so these 8 rows are the 9 medium entries.

## NEEDS-CHANGES, low (94), by area

Full detail is in each log; this is the shape.

- **Unicode and input edges (foundations):**
  - `normaliseTag`/`normaliseProofId` map `ı`, `ſ` and the Kelvin sign to ASCII.
  - `firstInvalidUtf8` reports an offset past the end of the file for a cut-off character.
  - `crlfToLf` isn't idempotent on `\r\r\n`.
  - `readProjectToml` skips the UTF-8 check (`toml-syntax` instead of `not-utf8`).
  - Windows reserved names (`con.md`) and trailing dots are accepted as paths (unverified).
  - `GitRun::started` is false on a timeout, so the message says to install Git.
  - QProcess may find a `git.exe` in the current folder first (unverified).
  - `readKit` skips a broken resource silently.
- **Statements and proofs:**
  - `findCycles` recursion depth on long premise chains (unmeasured).
  - `proofChecking` drops the fingerprint problem, so `lacks()` (Status.cpp:225-245) gives the wrong reason.
  - Origin regex (P2).
- **Papers:**
  - `http://` sources are reported as `pdf-path`.
  - The `authors-missing` wording.
  - `formatPaper` drops comments on rekey.
  - `arxivNumber` is copied in three files.
  - `escapeBibValue` misreads `\\%`.
  - `keyFitsBase`/`rekeyPaper` ignore step 4 (an unlettered `SmithJones24` beside `SmithJones24a`).
  - `formatBibliography` files problems under the key, not the real path.
  - `readPaper` names a draft by its folder.
- **Status and validation:**
  - The deciding-proof tie-break departs from the spec (S19).
  - `tagsInUse` misses folders with only a checks file.
  - `resolveProof` doc says it resolves aliases; it doesn't.
  - A stub as `superseded_by`, and `\paperref` to a statement, both pass silently.
  - `loadPapers` gives a duplicate `missing-field` after `bad-path`.
  - `StatementNode::paper` holds the folder, not the key.
  - `premiseCycles` and `ContentsChecker` do linear scans inside loops.
  - Plain `validate` creates a temporary folder it doesn't need.
- **CLI:**
  - The usage text has no `help` line.
  - The `--json` usage error embeds the whole usage text.
  - `runNewProject` has its own argument parser (`--name A --name B` is silent; `--project` is dropped).
  - `Utf8Console` isn't restored on Ctrl+C.
  - `editWithConfirmation` may outdate different proofs from those Dakota confirmed.
  - `runFingerprint` duplicates `readProject`.
  - `runRefute` returns exit 3 after a write that succeeded.
  - mintty pipes make Dakota's commands refuse in Git Bash.
- **Build and CI:**
  - Qt DLLs aren't deployed next to `loom.exe`, which the hook's fallback path needs (unverified).
  - The `windows` test preset has no `configuration`.
  - The link-check walker has two gaps.
  - `update_indexes.py` doesn't escape `|` in table cells.
- **Tests (about 25 entries):**
  - Every test file has a list of missing cases.
  - The weak spots are refusal paths: `proof-of-stub`, `premise-cycle`, `no-fingerprint`, `not-editable`, `hooks-failed` and `core.hooksPath`.
  - Also: `superseded_by` edits, merge with undated entries, and `install-hooks` on a non-project.
  - `WILL_FAIL` passes on a crash.
  - `StatusCli` refute relies on the test runner's stdin.

## Spec issues: Dakota to decide (27, merged across reviewers)

**Blocking a FAIL fix:**
- **S1. `core.hooksPath`.** Refuse, or write to `.git/hooks` and warn that the guard is off? The same question applies to `git lfs install --local` (F2).
- **S2. Empty strings.** Is `""` in a required or reference field an error on reading? (F3, F9, F10, F14.) All three reviewers who hit it assumed yes.
- **S9. Key case.** Do keys collide when equal ignoring case, for lettering and `paper-key-duplicate`? (F1, M3.)
- **S10. `arxiv_id`.** Allow the 2007–2014 `YYMM.NNNN` form; no `vN`; which code for a malformed one (F4).
- **S12. An empty `first`.** Write it as `{Last}` or `Last,` (F5)?
- **S13. Brace balance.** Do `\{`/`\}` count (M4)?
- **S21. Append-only.** The validation sentence should say "in the same order" (F16).

**Wording or contradictions:**
- **S3.** The CRLF rule isn't idempotent on `\r\r\n`.
- **S4.** "Shaped like a tag" is undefined. The code refuses `thm1`, `eq12` as aliases.
- **S5.** `new-project` step 1 (create the folder) contradicts the edge case. The code follows the edge case.
- **S6.** `superseded_by` says "another statement", but the code accepts a paper stub, and `superseded-invalid` has no wording for that case. Two reviewers raised this.
- **S7.** Is `edit-proof --level failed` Dakota's click? The spec says so; the code lets anyone set it.
- **S8.** The `record` prefix (`Records/…` or `Reviews/…`) isn't enforced. Add a code, or drop the rule.
- **S11.** Escaping `%`/`#` in `url` and `doi` breaks links. Exempt those fields?
- **S14.** The `authors-missing` wording for non-books.
- **S15.** Comments in `paper.toml` are dropped by rekey.
- **S16.** A `new-paper` draft can carry `verified` dates, which are Dakota's click.
- **S17.** The codes `bib-entry-skipped` and `bib-unverified` aren't in the spec.
- **S18.** `paper-key-rule` is also used for "can't make a key", which repeats `authors-missing`. Raised by three reviewers.
- **S19.** The deciding-proof tie-break: adopt the code's rule ("the proof that brought it into the proved set").
- **S20.** "What a proof gives" names only the agent's fail; rule 7 says either check.
- **S22.** `\paperref` to a non-stub has no problem code.
- **S23.** Tags on stubs: `loom.toml` allows them, but the stub table has no `tags`.
- **S24.** Is Dakota's `changes` text quoted in full or cut short?
- **S25.** Can deleted tags be drawn again? Generation checks only tags in use, not those in HEAD.
- **S26.** Refuting a statement by one of its own proofs is accepted.
- **S27.** Clock skew: a future-dated last check blocks `add-check` with an unexplained `check-order`.

**Missing from the spec only:**
- `help`'s exit code and stream.
- The JSON shapes of `new-tag`, `install-hooks`, `refute`, `rekey`, `build-bib` and `add-check`.
- The Verification table says the CLI tests are Catch2; they are CTest scripts.
- The spec calls `loom` "one self-contained program", but it needs Qt's DLLs.
- Whether `--outdate-checks` confirms exactly the proofs Dakota was shown.

## Needs a Windows check
- Case-insensitive key collisions (F1, M3).
- A PDF open in `local/` blocking a rename (M1).
- "Dubious ownership" in `readCommit` (M6).
- Reserved names as paths.
- `git.exe` found in the current folder.
- Qt DLLs and the hook's fallback `loom`.
- The console code page after Ctrl+C.
- mintty in Git Bash.

## Proposed fix batches

These are grouped by theme so that parallel fixers don't edit the same files. Each batch is one PR. Hard rule 7: any test that changes alongside its code goes under ⚠️ Flags.

1. **Write safety:** F1, F2/F8, M1, M2, M7. Touches PaperEdits, Hooks, ValidateCommands and Cli. Needs S1 and S9 first.
2. **Reader hardening:** F3, F9–F14 (P1, P2), `FieldReader::date`, `readProjectToml` UTF-8, `crlfToLf`, the Unicode case mapping. Touches the readers in StatementsAndProofs, Papers, Checks, ProjectToml, FieldReader, FrontMatter and Ids. Needs S2.
3. **Papers and bibliography:** F4, F5, M3, M4, `keyFitsBase` step 4, `escapeBibValue`, the `arxivNumber` copies, `paper-key-rule` messages. Touches PaperKeys, Bibliography, Papers (writer side) and ProjectContents' paper checks. Needs S9–S13 and S18.
4. **Validation and status:** F16, M6, `lacks()`/`proofChecking`, `tagsInUse`, `loadPapers`, `validateProject` temp dir, the linear scans. Touches Validation, Status, ProofChecks and ProjectContents.
5. **CLI:** F6, F7, F15 and P3 everywhere, M8, `runNewProject`, `usageError`, `runRefute`, `editWithConfirmation`, the CMake presets, `WILL_FAIL`. Touches LoomCli, Problem.cpp and CMake.
6. **Docs and tests sweep:** P6, plus the missing test cases not already covered by batches 1–5.

Batches 2 and 3 both touch Papers.cpp (readers vs. writer and keys). Run them one after the other, or give the reader half of Papers.cpp to batch 2 only.

## Process notes
- Reviewers couldn't message each other by name in this session, so the coordinator relayed nine cross-boundary notes. All were answered and are recorded in each log's Cross-boundary notes.
- Coordinator spot-checks against the source confirmed F1 (no existence check; unparsed papers skipped by `othersThan`), F15 (chained `.arg`) and F16 (multiset match). The other findings rest on the reviewers' traces.
