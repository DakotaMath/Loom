# Gotchas

Traps, dead ends and their fixes, in one folder per area (`qt`, `build`, `git`, `latex`, `windows`, …) so no folder grows without bound. Before adding one, check the area's list below for an existing page to extend. Template: [`_TEMPLATE.md`](_TEMPLATE.md).

<!-- BEGIN GENERATED: do not edit below; run python Tools/update_indexes.py -->
### ci

| Page | Summary |
| --- | --- |
| [aqtinstall 3.3.0 can't install Qt 6.11 on Windows](ci/aqtinstall-qt-6-11-windows.md) | `jurplel/install-qt-action@v4` uses aqtinstall 3.3.0 by default, which fails on every Qt 6.11 release for Windows because Qt changed its repository layout. Install aqtinstall from a pinned commit of its main branch with the action's `aqtsource` input until a fixed release is out. |

### cmake

| Page | Summary |
| --- | --- |
| [CMake's `CMAKE_MATCH_<n>` doesn't leave the function that matched](cmake/cmake-match-inside-function.md) | `if(x MATCHES …)` sets `CMAKE_MATCH_1` and the rest only in the current scope, so a helper `function()` that does the match leaves the caller's `CMAKE_MATCH_1` empty. In a CMake test script, capture groups at the top level with `string(REGEX REPLACE …)` (or match there), not through a helper function. |

### qt

| Page | Summary |
| --- | --- |
| [QFileInfo::canonicalFilePath() doesn't resolve junctions on Windows](qt/canonical-path-ignores-junctions.md) | On Windows, Qt 6's `QFileInfo::canonicalFilePath()` leaves NTFS junctions unresolved, both the junction itself and any path through it, so a path check built on it lets `Link/secret.tex` escape the project. Use `std::filesystem::canonical()`, which asks Windows for the final path, as `LoomCore`'s `ProjectFiles.cpp` does. |

### toml

| Page | Summary |
| --- | --- |
| [toml++ writes table keys in alphabetical order](toml/tomlplusplus-sorts-keys.md) | A `toml::table` in toml++ is a `std::map`, so it forgets the order keys were read or inserted in and writes them alphabetically. Never write Loom's files with toml++'s formatter; use Loom's own writer, which puts fields in the order the spec lists. |

### windows

| Page | Summary |
| --- | --- |
| [On Windows, `_isatty()` says `NUL` is a terminal](windows/isatty-true-for-nul.md) | `_isatty(_fileno(stdin))` is true whenever input is a character device, and on Windows `NUL` is one, so `loom … < NUL` would pass a "only at a terminal" guard. To tell whether a person can type, ask for the console mode instead: `GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode)` succeeds only for a real console. `loom`'s `inputIsTerminal()` (`Source/LoomCli/Cli.cpp`) does this. |
<!-- END GENERATED -->

Up: [Wiki index](../INDEX.md)
