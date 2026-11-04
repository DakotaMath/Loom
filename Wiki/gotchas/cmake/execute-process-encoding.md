---
type: gotcha
tags: [cmake, tests, windows, utf-8]
---

# CMake's `execute_process` garbles `loom`'s UTF-8 output on Windows

> **Summary:** On Windows, `execute_process` decodes a program's output in the console's code page, so the UTF-8 `→` that `loom` writes reaches a CMake test script as `ΓåÆ` and an exact comparison fails. Pass `ENCODING UTF-8` to every `execute_process` that reads `loom`'s output.

**Last verified:** 2026-11-04 · commit 414d5b1

## Symptom
`Tests/CMake/ValidateCli.cmake` compares `loom validate`'s problems with the expected text. On Windows the comparison fails, and the "actual" text has `H8KD ΓåÆ J3RW` where the expected text has `H8KD → J3RW`. On Linux it passes.

## Cause
`loom` writes UTF-8 (it switches the console to UTF-8 while it runs). `execute_process` doesn't know that: its default `ENCODING` on Windows decodes the child's output with the active console code page (437 or 850 in a Western setup). The three UTF-8 bytes of `→` are read as three separate characters.

## Fix
Tell CMake the output is UTF-8:

```cmake
execute_process(COMMAND ${ARGN} RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err ENCODING UTF-8)
```

CMake reads its own script files as UTF-8, so the expected text needs nothing special.

## See also
- Up: [Gotchas](../README.md)
- [`Tests/CMake/ValidateCli.cmake`](../../../Tests/CMake/ValidateCli.cmake): where it bit
