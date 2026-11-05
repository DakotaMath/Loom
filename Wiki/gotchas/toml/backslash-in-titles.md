---
type: gotcha
tags: [toml, latex, front-matter]
---

# A LaTeX macro in a title needs `\\` in the front matter

> **Summary:** Front-matter strings are TOML basic strings, where `\` starts an escape: `title = "Bound for \beta"` is a *backspace* and `eta`, and `\m…` is a syntax error. Write `\\beta` in the file (Loom's writer does this itself); a title read with a control character in it makes LaTeX stop with *"Unicode character ^^H (U+0008) not set up"*.

**Last verified:** 2026-11-05 · commit ad75623

## Symptom
A build fails with *"LaTeX stopped in the part Loom writes around G8TV: Unicode character ^^H (U+0008) not set up for use with LaTeX"*, or `loom validate` reports `toml-syntax` on a `title` line.

## Cause
TOML basic strings (`"…"`) have escapes: `\b`, `\t`, `\n`, `\f`, `\r`, `\"`, `\\`, `\uXXXX`. A LaTeX macro whose name starts with one of those letters becomes a control character plus the rest of its name; any other letter after `\` is invalid TOML.

## Fix
Write `\\` for each backslash in a hand-written front matter: `title = "Decay in every \\(L^p\\)"`, or use `$…$` math, which needs no backslash for simple cases. `loom new-statement --title` and `edit-statement` take the title as typed and write it escaped.

## See also
- Up: [Gotchas](../README.md)
- [The M1 spec](../../../Specs/m1-core.md): *Front matter* (canonical form: strings as basic strings)
