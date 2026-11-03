---
type: detail
tags: [glossary]
---

# Glossary

> **Summary:** Loom's terms, one line each. The full definitions are in `Specs/loom.md`.

**Last verified:** 2026-11-03 · commit c70b3e2

| Term | Meaning |
| --- | --- |
| **Project** | A research project: a folder Loom creates and owns, kept in local git |
| **Statement** | A node of the graph: one definition, lemma, theorem, conjecture or question, with no proof in the same file |
| **Proof** | An edge of the graph: from its premises, it proves one statement. A statement can have several |
| **Premises** | The statements a proof uses. Only premises affect status |
| **Stated using** | The statements (usually definitions) a statement needs just to be stated |
| **Status** | Computed from a statement's proofs and their checks: proved, conditional, unchecked, sketch, heuristic, open, cited, extracted, false |
| **Level** | A proof's own maturity, set by its author: sketch, heuristic, complete, failed |
| **Check** | A verdict on a proof by an agent or Dakota, appended to the proof's check history |
| **Fingerprint** | A short hash of what a check read (the proof, its premises, its statement); edits leave older checks behind |
| **Tag (ID)** | A statement's permanent four-character ID, e.g. `4KX7` |
| **Paper version** | One version of a paper (an arXiv version, the published one), with its own data, PDF and locators |
| **Stub** | The committed record of a paper's result: where it is in each version and a gist in our words |
| **Thread** | Plain LaTeX tracing a path through the graph, with prose; exports to a shareable paper |
| **Spec session** | A project workflow that explores what is provable near a target and proposes a statement |
| **Job** | One headless Claude Code run on its own branch, for one task |
| **Kept run** | A numerics run committed together with the exact code that made it |
| **Stale** | Made with older code than the current version: a run, a figure, a thread using it |
| **Record** | What a job did, attached to the objects it touched |

## See also
- Up: [Wiki index](INDEX.md)
