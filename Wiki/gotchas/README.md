# Gotchas

Traps, dead ends and their fixes, in one folder per area (`qt`, `build`, `git`, `latex`, `windows`, …) so no folder grows without bound. Before adding one, check the area's list below for an existing page to extend. Template: [`_TEMPLATE.md`](_TEMPLATE.md).

<!-- BEGIN GENERATED: do not edit below; run python Tools/update_indexes.py -->
### bibtex

| Page | Summary |
| --- | --- |
| [BibTeX splits a name without a comma, and counts every brace](bibtex/names-and-braces.md) | BibTeX reads a name with no comma as "First von Last", so `author = {Garcia Lopez}` becomes first name Garcia, surname Lopez; write such a name braced, `{Garcia Lopez}`. Its parser also counts every `{` and `}` in a value, `\{` and `\}` included, so a lone `\{` breaks the rest of the file. Loom's `build-bib` does both (Specs/m1-core.md, A5 and A6). |

### ci

| Page | Summary |
| --- | --- |
| [aqtinstall 3.3.0 can't install Qt 6.11 on Windows](ci/aqtinstall-qt-6-11-windows.md) | `jurplel/install-qt-action@v4` uses aqtinstall 3.3.0 by default, which fails on every Qt 6.11 release for Windows because Qt changed its repository layout. Install aqtinstall from a pinned commit of its main branch with the action's `aqtsource` input until a fixed release is out. |

### cmake

| Page | Summary |
| --- | --- |
| [CMake's `CMAKE_MATCH_<n>` doesn't leave the function that matched](cmake/cmake-match-inside-function.md) | `if(x MATCHES …)` sets `CMAKE_MATCH_1` and the rest only in the current scope, so a helper `function()` that does the match leaves the caller's `CMAKE_MATCH_1` empty. In a CMake test script, capture groups at the top level with `string(REGEX REPLACE …)` (or match there), not through a helper function. |
| [CMake's `execute_process` garbles `loom`'s UTF-8 output on Windows](cmake/execute-process-encoding.md) | On Windows, `execute_process` decodes a program's output in the console's code page, so the UTF-8 `→` that `loom` writes reaches a CMake test script as `ΓåÆ` and an exact comparison fails. Pass `ENCODING UTF-8` to every `execute_process` that reads `loom`'s output. |
| [CMake's `file(WRITE)` writes CRLF on Windows](cmake/file-write-crlf-on-windows.md) | On Windows, `file(WRITE)` and `file(APPEND)` turn every `\n` into `\r\n`, so a test file written as `"a\r\n"` holds `a\r\r\n`. Write plain `\n` when you want the platform's line ending, or use `file(CONFIGURE OUTPUT … CONTENT … NEWLINE_STYLE UNIX)` when the bytes must be exactly LF. |

### git

| Page | Summary |
| --- | --- |
| [`git rev-parse --git-path hooks/…` follows `core.hooksPath` out of the repository](git/hookspath-moves-hooks.md) | Asking git where a hook goes (`git rev-parse --git-path hooks/pre-commit`) gives `core.hooksPath` when it is set, locally or globally, which can be a folder every repository on the machine shares. Writing a hook there changes every one of those repositories. Check that the answer is inside the repository's own git directory (`git rev-parse --git-common-dir`), and refuse if it isn't; `loom install-hooks` and `loom new-project` do (A1). |
| [A failing `git rev-parse` doesn't mean "no repository" or "no commit yet"](git/rev-parse-failures.md) | `git rev-parse` fails the same way (exit 128) for "not a git repository", for a repository git refuses to use ("dubious ownership") and for a damaged one. Treating every failure as "nothing to compare with" silently skips checks. Tell the cases apart: `rev-parse --git-dir`'s message, read with `LC_ALL=C`, for "no repository"; `rev-parse --verify --quiet HEAD^{commit}` exiting 1 with nothing on stderr for "no commit yet"; anything else is a failure to report. |

### qt

| Page | Summary |
| --- | --- |
| [QFileInfo::canonicalFilePath() doesn't resolve junctions on Windows](qt/canonical-path-ignores-junctions.md) | On Windows, Qt 6's `QFileInfo::canonicalFilePath()` leaves NTFS junctions unresolved, both the junction itself and any path through it, so a path check built on it lets `Link/secret.tex` escape the project. Use `std::filesystem::canonical()`, which asks Windows for the final path, as `LoomCore`'s `ProjectFiles.cpp` does. |
| [QRegularExpression's `$` also matches before a final line break](qt/regex-dollar-before-final-newline.md) | In Qt 6, `QRegularExpression` (PCRE2) lets `$` match at the very end *or* just before a final `\n`, so `^[a-z0-9-]+$` accepts `"energy\n"`. A rule meant to check a whole value must be anchored with `QRegularExpression::anchoredPattern()` (which wraps the pattern in `\A(?:…)\z`), never with `^…$`. |

### toml

| Page | Summary |
| --- | --- |
| [toml++ says an empty array isn't an array of tables](toml/tomlplusplus-empty-array-not-array-of-tables.md) | In toml++ v3, `array::is_array_of_tables()` (and `is_homogeneous()`) return `false` for an empty array, so `versions = []` fails a check written as "must be an array of tables" with a confusing `wrong-type` message. Test `array->empty()` first, and treat an empty list as "none" (`missing-field` where one is required). |
| [toml++ writes table keys in alphabetical order](toml/tomlplusplus-sorts-keys.md) | A `toml::table` in toml++ is a `std::map`, so it forgets the order keys were read or inserted in and writes them alphabetically. Never write Loom's files with toml++'s formatter; use Loom's own writer, which puts fields in the order the spec lists. |

### windows

| Page | Summary |
| --- | --- |
| [On Windows, `_isatty()` says `NUL` is a terminal](windows/isatty-true-for-nul.md) | `_isatty(_fileno(stdin))` is true whenever input is a character device, and on Windows `NUL` is one, so `loom … < NUL` would pass a "only at a terminal" guard. To tell whether a person can type, ask for the console mode instead: `GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode)` succeeds only for a real console. `loom`'s `inputIsTerminal()` (`Source/LoomCli/Cli.cpp`) does this. |
<!-- END GENERATED -->

Up: [Wiki index](../INDEX.md)
