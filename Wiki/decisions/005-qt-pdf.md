---
type: decision
tags: [dependencies, qt, pdf, build, m2]
---

# 005: Qt PDF to show the compiled PDF

> **Summary:** The Statements tab shows each build's PDF with Qt PDF and Qt PDF Widgets (`Qt6::Pdf`, `Qt6::PdfWidgets`, Qt 6.11.1, LGPL v3 / GPL v3, built on PDFium), linked into `LoomApp` only. The PDF is loaded from memory, so the file is never held open, and SyncTeX runs as TeX Live's `synctex` program, not a library.

**Last verified:** 2026-11-05 · commit 81b7311
**Status:** Accepted
**Decided by:** Dakota, in the approved M2 spec (`Specs/m2-statements-review.md`, "Dependencies and toolchain" and "Decisions this spec makes", 2026-11-05); the details chosen by an agent for #36

## Context
Ctrl+S compiles a statement and shows the PDF beside the editor: it must render quickly, scroll and zoom, find text, map a click to a point on a page (inverse SyncTeX) and a box back to the view (forward SyncTeX), and never lock the file, because on Windows the next build can't overwrite a PDF another program holds open.

## Options considered
| Option | Pros | Cons |
| --- | --- | --- |
| Qt PDF and Qt PDF Widgets | Part of Qt, same version and licence as the rest; PDFium (Chrome's renderer); `QPdfView` scrolls, zooms and highlights search results; `QPdfDocument` loads from a `QIODevice`; no other dependency | `QPdfView` doesn't publish where it draws each page, which SyncTeX needs (Loom recomputes it); no text selection in the view |
| Poppler (Qt 6 bindings) | Mature; used by Okular and TeXstudio | GPL v2-or-later C++ library to build on Windows, with its own dependencies (FreeType, cairo or Splash, …); a widget to write |
| MuPDF | Fast, good rendering | AGPL v3 (or commercial); a widget to write |
| An external viewer (SumatraPDF) | Nothing to build | Not beside the editor; it locks or reloads the file on its own terms; SyncTeX through command lines |

## Decision
Qt PDF: `find_package(Qt6 … Pdf PdfWidgets)`, linked by `LoomAppLib`. CI installs it with `install-qt-action`'s `modules: qtpdf`. `LoomCore`'s link check (`Source/CMake/LoomCoreLinkCheck.cmake`) refuses `Qt6::Pdf*` as it refuses Gui, Widgets and WebEngine, so `LoomCore` stays free of it.

How `LoomApp` uses it (`Source/LoomApp/PdfPane.*`):
- Each build's `main.pdf` (or `last-good.pdf`) is read on the Build lane and loaded into a `QPdfDocument` from a `QBuffer`; the pane keeps the buffer while the document uses it.
- `LoomPdfView` (a `QPdfView`) recomputes the page layout `QPdfView` draws (multi-page, fit-width or custom zoom) to turn a double-click into a page and point for `synctex edit`, and a `synctex view` box into a mark on the page.
- Find in the PDF is `QPdfSearchModel` set on the view.

SyncTeX itself is `LoomCore/SyncTex.h`: it runs `synctex` with `ProcessTree` and reads its output, and finds a label's page in `main.aux`; no SyncTeX library is linked (the spec's decision *SyncTeX*).

## Why
- It is Qt's own module: same licence, same version, installed the same way, no extra build.
- Loading from memory meets the spec's "never held open" without temporary copies.
- Everything else a viewer would add (selection, annotations) Loom doesn't need in M2.

**Licence:** LGPL v3 / GPL v3, like the rest of Qt; PDFium inside it is BSD-3-Clause. Linked into Loom, which is GPL v3, so compatible. Listed in *Help → About Loom*.

## Consequences
- `LoomPdfView::pageRect()` copies a layout Qt doesn't publish; if a Qt update changes how `QPdfView` lays out pages, double-click and the forward-search mark land in the wrong place (the `synctexBothWays` Qt Test checks the double-click).
- Qt PDF renders pages in the background: a page is blank for a moment after loading, and a screenshot taken at once shows it blank.
- Windows builds need `Qt6Pdf.dll` and `Qt6PdfWidgets.dll` beside Qt's others (the Qt Online Installer's *Qt PDF* component, or aqt's `--modules qtpdf`).

## See also
- Up: [Decisions](README.md)
- [001: Qt 6](001-qt-6.md)
- [`Specs/m2-statements-review.md`](../../Specs/m2-statements-review.md): *The PDF pane*, *SyncTeX and the open statement's page*, *Dependencies and toolchain*
