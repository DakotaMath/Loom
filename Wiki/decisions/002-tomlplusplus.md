---
type: decision
tags: [dependencies, toml, tomlplusplus, build, m1]
---

# 002: toml++ for reading TOML, pinned to a commit

> **Summary:** `LoomCore` reads TOML with toml++ (MIT, header-only), fetched by CMake `FetchContent` from commit `3c71044e` (2026-11-01) and pinned by SHA-256, because the last tag, v3.4.0, is from 2023. Loom writes its TOML with its own writer, since toml++ sorts keys.

**Last verified:** 2026-11-03 · commit 7580298
**Status:** Accepted
**Decided by:** Dakota, in the design (`Specs/loom.md`, 2026-11-03) and the approved M1 spec (`Specs/m1-core.md`, 2026-11-03); the exact commit chosen by an agent for #3

## Context
Every Loom file has TOML: `loom.toml`, `paper.toml`, check histories, and the `+++` front matter of statements and proofs. The design named toml++. The M1 spec says to pin a commit rather than the v3.4.0 tag, and this page records which commit and why.

## Options considered
| Option | Pros | Cons |
| --- | --- | --- |
| toml++, tag v3.4.0 | A release | From 2023; parser fixes have landed since |
| toml++, a recent commit | Has the fixes; still header-only and CMake-native | Not a release, so we choose and record the commit ourselves |
| toml11 | Can keep key order | Not what the design chose; larger API |
| Our own parser | Full control | A parser is a project of its own, and easy to get subtly wrong |

## Decision
toml++ at commit [`3c71044e1e1bcfc50391d0d93e63609295ad3e18`](https://github.com/marzer/tomlplusplus/commit/3c71044e1e1bcfc50391d0d93e63609295ad3e18), fetched as GitHub's archive of that commit and checked by SHA-256 (`Source/CMake/LoomDependencies.cmake`). It links into `LoomCore` only, privately.

## Why
- It was the newest commit on toml++'s default branch on 2026-11-03, and its CI passed on Linux (GCC and Clang) and Windows (MSVC).
- It includes the parser fixes of 2026-11-01, among them reporting errors instead of asserting on malformed input (#321) and not scanning past an incomplete date-time (#314): both matter, because Loom reads files people and agents edit by hand.
- Pinning by SHA-256 means the build fails rather than silently using different code if the archive ever changes.

**Licence:** MIT, compiled into Loom; compatible with GPL v3.

## Consequences
- toml++ keeps tables in a `std::map`, so it writes keys alphabetically: Loom has its own writer ([gotcha](../gotchas/toml/tomlplusplus-sorts-keys.md)).
- Moving to a newer toml++ is a deliberate change: update the URL and hash, and this page.

## See also
- Up: [Decisions](README.md)
- [`Specs/m1-core.md`](../../Specs/m1-core.md): *Decisions this spec makes*, *Toolchain*
