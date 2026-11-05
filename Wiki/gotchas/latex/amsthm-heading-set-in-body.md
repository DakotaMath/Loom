---
type: gotcha
tags: [latex, amsthm, build, problems]
---

# amsthm sets a theorem's heading when its first paragraph starts

> **Summary:** An amsthm theorem or proof typesets its heading (and so its optional title) only when the first paragraph of its body starts, so an error in the title is reported at the body's first line, in the statement's file, not at the `\begin` line of `main.tex`. `loom.sty` starts the paragraph itself (`\leavevmode`) right after `\begin`, so title errors land in the part Loom writes.

**Last verified:** 2026-11-05 · commit ad75623

## Symptom
A statement whose title uses an undefined macro fails with *"Undefined control sequence"* at line 9 of its own file (the first line of its body), though the body is fine.

## Cause
amsthm's `\begin{theorem}[Title]` only stores the heading in an `\item[…]` label; TeX typesets the label through `\everypar` when the next paragraph begins. In a build, that is while TeX reads `\input{src/Statements/<tag>.tex}`, so the current file and line are the body's.

## Fix
In `loomstatement` and `loomproof` (Source/LoomCore/TeX/loom.sty), `\leavevmode` right after the amsthm environment begins starts the paragraph on `main.tex`'s line, then `\ignorespaces` swallows the line end. An error in a title is then `tex-error-generated` (*"LaTeX stopped in the part Loom writes around G8TV: …"*). The cost: a body that starts with a list or display starts on the line after the heading, not on the heading's line.

## See also
- Up: [Gotchas](../README.md)
- [The M2 spec](../../../Specs/m2-statements-review.md): *Problems from a build*
