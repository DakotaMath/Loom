# M1 review: cli-build

**Scope:** `Source/LoomCli/` (all files), `Source/LoomApp/`, `Source/CMake/`, `Source/LoomCore/CMakeLists.txt`, `CMakeLists.txt`, `CMakePresets.json`, `Tests/CMakeLists.txt`, `Tests/LoomCoreTests/CMakeLists.txt`, `Tests/LoomAppTests/` (the LoomApp stub's test), `Tests/CMake/` (CLI tests, CoreLinkCheck), `Tests/Projects/EveryProblem` and its expected output, `.github/workflows/checks.yml`, `Tools/`, `Templates/`, `pytest.ini`
**Summary:** 86 entries: 59 PASS, 24 NEEDS-CHANGES, 3 FAIL. The findings that matter most:
1. `loom help [subcommand]` doesn't exist: `loom help` prints usage and exits 2 (main.cpp:110-174).
2. `loom install-hooks --project <a git repo without loom.toml>` installs Loom's hook there and renames the user's own hook. If the folder sits inside a bigger repo, the hook goes into that repo and refuses every commit (ValidateCommands.cpp:69-106).
3. An unknown subcommand gives no message and no `{"error": …}` with `--json` (main.cpp:34-38, 173).
4. With `add-check --by dakota`, the `changes` text Dakota types is lost if the core then refuses the check (CheckCommands.cpp:97-116).

Hard rule 2 holds: `LoomCore` links Qt6::Core and toml++ only, the configure-time check enforces it, and no Gui or Widgets header is included. Every other subcommand and flag in the spec's table exists, with the specified output and exit codes. Only the Dakota-marked paths write on Dakota's behalf, and only after a terminal check and a typed `yes`.

## Cli.h / Cli.cpp

### `Context`, `ArgSpec`, `Args`, exit-code constants (Source/LoomCli/Cli.h:22-82)
**Verdict:** PASS
**Spec:** The `loom` command, exit codes
**Tests:** covered through every CLI script.

### `write` (Source/LoomCli/Cli.cpp:24-29)
**Verdict:** PASS
**Tests:** covered indirectly (UTF-8 output checked in ValidateCli with `→`).

### `jsonString` (Source/LoomCli/Cli.cpp:31-45)
**Verdict:** PASS
**Tests:** covered (`--json` path escaping in NewProjectCli).

### `jsonStringArray`, `jsonOptional`, `jsonIndex` (Source/LoomCli/Cli.cpp:47-63)
**Verdict:** PASS
**Tests:** covered (StatusCli, ChecksCli JSON shapes).

### `fail` (Source/LoomCli/Cli.cpp:65-71)
**Verdict:** PASS
**Spec:** The `loom` command: "With `--json`, a failure is `{"error": …}` on stdout as well"
**Tests:** covered.

### `usageText` (Source/LoomCli/Cli.cpp:73-108)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The `loom` command, `help [subcommand]`
**Finding:** The usage text has no `loom help [<subcommand>]` line, since the subcommand doesn't exist (see `main`). It shows `[--json]` on two lines only, then says every subcommand takes it.
**Change:** Add the `help` line when `help` is built. Make per-subcommand usage possible, which `help <subcommand>` needs.
**Tests:** not checked anywhere.

### `usageError` (Source/LoomCli/Cli.cpp:110-113)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** With `--json`, the error object's `message` holds the problem *plus the whole usage text*. For example, `loom new-statement --json` gives a roughly 1.5 kB message, which is unhelpful for a program reading `error.message`.
**Change:** Put only the problem in the JSON `message`, and print the usage text to stderr only.
**Tests:** usage exit codes covered; the JSON message isn't checked.

### `Args::value` (Source/LoomCli/Cli.cpp:115-120)
**Verdict:** PASS
**Tests:** covered.

### `parseArgs` (Source/LoomCli/Cli.cpp:122-159)
**Verdict:** PASS
**Tests:** covered: unknown option, missing value, extra positional, repeated single option (StatementsCli, ChecksCli, StatusCli).

### `findProject` (Source/LoomCli/Cli.cpp:161-176)
**Verdict:** NEEDS-CHANGES
**Severity:** medium
**Spec:** The project folder; exit code 3 "no project found"
**Finding:** With `--project <folder>`, it returns the folder without checking that `<folder>/loom.toml` exists, so every caller must catch "not a project" itself. Callers that go through `openProject`/the Edits API do. `install-hooks` doesn't, which is the FAIL below.
**Change:** Return nothing with the "no Loom project" message when `--project` names a folder without `loom.toml`. One check here covers every subcommand.
**Tests:** `--project` to an empty folder is covered for status, validate and new-statement; not for install-hooks.

### `listed` (Source/LoomCli/Cli.cpp:178-183)
**Verdict:** PASS
**Tests:** covered indirectly (outdated-checks message).

### `inputIsTerminal` (Source/LoomCli/Cli.cpp:185-194)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Decisions, "Dakota's clicks in the `loom` command"
**Finding:** On Windows, `GetConsoleMode` correctly rejects `NUL`, pipes and files (Wiki/gotchas/windows/isatty-true-for-nul.md). The same gotcha page says Git Bash's mintty gives programs pipes, so Dakota running `loom add-check --by dakota` in Git Bash would be told "Only Dakota can …, and only at a terminal". This is still unchecked.
**Change:** Have Dakota try it in mintty. If it refuses there, say so in the refusal message ("use Windows Terminal, PowerShell or cmd") and in the gotcha page.
**Tests:** the refusal is covered (ChecksCli, StatusCli). The terminal (true) branch is never tested. **Windows check** needed.

### `readTerminalLine` (Source/LoomCli/Cli.cpp:196-221)
**Verdict:** PASS
**Tests:** not covered (needs a console or pty). A Linux test under `script -qc` would cover the POSIX branch.

### `requireTerminal`, `askDakota` (Source/LoomCli/Cli.cpp:223-239)
**Verdict:** PASS
**Spec:** The `loom` command, "Dakota's clicks": wording matches exactly
**Tests:** the refusal is covered. The `yes` and non-`yes` answers aren't (same pty gap).

### `readTextFile` (Source/LoomCli/Cli.cpp:241-259)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** The not-UTF-8 message chains `.arg(path).arg(offset)`. A path containing `%1` (`C:\x%1y.tex`) is substituted first, and the second `.arg` then replaces the `%1` inside the path, not `%2`. The same chained-`.arg` pattern appears in StatusCommands.cpp:159-164 (see `runGraph`) and PaperCommands.cpp:125-129.
**Change:** Use the multi-argument form `.arg(a, b)`, with strings throughout, which substitutes in one pass.
**Tests:** unreadable file covered (exit 3); not-UTF-8 `--body-file` isn't.

### `readBody` (Source/LoomCli/Cli.cpp:261-270)
**Verdict:** PASS
**Tests:** covered (`--body`, `--body-file` with CRLF).

### `report` (Source/LoomCli/Cli.cpp:272-284)
**Verdict:** PASS
**Tests:** covered.

### `start` (Source/LoomCli/Cli.cpp:286-301)
**Verdict:** PASS
**Tests:** covered.

### `missing` (Source/LoomCli/Cli.cpp:303-310)
**Verdict:** PASS
**Tests:** covered.

## main.cpp

### `printUsage` (Source/LoomCli/main.cpp:34-38)
**Verdict:** FAIL
**Severity:** medium
**Spec:** The `loom` command: "With `--json`, a failure is `{"error": {"code": "…", "message": "…"}}` on stdout as well"; exit 2 "unknown subcommand"
**Finding:** `main` calls this for an unknown subcommand (line 173) and for no arguments (line 134). It writes only the usage text to stderr: no "`frobnicate` isn't a subcommand" line, and with `--json` nothing on stdout. Trace: `loom frobnicate --json` → `--json` removed → subcommand `frobnicate` → no match → `printUsage()` → stdout empty, exit 2. A program reading the JSON gets an empty stdout.
**Change:** Replace with `usageError(QStringLiteral("`%1` isn't a loom subcommand.").arg(subcommand), json)`. For no arguments, use `usageError("loom needs a subcommand.", json)`.
**Tests:** missing: unknown subcommand with and without `--json`.

### `runNewProject` (Source/LoomCli/main.cpp:45-86)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The project folder, `loom new-project`
**Finding:** It has its own argument loop instead of `parseArgs`. As a result `--name A --name B` silently keeps `B` (every other subcommand says "can be given only once"), and `--project X` is silently dropped by `main` (`loom new-project --project X --name N` then says it needs a folder, not that `--project` doesn't apply).
**Change:** Use `parseArgs(args, "new-project", {1, {"--name", "--idea"}, {}, {}}, …)` and `missing(…, {"--name"})`. Have `main` reject `--project` for `new-project`.
**Tests:** exit codes 0–3 and the JSON shape covered (NewProjectCli). Repeated `--name` isn't.

### `Utf8Console` (Source/LoomCli/main.cpp:91-100)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** It restores the console's output code page in its destructor. Ctrl+C at Dakota's prompt ends the process through the console control handler, which doesn't unwind `main`'s frame, so the console the user typed into is left at UTF-8 (65001). Unverified on Windows. The effect is mostly harmless, but it contradicts the doc comment.
**Change:** Also restore in a `SetConsoleCtrlHandler` handler, or reword the doc comment to say it isn't restored on Ctrl+C.
**Tests:** none (Windows console only). **Windows check.**

### `main` (Source/LoomCli/main.cpp:110-174)
**Verdict:** FAIL
**Severity:** medium
**Spec:** The `loom` command, table row `help` | `[subcommand]` | Usage | Usage text; exit 0 "Done"
**Finding:** (1) `help` isn't a subcommand. `loom help` and `loom help status` fall through to `printUsage()` and exit **2**, but asking for help is not wrong usage. (2) `--json` and `--project` are stripped from anywhere in the argument list before the subcommand parses its own options, so they're taken even when they're another option's value. `loom new-statement --kind lemma --title --json --body x` turns on JSON and then fails with "`--title` … needs a value" or similar, instead of using the title `--json`. `--project` as a value, as in `--body --project`, is taken as the project option. This is rare, but it is the only place where option values are not taken literally.
**Change:** (1) Add `help [<subcommand>]`: print usage, or that subcommand's usage, to stdout and exit 0. With an unknown subcommand, exit 2. (2) Strip `--json`/`--project` only where they stand as options: scan left to right and skip the value of any option that takes one. Or let `parseArgs` handle the common options.
**Tests:** missing: `loom help`, `loom help <subcommand>`, exit 0.

## StatementCommands.h / StatementCommands.cpp

### Declarations (Source/LoomCli/StatementCommands.h:1-33)
**Verdict:** PASS

### `oneBody` (Source/LoomCli/StatementCommands.cpp:10-17)
**Verdict:** PASS
**Tests:** covered (both and neither given).

### `reportEdit` (Source/LoomCli/StatementCommands.cpp:20-30)
**Verdict:** PASS
**Spec:** `edit-statement` prints "the changed fields; with `--json`, also `outdated_checks`"
**Tests:** covered.

### `editWithConfirmation` (Source/LoomCli/StatementCommands.cpp:38-51)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The `loom` command, `--outdate-checks` *(Dakota)*
**Finding:** Dakota confirms "outdating the checks of X, Y", computed by the first run. The second run, `edit(true)`, recomputes and outdates whatever the project needs by then. If another `loom` command changed the project between the two runs (the lock is released in between), Dakota's `yes` covers a different list than the one he was shown. The doc comment calls this a feature, but the confirmation is meant to be for exactly what he saw.
**Change:** Pass the confirmed list to the core, which refuses if the recomputed list differs. Or hold the lock across both runs.
**Tests:** the refusal without a terminal is covered (ChecksCli). The confirmed path isn't.

### `reportNew` (Source/LoomCli/StatementCommands.cpp:54-58)
**Verdict:** PASS
**Tests:** covered (`{"id", "path"}`).

### `runNewTag` (Source/LoomCli/StatementCommands.cpp:62-75)
**Verdict:** PASS
**Tests:** text output and refusal covered. `--json` (`{"id"}`) isn't checked.

### `runNewStatement` (Source/LoomCli/StatementCommands.cpp:77-103)
**Verdict:** PASS
**Tests:** covered (0, 1, 2, 3; CRLF body file; forgiving IDs).

### `runEditStatement` (Source/LoomCli/StatementCommands.cpp:105-138)
**Verdict:** PASS
**Tests:** `--title`, `--verified` and the refused body edit are covered. `--superseded-by`, `--add-tag`/`--remove-tag`, `--kind` and `--outdate-checks` on a statement aren't exercised from the CLI.

### `runNewProof` (Source/LoomCli/StatementCommands.cpp:140-163)
**Verdict:** PASS
**Tests:** covered.

### `runEditProof` (Source/LoomCli/StatementCommands.cpp:165-192)
**Verdict:** PASS
**Tests:** covered (level, no-op, refused body, `--outdate-checks` refusal).

## CheckCommands.h / CheckCommands.cpp

### Declarations (Source/LoomCli/CheckCommands.h:1-31)
**Verdict:** PASS

### `jsonCheck` (Source/LoomCli/CheckCommands.cpp:19-25)
**Verdict:** PASS
**Tests:** covered.

### `runFingerprint` (Source/LoomCli/CheckCommands.cpp:29-61)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** Lines 39-49 repeat `readProject` from StatusCommands.cpp:18-35 almost word for word: open, a switch on four statuses, load contents. It also parses and finds the project by hand instead of calling `start()`. This duplicated logic will drift.
**Change:** Move `readProject` into Cli.cpp and use it and `start()` here.
**Tests:** covered (text, JSON, unknown proof, no project).

### `runAddCheck` (Source/LoomCli/CheckCommands.cpp:63-135)
**Verdict:** NEEDS-CHANGES
**Severity:** medium
**Spec:** "Adding a check"; The `loom` command, `add-check` ("with `needs-changes` and no `--changes`, it asks for the message at the terminal"); CLAUDE.md "Never lose user text"
**Finding:** For `--by dakota --verdict needs-changes`, Dakota types the changes message (lines 101-110) and confirms with `yes`. Only then does `loom::addCheck` resolve the proof and validate anything. Scenario: `loom add-check 4kx7.b --by dakota --verdict needs-changes`, where `4KX7.b` doesn't exist (a typo for `.a`), or the project lock is held. Dakota types a paragraph, types `yes`, and gets "`4kx7.b` isn't a proof" (exit 1). The text is gone, because it was never echoed or saved. Also, lines 71-74 re-implement `missing()`.
**Change:** Before prompting, open the project and resolve the proof ID (as `runFingerprint` does) so typos fail first. On any refusal after the prompt, print the typed `changes` back on stderr ("Your message, so you can paste it again: …").
**Tests:** agent checks, JSON, fingerprint mismatch and Dakota's refusal without a terminal are covered. `--changes-file`, `--note` and the terminal prompt aren't.

### `runMergeChecks` (Source/LoomCli/CheckCommands.cpp:137-166)
**Verdict:** PASS
**Spec:** Checks, "Merging two check lists": exit 1 on a parse failure, result into ours, the restore message verbatim
**Tests:** covered (merge, restore, parse failure leaving ours, missing file, usage).

## StatusCommands.h / StatusCommands.cpp

### Declarations (Source/LoomCli/StatusCommands.h:1-30)
**Verdict:** PASS

### `readProject` (Source/LoomCli/StatusCommands.cpp:18-35)
**Verdict:** PASS
**Tests:** covered (no project → 3).

### `jsonStatus`, `jsonBool` (Source/LoomCli/StatusCommands.cpp:38-47)
**Verdict:** PASS

### `jsonStatement` (Source/LoomCli/StatusCommands.cpp:50-57)
**Verdict:** PASS
**Spec:** JSON shapes, `loom status --json`
**Tests:** covered (keys and order checked for E8TB, N7ZE, 2M9Q).

### `jsonCounts`, `textCounts` (Source/LoomCli/StatusCommands.cpp:60-85)
**Verdict:** PASS
**Tests:** covered (the worked example's counts).

### `runStatus` (Source/LoomCli/StatusCommands.cpp:89-134)
**Verdict:** PASS
**Tests:** covered (list, one ID typed in lower case, JSON, unknown ID, usage, no project).

### `runGraph` (Source/LoomCli/StatusCommands.cpp:136-181)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** JSON shapes, `loom graph --json`
**Finding:** Each proof object is built with a 9-argument `.arg(...)` followed by `.arg(jsonIndex(countingDakota))` for `%10` (lines 159-164). The second call replaces the *lowest-numbered placeholder left in the whole string*, including any inside values substituted by the first call. `ProofNode::level` is "as its file says" (Status.h:61). A hand-edited proof with `level = "%1"` (a `bad-value` error that `graph` still reports) yields `"level": "null"` or similar, and a literal `%10`, which is invalid JSON. Unverified whether a proof with an invalid level reaches `graph.proofs`.
**Change:** Build the object with `QString` concatenation, or split it so that no `.arg` runs after user text has been substituted.
**Tests:** shape covered on the worked example. No malformed-file case.

### `runRefute` (Source/LoomCli/StatusCommands.cpp:183-215)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Exit codes ("3 … Can't work here"; "1 … Nothing was written")
**Finding:** (1) After `refuteStatement` has written the file, re-reading the project for the new status can fail (lines 204-206). It then returns exit 3, which per the spec means "can't work here", though the refutation *was* written. (2) It asks for Dakota's `yes` before checking that the statement and proof exist, so a typo costs a confirmation and then fails.
**Change:** (1) If the re-read fails, still exit 0 and print that the refutation was written, without the status. (2) Resolve both IDs before asking, as in `runAddCheck`.
**Tests:** refusal without a terminal covered, with the file unchanged. The confirmed path isn't.

## PaperCommands.h / PaperCommands.cpp

### Declarations (Source/LoomCli/PaperCommands.h:1-35)
**Verdict:** PASS

### `jsonRenamed`, `tellRenamed` (Source/LoomCli/PaperCommands.cpp:13-29)
**Verdict:** PASS
**Tests:** covered (renames in JSON and on stderr).

### `runNewPaper` (Source/LoomCli/PaperCommands.cpp:33-52)
**Verdict:** PASS
**Spec:** `new-paper`, `{"key", "path", "renamed"}`
**Tests:** covered (key, shared-key lettering, wrong key refused, missing file).

### `runNewStub` (Source/LoomCli/PaperCommands.cpp:54-80)
**Verdict:** PASS
**Tests:** covered (by alias, paper-premise refused, JSON, unknown paper).

### `runRekey` (Source/LoomCli/PaperCommands.cpp:82-99)
**Verdict:** PASS
**Tests:** "already right" covered. An actual rename (`old → new`) from the CLI isn't.

### `runBuildBib` (Source/LoomCli/PaperCommands.cpp:101-134)
**Verdict:** PASS
**Spec:** `build-bib [--out <path>]`; Path safety
**Tests:** covered (default path, `--out` relative to the cwd, outside the project → 3, warnings).

## ValidateCommands.h / ValidateCommands.cpp

### Declarations (Source/LoomCli/ValidateCommands.h:1-24)
**Verdict:** PASS

### `counted`, `jsonProblem` (Source/LoomCli/ValidateCommands.cpp:15-26)
**Verdict:** PASS

### `runValidate` (Source/LoomCli/ValidateCommands.cpp:30-67)
**Verdict:** PASS
**Spec:** Validation (problems on stderr, count on stdout, exit 1 on errors); JSON shapes
**Tests:** covered (text, JSON, clean project, `--staged` without a repo, newer format, the commit guard).

### `runInstallHooks` (Source/LoomCli/ValidateCommands.cpp:69-106)
**Verdict:** FAIL
**Severity:** medium
**Spec:** Exit codes ("3 … no project found"); The project folder, `loom install-hooks`; CLAUDE.md "nothing changes a project, git … unless the user clicked" (here, a repo that isn't a project)
**Finding:** It refuses only `FormatTooNew`. `NotAProject` and `Unreadable` fall through to `loom::installHooks`, which never checks for `loom.toml` (Hooks.cpp:62-). Scenario 1: `loom install-hooks --project C:\code\some-repo` (no `loom.toml`) → `findProject` returns the folder unchecked → the user's own `.git/hooks/pre-commit` is renamed `pre-commit.before-loom`, Loom's hook is written, and `merge.loom-checks` is set. Every later commit there runs `loom validate --staged`, finds no project and exits 3, so every commit is refused. Scenario 2: `--project` names a subfolder without `loom.toml` of a larger repo. `git rev-parse --git-path` resolves to the *parent* repo's hooks, with the same effect on the parent repo.
Scenario 3 (from review-foundations, verified with git 2.55): `rev-parse --git-path hooks/pre-commit` honours `core.hooksPath`. With a global `core.hooksPath`, both `install-hooks` and `new-project` rename the user's *shared* pre-commit hook and put Loom's hook in the shared folder. Commits in every other repo that uses that folder are then refused.
**Change:** Refuse unless `openProject` returns `Opened`: exit 3 with `not-a-project` or `unreadable`, as `readProject` does (or fix `findProject`, above). Then check that `git rev-parse --show-toplevel` is the project folder. Also refuse to install, or install under `.git/hooks` and say so, when the hook path resolves outside the project's own `.git`. review-foundations logs the core-side guard on `installHooks`.
**Tests:** missing: install-hooks on a folder without `loom.toml`; on a repo with a foreign hook (the `.before-loom` rename and the text output); with `pre-commit.before-loom` already taken.

## LoomCli CMake

### `Source/LoomCli/CMakeLists.txt` (1-17)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The project folder ("`loom` is one self-contained program"); Verification, "Commit guard on Windows"
**Finding:** The project kit is compiled in, but nothing puts Qt's DLLs next to `loom.exe` (no `windeployqt` or copy step). `new-project` records `QCoreApplication::applicationFilePath()` in the hook as the fallback. Outside `ctest`, which prepends Qt's `bin` to PATH, that `loom.exe` can't start unless Dakota has Qt's `bin` on PATH. A `git commit` in a project then fails in the hook with a loader error, not with Loom's checks or the spec's "`loom` wasn't found" message. Unverified on Windows.
**Change:** Add a post-build step on Windows that runs `windeployqt --no-translations` for `loom`, or copies `Qt6Core*.dll`, so the built `loom.exe` runs on its own. Or record this in the build how-to and the spec as a known M1 limit.
**Tests:** none. **Windows check:** run `git commit` in a new project from a shell without Qt on PATH.

## LoomApp

### `Source/LoomApp/CMakeLists.txt` (1-11)
**Verdict:** PASS

### `main` (Source/LoomApp/main.cpp:14-24)
**Verdict:** PASS

### `MainWindow` (Source/LoomApp/MainWindow.h:9-17, MainWindow.cpp:5-10)
**Verdict:** PASS
**Tests:** see below.

### `Tests/LoomAppTests/` (CMakeLists.txt, MainWindowTests.cpp)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** The one test checks that the window is exposed. The constructor's documented behaviour, "titled Loom" and "doesn't show it", isn't checked.
**Change:** Add `QCOMPARE(window.windowTitle(), "Loom")` and `QVERIFY(!window.isVisible())` before `show()`.
**Tests:** as above.

## Build: CMake

### Top-level `CMakeLists.txt` (1-39)
**Verdict:** PASS
**Spec:** Toolchain (C++20, CMake 3.28, Qt 6.11, version 0.1.0, link check last)
**Tests:** covered by CI configure.

### `CMakePresets.json` (1-30)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Toolchain, "Presets"
**Finding:** The `windows` test preset has no `configuration`. With the multi-config Visual Studio generator, `ctest --preset windows` alone runs no tests ("Test not available without configuration"). The how-to and CI work only because they add `-C Debug`.
**Change:** Add `"configuration": "Debug"` to the `windows` build and test presets, or add `windows-debug`/`windows-release` presets.
**Tests:** n/a.

### `loom_set_compiler_options` (Source/CMake/LoomCompilerOptions.cmake:7-16)
**Verdict:** PASS
**Spec:** Toolchain: flags exactly as listed; warnings as errors only through the CI option.

### `loom_check_no_ui_links` (Source/CMake/LoomCoreLinkCheck.cmake:13-49)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Toolchain, Targets ("CMake fails the configure if anything adds Gui, Widgets or WebEngine to it"); CLAUDE.md hard rule 2
**Finding:** Today the rule holds: LoomCore links `Qt6::Core`, toml++ and its two resource object libraries, and no LoomCore source includes a Gui or Widgets header. But the walker follows a nested target only when it is bare or wrapped in `$<LINK_ONLY:…>`. Two cases slip through. (1) `target_link_libraries(LoomCore PRIVATE $<$<CONFIG:Debug>:Helper>)`, where `Helper` links `Qt6::Widgets`, isn't followed. (2) A raw library name or path (`Qt6Widgets`, `.../Qt6Widgetsd.lib`) doesn't match `Qt6?::`.
**Change:** Before the `TARGET` test, extract target-like names from any generator expression (`[A-Za-z0-9_:]+` after the last `:` inside `$<…>`). Add `Qt6(Gui|Widgets|WebEngine)` (no `::`) to the forbidden pattern.
**Tests:** CoreLinkCheck covers the seven cases it claims; the two above are missing.

### `Source/CMake/LoomDependencies.cmake` (1-20)
**Verdict:** PASS
**Spec:** Toolchain: toml++ pinned to a commit and Catch2 v3.16.1, both by URL and SHA-256.

### `Source/LoomCore/CMakeLists.txt` (1-85)
**Verdict:** PASS
**Spec:** Toolchain, Targets (static; `Qt6::Core` public, toml++ private); The project folder (kit and hook as Qt resources)

## Tests: CMake and scripts

### `Tests/CMakeLists.txt` (1-91)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Verification, CLI ("Exit codes 0–3")
**Finding:** `loom.no-arguments-is-usage-error` uses `WILL_FAIL TRUE`, which passes on *any* non-zero exit, a crash included. The comment above it says "the usage test checks the code instead", but nothing checks for 2. There are no tests for `help` or for an unknown subcommand.
**Change:** Run it through a `cmake -P` script that asserts exit 2, as the other CLI tests do. Add `loom help` (0) and `loom frobnicate --json` (2, with a JSON error).
**Tests:** as above.

### `Tests/LoomCoreTests/CMakeLists.txt` (1-50)
**Verdict:** PASS

### `Tests/CMake/CoreLinkCheck/CMakeLists.txt` (1-43)
**Verdict:** PASS
**Tests:** seven cases. See `loom_check_no_ui_links` for the two missing ones.

### `Tests/CMake/NewProjectCli.cmake`
**Verdict:** PASS
**Tests:** exit codes 0–3, path text and JSON, one commit, own git identity. Missing git and missing identity are left to LoomCoreTests (fake git), which is reasonable.

### `Tests/CMake/StatementsCli.cmake`
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** Missing cases: `new-tag --json`; `edit-statement` with `--superseded-by`, `--add-tag`/`--remove-tag`, `--kind`; a non-UTF-8 `--body-file` (exit 2).
**Change:** Add them.
**Tests:** as above.

### `Tests/CMake/ChecksCli.cmake`
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** Good use of `INPUT_FILE` so input is never a terminal. Missing cases: `--changes-file`, `--note`, an agent `needs-changes` with `--changes`, and `edit-statement --outdate-checks` refused without a terminal (only `edit-proof` is tested).
**Change:** Add them.
**Tests:** as above.

### `Tests/CMake/StatusCli.cmake`
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** The `refute` refusal test (line 64) relies on input not being a terminal, but unlike ChecksCli the `execute_process` has no `INPUT_FILE`, so it inherits whatever stdin the test runner has. It passes under `ctest` (STATUS.md: all tests pass on Windows). Run by hand with `cmake -P` from a console, `loom` would see a console, print the prompt and wait. Unverified.
**Change:** Add `INPUT_FILE` with a `yes` file to `run_loom`, as in ChecksCli.
**Tests:** an actual `rekey` rename is also missing here and in PapersCli.

### `Tests/CMake/ValidateCli.cmake`
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** It covers validate well (0–3, text, JSON, newer format) and the commit guard end to end. Missing install-hooks cases: a folder without `loom.toml` (the FAIL above), a foreign hook being renamed (`moved_hook` non-null, and the text output), and `pre-commit.before-loom` already present (exit 1). Also missing: two `loom` commands at once (the lock → exit 3, "Another loom command is changing this project"), which no CLI test checks. Also missing (from review-foundations): a file name containing `%` in the text output. `loom::problemLine` (Problem.cpp:8-13, theirs) chains `.arg()`, so `Smith%2C%20Jones.pdf` prints as `Smith1C%20Jones.pdf:1: …`. `runValidate` prints through it unchanged.
**Change:** Add them.
**Tests:** as above.

### `Tests/CMake/PapersCli.cmake`, `Tests/CMake/PapersCli/draft.toml`
**Verdict:** PASS
**Tests:** covered. An actual `rekey` rename (`old → new`) would be a good addition.

### `Tests/Projects/EveryProblem/` and `EveryProblem-expected.txt`
**Verdict:** PASS
**Spec:** Verification, "Validation messages"
**Tests:** Every validation code in the spec appears in the expected list, except `format-too-new`, which stops validation (checked by script). The spec asks for "text and JSON", but `ValidationTests.cpp` compares only the text for this fixture; the JSON is compared only for the worked example (ValidateCli). That test is review-status-validation's, so noted here only. The fixture uses made-up data.

## CI

### `.github/workflows/checks.yml` (1-60)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Toolchain, "CI"
**Finding:** It matches the spec: the `repo` job, plus a `build` matrix of exactly `windows-2025-vs2026` and `ubuntu-24.04`; Qt through the action, cached; configure with the preset; build; `ctest` with output on failure; warnings as errors; offscreen UI tests. Two weaker points. The `repo` job runs on `ubuntu-latest`, against the "named exactly" principle. Only Debug is built, so Release-only warnings (unused variables behind `Q_ASSERT`) under `/W4 /WX` are never seen until Dakota builds Release.
**Change:** Pin `repo` to `ubuntu-24.04`. Add a Release build on Windows, building only, without tests.
**Tests:** n/a.

## Tools

### `read_page`, `is_page`, `section_pages` (Tools/update_indexes.py:40-90)
**Verdict:** PASS
**Tests:** covered.

### `render_table` (Tools/update_indexes.py:93-109)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** Titles and summaries are put into the table cells unescaped. A summary containing `|`, such as one quoting `edit-statement`'s `equation|technique|other`, splits the row into extra columns.
**Change:** Escape `|` as `\|` in the title and summary.
**Tests:** missing: a summary with `|`.

### `updated_readme`, `section_readmes`, `main` (Tools/update_indexes.py:112-160)
**Verdict:** PASS
**Tests:** see the test file.

### `Tools/tests/test_update_indexes.py`
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** The 6 tests pass (run for this review). `main()` is untested: `--check` exiting 1 on a stale index and 0 on a fresh one, a rewrite fixing a stale table, an error page giving exit 1. `section_readmes` (README without markers is skipped) is untested too. These are the paths CI depends on.
**Change:** Add tests that call `main(["--check"])` on a temporary repo. `REPO` needs to be a parameter, or patched with `monkeypatch`.
**Tests:** as above.

### `pytest.ini`
**Verdict:** PASS

## Templates

### `Templates/hooks/pre-commit`
**Verdict:** PASS
**Spec:** `loom install-hooks`: PATH first, then the absolute path; refusal message verbatim. The path is single-quoted by `shellQuoted`, and CRLF is stripped when installed.

### `Templates/project/CLAUDE.md`, `gitignore`, `gitattributes`, `TeX/preamble.tex`, `TeX/macros.tex`
**Verdict:** PASS
**Spec:** The project folder, "Files it creates": contents match the spec byte for byte, all LF (the repo's `.gitattributes` keeps them LF on Windows checkouts).

### `Templates/README.md`, `Templates/LICENSE`
**Verdict:** PASS

## Cross-boundary notes
- **To review-papers (sent through the coordinator, since sending by name failed):** The fixture's expected output shows `paper-key-rule` used for Papers/Nobody19 with a message that isn't the spec's ("Can't make a key for this paper: it has no authors or editors."), on a paper that already has the error `authors-missing`.
- **To review-foundations (sent through the coordinator):** `loom::installHooks` (Hooks.cpp:62) doesn't check for `loom.toml` or that root is the git top level, so `install-hooks` on a non-project, or on a subfolder of another repo, installs into that repo (see `runInstallHooks`).
- **Checked and dropped before sending:** I suspected that a CRLF `--changes-file` would put `\r` into Dakota's `changes`. It doesn't: `addCheck` turns CRLF into LF (Edits.cpp:442-443).
- Received: the coordinator's note on how to route messages.
- **From review-foundations (relayed):** (1) They agree on install-hooks and log a FAIL (medium) on `installHooks` with a guard: `loom.toml` exists and `show-toplevel` equals root. They found that `rev-parse --git-path` honours `core.hooksPath` (verified with git 2.55), so a global `core.hooksPath` makes `install-hooks`/`new-project` take over the user's shared hook folder. Added to `runInstallHooks` as scenario 3. (2) `problemLine` (Problem.cpp:8-13) chains `.arg()`, so file names containing `%1`/`%2` are garbled in `loom validate` text output. This is the same pattern I logged in `readTextFile` and `runGraph`. Added the missing CLI test to the ValidateCli entry.

## Spec issues
- **Verification, "CLI | Catch2":** the CLI tests are CMake `-P` scripts run by CTest, not Catch2. They test what the row asks for (exit codes 0–3, `--json` shapes, Dakota refusals), so the spec row should say "CTest scripts".
- **`help [subcommand]`:** the spec doesn't say which exit code `help` returns or where it prints. The exit-code table implies 0, and "results to stdout" implies stdout. The code doesn't implement `help` at all.
- **JSON shapes not given:** `new-tag`, `install-hooks`, `refute`, `rekey`, `build-bib` and `add-check` print JSON the spec doesn't define. The code's choices are reasonable (`{"id"}`; `{"hook", "moved_hook", "merge_driver"}`; `{"id", "path", "status", "explanation"}`; `{"from", "to", "renamed"}`; `{"path", "problems"}`; the check entry plus standing) and should be written into the spec.
- **"`loom` is one self-contained program":** true of the kit, but `loom.exe` needs Qt's DLLs, and the hook's fallback absolute path assumes a `loom` that runs on its own. The spec doesn't say how `loom` is deployed in M1.
- **`paper-key-rule`:** the spec gives one message. The core also uses the code for "Can't make a key for this paper" (EveryProblem-expected.txt, Nobody19), and the spec is silent on whether a paper with no key derivable should warn at all, given `authors-missing`.
- **`--outdate-checks` confirmation:** the spec says the refusal "names the proofs whose checks it would outdate", but not whether Dakota's confirmation covers exactly those proofs or whatever the edit outdates when it runs (see `editWithConfirmation`).
