# M1 final review: final-spec (conformance of `main` at 5626c88 to `Specs/m1-core.md` with A1–A17)

## Verdict
The code on `main` conforms to the approved spec. Each amendment A1–A17 has code that does what the spec text says. I checked the codes, messages and exit codes against the source, and for A1, A3, A10, A11, A12, A15 and A16 I also ran `loom.exe` in scratch folders, including a linked worktree for A10. All 82 codes in the Validation tables exist in the code with the spec's severity. Each one is in `EveryProblem-expected.txt` except `format-too-new`, which stops validation instead of being listed and has its own test. Each one also has a row in `Wiki/how-to/fix-validation-problems.md`. The worked example gives exactly the spec's statuses, counts, sentences and five expected problems, as text and as JSON. Every *(Dakota)* subcommand refuses without a terminal with the spec's message, exit 1, code `dakota-only`. Every row of the Verification table has a test that does what the row says. One exception: the "Validation messages" row says "text and JSON", and the JSON half is tested only on the worked example, not on the every-problem project. I found **no blockers**. What remains is wording drift, three partial A9/A16 conformance gaps (all low), two spec texts that still disagree with the code (S4, S5), one design question for Dakota, and `STATUS.md`, which is several PRs out of date.

## Blockers
None. Nothing I found contradicts the spec in a way users will hit, loses text, writes without a click or lets someone claim a status. The closest is the design question in Follow-ups F1. The code does exactly what the spec says there.

## Amendments A1–A17
| # | Conforms | Where | Note |
| --- | --- | --- | --- |
| A1 | yes | `Hooks.cpp:104-136` (`installHooks`), `NewProject.cpp:121-133` | Ran it: a folder that isn't the top of its repository gives `hooks-not-top-level`, exit 3. A local `core.hooksPath` gives `hooks-path-outside`, exit 3, nothing written. A global `core.hooksPath` stops `new-project` before anything is written. Messages match the spec word for word. |
| A2 | yes | `TomlFields.cpp:56-62` (`emptyField`) | "`{field}` is empty; every {thing} needs one." Drift: a stub locator's empty `.at` says "every locator says where the result is" (`Papers.cpp:976`). |
| A3 | yes | `PaperKeys.cpp`, `PaperEdits.cpp:234` (`paper-exists`) | Tests: "Keys that differ only in case are the same key (A3)", "new-paper never replaces a folder that is already there". |
| A4 | yes | `Papers.cpp:503-507` | Message matches word for word. Test "arxiv_id is an arXiv identifier without its version (A4)". |
| A5 | yes | `Bibliography.cpp:18` | `{Last}`. Test "…surname in braces (A5)". |
| A6 | yes | `Bibliography.cpp:160-171` | Counts every brace. |
| A7 | yes | `Validation.cpp:624-670` | Subsequence check; message says "changed, moved or removed". Test "committed checks keep their order, even with the same date". |
| A8 | yes | `TomlFields.cpp:60-61` | Wording matches. Same `.at` drift as A2. |
| A9 | **partly** | `TomlFields.cpp:87` | Front matter, `loom.toml`, `paper.toml` and stubs conform. **Check histories don't.** `EntryReader::date()` (`Checks.cpp:66-94`) builds `QDate(0,1,1)` with no check. I reproduced it: a check dated `0000-01-01T10:00:00Z` gives no `bad-value`, and `status` still treats the agent pass as present. Found by final-core, confirmed here. Low. |
| A10 | yes | `NewProject.cpp:128-132`, `Hooks.cpp:126-136`, `PaperEdits.cpp:265-279` | `new-project` has its own message. `--git-common-dir` is used, and I ran `install-hooks` in a linked worktree: accepted, hook written into the main `.git/hooks`. `bib-out-project-file` ran: exit 1, message matches, case is ignored (`loom.TOML`). |
| A11 | yes | `main.cpp:43-55, 120-127`, `Cli.cpp:185-217` | `help`, `help <sub>` and `--json` exit 0. An unknown or missing subcommand exits 2 with the spec's words. `--title --json` is read as a title. `--project` given twice, or given to `new-project`, is a usage error. Stale comment at `main.cpp:39-41` ("the spec gives no exit code for `help`"). |
| A12 | yes | `ProjectContents.cpp:210-231`, `Validation.cpp` (`paperref-not-stub`), `Edits.cpp:559-563` | Ran `loom refute 4KX7 --by 4KX7.a`: the spec's message, exit 1, checked before Dakota is asked. |
| A13 | yes | `Status.cpp:136-157` (rounds, lowest letter), `Status.cpp:233-241`, `ProofChecks.cpp:11-41`, `Validation.cpp:404-445` | The proved deciding proof is the one from its entry round. The fingerprint explanation matches the spec's example. `git-failed` message matches. |
| A14 | yes | `ProjectContents.cpp:324-336`, `Papers.cpp:508-511, 317` | Skipped when `authors-missing` applies; otherwise the key rule's message. |
| A15 | yes | `CheckCommands.cpp`, `StatementCommands.cpp`, `StatusCommands.cpp`, `PaperCommands.cpp`, `ValidateCommands.cpp:100-102` | Ran every shape except merge-checks: all keys present. `build-bib`'s `path` is an absolute native path, while `new-*` give project-relative paths (the spec is silent). |
| A16 | **partly** | `StatementCommands.cpp:40-65`, `Edits.cpp:70-101`, `PaperCommands.cpp:40`, `Git.cpp` | The confirmed set is exact: changing the proof set in between gives `outdates-checks`, with the spec's sentence. `failed` is a click in `new-proof` and `edit-proof`, and going back from `failed` is not. `new-paper` sets the dates to `false` and says so. **`git-timeout` is not used everywhere.** A timed-out git gives `git-failed`, still exit 3, in: new-project steps 4–5 (`NewProject.cpp:172-188`), install-hooks' `--git-path`/`--git-common-dir`/`config` calls (`Hooks.cpp:126-131, 179-185`), and validate's `ls-tree`/`cat-file` (`Validation.cpp:449, 463`). Known from PR #26. Low. |
| A17 | yes | `PaperEdits.cpp` (lookup by key), `Validation.cpp:190-203` (`cite-alias`), `Papers.cpp` (`alias-duplicate`), `Bibliography.cpp:301` + `PaperEdits.cpp:555-570` | Ran `new-stub smithjones24` and `rekey smithjones24A`: both found the paper. Tests exist for the case-only rename and the case-alias duplicate. |

