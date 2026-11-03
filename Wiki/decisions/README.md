# Decisions

One page per decision, numbered from `001`, with the options weighed and why. Template: [`_TEMPLATE.md`](_TEMPLATE.md). The decisions made while designing Loom are listed in the design spec's table, [Decisions made in the design chat](../../Specs/loom.md#decisions-made-in-the-design-chat-2026-11-03); new ones get a page here.

<!-- BEGIN GENERATED: do not edit below; run python Tools/update_indexes.py -->
| Page | Summary |
| --- | --- |
| [001: Qt 6.11 for the UI, and Qt Core in LoomCore](001-qt-6.md) | Loom uses Qt 6.11.1 (LGPL v3): Widgets, Gui and Test for `LoomApp` and its tests, and Qt Core alone in `LoomCore`, which gets SHA-256, Unicode normalisation, atomic writes, processes and random numbers from it without another library. |
| [002: toml++ for reading TOML, pinned to a commit](002-tomlplusplus.md) | `LoomCore` reads TOML with toml++ (MIT, header-only), fetched by CMake `FetchContent` from commit `3c71044e` (2026-11-01) and pinned by SHA-256, because the last tag, v3.4.0, is from 2023. Loom writes its TOML with its own writer, since toml++ sorts keys. |
| [003: Catch2 v3 for LoomCore and `loom` tests](003-catch2.md) | Tests of `LoomCore` and `loom` use Catch2 v3.16.1 (BSL-1.0), fetched by CMake `FetchContent` and pinned by SHA-256; `catch_discover_tests` registers each test case with CTest. It is used by tests only and never shipped. UI tests use Qt Test. |
<!-- END GENERATED -->

Up: [Wiki index](../INDEX.md)
