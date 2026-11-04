---
type: gotcha
tags: [cmake, tests]
---

# CMake's `CMAKE_MATCH_<n>` doesn't leave the function that matched

> **Summary:** `if(x MATCHES …)` sets `CMAKE_MATCH_1` and the rest only in the current scope, so a helper `function()` that does the match leaves the caller's `CMAKE_MATCH_1` empty. In a CMake test script, capture groups at the top level with `string(REGEX REPLACE …)` (or match there), not through a helper function.

**Last verified:** 2026-11-04 · commit 6e81905

## Symptom
A CMake test script (`Tests/CMake/StatementsCli.cmake`) checks `loom`'s output with a helper `expect_match()` and then reads `${CMAKE_MATCH_1}` to get the new statement's tag. The tag comes out empty, and the next step fails with a path like `Statements/.tex`.

## Cause
`function()` opens a new variable scope. `MATCHES` sets `CMAKE_MATCH_<n>` like any variable, in that scope, and they're gone when the function returns. (`macro()` has no scope of its own, so a macro would leak them, along with everything else it sets.)

## Fix
Keep the helper for the check, and take the capture at the top level:

```cmake
expect_match("new-statement --json" "${out}" "^{\"id\": \"([0-9A-Z]+)\", …")
string(REGEX REPLACE "^{\"id\": \"([0-9A-Z]+)\".*$" "\\1" theorem "${out}")
```

## See also
- Up: [Gotchas](../README.md)
- [`Tests/CMake/StatementsCli.cmake`](../../../Tests/CMake/StatementsCli.cmake): where it bit
