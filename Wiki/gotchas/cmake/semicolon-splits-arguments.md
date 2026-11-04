---
type: gotcha
tags: [cmake, tests]
---

# A `;` in a CMake argument splits it in two

> **Summary:** CMake lists are strings joined by `;`, so a quoted argument holding `;` reaches a function's `${ARGN}` as two arguments, and `execute_process(COMMAND … ${ARGN})` passes `loom` two words. In CLI test scripts, keep `;` out of `--body`, `--title` and other values, or pass them through a file (`--body-file`).

**Last verified:** 2026-11-04 · commit c688de4

## Symptom
`run_loom(1 out new-proof X --level failed --body "Tried the energy method; the sign is wrong." --json)` in a `Tests/CMake/*Cli.cmake` script expects exit 1 and gets exit 2: *"`loom new-proof` doesn't take ` the sign is wrong.`."* The usage error names the half of the body after the `;`.

## Cause
A CMake list is a string with `;` between its elements. The quoted argument is one element when the call is made, but `run_loom` forwards its extra arguments as `${ARGN}`, and expanding that unquoted inside `execute_process(COMMAND "${LOOM}" ${ARGN})` splits on every `;`, including the one in the body.

## Fix
- Write test values without `;` (a comma reads as well).
- When the text must hold a `;`, write it to a file with `file(WRITE)` and pass `--body-file`, `--changes-file` or the like.

## See also
- Up: [Gotchas](../README.md)
- [`Tests/CMake/StatementsCli.cmake`](../../../Tests/CMake/StatementsCli.cmake): where it bit
- [CMake's `file(WRITE)` writes CRLF on Windows](file-write-crlf-on-windows.md)