## Validation codes
**Spec → code, expected file and wiki:** all 82 codes are emitted in `Source/`, with the spec's severity.
- **Expected file:** every code is in `Tests/Projects/EveryProblem-expected.txt` except `format-too-new`, which stops validation instead of being listed. It is tested by "validate stops on a project from a newer Loom".
- **Wiki:** every code has a row in the how-to.

Message drift I saw, none of it changing the meaning:
- `ref-not-listed` puts backticks around the field name.
- `authors-missing` adds "(and no editors)" only for a book. That matches the spec, since only a book may have editors instead of authors.
- `version-id` uses "an arxiv version".
- A stub locator's `.at` uses the wording "every locator says where the result is".
- A whitespace-only `work.title` gives `wrong-type`: "should be a non-empty string, not an empty string" (`Papers.cpp:386`). The value isn't empty; the Checks reader's "not only whitespace" is the better wording.

**Code → spec:** codes the code emits that aren't in the Validation tables.

| Code | Covered by the spec? |
| --- | --- |
| `usage`, `dakota-only`, `outdates-checks`, `paper-exists`, `bib-out-project-file`, `bib-entry-skipped`, `refuted-self` (refute), `hooks-not-top-level`, `hooks-path-outside`, `git-failed`, `git-timeout` | Named in the spec's text |
| `not-a-project`, `unreadable`, `locked`, `write-failed`, `format-too-new` (as a command error) | Exit-3 cases the spec describes without naming a code |
| `git-missing`, `git-lfs-missing`, `git-identity-missing`, `folder-not-empty`, `not-a-folder`, `parent-missing`, `name-empty`, `kit-missing`, `hooks-failed`, `not-a-repository` | new-project and install-hooks failures; the spec gives the messages but no codes |
| `not-confirmed` | Dakota typed something other than `yes`; the spec only says "go ahead only on yes" |
| `hook-backup-exists` (exit 1) | Not in the spec: `pre-commit.before-loom` already exists. Covered in the how-to's "If it goes wrong" |
| `no-fingerprint`, `fingerprint-needed`, `proof-changed` | add-check refusals; the spec gives the "changed after you read it" message but no code |
| `already-there`, `not-there`, `not-editable`, `no-tag` | Edit refusals; the spec is silent |
| `merge-failed` | merge-checks exit 1; the spec gives the behaviour |
| `bib-unverified` | build-bib's unverified-fields warning; the spec gives the message but no code |

Validation codes are also reused as CLI refusal codes, which is fine: `ref-unknown`, `check-verdict`, `paper-key-rule`, `field-shape` and others when a draft has errors, `tag-duplicate` through "adds no error", and `not-utf8` for a bad `--body-file` (exit 2). None of these is a problem; they are internal or CLI codes for situations the spec covers.

