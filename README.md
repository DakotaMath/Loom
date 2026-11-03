# Loom

Loom is a Windows desktop app for doing mathematical research together with AI agents.

A research project is a folder Loom creates and owns, kept in local git. Its core is a dependency graph: **statements** are nodes, **proofs** are edges ("from A₁…Aₙ we get B"), and every statement's status is computed from its proofs and their checks instead of being typed by hand. Around the graph, Loom handles the rest of the workflow:

- finding and reading papers, with reading sessions and a literature database that records every source;
- spec sessions that explore what is provable near a target;
- agent jobs that check proofs, write step-by-step versions and audit dependencies;
- threads that weave a path through the graph into a paper you can share;
- numerics with a Python editor and runs tied to the exact code that made them;
- nightly reviewers;
- Lean.

Agents run as Claude Code jobs. Everything an agent changes lands on its own branch and is shown as compiled math before you merge it.

## Status

**Design phase.** There is no code yet. The design is [`Specs/loom.md`](Specs/loom.md); the current state of work is [`STATUS.md`](STATUS.md).

## Repository layout

| Path | What |
| --- | --- |
| [`Specs/`](Specs/README.md) | Specs for the software. A build task needs an approved spec |
| [`Wiki/`](Wiki/INDEX.md) | Documentation: start-here pages, decisions, how-tos, gotchas, session logs |
| `Templates/` | The project kit Loom copies into every new project |
| `Tools/` | Small scripts for working on this repo, with tests |
| `Source/`, `Tests/` | The C++ code and its tests (from milestone M1) |

## Licensing

| What | Licence |
| --- | --- |
| The software: everything except `Wiki/`, `Specs/` and `Templates/` | [GNU GPL v3](LICENSE) |
| Documentation: `Wiki/` and `Specs/` | [CC BY 4.0](LICENSE-docs) |
| The project kit: `Templates/` | [CC0 1.0](Templates/LICENSE), so projects made with Loom carry no licence from it |

Loom is GPL v3 because it uses QScintilla, which is available under the GPL v3.
