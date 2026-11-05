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
| [`file(COPY)` skips a file whose destination has the same time](cmake/file-copy-skips-same-timestamp.md) | CMake's `file(COPY)` keeps the source's timestamps and doesn't copy a file whose destination already has the same time, and on Windows "the same" can mean the same second. Copying an overlay over files written moments before it silently skips some of them. Copy overlays with `file(COPY_FILE)`, one file at a time. |
| [CMake's `file(WRITE)` writes CRLF on Windows](cmake/file-write-crlf-on-windows.md) | On Windows, `file(WRITE)` and `file(APPEND)` turn every `\n` into `\r\n`, so a test file written as `"a\r\n"` holds `a\r\r\n`. Write plain `\n` when you want the platform's line ending, or use `file(CONFIGURE OUTPUT … CONTENT … NEWLINE_STYLE UNIX)` when the bytes must be exactly LF. |
| [A `;` in a CMake argument splits it in two](cmake/semicolon-splits-arguments.md) | CMake lists are strings joined by `;`, so a quoted argument holding `;` reaches a function's `${ARGN}` as two arguments, and `execute_process(COMMAND … ${ARGN})` passes `loom` two words. In CLI test scripts, keep `;` out of `--body`, `--title` and other values, or pass them through a file (`--body-file`). |

### git

| Page | Summary |
| --- | --- |
| [`git rev-parse --git-path hooks/…` follows `core.hooksPath` out of the repository](git/hookspath-moves-hooks.md) | Asking git where a hook goes (`git rev-parse --git-path hooks/pre-commit`) gives `core.hooksPath` when it is set, locally or globally, which can be a folder every repository on the machine shares. Writing a hook there changes every one of those repositories. Check that the answer is inside the repository's own git directory (`git rev-parse --git-common-dir`), and refuse if it isn't; `loom install-hooks` and `loom new-project` do (A1). |
| [A failing `git rev-parse` doesn't mean "no repository" or "no commit yet"](git/rev-parse-failures.md) | `git rev-parse` fails the same way (exit 128) for "not a git repository", for a repository git refuses to use ("dubious ownership") and for a damaged one. Treating every failure as "nothing to compare with" silently skips checks. Tell the cases apart: `rev-parse --git-dir`'s message, read with `LC_ALL=C`, for "no repository"; `rev-parse --verify --quiet HEAD^{commit}` exiting 1 with nothing on stderr for "no commit yet"; anything else is a failure to report. |

### latex

| Page | Summary |
| --- | --- |
| [amsthm sets a theorem's heading when its first paragraph starts](latex/amsthm-heading-set-in-body.md) | An amsthm theorem or proof typesets its heading (and so its optional title) only when the first paragraph of its body starts, so an error in the title is reported at the body's first line, in the statement's file, not at the `\begin` line of `main.tex`. `loom.sty` starts the paragraph itself (`\leavevmode`) right after `\begin`, so title errors land in the part Loom writes. |
| [BibTeX refuses `max_print_line=100000` ("3 is a bad bad")](latex/bibtex-rejects-long-max-print-line.md) | Setting `max_print_line` in the environment, so pdfLaTeX's log lines don't wrap, also reaches BibTeX, which refuses any value at or above its buffer size and stops with "3 is a bad bad". Scope the variables to pdfLaTeX: `max_print_line.pdflatex`, `error_line.pdflatex`, `half_error_line.pdflatex`. |
| [After an error, every `\ref` in the log reads as undefined](latex/latexmk-stops-before-references.md) | latexmk stops after a pdfLaTeX pass with errors, before the pass that reads the labels back from `main.aux`, so the log of a failed build calls references and citations undefined that are fine. Loom leaves `tex-ref-undefined` and `tex-cite-undefined` out of a build that has errors. |

### qt

| Page | Summary |
| --- | --- |
| [QFileInfo::canonicalFilePath() doesn't resolve junctions on Windows](qt/canonical-path-ignores-junctions.md) | On Windows, Qt 6's `QFileInfo::canonicalFilePath()` leaves NTFS junctions unresolved, both the junction itself and any path through it, so a path check built on it lets `Link/secret.tex` escape the project. Use `std::filesystem::canonical()`, which asks Windows for the final path, as `LoomCore`'s `ProjectFiles.cpp` does. |
| [A variable or callback named `emit` silently does nothing](qt/emit-is-a-macro.md) | Qt defines `emit` as an empty macro, so `emit(item);` with a callback called `emit` compiles to `(item);` and never calls it, with no error or warning. Name callbacks something else (`report`, `onItem`). |
| [A `const QString&` from a getter changes when the object does](qt/getter-reference-changes-under-you.md) | `f(buffer, buffer->proof().level)` passes a reference to the buffer's own field. If `f` changes the buffer before it copies that argument, it reads the new value, not the one you passed. Copy the value into a local first. |
| [QLockFile treats a lock older than 30 seconds as stale, even while its owner runs](qt/qlockfile-stale-by-age.md) | By default a `QLockFile` held longer than its stale time (30 s) can be removed by another process that tries to take it, even though the holder is still running. For a lock held for a window's whole life (`.loom/app.lock`) or a long build, call `setStaleLockTime(0)`, so only a dead owner makes it stale. In tests that fake a crashed owner's lock file, leave the host line empty: on Windows QLockFile spells the machine name in capitals, not as `QSysInfo::machineHostName()` does. |
| [QPdfView at Fit width can lay out for ever](qt/qpdfview-fit-width-scrollbar-loop.md) | With `ZoomMode::FitToWidth` and the default vertical scroll bar (as needed), some window sizes make `QPdfView` (Qt 6.11) add and remove its scroll bar endlessly: the window stops responding at 100% of one core. Keep the vertical scroll bar always on (`Qt::ScrollBarAlwaysOn`), as `LoomPdfView` does. |
| [QScintilla's `annotation(line)` returns text it never read](qt/qscintilla-annotation-returns-garbage.md) | In QScintilla 2.14.1, `QsciScintilla::annotation(int line)` allocates a buffer for the line's annotation but never asks Scintilla to fill it, so it returns uninitialised memory (it printed as a row of `?`). Read an annotation with `SCI_ANNOTATIONGETTEXT` yourself. |
| [QScintilla's hover (dwell) doesn't fire from `QTest::mouseMove`](qt/qscintilla-dwell-needs-a-real-pointer.md) | The editor's hover card comes from Scintilla's `SCN_DWELLSTART`, which never arrived in a Qt Test after `QTest::mouseMove()` and a 5-second wait. Tests emit `SCN_DWELLSTART` and `SCN_DWELLEND` themselves, with a real position and point, and a real hover is left to the Windows check. |
| [QScintilla takes every shortcut its keymap binds](qt/qscintilla-takes-bound-shortcuts.md) | While a QScintilla editor has focus, it accepts the `ShortcutOverride` event for any key its command set binds (Ctrl+-, Ctrl++, Ctrl+L, Ctrl+D, Ctrl+T, Ctrl+U, …), so a window action with the same shortcut never fires. Unbind the key from `standardCommands()` if the window's action should win. |
| [QRegularExpression's `$` also matches before a final line break](qt/regex-dollar-before-final-newline.md) | In Qt 6, `QRegularExpression` (PCRE2) lets `$` match at the very end *or* just before a final `\n`, so `^[a-z0-9-]+$` accepts `"energy\n"`. A rule meant to check a whole value must be anchored with `QRegularExpression::anchoredPattern()` (which wraps the pattern in `\A(?:…)\z`), never with `^…$`. |
| [Window shortcuts in Qt Tests need the window active, which parallel tests take away](qt/shortcuts-need-an-active-window.md) | `QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier)` triggers a menu action's shortcut only while that window is the active window. Under `ctest -j8` on Windows another test program's window can take activation at any moment, so the key goes nowhere and a `QTRY_…` waiting for its effect runs into Qt Test's 300-second watchdog. Make the window active just before each shortcut, and check right after that it started what it should, with a short wait. |

### toml

| Page | Summary |
| --- | --- |
| [A LaTeX macro in a title needs `\\` in the front matter](toml/backslash-in-titles.md) | Front-matter strings are TOML basic strings, where `\` starts an escape: `title = "Bound for \beta"` is a *backspace* and `eta`, and `\m…` is a syntax error. Write `\\beta` in the file (Loom's writer does this itself); a title read with a control character in it makes LaTeX stop with *"Unicode character ^^H (U+0008) not set up"*. |
| [toml++ says an empty array isn't an array of tables](toml/tomlplusplus-empty-array-not-array-of-tables.md) | In toml++ v3, `array::is_array_of_tables()` (and `is_homogeneous()`) return `false` for an empty array, so `versions = []` fails a check written as "must be an array of tables" with a confusing `wrong-type` message. Test `array->empty()` first, and treat an empty list as "none" (`missing-field` where one is required). |
| [toml++ writes table keys in alphabetical order](toml/tomlplusplus-sorts-keys.md) | A `toml::table` in toml++ is a `std::map`, so it forgets the order keys were read or inserted in and writes them alphabetically. Never write Loom's files with toml++'s formatter; use Loom's own writer, which puts fields in the order the spec lists. |

### windows

| Page | Summary |
| --- | --- |
| [On Windows, `_isatty()` says `NUL` is a terminal](windows/isatty-true-for-nul.md) | `_isatty(_fileno(stdin))` is true whenever input is a character device, and on Windows `NUL` is one, so `loom … < NUL` would pass a "only at a terminal" guard. To tell whether a person can type, ask for the console mode instead: `GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode)` succeeds only for a real console. `loom`'s `inputIsTerminal()` (`Source/LoomCli/Cli.cpp`) does this. |
<!-- END GENERATED -->

Up: [Wiki index](../INDEX.md)
