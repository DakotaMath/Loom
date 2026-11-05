---
type: gotcha
tags: [latex, latexmk, build, problems]
---

# After an error, every `\ref` in the log reads as undefined

> **Summary:** latexmk stops after a pdfLaTeX pass with errors, before the pass that reads the labels back from `main.aux`, so the log of a failed build calls references and citations undefined that are fine. Loom leaves `tex-ref-undefined` and `tex-cite-undefined` out of a build that has errors.

**Last verified:** 2026-11-05 · commit ad75623

## Symptom
A build with one real error also lists *"`\ref{A2DF}` points to nothing in this build or the project"* for statements that are right there in the build. Fix the error and they go.

## Cause
LaTeX resolves `\ref` from the `.aux` file of the previous pass. On a fresh build folder the first pass has no `.aux`, so every reference is undefined in that pass; latexmk normally runs again, but not after a pass with errors (without `-f` it stops: "Errors, so I did not complete making targets"). `main.log` is the log of the last pass, the first one.

## Fix
`mapTexLog()` (Source/LoomCore/TexLog.cpp) drops `tex-ref-undefined` and `tex-cite-undefined` when the log has an error. Real dangling references come back once the errors are fixed. `loom validate` reports dangling `\ref`s (`ref-dangling`) without TeX anyway.

## See also
- Up: [Gotchas](../README.md)
- [The M2 spec](../../../Specs/m2-statements-review.md): *Problems from a build*
