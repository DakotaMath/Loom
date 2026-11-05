---
type: gotcha
tags: [latex, bibtex, latexmk, build]
---

# BibTeX refuses `max_print_line=100000` ("3 is a bad bad")

> **Summary:** Setting `max_print_line` in the environment, so pdfLaTeX's log lines don't wrap, also reaches BibTeX, which refuses any value at or above its buffer size and stops with "3 is a bad bad". Scope the variables to pdfLaTeX: `max_print_line.pdflatex`, `error_line.pdflatex`, `half_error_line.pdflatex`.

**Last verified:** 2026-11-05 · commit ad75623

## Symptom
A build with a bibliography fails although the LaTeX is fine. latexmk's output shows `Running 'bibtex "main.aux"'`, then `3 is a bad bad`, and *"bibtex main: Could not open bibtex log file for 'main'"*; latexmk exits 12.

## Cause
TeX Live's programs read `max_print_line`, `error_line` and `half_error_line` through kpathsea, from `texmf.cnf` or the environment. BibTeX reads `max_print_line` too and checks it against its own buffer size; 100000 is over it, so BibTeX's sanity check ("bad" number 3) fails before it does anything.

## Fix
kpathsea looks for `<variable>.<program>` before `<variable>`, so give the values to pdfLaTeX only (Source/LoomCore/Build.cpp, `latexmkEnvironment()`):

```
max_print_line.pdflatex=100000
error_line.pdflatex=254
half_error_line.pdflatex=238
```

A shell can't `export` a name with a dot; `env 'max_print_line.pdflatex=100000' latexmk …` or `QProcessEnvironment` can set it.

## Dead ends
- A smaller `max_print_line` that BibTeX accepts: it would wrap long log lines again, which breaks reading `file:line:` errors.

## See also
- Up: [Gotchas](../README.md)
- [The M2 spec](../../../Specs/m2-statements-review.md): *Running `latexmk`*
