# M1 final review: papers, bibliography, `loom`, build and tests

**Reviewer:** final-papers-cli
**Reviewed:** `main` at 5626c88, against the M1 review at ee04f71 (`m1/review`: `papers.md`, `cli-build.md`, `CONSOLIDATED.md`, `COORDINATOR-NOTES.md`) and PRs #21–#28.
**Scope:** `Source/LoomCore/` Papers*, PaperKeys*, PaperEdits*, Bibliography*; the paper parts of ProjectContents (`ContentsChecker::papers`, `loadPapers`, `paperWithKey`) and Validation's `cite-alias`; all of `Source/LoomCli/`; `CMakeLists.txt`, `CMakePresets.json`, `Tests/CMakeLists.txt`, `Tests/CMake/*`; FakeGit, TestSupport, ProjectFixture; `Tools/`.
**Method:** I read the code on `main` and the diff since ee04f71. I ran `ctest --test-dir build/windows -C Debug`: **269/269 pass**. I also ran `build/windows/bin/Debug/loom.exe` by hand in scratch projects under my temp folder, on Windows:
- the F1 overwrite cases;
- the M1 rollback, with a file held open by PowerShell;
- case-only keys;
- `cite-alias`;
- `build-bib --out` with tricky paths.

## Verdict
**M1-ready, with follow-ups.** I found no blockers in this slice:
- Every FAIL from `papers.md` and `cli-build.md` is fixed in the code, and each has a test.
- Every medium item (M1, M2, M3, M4, M5, M7, M8) is fixed.
- `new-paper` never replaces an entry. I checked an unparsed entry, a key/folder mismatch and a case-only folder, by hand on Windows.
- A rename that fails part way is undone. I held a PDF open on Windows, and the `paper.toml` hashes were identical afterwards. The retry then lettered the papers in posting order.
- `build-bib --out` refuses every Loom-owned file, including through `LOOM.TOML`, `loom.toml.`, `./`, `TeX/..`, junctions and 8.3 names.
- No `loom` path writes Dakota's clicks without his typed `yes` at a terminal: `refute`, `add-check --by dakota`, `--outdate-checks`, `--level failed` on `new-proof` and `edit-proof`, and `new-paper`'s `verified`/`checked` dates, which it writes as `false`.
- No path loses the text he typed: `add-check` checks the proof first and gives his message back on any refusal or a non-`yes` answer.

The most important new finding is a medium follow-up. A lettered key that differs from the rule's key **only in case** is re-lettered, not re-cased. The usual cause is an author's surname corrected from `MacDonald` to `Macdonald`. This contradicts step 4 ("a lettered key never changes") and A3. I don't call it a blocker: nothing is lost, the old key stays as an alias, it takes an explicit `loom rekey`, and it needs a rare combination of a shared base and a case-only surname fix.

## Blockers
None.

## Original findings

Status is checked against the code on `main`, not the PR descriptions. "Deferred" is used only where COORDINATOR-NOTES or a fix PR's body defers the item.

### papers.md

