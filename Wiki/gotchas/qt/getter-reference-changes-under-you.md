---
type: gotcha
tags: [qt, c++, app]
---

# A `const QString&` from a getter changes when the object does

> **Summary:** `f(buffer, buffer->proof().level)` passes a reference to the buffer's own field. If `f` changes the buffer before it copies that argument, it reads the new value, not the one you passed. Copy the value into a local first.

**Last verified:** 2026-11-05 · commit b31e71e

## Symptom
In #38, **Mark this proof failed…** called `chooseFailed(open, open->proof().level)`. Cancelling the dialog should have put the level back to `complete`, but the form stayed on `failed`, and no error was shown.

## Cause
`EditBuffer::proof()` returns `const Proof&`, so `open->proof().level` is a reference into the buffer. `chooseFailed(EditBuffer*, const QString& previous)` first runs `buffer->setProof(p)` with `p.level = "failed"`, which overwrites the field that `previous` refers to. Only after that does it capture `previous` by value in its callback, so it captures `"failed"`. Qt's implicit sharing doesn't protect you here: the reference is to the member itself, not to a shared copy of its data.

## Fix
Copy into a local before the call: `const QString previous = open->proof().level; chooseFailed(open, previous);`. Do this whenever a function both takes a value by `const&` and changes the object that value came from.

## See also
- Up: [Gotchas](../README.md)
- `StatementsPage::markProofFailed` in `Source/LoomApp/StatementsPage.cpp`
