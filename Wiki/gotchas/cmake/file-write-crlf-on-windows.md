---
type: gotcha
tags: [cmake, tests, windows, line-endings]
---

# CMake's `file(WRITE)` writes CRLF on Windows

> **Summary:** On Windows, `file(WRITE)` and `file(APPEND)` turn every `\n` into `\r\n`, so a test file written as `"a\r\n"` holds `a\r\r\n`. Write plain `\n` when you want the platform's line ending, or use `file(CONFIGURE OUTPUT … CONTENT … NEWLINE_STYLE UNIX)` when the bytes must be exactly LF.

**Last verified:** 2026-11-04 · commit fa7ae66

## Symptom
A CLI test writes a `--changes-file` with `file(WRITE "${WORK}/changes.txt" "Step 2.\r\nStep 3.\r\n")` to check that Loom reads CRLF as LF. On Windows `loom add-check --json` prints `"changes": "Step 2.\u000d\nStep 3.\u000d\n"`: a carriage return survives. On Linux the test passes.

## Cause
CMake writes these files in text mode on Windows (seen with CMake 4.3.1, Visual Studio's build). Each `\n` in the content becomes `\r\n`, including the one after a `\r` you wrote yourself, which gives `\r\r\n`. Loom turns `\r\n` into `\n` once and leaves the first `\r`.

## Fix
- To test CRLF input, write plain `\n`: the file is CRLF on Windows and LF on Linux, and both must read the same.
- When a file must be LF on every platform:
  ```cmake
  file(CONFIGURE OUTPUT "${WORK}/a.txt" CONTENT "a\nb\n" NEWLINE_STYLE UNIX)
  ```
  `CONTENT` is expanded like `configure_file`: `${var}` and `@var@` are substituted.

## Dead ends
- Writing `\r\n` yourself: on Windows it becomes `\r\r\n`, which tests something else.

## See also
- Up: [Gotchas](../README.md)
- [`Tests/CMake/ChecksCli.cmake`](../../../Tests/CMake/ChecksCli.cmake): where it bit
- [CMake's `execute_process` garbles `loom`'s UTF-8 output on Windows](execute-process-encoding.md)
