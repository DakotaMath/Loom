# Session logs

One log per agent session, filed in a folder per month (`2026-11/`), newest first below. Logs are records: never edited after the session. Anything durable a session learned also goes into a wiki page. Template: [`_TEMPLATE.md`](_TEMPLATE.md).

<!-- BEGIN GENERATED: do not edit below; run python Tools/update_indexes.py -->
### 2026-11

| Page | Summary |
| --- | --- |
| [2026-11-03: The M1 core spec](2026-11/2026-11-03-m1-core-spec.md) | Wrote `Specs/m1-core.md`, the M1 spec for formats, IDs, checks, status, papers, validation, the `loom` command and the toolchain, with ten departures from the design; Dakota answered its four open questions and approved it. |
| [2026-11-03: The design chat](2026-11/2026-11-03-design-chat.md) | Designed Loom with Dakota in one conversation, wrote the design as `Specs/loom.md` (draft, no open questions), and set up this repo's skeleton. |
| [2026-11-03: CMake project, targets, tests and CI](2026-11/2026-11-03-cmake-skeleton.md) | Set up the CMake build for #3: `LoomCore` (a version string; Qt Core and toml++ only, enforced at configure time), `loom --version`, an empty `LoomApp` window, a Catch2 test, a Qt Test, a CI build job on `windows-2025-vs2026` and `ubuntu-24.04`, the Windows build how-to and a decision page per dependency. |
<!-- END GENERATED -->

Up: [Wiki index](../INDEX.md)
