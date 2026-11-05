# Specs

Specs for the software, one per system or milestone, starting with the whole design in [`loom.md`](loom.md). A Build task needs its spec here, marked approved by Dakota, with no open questions. Template: [`_TEMPLATE.md`](_TEMPLATE.md).

These are specs for building Loom. Specs inside a research project (spec sessions) are a different thing, described in `loom.md`.

<!-- BEGIN GENERATED: do not edit below; run python Tools/update_indexes.py -->
| Page | Summary |
| --- | --- |
| [Spec: Loom, the design](loom.md) | Loom is a Windows desktop app (C++, Qt 6) for doing mathematical research with AI agents. A research project is a folder Loom creates and owns, kept in local git. Its core is a graph: **statements** are nodes, **proofs** are edges ("from A₁…Aₙ we get B"), and a statement's status is **computed** from its proofs, never typed by hand. Around the graph sit the workflows: gathering and reading papers, spec sessions that propose what to prove next, per-proof agent jobs, threads that weave a path through the graph into a shareable paper, numerics with kept runs, scheduled reviewers and Lean. Agents run as Claude Code jobs on Dakota's subscription; reviewers use the Claude API. Everything an agent changes lands on its own branch and is reviewed as compiled math before Dakota merges it. This page is the design that the per-system specs will split out from. |
| [Spec: M1, the core formats, rules and toolchain](m1-core.md) | Everything `LoomCore` and the `loom` command do in M1, precisely enough to build without guessing: the folder `loom new-project` creates, `loom.toml`, the `+++` TOML front matter, statements, proofs, IDs, check histories and their fingerprints, the status rules with a worked example, paper entries, stubs, the key rule and the BibTeX it produces, every check `loom validate` makes with its message, every `loom` subcommand, and the toolchain (MSVC, CMake, Qt 6, toml++, Catch2) with the CI matrix. Departures from `Specs/loom.md` are listed in one place. |
| [Spec: M2, statements and review](m2-statements-review.md) | Everything `LoomApp`, `LoomCore` and `loom` do in M2, the milestone that replaces TexViewer's daily use: the app shell (start screen, the side bar of tabs, Settings, *Check my setup*), how the app calls `LoomCore` and turns Dakota's clicks into its confirmations, building LaTeX (the wrapper document, three modes, verbose, `\paperref`, errors mapped to source lines, SyncTeX, `loom build`, builds of `HEAD`), the Statements tab (list, editor, IDs, status and checks), never losing text, the Review tab and the git panel, with a TexViewer parity table, the dependencies, a demo project and how the UI is tested. |
<!-- END GENERATED -->

Up: [README](../README.md)
