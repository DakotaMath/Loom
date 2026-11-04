# M1 final review pass

> **Summary:** The final pass over `main` at 5626c88, after the review fixes (PRs #21–#27) and the spec amendments A1–A17 (PRs #20, #28). Three reviewers checked every original finding against the code, reviewed the new code, and checked the code against the spec. **No blockers.** Every FAIL and every medium item from the review is fixed and tested. What remains: one design question for Dakota, a short list proposed for one last PR before M1 is called done, and follow-ups.

**Date:** 2026-11-04 · **Commit:** 5626c88 · **Tests:** 269/269 on Windows (`ctest`), index check and tool tests pass.

## Reviewers and logs
| Reviewer | Scope | Log |
| --- | --- | --- |
| final-core | LoomCore except papers: formats, IDs, checks, status, validation, edits, hooks, new-project, git | [final/final-core.md](final/final-core.md) |
| final-papers-cli | Papers, keys, bibliography, the `loom` command, build and test infrastructure | [final/final-papers-cli.md](final/final-papers-cli.md) |
| final-spec | Conformance with `Specs/m1-core.md` and A1–A17, both ways; templates, wiki, `STATUS.md` | [final/final-spec.md](final/final-spec.md) |

Every reviewer read the code on `main`, not the PR descriptions, and ran `loom.exe` on Windows in scratch projects to confirm what it could.

## Original findings
| Slice | FAIL / needs-changes entries | Resolved | Deferred | Open (all low) |
| --- | --- | --- | --- | --- |
| Core (foundations, statements-proofs, status-validation) | 63 | 49 | 7 | 7 |
| Papers and CLI (papers, cli-build) | 57 | 46 | 7 | 4 |
| **Total** | **120** | **95** | **14** | **11** |

- **Every FAIL (F1–F16) and every medium item (M1–M8) is resolved**, each with a named test.
- **Deferred** items are the ones on the deferral list in [COORDINATOR-NOTES.md](COORDINATOR-NOTES.md) or deferred in a fix PR's body.
- **Spec questions:** the amendments answer those the fixes needed (final-core confirms S1, S2, S6, S7, S19–S22 and S26 among them). S3, S8, S24, S25 and S27 are deferred. **S4** (what "shaped like a tag" means for an alias) and **S5** (`new-project` creates the folder after its checks, not before as step 1 says) are still unanswered.

## Conformance
- A1–A17: each has code that matches the spec text.
- All 82 validation codes exist with the spec's severity and have a row in the fix-validation-problems how-to; all but `format-too-new` (which stops validation) are in `EveryProblem-expected.txt`.
- The worked example's statuses, counts, sentences and problems match exactly, as text and JSON.
- Every *(Dakota)* command refuses without a terminal; Dakota's typed `changes` text is never lost.
- Every row of the Verification table has a test (one partial: the every-problem project checks text, not JSON).
- 13 wording and consistency drifts, none changing meaning (final-spec's log).

## Design question for Dakota
**`level` isn't in the fingerprint.** A sketch with an agent pass and Dakota's pass becomes **proved** after `edit-proof --level complete`, with no new check and no click. The code follows the spec, but the status rises without anyone re-checking. Options:
- **(a) Put `level` in the fingerprint**, so a level change outdates the proof's checks like any other edit. One rule; changes every existing fingerprint once.
- **(b) Make raising the level Dakota's click**, like setting `failed` (A16).
- **(c) Leave it.**

## Proposed before calling M1 done (one PR)
| # | Item | Source |
| --- | --- | --- |
| 1 | A year-0 `date` in a check history passes and the check counts; A9 says `bad-value` in any file | final-core, final-spec (`Checks.cpp:66-94`) |
| 2 | A lettered key that differs from the rule's only in case gets a new letter on `rekey` (`MacDonald24a` → `Macdonald24c`); should keep its letter and fix the case | final-papers-cli (`keyFitsBase`, PaperEdits.cpp, ProjectContents.cpp) |
| 3 | `git-timeout` isn't used for every git call (install-hooks' later calls, new-project steps 4–5, validate's `ls-tree`/`cat-file`) | final-core, final-spec |
| 4 | Whitespace-only `title` and a stub's whitespace-only locator `at` validate clean | final-core, final-papers-cli |
| 5 | The kit's `CLAUDE.md` doesn't list marking a proof `failed` or `--outdate-checks` among Dakota's clicks | final-spec |
| 6 | Answer S4 and S5 in the spec (or defer them explicitly) | final-core, final-spec |
| 7 | The design question above, if Dakota chooses (a) or (b) | final-spec |
| 8 | Wording drifts and stale comments (final-spec's list), and `STATUS.md` | final-spec |

## Follow-ups (after M1)
- **`\cite` of an old key:** D9 says old keys keep working through the alias, but `refs.bib` has no entry under old keys, so BibTeX won't find `\cite{OldKey}`. A design point for M2, which builds LaTeX (e.g. biblatex `ids`, or a duplicate entry).
- `new-project`'s `core.hooksPath` message says "global" for a system-config setting.
- `refute` exits 3 if re-reading fails after a successful write (not reproducible in tests).
- Qt's DLLs aren't deployed next to `loom.exe`; the spec says "self-contained".
- The configure-time UI-link check misses generator expressions; CI's `repo` job uses `ubuntu-latest`; no Windows Release build in CI.
- `build-bib --out "Statements./refs.bib"` creates `Statements/refs.bib` when the folder doesn't exist yet (nothing overwritten).
- `--project ""` falls back to the current folder; `rekey` prints `K → K` when only the folder was renamed.
- Windows reserved names (`con.md`) pass `isProjectPath`; `runGit` might find a `git.exe` in the current folder (unverified).
- Smaller items in each log's Follow-ups section.

## See also
- [CONSOLIDATED.md](CONSOLIDATED.md): the original review
- [COORDINATOR-NOTES.md](COORDINATOR-NOTES.md): severities and the deferral list
- `Specs/m1-core.md`: the amendments table
