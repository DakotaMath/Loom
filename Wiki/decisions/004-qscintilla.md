---
type: decision
tags: [dependencies, qscintilla, editor, build, m2]
---

# 004: QScintilla 2.14.1 for the editor, built by our own CMake

> **Summary:** The Statements tab's text editor is QScintilla 2.14.1 (GPL v3), compiled into `LoomApp` as a static library from Riverbank's source archive, pinned by SHA-256 and built by `Source/CMake/QScintilla.cmake`, because QScintilla ships only qmake files. Its printing support is left out, so Loom doesn't need Qt Print Support.

**Last verified:** 2026-11-05 · commit 3be5679
**Status:** Accepted
**Decided by:** Dakota, in the approved M2 spec (`Specs/m2-statements-review.md`, "Dependencies and toolchain", 2026-11-05); the build details chosen by an agent for #35

## Context
Statements and proofs are LaTeX, edited for hours at a time. The editor needs LaTeX highlighting, line numbers, brace matching, margin marks on lines with problems, annotations under lines (for IDs with their titles, #37), find and replace with regular expressions, and good performance. `QPlainTextEdit` has none of the margins or annotations and would need all of it written by hand.

## Options considered
| Option | Pros | Cons |
| --- | --- | --- |
| QScintilla | Scintilla's editor as a Qt widget: a TeX lexer, margins and markers, annotations, brace matching, regex find, undo; maintained by Riverbank since 2003, released 2.14.1 in 2023, works with Qt 6 | Ships qmake files only; GPL v3 or a commercial licence |
| `QPlainTextEdit` and our own code | No dependency | Line numbers, markers, annotations, brace matching and highlighting all to write and maintain |
| KTextEditor (KDE) | Full-featured | Pulls in KDE Frameworks; far too heavy for one widget |
| Qt Creator's editor | Excellent | Not a library; deeply tied to Qt Creator |

## Decision
QScintilla 2.14.1, fetched by `FetchContent` from `https://www.riverbankcomputing.com/static/Downloads/QScintilla/2.14.1/QScintilla_src-2.14.1.tar.gz` and checked by SHA-256 (`dfe13c6a…cdd4d`). `Source/CMake/QScintilla.cmake` builds the `QScintilla` static library the way its `src/qscintilla.pro` does: every source of `src/` and Scintilla's `lexers/`, `lexlib/` and `src/`, with `SCINTILLA_QT`, `SCI_LEXER` and `INCLUDE_DEPRECATED_FEATURES`, AUTOMOC on its headers, its warnings off, and its headers `SYSTEM` for our targets. Left out: `qsciprinter.cpp` (printing, which needs Qt Print Support) and `MacPasteboardMime.cpp` (macOS only). `LoomAppLib` links it; `LoomCore` never does.

## Why
- It does everything the spec asks of the editor, now and in #37 (annotations, indicators), without writing an editor.
- Building it with CMake keeps one build system: no qmake step on Dakota's machine or in CI, and the same flags on Windows and Linux.
- Leaving printing out keeps Qt's modules to what the spec lists.

**Licence:** GPL v3 (Riverbank also sells a commercial licence). Compiled into Loom, which is GPL v3, so compatible. Listed in *Help → About Loom*.

## Consequences
- QScintilla's own key bindings take shortcuts away from the window while an editor has focus ([gotcha](../gotchas/qt/qscintilla-takes-bound-shortcuts.md)); `LatexEditor` unbinds Ctrl+- and Ctrl++ so they change the font size in Settings.
- The first configure downloads the archive (about 3 MB); building the library adds about a minute to a clean build.
- Moving to a newer QScintilla is a deliberate change: update the URL, hash and this page, and check the sources list against its `qscintilla.pro`.

## See also
- Up: [Decisions](README.md)
- [001: Qt 6](001-qt-6.md)
- [`Specs/m2-statements-review.md`](../../Specs/m2-statements-review.md): *Dependencies and toolchain*, *The editor*
