---
type: gotcha
tags: [cmake, tests]
---

# `file(COPY)` skips a file whose destination has the same time

> **Summary:** CMake's `file(COPY)` keeps the source's timestamps and doesn't copy a file whose destination already has the same time, and on Windows "the same" can mean the same second. Copying an overlay over files written moments before it silently skips some of them. Copy overlays with `file(COPY_FILE)`, one file at a time.

**Last verified:** 2026-11-05 · commit ad75623

## Symptom
A test script copies `Tests/Projects/Demo-edits/review/` over a copy of the demo with `file(COPY … DESTINATION …)`. Most files change, but `Papers/SmithJones24/notes.md` stays the demo's version, and `git status` doesn't list it.

## Cause
`file(COPY)` documents that it "optimizes out a file if it exists at the destination with the same timestamp". The demo and its overlays were written within the same second, and the first copy kept the demo file's time, so the overlay's file looked unchanged.

## Fix
Copy file by file with `file(COPY_FILE <from> <to>)`, which always copies unless given `ONLY_IF_DIFFERENT` (Tests/CMake/BuildCli.cmake, `apply_overlay()`). C++ tests use `QFile::copy` after removing the target (Tests/Support/DemoProject.cpp).

## See also
- Up: [Gotchas](../README.md)
