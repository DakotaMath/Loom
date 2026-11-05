---
type: gotcha
tags: [qt, macros, callbacks]
---

# A variable or callback named `emit` silently does nothing

> **Summary:** Qt defines `emit` as an empty macro, so `emit(item);` with a callback called `emit` compiles to `(item);` and never calls it, with no error or warning. Name callbacks something else (`report`, `onItem`).

**Last verified:** 2026-11-05 · commit d81778a

## Symptom
A function that reports results through a callback parameter named `emit` returns nothing: the callback never runs. It compiles cleanly, even with `/W4` and warnings as errors.

## Cause
`<QtCore/qobjectdefs.h>` has `#define emit` (empty, for readability in `emit someSignal();`). Any later `emit(x)` is preprocessed to `(x)`, a discarded expression.

## Fix
Don't use `emit` (or `signals`, `slots`, `foreach`) as an identifier in code that includes Qt headers. LoomCore's `checkSetup()` calls its callback `report`.

## See also
- Up: [Gotchas](../README.md)
