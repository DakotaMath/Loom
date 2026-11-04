# M1 final review: final-core

**Reviewed:** `main` at 5626c88, against the M1 review at ee04f71 (`m1/review`) and fix PRs #21–#27, spec amendments A1–A17.
**Scope:** `Source/LoomCore/` except the paper and bibliography files: FrontMatter, TomlFields/FieldReader, ProjectToml, ProjectFiles, Ids, StatementsAndProofs, Checks, ProofChecks, Cycles, Status, Validation, ProjectContents (non-paper parts), Edits, EditSession, Hooks, NewProject, Git, Problem; plus their tests.
**Method:** I read the diff `ee04f71..main` for these files and every FAIL or NEEDS-CHANGES entry in `foundations.md`, `statements-proofs.md` and `status-validation.md` against the code on `main`. I ran `ctest --test-dir build/windows -C Debug`: **269/269 pass**. I also ran `loom.exe` in a scratch project (outside the repo) for: a year-0 check date, a whitespace-only title, `edit-proof --level failed` without a terminal, and `install-hooks` on a subfolder holding a `loom.toml`.

## Verdict
**M1-ready for the core.** Every FAIL in this slice is fixed in the code, and each fix has a named test:
- F2 (core side): `installHooks`
- F3, F9, F10: the readers
- F11: `isCheckAuthor`
- F12: `readTags`
- F15: `problemLine`
- F16: `checkAgainstCommit`

The medium items are fixed too:
- M6: `readCommit`
- `proofChecking`/`lacks`
- `tagsInUse`

The spec questions in this slice are settled in the code as amendments A1, A2, A7, A9, A12, A13 and A16 describe.

The new code adds no way to lose user text, write without a click, claim a status or corrupt a project file:
- Every new refusal comes before the write.
- The new `install-hooks` and `new-project` checks run before anything is written.
- `--outdate-checks` now covers exactly the set Dakota confirmed.
- Setting `failed` is now his click.

What is left is low severity: seven original items still open and a few small new gaps. One of the new gaps is a small departure from the spec (a year-0 date in a check history isn't reported, against A9's "in any file"). Users won't hit it, so I list it as a follow-up, not a blocker.

## Blockers
None.

## Original findings
Status counts below: **resolved 49, deferred 7, open 7** (63 entries: 27 foundations, 18 statements-proofs, 18 status-validation). "Deferred" means it's on COORDINATOR-NOTES' deferral list or a fix PR's body explicitly defers it (PR #22's table, PR #24's and PR #28's Noticed).

