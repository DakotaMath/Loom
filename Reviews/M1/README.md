# M1 code review

A review of Milestone 1 (all code on `main` at ee04f71, after PR #19 was merged) before M1 is called done. Five reviewers each own one slice of the code base and write one log here. The consolidated review is `CONSOLIDATED.md`.

| Reviewer | Log | Owns |
| --- | --- | --- |
| foundations | `foundations.md` | Ids, FrontMatter, FieldReader, TomlFields, TomlWriter, ProjectToml, ProjectFiles, NewProject, Problem, Version, Git, Hooks, Fingerprint |
| statements-proofs | `statements-proofs.md` | StatementsAndProofs, Edits, EditSession, Checks, ProofChecks, Cycles |
| papers | `papers.md` | Papers, PaperKeys, PaperEdits, Bibliography |
| status-validation | `status-validation.md` | ProjectContents, Status, Validation |
| cli-build | `cli-build.md` | LoomCli, LoomApp, CMake files, CI, Tools, Templates, `Tests/CMake`, `Tests/Projects` |

Each reviewer also reviews the tests in `Tests/LoomCoreTests/` for its own code.

## Log format

Every reviewer's log follows this shape.

```markdown
# M1 review: <reviewer>

**Scope:** <files>
**Summary:** <counts of PASS / NEEDS-CHANGES / FAIL, and the two or three findings that matter most>

## <File or file pair>

### `<symbol>` (<path>:<first line>-<last line>)
**Verdict:** PASS | NEEDS-CHANGES | FAIL
**Severity:** high | medium | low            (omit for PASS)
**Spec:** <m1-core.md section, if one applies>
**Finding:** <what is wrong, with the input that shows it>   (omit for PASS)
**Change:** <the concrete change to make>                     (omit for PASS)
**Tests:** <covered / which case is missing>

## Cross-boundary notes
Messages sent to and received from other reviewers, and what came of them.

## Spec issues
Places where the spec is wrong, silent or contradicted by the code.
```

**Verdicts.** FAIL: the code is wrong: it breaks the spec, a hard rule in `CLAUDE.md`, or gives the wrong result for some input. NEEDS-CHANGES: it works but should change: a missing test, a missing or wrong doc comment, an unhandled edge case that the spec doesn't cover, fragile or duplicated code. PASS: nothing to change. A PASS entry may be one line.

A "code block" is a function, a method, a class or struct declaration, or a test file (one entry per test file, listing missing cases).
