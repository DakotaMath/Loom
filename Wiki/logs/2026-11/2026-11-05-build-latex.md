---
type: log
tags: [m2, build, latex]
---

# 2026-11-05: Building LaTeX in LoomCore, and `loom build`

> **Summary:** Built #33: `LoomCore` composes the wrapper document and copies, runs `latexmk` with a timeout that stops pdfLaTeX too, maps log problems to source lines, reuses unchanged builds and builds commits without touching the working tree; `loom build` exposes it; the demo project, its overlays and 31 new tests come with it (PR #44).

**Session model:** Opus 5.5
**Issues and PRs:** #33, PR #44

## Done
- `LoomCore`: `BuildDocument` (modes, order, `main.tex`, copies, `\paperref`, `\cite`, refs.bib), `TeX/loom.sty`, `Build`, `ProcessTree`, `TexLog`, `Revision`; three refactors with no change of behaviour (`LatexCommands.h`, git listing in `Git.h`, `projectBibliography()`).
- `loom build` with the spec's options, output, JSON and exit codes.
- `Tests/Projects/Demo` with the spec's statuses, `Demo-edits/review` and `Demo-edits/latex-error`, `Tests/Support/DemoProject`.
- Tests: 310/310 on Windows with TeX Live 2026; the no-TeX paths (skip, and fail under `LOOM_REQUIRE_TEX=1`) run by hand. CI installs TeX on Linux.
- Five gotcha pages; the departures and choices are listed under Flags in PR #44.

## Learned
- BibTeX refuses `max_print_line=100000` ("3 is a bad bad"); scope the variables to pdfLaTeX with `<variable>.pdflatex` ([gotcha](../../gotchas/latex/bibtex-rejects-long-max-print-line.md)).
- After a pass with errors latexmk stops, so the log calls every `\ref` undefined ([gotcha](../../gotchas/latex/latexmk-stops-before-references.md)).
- amsthm sets a heading only when the body's first paragraph starts, so title errors were blamed on the body ([gotcha](../../gotchas/latex/amsthm-heading-set-in-body.md)).
- A TOML title with `\b…` holds a backspace ([gotcha](../../gotchas/toml/backslash-in-titles.md)); CMake's `file(COPY)` skips files with the same time to the second ([gotcha](../../gotchas/cmake/file-copy-skips-same-timestamp.md)).
- Verified on Windows what the spec had only planned: the job object stops `latexmk` and the `pdflatex` it started (the timeout test checks pdfLaTeX stops writing).
- TeX reports an unclosed brace where it notices it (the `\]` a line later), so the `latex-error` overlay says which line to expect.

## Next
- Dakota reviews PR #44 and runs its Windows check.
- #34 (the app shell) can start; #35–#40 use the demo project and `Tests/Support/DemoProject`.
- #36 picks up SyncTeX, the `.aux` page and the link check for Qt PDF (see Noticed in PR #44).