### foundations.md
| Entry | Status | Where (main) | Test |
| --- | --- | --- | --- |
| `normaliseTag` Unicode mapping (NC) | deferred | Coordinator deferral list (Unicode case mapping); PR #22 table | n/a |
| `normaliseProofId` Kelvin sign (NC) | deferred | as above | n/a |
| `inAlphabet`, `isLowerLetters` doc (NC) | resolved | Ids.cpp:15-34 | n/a |
| IdsTests gaps (NC) | resolved (Unicode cases deferred) | — | "A proof letter run too long to count is no number…", "An alias's length counts characters, not UTF-16 units" |
| `firstInvalidUtf8` past-the-end offset for a cut-off sequence (NC) | **open** (low) | FrontMatter.cpp:51 still `return i + k` | test still pins `== 3` |
| `crlfToLf` not idempotent on `\r\r\n` (NC) | deferred | PR #22 table "deferred past M1"; S3 deferred (PR #28) | n/a |
| FrontMatterTests gaps (NC) | resolved (`\r\r\n` deferred) | — | "A byte-order mark and CRLF together", "Empty front matter is found, and empty", "A bad byte in a CRLF file…" |
| `FieldReader.h` includes `StatementsAndProofs.h` (NC) | deferred | Coordinator deferral list | n/a |
| `FieldReader::list` has no `missingLine` (NC) | **open** (low) | FieldReader.cpp:73-80 still reports at line 1; harmless today | n/a |
| `FieldReader::date` year 0 (NC, part of F3) | resolved | FieldReader.cpp:106-127, `FieldChecker::date` TomlFields.cpp | "A date Qt can't hold is reported, and the file isn't rewritten" |
| `FieldChecker::stringField` duplicate rule (NC) | resolved | One rule, `FieldChecker::reportEmpty` (TomlFields.cpp), used by `stringField` and `FieldReader::string` | ProjectTomlTests section "empty values are missing values" |
| `escapeBasic` control characters untested (NC) | resolved | — | "The TOML writer escapes every control character and writes any offset" |
| `tomlOffsetDateTime` offsets untested (NC) | resolved | — | same test (`Z`, `+05:30`, `-06:00`) |
| **F12** `readTags` regex `$` | resolved | ProjectToml.cpp:101-102 `anchoredPattern` | ProjectTomlTests section "a tag ending in a line break" |
| `readProjectToml` skips UTF-8 check (NC) | resolved | ProjectToml.cpp:149-155 `parsePlainToml` | "loom.toml that isn't UTF-8" |
| ProjectTomlTests gaps (NC) | resolved | — | "A CRLF loom.toml reads as an LF one", "A preamble file through a link…", section "a loom.toml that is a folder can't be read" |
| `isProjectPath` reserved names, trailing dots (NC) | **open** (low, Windows-only, unverified) | ProjectFiles.cpp:48-58 unchanged | none |
| ProjectFilesTests dangling link (NC) | resolved | — | "A dangling link is refused, wherever it would have led" |
| `readKit` skips an unreadable resource (NC) | **open** (low) | NewProject.cpp:41-43 still `continue` | none |
| `createProject` (NC): identity from env vars; LFS under `core.hooksPath`; duplicated `whyGitFailed` | **open**, partly: the `core.hooksPath` part is resolved (NewProject.cpp:121-135, A1/A10); identity from `GIT_AUTHOR_*`/`includeIf` and the duplicated helper (NewProject.cpp:177-185) remain | — | "new-project stops before writing anything when git's global config sets core.hooksPath" |
| NewProjectTests gaps (NC) | **open** (low): `hooks-failed` and the quoted name are covered; identity from env vars isn't | — | "new-project says when install-hooks fails…", "new-project keeps a name with quotes…" |
| **F15** `problemLine` chained `.arg` | resolved | Problem.cpp:8-14, one `.arg` call | CTest `loom.validate` (ValidateCli.cmake:78-83, `Statements/Smith%2C.tex`) |
| `GitRun::started` false on a timeout (NC) | resolved | Git.h `timedOut`, Git.cpp:25-27, `gitDidntRunMessage/Code` | "A git that doesn't finish in time is told apart from a git that isn't there" |
| `runGit` may run a `git.exe` in the current folder (NC, Windows) | **open** (low, unverified) | Git.cpp unchanged | none |
| **F2** `installHooks` (core side) | resolved | Hooks.cpp:101-137: `loom.toml`, `--show-toplevel`, hook folder inside `--git-common-dir`, all before writing. `setPermissions` is still unchecked (PR #26 Noticed, not reproduced) | "install-hooks refuses a folder without loom.toml, writing nothing", "…isn't the top of its repository", "…when git runs hooks from outside the repository" (local and global sections) |
| HooksTests gaps (NC) | resolved | — | as above, plus "install-hooks stops at an existing hook it can't read" |
| `ProjectFixture` doc (NC) | resolved | ProjectFixture.h | n/a |

### statements-proofs.md
| Entry | Status | Where (main) | Test |
| --- | --- | --- | --- |
| `Writer` method docs (NC) | resolved | StatementsAndProofs.cpp:73-99 (nit: `text +=QStringLiteral` at :97) | n/a |
| **F3** `readStatement` empty values, year 0 | resolved | StatementsAndProofs.cpp:133-145 (`Need::Required`/`Naming`); `refuted_by = ""` is now `missing-field`. The review's side note "treat a whitespace-only `title` as empty" is not done: `title = "   "` validates clean (confirmed with loom.exe) | "Empty values are missing values" (sections statements, "a refutation by nothing is an error"); "A date Qt can't hold…" |
| **F9** `readProof` empty `id`/`proves`/`level` | resolved | StatementsAndProofs.cpp:195-200 | "Empty values are missing values" section proofs |
| **F10** `readVerboseProof` empty fields, fingerprint `\n` | resolved | StatementsAndProofs.cpp:237-252 (anchored regex; still a copy of `isFingerprint`) | section "verbose proofs"; "A value ending in a line break doesn't pass a rule" |
| Origin regex `$` (NC) | resolved | StatementsAndProofs.cpp:164-165 | "A value ending in a line break doesn't pass a rule" |
| StatementsAndProofsTests gaps (NC) | resolved | — | "Reference fields of the wrong type…", "A verbose proof's unknown field and broken TOML", "A statement's body keeps a lone CR…" |
| `editStatement` tests; `superseded_by` stub (NC) | resolved | Stub targets refused via `superseded-invalid` (A12) | EditsTests "Edits refuse a premise cycle, a stub, an empty title and a superseded_by that names nothing" |
| `createProof` refusal tests (NC) | resolved | — | same test, sections "a proof of a paper stub", "new-proof: a premise that closes a cycle", "a paper stub as a premise is fine" |
| `addCheck` refusal tests (NC) | resolved (clock skew S27 deferred) | — | "add-check refuses a proof without a fingerprint, a history it can't rewrite, and a stale fingerprint from Dakota" |
| `begin` stray doc line (NC) | resolved | EditSession.cpp:31 removed | n/a |
| EditsTests gaps (NC) | deferred, partly: most are added; a non-UTF-8 body and `addTag` on CRLF are still missing (PR #27 Noticed; the coordinator deferred test gaps that guard no FAIL) | — | — |
| **F11** `isCheckAuthor` `$` | resolved | Checks.cpp:170-175 | "A check's author can't end in a line break" |
| `mergeChecks` tests (NC) | resolved | — | ChecksTests sections "an undated entry is left for a person…", "a base entry one side edited comes back…" |
| ChecksTests gaps (NC) | resolved | — | sections "an element of checks that isn't a table", "dates with fractions…", "changes that are only spaces" |
| `proofChecking` drops the fingerprint problem (NC) | resolved | ProofChecks.cpp:51, `ProofChecking::fingerprintProblem` | StatusTests section "a fingerprint Loom can't compute" |
| ProofChecksTests gaps (NC) | resolved | — | the add-check test above |
| `findCycles` recursion depth; stale header (NC) | deferred (recursion, coordinator list). The stale header comment "premises in #7" (Cycles.h:3) is still there; trivial | — | — |
| CyclesTests gaps (NC) | resolved (long chain deferred) | — | "One group with two circles…", "A successor that isn't a key…", "The result doesn't depend on the order…", "Several circles, a self-loop…" |

### status-validation.md
| Entry | Status | Where (main) | Test |
| --- | --- | --- | --- |
| `tagsInUse` misses checks-only folders (NC) | resolved (reusing tags deleted since HEAD, S25, deferred) | ProjectContents.cpp:707-735, `ProjectContents::proofFolders` | "Tags in use: every statement, stub, alias and proof folder" |
| `resolveProof` doc (NC) | resolved | ProjectContents.h:152-154 | n/a |
| `ContentsChecker` constructor doc, "linear" claim (NC) | resolved (scans deferred) | ProjectContents.cpp:45-58 | n/a |
| `ContentsChecker::statements` stub target, empty values (NC) | resolved | ProjectContents.cpp:206-221 (A12); empty values via the readers | "superseded_by names a statement, not a paper stub" |
| `ContentsChecker::papers` step 4, no-authors, case (NC) | resolved (paper area; final-papers-cli owns the detail) | ProjectContents.cpp:280-340 | "An unlettered key beside a lettered one…", "Keys and old keys that differ only in case…", "A paper with no authors gets authors-missing…" |
| `loadPapers` duplicate `missing-field` (NC) | resolved (paper area) | ProjectContents.cpp `sawPaper` | "A paper.toml Loom refuses to read is reported once, as bad-path" |
| `StatementNode::paper` folder vs key (NC) | resolved | Status.cpp:452-470 | "A stub's paper is named by its key" |
| `Computation::isStub` doc (NC) | resolved | Status.cpp:91-93 | n/a |
| `computeProvedSet` deciding-proof tie-break vs spec (NC) | resolved by spec amendment A13 (code unchanged, now conforms) | — | StatusTests (J3RW.b) |
| `Computation::lacks` wrong reason (NC) | resolved | Status.cpp:233-242 | section "a fingerprint Loom can't compute" |
| `premiseCycles` scans; Status.h ↔ ProjectContents cycle (NC) | deferred (linear scans on the coordinator list). The include cycle (ProjectContents.cpp:7 includes Status.h) remains: layering only | — | — |
| `validateProject` temp dir (NC) | resolved | Validation.cpp:679-690 `std::optional<QTemporaryDir>` | "validate without --staged makes no temporary folder" |
| `BodyChecker::resolve` `\paperref` to a statement (NC) | resolved | Validation.cpp:152-175 (`paperref-not-stub`, A12) | "body references: \paperref names a paper stub" |
| **M6** `readCommit` hides git failures | resolved | Validation.cpp:421-446 (`--git-dir` with `LC_ALL=C`, then exit 1 and empty stderr = no commit; else `git-failed`) | "validate reports a git failure reading the last commit", "validate in a repository with no commit yet…" |
| **F16** `checkAgainstCommit` multiset match | resolved | Validation.cpp:624-666: greedy subsequence match, line taken from the current file | "committed checks keep their order, even with the same date" |
| ProjectContentsTests gaps (NC) | resolved | — | "Stubs share the tags and aliases…", "A folder inside a proof folder…", "IDs typed by a person…" |
| StatusTests gaps (NC) | resolved | — | "The deciding proof: `z` comes before `aa`", "An empty project…", "A proof filed under a definition or a stub…", section "refuted_on without refuted_by isn't false" |
| ValidationTests gaps (NC) | resolved | — | the tests named above, plus "body references: an old name in a verbose proof…" |

### Spec issues raised in these three logs
- **Resolved by an amendment:**
  - S1: A1
  - S2: A2
  - S6, S22, S26: A12
  - S7: A16
  - S19, S20: A13
  - S21: A7
- **Deferred:** S3, S8, S24, S25, S27 (PR #24 and PR #28 Noticed).
- **Still open (spec wording; final-spec's area, sent via main):**
  - S4, "shaped like a tag" (m1-core.md:274);
  - S5, new-project step 1 still creates the folder before step 2's checks (m1-core.md:74-75).

## New code
The changed functions in my slice. A PASS entry is one line.

### TomlFields / FieldReader
- `Need` (TomlFields.h:31-40): **PASS**. Spec: Front matter, A2/A8.
- `FieldChecker::emptyField`, `reportEmpty` (TomlFields.cpp:57-73): **PASS**. One rule for empty values; it cites A2.
- `FieldChecker::date` (TomlFields.cpp:80-90): **PASS**. Spec: A9. The caller must pass a date node, as documented.
- `FieldChecker::stringField` (TomlFields.cpp:92-109): **PASS**.
- `FieldReader::string` (FieldReader.cpp:51-71): **PASS**. An empty value is not `lost`. I traced the cases:
  - A rewrite of an empty required field turns "is empty" into "is missing", so `addedErrors` refuses it.
  - An empty optional field is dropped silently, which loses nothing.
  - `refuted_by = ""` with `refuted_on` set: the rewrite gives `refuted-incomplete`, so it is refused.
- `FieldReader::date` (FieldReader.cpp:106-127): **PASS**. Spec: A2, A9.

### StatementsAndProofs
- `Writer` (StatementsAndProofs.cpp:73-99): **PASS**. Nit: `text +=QStringLiteral` at :97 is missing a space.
- `readStatement` (:118-175): **PASS**. Follow-up: `title = "   "` passes, since the spec says "Non-empty string" and is silent on blanks.
- `readProof` (:177-222): **PASS**.
- `readVerboseProof` (:224-256): **PASS**. The fingerprint regex duplicates `isFingerprint()`; cosmetic.

### Checks
- `EntryReader::string` (Checks.cpp:32-63): **PASS**. `changes` stays free text, the rest follow A2. An agent check with `record = ""` gets both `missing-field` and `check-record-missing`, and an empty `changes` on Dakota's needs-changes gets both `wrong-type` and `check-changes-missing`. That is noise, not a bug.
- `EntryReader::date` (Checks.cpp:66-90): **NEEDS-CHANGES**
  - **Severity:** low
  - **Spec:** A9 ("Such a date, in any file, is `bad-value`", m1-core.md:183).
  - **Finding:** an offset date-time in year 0 (`date = 0000-01-01T10:00:00Z`) builds an invalid QDateTime with no problem reported and without setting `lost`. I confirmed it with loom.exe. The entry: agent pass, matching fingerprint, year-0 date. It was followed by Dakota's pass dated 2026. `loom validate` gave 0 errors, and `loom status` showed the proof as checked, since an invalid QDateTime is "older" than any valid one. `check-order` skips invalid dates. Later, `merge-checks` refuses the file as undated.
  - **Change:** after the type check, build the QDate. If it is invalid, report `bad-value` with A9's message and set `lost`, as `FieldChecker::date` does.
  - **Tests:** missing; add a year-0 case to "Empty values in a check history are missing values" or a new test case.
- `isCheckAuthor` (Checks.cpp:170-175): **PASS**.

### ProjectToml
- `readTags` (ProjectToml.cpp:89-140): **PASS**.
- `readProjectToml` (ProjectToml.cpp:144-215): **PASS**. `created` uses `FieldChecker::date` and `reportEmpty`.

### ProofChecks / Status
- `ProofChecking`, `proofChecking` (ProofChecks.h:26-41, ProofChecks.cpp:47-56): **PASS**.
- `Computation::isStub` (Status.cpp:91-93): **PASS**. A stub's `paper` is never empty (key, else folder).
- `Computation::premiseState` (Status.cpp:212-226): **PASS**. Its only caller (:310) was updated to "which %2".
- `Computation::lacks` (Status.cpp:228-250): **PASS**. Spec: Tie-breaks and explanations, A13; the doc cites it.
- `buildGraph` stub `paper` (Status.cpp:452-470): **PASS**. Status stays a pure function of the files; there is no setter.

### ProjectContents (non-paper)
- `ContentsChecker` constructor (ProjectContents.cpp:45-75): **PASS**.
- `ContentsChecker::statements` (ProjectContents.cpp:190-245): **PASS**. `superseded-invalid` for a stub and `refuted-self` (A12) are checked. `refuted-self` runs only when `refuted_by` names a real proof, so it doesn't double up with `refuted-unknown`. Status still treats such a statement as false; validation blocks the commit.
- `loadContents` `proofFolders` (ProjectContents.cpp:~652): **PASS**.
- `tagsInUse` (ProjectContents.cpp:707-735): **PASS**. Spec: IDs, Generation.

### Edits
- `dakotaOnly` (Edits.cpp:39-42): **PASS**.
- `commit` (Edits.cpp:56-107): **PASS**.
  - Spec: Statements, A16.
  - Refusals come in the order rules, `failed`, outdated checks, then a different confirmed set.
  - A confirmed set that differs in either direction is refused before `writeProjectFile`.
- `paperKeyOf` (Edits.cpp:109-119): **PASS**. An unparsed entry has an empty key, so it falls back to the folder.
- `editStatement` (Edits.cpp:274-357): **PASS**. Takes the confirmed list.
- `createProof` (Edits.cpp:359-392): **PASS**. A16: `new-proof --level failed` needs `failedConfirmed`.
- `editProof` (Edits.cpp:394-451): **PASS**. Only *setting* `failed` needs the click, as A16 says. Un-failing is free; PR #28 asks Dakota to confirm that.
- `checkRefutation` (Edits.cpp:547-579): **PASS**. It has no side effects, and `refuteStatement` runs it again under the lock.
- `refuteStatement` (Edits.cpp:581-611): **PASS**. `*resolveProof` is safe, because `checkRefutation` resolved it on the same contents.
- `ChangeOptions`/`StatementEdit`/`NewProof`/`ProofEdit` (Edits.h): **PASS**. Documented.

### Git / Hooks / NewProject
- `GitTool::timeoutMs`, `GitRun::timedOut`, `runGit` (Git.h, Git.cpp:7-38): **PASS**.
- `gitDidntRunMessage`, `gitDidntRunCode` (Git.cpp:40-52): **PASS**.
- `whyGitFailed` (Hooks.cpp:38-49): **PASS**.
- `resolvedPath`, `fileNameCase` (Hooks.cpp:51-73): **PASS**. They resolve links, junctions and 8.3 names in the existing part, and compare case-insensitively on Windows only.
- `hooksPathOutsideMessage` (Hooks.cpp:76-82): **PASS**.
- `installHooks` (Hooks.cpp:101-180): **PASS**.
  - All three A1 checks run before any write. I confirmed `hooks-not-top-level` (exit 3) with loom.exe.
  - Follow-ups:
    - (a) a timeout in the `--git-path`/`--git-common-dir` calls is coded `git-failed`, not `git-timeout` (A16);
    - (b) the core doesn't refuse `FormatTooNew` (the CLI does);
    - (c) `setPermissions` is unchecked (carried over).
- `createProject` (NewProject.cpp:64-200): **PASS**.
  - The `core.hooksPath` check reads global and system config and runs before the folder is created. Timeouts in the up-front checks are `git-timeout`.
  - Follow-up: for a *system* `core.hooksPath` the message's `--global --unset` is wrong (flagged in PR #28, A10).

### Validation
- `BodyChecker` constructor and indexes (Validation.cpp:75-145): **PASS**.
- `BodyChecker::resolve` (Validation.cpp:152-175): **PASS**. Spec: A12 `paperref-not-stub`, `ref-dangling` for `\paperref`.
- `BodyChecker::body` `\cite` case warning (Validation.cpp:183-203): **PASS**. Paper area, A17.
- `gitDidntRun`, `whyGitFailed`, `copyIndex` (Validation.cpp:342-400): **PASS**.
- `commitUnreadable` (Validation.cpp:404-408): **PASS**.
- `readCommit` (Validation.cpp:410-466): **PASS**.
  - The doc states the rule and cites Checks rule 1.
  - Windows check (already in PR #24): under a non-English Git for Windows, `LC_ALL=C` must still give "not a git repository".
  - A repository git refuses as "dubious ownership" (network or exFAT drives) now makes plain `loom validate` exit 3. That is per A13; a hint about `safe.directory` in the message would help.
- `checkAgainstCommit` (Validation.cpp:604-670): **PASS**.
  - Spec: rule 1, A7. Greedy in-order matching succeeds exactly when the committed list is a subsequence.
  - A correct merge (ours first, stable sort by date) always passes.
  - `headerLine` indexes `lines` aligned with `checks` (Checks.cpp:150-151), and is never called when the file is gone.
- `validateProject` (Validation.cpp:675-690): **PASS**.

### Hard rules
- **LoomCore has no UI code:** no QtWidgets or QtGui includes in `Source/LoomCore`.
- **Doc comments:** every new function and struct in my slice has one.
- **Status and fingerprint code cites the spec:** `lacks`, `readCommit`, `checkAgainstCommit`, `reportEmpty` and `commit` all do.
- **No personal paths:** the only path in the diff is a made-up `C:/Users/someone/Downloads/draft.toml` in PapersTests.

## Cross-boundary notes
- **Sent to main for final-spec:**
  - the year-0 check date against A9;
  - S4 and S5 still unanswered in the spec;
  - A16's `git-timeout` not used for every hung git command.

## Follow-ups (all low; none blocks M1)
- `EntryReader::date` (Checks.cpp:66-90): report a year-0 check date as `bad-value` and set `lost` (A9, "in any file"); add a test.
- `readStatement`: treat a whitespace-only `title` as empty (spec says "Non-empty", silent on blanks).
- `firstInvalidUtf8` (FrontMatter.cpp:51): report the lead byte for a cut-off sequence; the test pins the old offset, so it needs a ⚠️ Flag.
- `FieldReader::list`: add `missingLine`, as the other readers have.
- `isProjectPath`: refuse Windows reserved names and segments ending in `.` or space (needs a Windows check).
- `readKit`: fail with `kit-missing` if any listed resource can't be read.
- `runGit`: resolve `git` with `QStandardPaths::findExecutable` (Windows current-folder lookup).
- `createProject`: accept identity from `GIT_AUTHOR_*`/`GIT_COMMITTER_*`, or document that only config counts; share `whyGitFailed` with Hooks.cpp.
- `createProject`: give a *system* `core.hooksPath` its own message (A10, PR #28).
- `installHooks`:
  - code a timeout in `--git-path`/`--git-common-dir` as `git-timeout` (A16);
  - refuse `FormatTooNew` in the core, not only in the CLI;
  - check `setPermissions` on non-Windows.
- Cycles.h:3: drop "premises in #7".
- StatementsAndProofs.cpp:97: `text +=QStringLiteral`, missing space.
- `readVerboseProof`: use `isFingerprint()` instead of its own regex.
- `EntryReader`: avoid double reports: `record = ""` on an agent check gives `missing-field` and `check-record-missing`, and an empty `changes` on Dakota's needs-changes gives `wrong-type` and `check-changes-missing`.
- `readCommit` `git-failed` message: hint at `git config --global --add safe.directory` for "dubious ownership".
- EditsTests: a non-UTF-8 body refused; `addTag` on a CRLF `loom.toml`.
- Spec wording (final-spec): S4 "shaped like a tag"; S5 new-project step order.