## Command and JSON
- **Subcommands and arguments:** all 21 entries in the table exist with the arguments listed. The usage text matches the table.
- **Exit codes:** these runs gave the spec's codes:
  - 0: status, graph, `--version`, `help`.
  - 1: refute of its own proof, a stale `--fingerprint`, a duplicate tag, `bib-out-project-file`, a non-empty folder for `new-project`, validate with errors.
  - 2: no subcommand, an unknown one, `graph` without `--json`, `help a b`, `--project` twice.
  - 3: no project, `hooks-path-outside`, `hooks-not-top-level`, a global `hooksPath` for `new-project`, no git identity.
- ***(Dakota)* without a terminal:** ran each with input from `/dev/null`. `refute`, `add-check --by dakota`, `new-proof --level failed`, `edit-proof --level failed` and `edit-statement … --outdate-checks` each refuse with "Only Dakota can {…}, and only at a terminal.", exit 1, `dakota-only`. The `--json` error object is on stdout.
- **What is checked before Dakota is asked:** refute and add-check check what they can before asking, so a mistyped ID fails as itself. A message he typed is printed back if it wasn't saved.
- **JSON:**
  - `status --json`: the counts match the spec's example exactly (8/6/4/2/1/2/1/1/1/2).
  - Every `graph` key and every proof field is present.
  - `validate --json` and the A15 objects (new-project, install-hooks, new-tag, new-statement, new-proof, new-stub, edit-*, fingerprint, add-check, refute, new-paper with `unclicked`, rekey, build-bib) were all checked by running them.
- **Small drifts:**
  1. `loom --version extra` says "`--version` isn't a loom subcommand".
  2. The timeout message names only git's first argument ("`git config` didn't finish…"), where the spec says `{git command}` (`Git.cpp:42-45`).
  3. new-paper's stderr says "Set to false…: verified, …" but a top-level `verified` is left out, not set to false.
  4. `build-bib --json` gives an absolute `path`; the other subcommands give project-relative ones.

