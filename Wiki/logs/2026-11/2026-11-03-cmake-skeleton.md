---
type: log
tags: [m1, build, ci]
---

# 2026-11-03: CMake project, targets, tests and CI

> **Summary:** Set up the CMake build for #3: `LoomCore` (a version string; Qt Core and toml++ only, enforced at configure time), `loom --version`, an empty `LoomApp` window, a Catch2 test, a Qt Test, a CI build job on `windows-2025-vs2026` and `ubuntu-24.04`, the Windows build how-to and a decision page per dependency.

**Session model:** Opus 5.5
**Issues and PRs:** #3, PR #13

## Done
- `CMakeLists.txt`, `CMakePresets.json` (`windows`: Visual Studio 18 2026, multi-config; `linux`: Ninja, Debug) and modules in `Source/CMake/`: compiler flags per the spec, toml++ (commit `3c71044e`) and Catch2 v3.16.1 by `FetchContent` with SHA-256, and the `LoomCore` link check.
- Targets: `LoomCore`, `loom`, `LoomAppLib` + `LoomApp`, `LoomCoreTests` (Catch2), `LoomAppTests` (Qt Test).
- Tests under `ctest`: the version (Catch2), that the window opens (Qt Test), `loom --version` and `--version --json` and the usage exit code, and seven cases of the link check, configured as a small project with stand-in Qt targets.
- CI `build` job: Qt 6.11.1 by install-qt-action, configure with warnings as errors, build, `ctest`. Green on both runners: 12 of 12 tests on each, MSVC 19.51 and GCC 13, no compiler warnings.
- Wiki: [how to build on Windows](../../how-to/build-on-windows.md); decisions [001 Qt 6](../../decisions/001-qt-6.md), [002 toml++](../../decisions/002-tomlplusplus.md), [003 Catch2](../../decisions/003-catch2.md); a [gotcha](../../gotchas/ci/aqtinstall-qt-6-11-windows.md).

## Learned
- aqtinstall 3.3.0, install-qt-action's default, can't install any Qt 6.11 for Windows: Qt split its Windows repository into one folder per compiler. The fix is on aqtinstall's main branch, unreleased; CI pins commit `076e1659` through `aqtsource` ([gotcha](../../gotchas/ci/aqtinstall-qt-6-11-windows.md)).
- Visual Studio 2026 ships CMake 4.3.1 and Ninja, so the Developer PowerShell is enough on Windows; no separate CMake install.
- CMake word-wraps `message(FATAL_ERROR)` text, so tests that match it must allow any whitespace between words.
- On Windows, Catch2's test discovery runs the test program at build time, so it needs Qt's DLLs: `catch_discover_tests(... DL_PATHS <Qt bin>)` covers both discovery and the test runs.
- Qt 6.11.2 and 6.11.3 are on Qt's servers; the spec pins 6.11.1.

## Next
- Dakota reviews PR #13 and does the Windows check.
- Then #4 (`loom new-project`).

## Waiting on Dakota
- PR #13: review and the Windows check.
