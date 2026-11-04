---
type: gotcha
tags: [bibtex, bibliography, papers]
---

# BibTeX splits a name without a comma, and counts every brace

> **Summary:** BibTeX reads a name with no comma as "First von Last", so `author = {Garcia Lopez}` becomes first name Garcia, surname Lopez; write such a name braced, `{Garcia Lopez}`. Its parser also counts every `{` and `}` in a value, `\{` and `\}` included, so a lone `\{` breaks the rest of the file. Loom's `build-bib` does both (Specs/m1-core.md, A5 and A6).

**Last verified:** 2026-11-04 · commit be0d3a7

## Symptom
- A paper whose author has an empty `first` (`last = "Garcia Lopez"`, or an institution) is cited as "G. Lopez" and sorted under L.
- A title such as `On the set \{x > 0` makes BibTeX report errors on later entries, or silently swallow them.

## Cause
- **Names.** BibTeX's name parser treats `Last, First` as surname then first names; without a comma it takes the last word as the surname and the words before it as first names (Oren Patashnik, *Designing BibTeX Styles*, and *Tame the BeaST*, "Names"). A group in braces is one word.
- **Braces.** When BibTeX reads a `{…}` value it matches braces by counting them, and a backslash doesn't escape a brace for that count. `\{` is an escape for LaTeX, later, not for BibTeX's parser.

These come from BibTeX's documentation; they weren't run through BibTeX when Loom's code was written.

## Fix
- Write a name as `Last, First`, and one with no first name as `{Last}`: `bibNames()` in `Source/LoomCore/Bibliography.cpp`.
- Count every brace when checking a value (`bracesBalanced()`), and skip an entry whose braces don't balance, with an error, rather than write it. A literal brace in a title needs its partner: `\{x > 0\}`.

## See also
- Up: [Gotchas](../README.md)
- [`Specs/m1-core.md`](../../../Specs/m1-core.md), *The bibliography*