## Verification table
| Row | Tests |
| --- | --- |
| New project layout | NewProjectTests "new-project writes exactly the spec's files and makes one commit", "…refuses a folder it can't use…", "…stops before writing anything without git, Git LFS or a git identity"; HooksTests "new-project installs the hook, which then guards every commit"; `loom.new-project` (NewProjectCli.cmake) |
| Front matter round trip | FrontMatterTests (all 15 tests); StatementsAndProofsTests "Rewriting keeps the body byte for byte", "a byte-order mark is dropped"; EditsTests "edit-statement rewrites a CRLF file as LF" |
| Canonical writer | StatementsAndProofsTests "The canonical writer", "The spec's examples are in canonical form"; PapersTests "Version fields are written in the spec's order" |
| Field validation | StatementsAndProofsTests "Statement problems", "Proof problems", "Empty values…"; PapersTests "Field problems in paper.toml, with their lines", "Stub problems, with their lines"; ProjectTomlTests "loom.toml problems: syntax and fields"; ChecksTests "Problems a check history shows on its own" |
| IDs | IdsTests (8 tests) |
| Fingerprint | FingerprintTests "The fingerprint of a fixed example: its bytes and its hash", "Every input…", "Line-end whitespace, CRLF, final newlines and premise order…" |
| Verdict table | ChecksTests "The verdict table": all 16 rows plus the invalid row |
| Check merge | ChecksTests "Merging two check lists" |
| Worked example | StatusTests "The worked example: every status"; ValidationTests "validate finds exactly the worked example's problems"; StatusCli.cmake / ValidateCli.cmake (text and JSON) |
| Status explanations | StatusTests "Explanations: one sentence for each status", "Explanations of less common cases" |
| Key rule | PaperKeysTests "Every row of the key table", "Shared keys: …" (4 tests); PaperEditsTests "rekey renames a preprint once it is published, keeping the old key…" |
| BibTeX | BibliographyTests (16 tests, including the spec's two examples and "one entry per other type") |
| Validation messages | ValidationTests "validate reports one of every problem, exactly" (text). **The JSON half is partial:** JSON is checked only on the worked example (ValidateCli.cmake:68-77). The comment at ValidationTests.cpp:2-3 says the every-problem JSON is checked there, but it isn't |
| CLI | `loom.usage`, `.new-project`, `.statements`, `.checks`, `.papers`, `.status`, `.validate` (Tests/CMake/*.cmake): exit codes 0–3, JSON, and `dakota-only` refusals (ChecksCli:92-123, StatementsCli:100-107, StatusCli:78) |
| Path safety | ProjectFilesTests (7 tests) |
| Build and tests on Windows; Commit guard on Windows | Manual. I ran the commit guard on Windows from Git Bash: a broken `kind` is refused with the message, and the commit goes through once fixed. These two rows are Dakota's to sign off |

The worked example's statuses and sentences match the spec. The output of `loom status --json` agrees with the table row by row, and the six example sentences in the spec are reproduced exactly (4KX7, 6BWT, 8CJA, D9XF, F2NS; B7MP has the same shape). The spec's "H8KD proved (via J3RW.b)" means J3RW is decided by J3RW.b, and the code gives exactly that. H8KD's own deciding proof is H8KD.a.

## Templates and wiki
- **`Templates/project/`:** it matches the spec. CLAUDE.md is identical to the spec's quoted block; `TeX/preamble.tex`, `TeX/macros.tex`, `gitignore`/`gitattributes` (renamed with a dot), and `loom.toml` are as specified. I ran `new-project` and the output was identical. The pre-commit hook's not-found message matches. **Known gap (A16), confirmed:** the kit's CLAUDE.md doesn't say that marking a proof `failed` is Dakota's click. It doesn't mention `--outdate-checks` either; agents can't run it without a terminal, but the list of clicks is incomplete.
- **Wiki:**
  - `how-to/fix-validation-problems.md`: current; no contradictions found.
  - `gotchas/git/hookspath-moves-hooks.md` says "Not yet tried with a linked worktree". I tried it on Windows (Git for Windows) and `install-hooks` accepts it and writes into the common `.git/hooks`. Update the line, and add a test (there is no automated worktree test for A10).
  - `gotchas/windows/isatty-true-for-nul.md` "mintty not yet checked": still open.
  - `how-to/build-on-windows.md`, `start-here/*` and the other gotchas: no contradictions with the code or spec.
- **Spec texts the review flagged, still unchanged** (relayed from final-core, verified):
  - **S4:** "shaped like a tag" (m1-core.md:274) is still undefined; the code refuses `thm1` and `eq12` as aliases.
  - **S5:** `new-project` step 1 still says "Creates `<folder>`" before step 2's checks (m1-core.md:74-75). The code creates it only after the checks, which agrees with the edge case "stops before writing anything".

  Neither is on the deferral list or in PR #28's note. The spec should be amended to say what the code does.

## STATUS.md lines out of date
- `**Updated:** 2026-11-04 · main at 414d5b1` should be main at 5626c88.
- `**Current milestone:** … #9 … is in review`: #9 is merged (PR #19, ee04f71), the M1 review is done (PR #20), and fixes PRs #21–#27 and the spec amendments PR #28 are merged; now in the final review.
- "Done recently": missing #9 / PR #19, the M1 review (PR #20, A1–A7), PRs #21–#27 (fixes; logs `2026-11-04-fix-*.md`, `reader-hardening`), and PR #28 (A8–A17).
- "In progress": the PR #19 line is obsolete; it should be the M1 final review.
- "Next up": "once #19 is merged, M1 is done" is out of date.
- "Waiting on Dakota": the PR #19 line is obsolete. It should be the Windows manual checks (the last two rows of the Verification table) and whatever this final pass raises.
- "Watch out for": could add the `core.hooksPath` gotcha.

## Follow-ups
1. **Spec question for Dakota (not a code bug):** `level` isn't in the fingerprint, and checks on a sketch count. So a sketch that has an agent pass and Dakota's pass becomes **proved** when anyone runs `edit-proof X --level complete`, with no new click. That follows the spec ("What isn't in it: … `level`"). It may still not be what Dakota means by "status is never claimed". Options: make raising the level to `complete` outdate the checks, or make it a click.
2. A9 for check histories: `Checks.cpp:91` should report `bad-value` for year 0 and drop the entry, as the other readers do. Low.
3. A16: use `git-timeout` for every timed-out git (`NewProject.cpp:172-188`, `Hooks.cpp:126-131, 179-185`, `Validation.cpp:449, 463`), and name the full git command in `gitDidntRunMessage`. Low.
4. Amend the spec for S4 (define "shaped like a tag") and S5 (create the folder after the checks).
5. The kit's CLAUDE.md: add "never mark a proof `failed` or outdate checks" (A16).
6. Test the every-problem JSON through `loom validate --json`, or correct the comment at ValidationTests.cpp:2-3. Add a linked-worktree test for A10.
7. Wording:
   - the locator `.at` empty message → A8 form;
   - the whitespace `work.title` message;
   - new-paper's "Set to false" for a top-level `verified` that is left out;
   - `loom --version extra`;
   - the stale comment at `main.cpp:39-41`;
   - the test name "committed checks may move…" reads against A7;
   - `build-bib --json` `path` form;
   - new-project's `hooksPath` message says "global" when the setting comes from the system config (the spec says the same).
8. Wiki: update the worktree line in `hookspath-moves-hooks.md`; update `STATUS.md` as above.

**Drift count:** 13. These are items 2, 3, 6, 7 (eight items) and 8 (one item) above, plus `ref-not-listed`'s backticks.