| Entry | Status | Where (main) | Test |
| --- | --- | --- | --- |
| `ReadingEntry::lines` doc (NC low) | resolved | Papers.h:109-110 | n/a |
| `publishedFields` doc (NC low) | resolved | Papers.cpp:64-66 | n/a |
| `PaperReader::field`, `http://` source reported as `pdf-path` (NC low) | resolved | Papers.cpp:313-318 (`looksLikeLink`, `field-source`) | PapersTests "A source that is a link but not https:// says so" |
| `PaperReader::work`, `authors-missing` wording (NC low, S14) | deferred | PR #25 body ("Deferred past M1, as asked") | — |
| `PaperReader::version`: regex `$` (**F13**) and `arxiv_id` form (**F4**) | resolved | Papers.cpp:419-422 (`anchoredPattern`); `isArxivId` 163-180; `arxivForm` 502-512, called at 470-471 | PapersTests "A version id or session ending in a line break doesn't pass its rule", "arxiv_id is an arXiv identifier without its version (A4)", "primary_class is an arXiv category" |
| `PaperReader::readings`, session regex (**F13**) | resolved | Papers.cpp:546-547 | PapersTests "A version id or session ending in a line break…" |
| `PaperField::text`, empty `first` in BibTeX (**F5**) | resolved | Bibliography.cpp:14-20 (`bibNames`), used at 56-57 | BibliographyTests "a name whose first is empty is its surname in braces (A5)". The sub-point of a `last` containing ` and ` is still unbraced (low; see Follow-ups) |
| `readPaper` names a draft by its folder (NC low) | resolved | Papers.cpp:876-878 | PapersTests "A draft without a key is named by its file in messages" |
| `readStub`, empty `id`/`kind`/`extracted_by` (**F14**) | resolved | Papers.cpp:920-926 (`Need::Required` now rejects `""`, TomlFields.h:33-40); empty `at` 972-976 | PapersTests "Empty values in a stub are missing values", "A stub's extracted_by can't end in a line break". The sub-point of a whitespace-only `at = "  "` is still accepted (low) |
| `formatPaper` drops comments on rekey (NC low, S15) | deferred | PR #25 body | — |
| `arxivNumber` copied three times (3 × NC low) | resolved | `PaperVersion::arxivNumber`, Papers.cpp:743-748; used in PaperKeys.cpp:138, PaperEdits.cpp:45, Bibliography.cpp:102, 202 | indirect (key year, default cite, eprint tests) |
| `keyFitsBase`/step 4 (NC low) | resolved | `keyFollowsRule`, PaperKeys.cpp:185-202; used by ProjectContents.cpp:332 and PaperEdits.cpp:494 | PaperKeysTests "An unlettered key beside a lettered one… (step 4)"; PaperEditsTests "An unlettered key beside a lettered one of its base is warned about, and rekey letters it" |
| `assignKey` case-sensitive (**M3**) | resolved | PaperKeys.cpp:62-75 (`sameKey`, `letterAfter`), 217-274 | PaperKeysTests "Keys that differ only in case are the same key (A3)". By hand: `MacDonald24` + `Macdonald` draft gives `MacDonald24b`/`Macdonald24a`. **New, related issue:** see `keyFollowsRule`/`rekeyPaper` below |
| `carryOut` doesn't undo earlier renames (**M1**) | resolved | PaperEdits.cpp:118-177 (`DoneRename`, `undo`, `carryOut`) | PaperEditsTests "A rename that fails part way is undone, every rename before it too". Verified by hand on Windows with an open file |
| `commitPapers`: extra write after renames; no target check (**M1**) | resolved | PaperEdits.cpp:184-213 (undo on any failure; rmdir of an empty new folder); targets checked up front at 238-258 (`renameTargetTaken`) | Rename path covered by the test above, plus "rekey refuses before renaming anything when a folder is in the way". The failing-`extra`-write path has no test; PR #21 found no portable trigger |
| `createPaper` overwrites `paper.toml` (**F1**, high) | resolved | PaperEdits.cpp:387-393 (`entryInPapers`, any entry, ignoring case) | PaperEditsTests "new-paper never replaces a folder that is already there" (3 sections); PapersCli `paper-exists`. All three cases verified by hand on Windows |
| `rekeyPaper`: wrong comment; step 4 not repaired (NC low) | resolved | PaperEdits.cpp:494-503 | PaperEditsTests "rekey renames the folder of a key that follows the rule…", "…rekey letters it", "rekey refuses an entry it can't rewrite…" |
| `buildBibliography`: `--out` onto project files; no "adds no error" (**M2**) | resolved | PaperEdits.cpp:260-279 (`isProjectOwnFile`), 545-552; header corrected (PaperEdits.h); entries with errors skipped 561-570; CLI resolves links/8.3 (PaperCommands.cpp:50-58, 141-150) | PaperEditsTests "build-bib never writes over the project's own files", "build-bib skips the entry of a paper whose paper.toml has errors"; PapersCli junction and `STATEM~1`. The sub-point of the "Loom can't read" message for a parsed entry without `key` is unchanged (low) |
| `arxivNumber` in Bibliography (NC low) | resolved | see above | — |
| `EntryBuilder::eprint` relies on unchecked `arxiv_id` (NC low) | resolved | Reader validates (F4), and build-bib skips the entry | PaperEditsTests "build-bib skips the entry of a paper whose paper.toml has errors" |
| `escapeBibValue`: `\\%`, `$$` (NC low) | resolved | Bibliography.cpp:120-150 | BibliographyTests "a % after a LaTeX line break \\\\ is escaped", "display math between $$ pairs…" |
| `bracesBalanced` skips `\{` (**M4**) | resolved | Bibliography.cpp:160-171; `endsInEscape` 152-158 | BibliographyTests "a value with unbalanced braces skips its entry" (assertion changed, flagged in PR #25), "a value ending in a lone backslash skips its entry" |
| `bibEntry` lists `type` as unverified (NC low) | resolved | Bibliography.cpp:184-186 | BibliographyTests "an unverified type isn't listed…"; PapersCli:81 |
| `formatBibliography` files problems by key (NC low) | resolved | `BibSource`, Bibliography.h:53-59, .cpp:286-298. Codes outside the spec (S17): deferred (PR #25); since added to the spec by A17 | BibliographyTests "problems are filed under the paper's own paper.toml"; PaperEditsTests "build-bib files a skipped entry under the paper's own paper.toml" |
| PapersTests.cpp gaps (NC low) | resolved | — | CRLF, non-ASCII, multi-line summary, empty values, renumbering, non-tables, `http://`, `arxiv_id`, draft name all added. A BOM case isn't (minor) |
| PaperKeysTests.cpp gaps (NC low) | resolved | — | `z` exhausted, unlettered beside lettered, case-only, no year, U+2010/2011, lower-case letters |
| PaperEditsTests.cpp gaps (**M5**) | resolved | — | overwrite, rollback, rekey lossy, rekey folder ≠ key, new-paper lossy rename, `--out`, skipped entry, new-stub bad kind / two-line title |
| BibliographyTests.cpp gaps (NC low) | resolved | — | empty `first`, `\{`, trailing `\`, `\\%`, cite with no version, `type`, talk with no venue |
| Cross-boundary: `paper-key-duplicate` case-sensitive | resolved | ProjectContents.cpp:280-317 (case-folded) | PaperEditsTests "Keys and old keys that differ only in case are duplicates (A3)" |
| Cross-boundary: `paper-key-rule` repeats `authors-missing` (S18) | resolved | ProjectContents.cpp:325-331 | PaperEditsTests "A paper with no authors gets authors-missing, not also paper-key-rule"; EveryProblem line removed |

### cli-build.md

| Entry | Status | Where (main) | Test |
| --- | --- | --- | --- |
| `usageText` has no `help` line (NC low) | resolved | Cli.cpp:84-150 | loom.usage (UsageCli.cmake) |
| `usageError` JSON embeds the usage (NC low) | resolved | Cli.cpp:161-167 | StatementsCli usage JSON check |
| `findProject` doesn't check `loom.toml` (**M7**) | resolved | Cli.cpp:254-265 | ValidateCli install-hooks on a plain repo; other CLI `--project` empty-folder checks |
| `inputIsTerminal` under mintty (NC low) | deferred | COORDINATOR-NOTES ("mintty pipes… better as gotcha pages") | — |
| `readTextFile` chained `.arg` (NC low, P3) | resolved | Cli.cpp:352-354 | StatementsCli non-UTF-8 file with `%1` in its name |
| `printUsage`: unknown subcommand gives no message or JSON (**F6**) | resolved | main.cpp:32-36, 136-137, 183 | loom.usage (exit 2 exactly, JSON error) |
| `runNewProject` has its own parser (NC low) | resolved | main.cpp:60-92, 142-147 | NewProjectCli repeated `--name`, `--project` refused |
| `Utf8Console` after Ctrl+C (NC low) | deferred | COORDINATOR-NOTES | — |
| `main`: no `help`; common options stripped anywhere (**F7**) | resolved | main.cpp:44-53, 127-141; Cli.cpp:169-206 (`takeCommonOptions`) | loom.usage; StatementsCli `--title --json --body --project` |
| `editWithConfirmation` may outdate other proofs than he confirmed (NC low) | resolved | StatementCommands.cpp:50-72; Edits.cpp `commit` compares sets | EditsTests "Dakota's --outdate-checks covers only the proofs he confirmed" |
| `runFingerprint` duplicates `readProject` (NC low) | resolved | CheckCommands.cpp:71-94; Cli.cpp:404-421 | ChecksCli |
| `runAddCheck`: Dakota's typed text lost on refusal (**M8**) | resolved | CheckCommands.cpp:34-67 (`checkBeforeAsking`, `giveBack`), 130-162 | ChecksCli `add-check ZZZZ.a --by dakota` → `ref-unknown`, and `--verdict accept` → `check-verdict`, both before `dakota-only`. `giveBack` needs a terminal and is untested (Windows check in PR #23) |
| `runGraph` chained `.arg` (NC low) | resolved | StatusCommands.cpp:134-143 (one 10-argument `.arg`) | StatusCli level `"%1"` |
| `runRefute`: (1) exit 3 after a written refutation; (2) asks before resolving IDs (NC low) | **open** for (1), resolved for (2) | (2): StatusCommands.cpp:174-183 (`checkRefutation` before `requireTerminal`). (1): unchanged at 189-195. PR #23: "not reproduced, so not changed" | StatusCli unknown IDs, `refuted-self`, already refuted, stub |
| `runInstallHooks` on a non-project (**F8**) | resolved | ValidateCommands.cpp:77-86, plus core guards (Hooks.cpp, final-core's) | ValidateCli plain repo / below the top / `core.hooksPath`; HooksTests |
| LoomCli CMake: Qt DLLs not next to `loom.exe` (NC low) | **open** | Unchanged; spec line 76 still says "self-contained". Not on the deferral list | none. Windows check: `git commit` from a shell without Qt on PATH |
| `Tests/LoomAppTests` title/visibility (NC low) | deferred | COORDINATOR-NOTES (test gaps not guarding a FAIL) | — |
| `CMakePresets.json` test preset has no configuration (NC low) | resolved | CMakePresets.json:27 | PR #23 Windows check |
| `loom_check_no_ui_links` walker gaps (NC low) | **open** | `Source/CMake/` unchanged; not on the deferral list | none |
| `Tests/CMakeLists.txt` `WILL_FAIL` passes on a crash (NC low) | resolved | Tests/CMakeLists.txt:13-18 (`loom.usage`) | loom.usage |
| StatementsCli gaps (NC low) | resolved | — | `new-tag --json`, `--superseded-by`, add/remove tag, `--kind`, non-UTF-8 |
| ChecksCli gaps (NC low) | resolved | — | `--changes-file`, `--note`, agent `--changes`, `edit-statement --outdate-checks` |
| StatusCli: `refute` relies on stdin (NC low) | resolved | `INPUT_FILE` in `run_loom` | StatusCli. A real `rekey` rename from the CLI is still missing: a test gap, deferred |
| ValidateCli gaps (NC low) | resolved for the FAIL cases | install-hooks on a non-project / not the top / `core.hooksPath`; `%` in a file name | ValidateCli. Remaining gaps (a foreign hook renamed, lock contention): deferred as test gaps |
| CI `checks.yml` (NC low) | **open** | `.github/` unchanged (`ubuntu-latest`, no Release build); not on the deferral list | n/a |
| `update_indexes.py` `\|` in cells (NC low) | deferred | COORDINATOR-NOTES | — |
| `test_update_indexes.py` `main()` untested (NC low) | deferred | COORDINATOR-NOTES (test gaps); PR #27 "Not done" | — |

**Counts:** 57 rows: **resolved 46**, **deferred 7**, **open 4** (all low: `runRefute` exit 3, Qt DLLs, link-check walker, CI). By log:
- papers.md: 30 rows, 28 resolved and 2 deferred.
- cli-build.md: 27 rows, 18 resolved, 5 deferred and 4 open.

The three `arxivNumber` entries share one row. The resolved rows that kept a low sub-point are listed in Follow-ups.

## New code

Only functions changed since ee04f71 in my slice. Doc comments: every new or changed function and struct in these files has one. I found no personal paths; the only Windows path in a test is `C:/Users/someone/…`, a made-up example.

### PaperEdits.h / PaperEdits.cpp

#### `renameInModel` (Source/LoomCore/PaperEdits.cpp:65-103)
**Verdict:** PASS
**Spec:** The key rule, step 5; A17 (a case-only rename adds no old key and drops old keys equal to the new key)
**Tests:** covered ("rekey renames a paper whose key differs from the rule's only in case", including dropping `SMITHJONES24`).

#### `DoneRename`, `undo` (Source/LoomCore/PaperEdits.cpp:118-144)
**Verdict:** PASS
**Finding:** It undoes in reverse: the old `paper.toml` is written back, then the folder is renamed back. It keeps going past a failed step and names what to fix by hand. I checked it on Windows: with `Papers/SmithJones23/local/x.pdf` held open, `loom rekey SmithJones23` exits 3 with "Loom undid the renames…". Both folders and every file hash are as before, and the retry gives `SmithJones24a`/`SmithJones24b` in posting order.
**Tests:** covered.

#### `carryOut` (Source/LoomCore/PaperEdits.cpp:151-177)
**Verdict:** PASS
**Finding:** A case-only target is allowed (line 162). `done` is pushed right after the folder rename, so a failed read or write still undoes it. A failed `QSaveFile` write leaves the old file in place, so leaving `before` unset is correct.
**Tests:** covered (rollback, case-only rekey).

#### `commitPapers` (Source/LoomCore/PaperEdits.cpp:184-213)
**Verdict:** PASS
**Finding:** A failed `extra` write removes the new folder if it is empty, then undoes the renames, so a retry isn't refused as `paper-exists`.
**Tests:** the rename-failure path is covered. A failing `extra` write isn't, since there is no portable trigger.

#### `entryInPapers`, `paperExists` (Source/LoomCore/PaperEdits.cpp:219-236)
**Verdict:** PASS
**Spec:** `new-paper` (A3, `paper-exists`, the folder as on disk)
**Tests:** covered (three sections), plus PapersCli.

#### `renameTargetTaken` (Source/LoomCore/PaperEdits.cpp:243-258)
**Verdict:** PASS
**Finding:** It lists `Papers/` once and compares ignoring case. A folder that this rename or an earlier one moves away doesn't count. `earlier.from == entry` is an exact comparison, which is right, because `from` is the on-disk folder name.
**Tests:** covered ("rekey refuses before renaming anything when a folder is in the way", both sections).

#### `isProjectOwnFile` (Source/LoomCore/PaperEdits.cpp:265-279)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** `build-bib` row (A10): the list matches the spec exactly.
**Finding:** Two gaps, neither of which replaces a Loom file:
1. When `Statements/` (or `Proofs/`, `Papers/`) doesn't exist yet, `loom build-bib --out "Statements./refs.bib"` passes the guard. `weakly_canonical` can't resolve a folder that doesn't exist, and Windows strips the trailing dot when it creates the folder. So `Statements/refs.bib` is created, and `loom validate` then reports a `stray-file` error. I ran this on Windows: exit 0, then validate exit 1. Nothing is overwritten, since the folder didn't exist.
2. As the spec says, user files outside the listed folders aren't protected: `--out Records/review.md` or `--out TeX/main.tex` replaces them. The user typed that path, so this is a spec question, not a bug.
**Change:** (1) Reject a path segment that ends in `.` or a space (also useful for `isProjectPath` generally; foundations raised this earlier). (2) Optionally refuse an existing file that isn't `.bib`, as the original review suggested.
**Tests:** the listed files, junction and 8.3 cases are covered; the trailing-dot case isn't.

#### `withoutClicks` (Source/LoomCore/PaperEdits.cpp:287-320)
**Verdict:** PASS
**Spec:** `new-paper` (A16/S16)
**Tests:** covered ("new-paper writes the draft's verified and checked dates as false, and says which (S16)"; PapersCli `unclicked`).

#### `createPaper` (Source/LoomCore/PaperEdits.cpp:344-417)
**Verdict:** PASS
**Finding:** The existence check (387-393) comes before any rename is planned, and rename targets are checked before anything is written. The draft's `key` is compared with the rule's key exactly. A draft keyed `smithjones24` is refused with "by the key rule it is `SmithJones24`", which is acceptable. `withoutClicks` runs before formatting, so no click is written.
**Tests:** covered.

#### `rekeyPaper` (Source/LoomCore/PaperEdits.cpp:467-529)
**Verdict:** NEEDS-CHANGES
**Severity:** medium (follow-up)
**Spec:** The key rule, step 4: "After that, a lettered key never changes"; A3: keys are compared ignoring case "here and everywhere else"; step 5 / A17: a case-only rename keeps no old key.
**Finding:** `keyFollowsRule` is case-sensitive through `keyFitsBase`. So a **lettered** key that differs from the rule's key only in case doesn't "follow". `rekey` then calls `assignKey`, which ignores the paper's own key and hands out the next free letter. A case-only rename that keeps the letter would be right. I reproduced this on Windows:
1. `new-paper` two MacDonald 2024 drafts → `MacDonald24a` (earlier) and `MacDonald24b`.
2. Correct the surname in `Papers/MacDonald24a/paper.toml` from `MacDonald` to `Macdonald`.
3. `loom validate` warns: *"By the key rule this paper's key is `Macdonald24c`, not `MacDonald24a`. Run `loom rekey MacDonald24a`."*
4. `loom rekey MacDonald24a` gives `MacDonald24a → Macdonald24c`.

The earlier-posted paper now carries `c`, beside `b`, so the posting order is permanently wrong. The same happens to a hand-written `macdonald24a` (→ `Macdonald24d`, with 4 papers). Unlettered keys are fine (`smithjones24 → SmithJones24`).
**Change:** In `rekeyPaper`, and for the expected key in the `paper-key-rule` message, check first whether the key equals `base + letter` ignoring case. If it does and that letter isn't held by another paper, the expected key is `base.key + letter` (the rule's spelling, same letter), reached by a case-only rename. Only otherwise call `assignKey`. Equivalently, give `assignKey` the paper's current key and keep its letter when it matches ignoring case.
**Tests:** missing: rekey of a lettered key that differs only in case keeps its letter, and the matching validation message.

#### `buildBibliography` (Source/LoomCore/PaperEdits.cpp:531-584)
**Verdict:** PASS
**Finding:** The guard runs before anything is written. Entries with their own reading errors are skipped (A17). One cosmetic leftover from the original log: an entry that parses but has no `key` is reported as "Loom can't read …" (line 556-558).
**Tests:** covered.

### PaperKeys.h / PaperKeys.cpp

#### `sameKey`, `letterAfter` (Source/LoomCore/PaperKeys.cpp:62-75)
**Verdict:** PASS
**Tests:** covered.

#### `keyFollowsRule` (Source/LoomCore/PaperKeys.cpp:185-202)
**Verdict:** NEEDS-CHANGES
**Severity:** medium (follow-up; same root cause as `rekeyPaper`)
**Finding:** Its step-4 part is right and ignores case. Its shape part (`keyFitsBase`) is exact, which makes a case-only difference on a lettered key "not following", with the consequence above. `keyFitsBase` being exact is pinned by the test "Which keys fit a base" (`SmithJones24B` doesn't fit), and that is a fine *shape* rule. The fix belongs in how the expected key is computed, not necessarily here.
**Tests:** covered for step 4; the case-only lettered case is missing.

#### `assignKey` (Source/LoomCore/PaperKeys.cpp:217-274)
**Verdict:** PASS
**Spec:** Step 4, A3
**Finding:** Collisions and letters ignore case. The renamed paper keeps its own spelling. Running out of letters clears the renames. Verified by hand: `MacDonald24` + a `Macdonald` draft gives `Macdonald24a` and `MacDonald24b`; a third `MACDONALD` paper gives `MACDONALD24c`.
**Tests:** covered.

### Papers.h / Papers.cpp

#### `looksLikeLink`, `isArxivId`, `isArxivCategory`, `PaperReader::arxivForm` (Papers.cpp:148-191, 502-512)
**Verdict:** PASS
**Spec:** A4, A14
**Finding:** The regexes are anchored with `anchoredPattern`. The new-style ID year ranges match A4. Old-style IDs check only the month, as PR #25 says. `math.ap` is accepted (documented).
**Tests:** covered.

#### `PaperReader::field` / `work` / `version` / `readings` / `renumberings` changes (Papers.cpp:228-620)
**Verdict:** PASS
**Finding:** Year-0 dates go through `FieldChecker::date` and mark the reading lossy. Empty strings are `missing-field` and are kept for rewriting. Version and session regexes are anchored.
**Tests:** covered.

#### `PaperVersion::arxivNumber` (Papers.cpp:743-748)
**Verdict:** PASS

#### `oldKeysInAnotherCase`, `readPaper` (Papers.cpp:816-890)
**Verdict:** PASS
**Spec:** A3/A17 (`alias-duplicate`)
**Tests:** covered ("An old key can't be the paper's own key, or another old key, in another case (A3)"). Checked by hand too.

#### `readStub` (Papers.cpp:905-1000)
**Verdict:** PASS
**Finding:** Empty values are now reported. A whitespace-only `at` is still accepted (low, below).
**Tests:** covered.

### Bibliography.h / Bibliography.cpp

#### `bibNames`, `EntryBuilder::text`/`field` (Bibliography.cpp:14-58)
**Verdict:** PASS
**Spec:** A5
**Finding:** A `last` containing ` and ` with a non-empty `first` (`Johnson and Johnson, X`) still splits in BibTeX. The spec doesn't cover it (low).
**Tests:** covered.

#### `escapeBibValue`, `endsInEscape`, `bracesBalanced` (Bibliography.cpp:120-171)
**Verdict:** PASS
**Spec:** A6, A17
**Finding:** I traced `$$…$$`, `$a$$b$`, `A\\% B` and `\{`: all as the spec says.
**Tests:** covered.

#### `bibEntry`, `BibSource`, `formatBibliography` (Bibliography.cpp:174-310)
**Verdict:** PASS
**Tests:** covered.

### ProjectContents (paper parts)

#### `ContentsChecker::papers` (ProjectContents.cpp:278-350)
**Verdict:** NEEDS-CHANGES
**Severity:** medium (follow-up)
**Finding:** `paper-key-duplicate` is case-folded and correct. `paper-key-rule` skips papers without names (S18). But for a lettered key that differs only in case, the warning names a new letter (`Macdonald24c`) and tells Dakota to run `rekey`. It should name the same letter (`Macdonald24a`). See `rekeyPaper`.
**Tests:** covered except that case.

#### `loadPapers` (ProjectContents.cpp:508-565)
**Verdict:** PASS
**Tests:** covered ("A paper.toml Loom refuses to read is reported once, as bad-path").

#### `paperWithKey` (ProjectContents.cpp:585-599)
**Verdict:** PASS
**Spec:** A17 (an exact match first)
**Finding:** It looks at the folder first, then the key and old keys, in each case pass, as before.
**Tests:** covered ("A key typed in another case finds the paper (A3)"). By hand: `rekey SMITHJONES24`, `new-stub smithJONES24`.

### Validation (`cite-alias`)

#### `BodyChecker` index and `cite` handling (Validation.cpp:125-143, 186-203)
**Verdict:** PASS
**Spec:** A17
**Finding:** Verified by hand: `\cite{smithjones24}` gives "spells the key of SmithJones24 in another case". The message for an old key, "It still works", is true inside Loom. But `refs.bib` has no entry under an old key, so BibTeX won't resolve `\cite{SmithJones23}`. That is a spec-level point and not new (Follow-ups).
**Tests:** covered (ValidationTests).

### LoomCli

#### `jsonError`, `UsageEntry`, `usageEntries`, `commonOptionsLine`, `usageText`, `subcommandUsage`, `usageError` (Cli.cpp:72-167)
**Verdict:** PASS
**Spec:** A11
**Tests:** loom.usage.

#### `valuelessOptions`, `takeCommonOptions` (Cli.cpp:169-206)
**Verdict:** PASS
**Finding:** Every flag any `ArgSpec` declares (`--staged`, `--verified`, `--outdate-checks`) plus `--json` and `--version` is listed. One low point, which predates this change: `--project ""` (an unset shell variable) falls back to the project found from the current folder, rather than refusing.
**Tests:** covered.

#### `findProject`, `readProject` (Cli.cpp:254-277, 404-421)
**Verdict:** PASS
**Tests:** covered.

#### `unknownSubcommand`, `runHelp`, `runNewProject`, `main` (main.cpp)
**Verdict:** PASS
**Tests:** loom.usage, NewProjectCli.

#### `checkBeforeAsking`, `giveBack`, `runAddCheck` (CheckCommands.cpp:34-182)
**Verdict:** PASS
**Spec:** `add-check` *(Dakota)*; CLAUDE.md "Never lose user text"
**Finding:** I traced every exit after Dakota types: a non-`yes` answer, Ctrl+Z (EOF), a core refusal and a held lock all call `giveBack` when the message was typed at the terminal. Text given by `--changes`/`--changes-file` isn't echoed, but it isn't lost either. Agents can't add Dakota's checks: the core refuses `--by dakota` without `confirmed`.
**Tests:** pre-checks covered (ChecksCli). The prompt and `giveBack` need a terminal (Windows check).

#### `Confirmations`, `editWithConfirmation`, `runEditStatement`, `runNewProof`, `runEditProof` (StatementCommands.cpp:32-224)
**Verdict:** PASS
**Spec:** A16
**Finding:** The rules are checked first, then `failed`, then outdated checks. Each click needs `requireTerminal` and `askDakota`, and the confirmed list is passed to the core. Low, follow-up: `new-proof --level failed` asks to "create T.b", but if another proof of T is created between the two runs, the confirmed run creates T.c.
**Tests:** refusals without a terminal covered (StatementsCli, ChecksCli); EditsTests for the core.

#### `runGraph`, `runRefute` (StatusCommands.cpp:115-209)
**Verdict:** PASS for `runGraph`. NEEDS-CHANGES (low) for `runRefute`, part (1) of the original entry: after a successful write, a failed re-read exits 3.
**Tests:** StatusCli.

#### `tellUnclicked`, `resolvedPath`, `runNewPaper`, `runRekey`, `runBuildBib` (PaperCommands.cpp)
**Verdict:** PASS, with one cosmetic low in `runRekey`. A folder-only rename (key `SmithJones24` in folder `Foo`) prints `SmithJones24 → SmithJones24`, since `from` is the old key, not the folder. I verified this by hand.
**Tests:** PapersCli.

#### `runInstallHooks` (ValidateCommands.cpp:69-104)
**Verdict:** PASS
**Tests:** ValidateCli.

### Build and test infrastructure

#### `CMakePresets.json`, `Tests/CMakeLists.txt` (`loom.usage`), `Tests/CMake/UsageCli.cmake` and the other CLI scripts
**Verdict:** PASS
**Finding:** UsageCli checks exit codes exactly (2 and 0) and the JSON error shape. StatementsCli and StatusCli now feed a `yes` file, so input is never a terminal.

#### `FakeGit.cpp`, `TestSupport.h`, `ProjectFixture.h`
**Verdict:** PASS
**Finding:** The new modes (`head-fails`, `dubious`, `slow`) and `core.hooksPath` unset are documented. Doc comments were added to the helpers.

#### `Tools/`
**Verdict:** PASS (unchanged since ee04f71; its two NC items are deferred).

## Follow-ups
- **(medium)** A lettered key that differs from the rule's only in case is re-lettered by `rekey`, and `paper-key-rule` recommends it. It should be re-cased with the same letter (PaperEdits.cpp:494-503, ProjectContents.cpp:332-337; contradicts step 4 and A3). Reproduced: surname `MacDonald`→`Macdonald` turns `MacDonald24a` into `Macdonald24c`.
- (low) `build-bib --out "Statements./refs.bib"` creates `Statements/refs.bib` (a `stray-file` error) when `Statements/` doesn't exist yet. Reject segments ending in `.` or a space.
- (low) `build-bib --out` may replace user files outside the listed folders (`Records/`, `TeX/main.tex`). This is spec-silent; consider refusing an existing non-`.bib` file.
- (low) `runRefute` exits 3 if the re-read fails after the refutation was written (StatusCommands.cpp:192-195).
- (low) Qt's DLLs aren't deployed next to `loom.exe`, which the hook's fallback path relies on. The spec still says "self-contained". Windows check needed.
- (low) `loom_check_no_ui_links` doesn't follow targets inside generator expressions or catch raw `Qt6Widgets` names (unchanged).
- (low) CI: `repo` job on `ubuntu-latest`; no Windows Release build (unchanged).
- (low) A whitespace-only stub locator `at = "  "` is accepted.
- (low) A `last` containing ` and ` with a non-empty `first` isn't braced in BibTeX.
- (low) `build-bib` says "Loom can't read" for a parsed entry that has no `key`.
- (low) `loom rekey` prints `K → K` when only the folder was renamed.
- (low) `--project ""` falls back to the current folder's project rather than refusing.
- (low) The `new-proof --level failed` prompt names a proof ID that a concurrent `new-proof` could take, so the confirmed run creates the next letter.
- (spec) `cite-alias` says an old key "still works", but `refs.bib` writes no entry under old keys, so BibTeX can't resolve `\cite{OldKey}`. Either emit them (e.g. a `@misc{OldKey, crossref=…}` or biblatex `ids`) or reword the message.
- (tests) No CLI test of an actual `rekey` rename; no test of a failing `extra` write in `commitPapers`; `giveBack` and the confirmed prompts need a pty harness.
