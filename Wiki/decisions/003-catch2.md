---
type: decision
tags: [dependencies, catch2, tests, build, m1]
---

# 003: Catch2 v3 for LoomCore and `loom` tests

> **Summary:** Tests of `LoomCore` and `loom` use Catch2 v3.16.1 (BSL-1.0), fetched by CMake `FetchContent` and pinned by SHA-256; `catch_discover_tests` registers each test case with CTest. It is used by tests only and never shipped. UI tests use Qt Test.

**Last verified:** 2026-11-03 · commit 7580298
**Status:** Accepted
**Decided by:** Dakota, in the design (`Specs/loom.md`, 2026-11-03) and the approved M1 spec (`Specs/m1-core.md`, 2026-11-03); this page records it for #3

## Context
Most of M1 is rules (status, fingerprints, keys, validation messages) that must be tested case by case against the spec. The design wants those tests to read like the spec.

## Options considered
| Option | Pros | Cons |
| --- | --- | --- |
| Catch2 v3 | Test names are free text, so a test can be named after the spec's rule; `SECTION`s share setup; CMake integration registers each case with CTest | v3 is a compiled library, not a single header, so it adds some build time |
| GoogleTest | Very widely used | Test names must be C++ identifiers; more ceremony per test |
| doctest | Fast to compile, Catch2-like | Smaller community; no gain big enough to depart from the design |
| Qt Test for everything | Already a dependency | Test names are slot names; built for UI and Qt types, clumsy for table-driven rule tests |

## Decision
Catch2 v3.16.1 (released 2026-10-08), fetched as GitHub's archive of the tag and checked by SHA-256 (`Source/CMake/LoomDependencies.cmake`). Test programs link `Catch2::Catch2WithMain`. On Windows, `catch_discover_tests` is given Qt's `bin` folder through `DL_PATHS`, because listing the tests runs the program, which needs Qt's DLLs.

## Why
It is the design's choice, and the reasons hold: tests named in the spec's words make it easy to see that every rule of the spec has a test.

**Licence:** BSL-1.0, used only by tests and never shipped; compatible with GPL v3.

## Consequences
- Each `TEST_CASE` is its own CTest test, so `ctest` shows and reruns failures one by one.
- Updating Catch2 is a deliberate change: update the URL, hash and this page.

## See also
- Up: [Decisions](README.md)
- [001: Qt 6](001-qt-6.md): Qt Test for the UI
- [`Specs/m1-core.md`](../../Specs/m1-core.md): *Toolchain*
