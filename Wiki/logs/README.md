# Session logs

One log per agent session, filed in a folder per month (`2026-11/`), newest first below. Logs are records: never edited after the session. Anything durable a session learned also goes into a wiki page. Template: [`_TEMPLATE.md`](_TEMPLATE.md).

<!-- BEGIN GENERATED: do not edit below; run python Tools/update_indexes.py -->
### 2026-11

| Page | Summary |
| --- | --- |
| [2026-11-04: Statements, proofs and IDs](2026-11/2026-11-04-statements-proofs.md) | Built #5: `LoomCore` reads and writes statement, proof and verbose proof files (TOML front matter around a body kept byte for byte), checks every field and the references between files, generates tags and proof letters, and creates and edits statements, proofs and tags behind the write lock; `loom` gains `new-tag`, `new-statement`, `edit-statement`, `new-proof` and `edit-proof`. All 82 `ctest` tests pass on Windows. |
| [2026-11-04: Check histories and fingerprints](2026-11/2026-11-04-checks.md) | Built #6: `LoomCore` reads and writes each proof's append-only `.checks.toml`, computes fingerprints as the spec says, works out which checks count and whether a proof is checked (every row of the verdict table is a test), refuses edits that would outdate a checked proof's checks unless Dakota confirms, and merges two check lists; `loom` gains `fingerprint`, `add-check`, `merge-checks` and `--outdate-checks`. All 100 `ctest` tests pass on Windows. |
| [2026-11-03: `loom new-project`, opening a project, path safety](2026-11/2026-11-03-new-project.md) | Built #4: `LoomCore` creates a project folder from the compiled-in project kit with its first commit, opens a project by reading `loom.toml` with every problem the spec lists for it, and checks every path it reads or writes stays inside the project; `loom new-project` exposes it. All 34 `ctest` tests pass on Windows. |
| [2026-11-03: The M1 core spec](2026-11/2026-11-03-m1-core-spec.md) | Wrote `Specs/m1-core.md`, the M1 spec for formats, IDs, checks, status, papers, validation, the `loom` command and the toolchain, with ten departures from the design; Dakota answered its four open questions and approved it. |
| [2026-11-03: The design chat](2026-11/2026-11-03-design-chat.md) | Designed Loom with Dakota in one conversation, wrote the design as `Specs/loom.md` (draft, no open questions), and set up this repo's skeleton. |
| [2026-11-03: CMake project, targets, tests and CI](2026-11/2026-11-03-cmake-skeleton.md) | Set up the CMake build for #3: `LoomCore` (a version string; Qt Core and toml++ only, enforced at configure time), `loom --version`, an empty `LoomApp` window, a Catch2 test, a Qt Test, a CI build job on `windows-2025-vs2026` and `ubuntu-24.04`, the Windows build how-to and a decision page per dependency. |
<!-- END GENERATED -->

Up: [Wiki index](../INDEX.md)
