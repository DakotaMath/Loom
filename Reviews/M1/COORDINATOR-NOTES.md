# M1 review: coordinator's check of the findings

A second pass over `CONSOLIDATED.md` by the coordinator, against the source at ee04f71. I read the code behind every FAIL and every medium NEEDS-CHANGES; the low items I sampled. Nothing was built or run here either.

## Confirmed as stated
Each of these holds up against the code, with the reviewer's severity:
- **F1 (high).** `createPaper` (PaperEdits.cpp:191-250) never checks for an existing `paper.toml`. `othersThan` skips entries that don't parse, and the write goes through `commitPapers` → `writeProjectFile`, which replaces silently. The "key disagrees with its folder" case is as real as the unparsed one.
- **F2/F8 (medium).** `installHooks` (Hooks.cpp:63) checks only that `rev-parse --git-path` succeeds. `runInstallHooks` (ValidateCommands.cpp:69-106) refuses only `FormatTooNew`. `findProject` (Cli.cpp:161-176) returns `--project` as given. Without `--project`, the folder walk does require `loom.toml`, so only `--project` (and `core.hooksPath`) are exposed. Recoverable (the old hook is kept as `.before-loom`), so medium rather than high.
- **P1: F3, F9, F10, F14.** The guards are explicitly `!x.isEmpty() && …`, and `title` uses `RequiredNonEmpty`, so the distinction exists and simply wasn't applied to `id`, `kind`, `proves`, `level` and the reference fields. Contradicts the spec where the field is "one of …".
- **P2: F10–F13.** PCRE2 without `DOLLAR_ENDONLY` is Qt's default, so `$` matches before a final `\n`.
- **P3: F15.** Traced the example: `.arg(file)` then `.arg(line)` replaces `%2` inside the file name. `%20` is marker 20 and survives, so the output is exactly `Smith1C%20Jones.pdf:1: …`.
- **F4, F5 (medium).** Spec line 457 gives `arxiv_id` as `YYMM.NNNNN`; nothing checks it. Spec line 522 says `Last, First`, and `PaperField::text` (Papers.cpp:623) writes `Last` alone when `first` is empty, so BibTeX splits a multi-word surname.
- **M1, M2, M4, M8 (medium).**
  - M1: `carryOut` undoes only the rename it's on.
  - M2: `buildBibliography` checks `--out` only for path safety.
  - M4: `bracesBalanced` skips any character after `\`.
  - M8: `runAddCheck` prompts, then calls `addCheck`, which is the first thing to resolve the proof.

## Severity I'd change
| Item | Logged | I'd say | Why |
| --- | --- | --- | --- |
| F6, F7 (`help`, unknown subcommand) | FAIL medium | FAIL **low** | Real departures from the spec's table and its `--json` error rule, but no data or trust at stake. |
| F16 (check order) | FAIL low | FAIL low, **practically unreachable** | It matters only when two entries have the *same timestamp to the second*. Rule 6 (Checks.cpp:286-287) and the merge tie-break are the only places position counts. Two such entries need a merge from two machines in the same second, and then a hand edit to swap them. Worth the cheap fix (same relative order), not urgent. |
| M6 (`readCommit`) | NC medium | NC **low** | Git refuses its own commands in a "dubious ownership" repo, including `commit`, so the commit guard isn't weakened. Only a plain `loom validate` is less strict. Still worth separating exit 1 (no HEAD) from 128. |
| F4 (`arxiv_id`) | FAIL medium | FAIL **low** | The damage is one malformed `eprint` in a generated `.bib`; nothing in the project is lost or misreported. |

## What I'd defer past M1 (won't-fix for now)
These are logged correctly but cost more than they buy at this stage:
- **Unicode case mapping** of `ı`, `ſ` and the Kelvin sign in `normaliseTag`/`normaliseProofId`. Nobody types these into an ID.
- **`findCycles` recursion depth.** It needs a premise chain thousands of statements long.
- **Linear scans** in `premiseCycles`/`ContentsChecker`, and the unused temporary folder in `validateProject`. Not measurable at project sizes M1 sees.
- **`FieldReader.h` including `StatementsAndProofs.h`** for `FieldLines`. Layering only.
- **`Utf8Console` after Ctrl+C**, and mintty pipes. Better as gotcha pages than code.
- **`update_indexes.py` and `|` in summaries.** A workaround is to not write `|` in a summary.
- **The ~25 test-gap entries in general.** Fold the missing tests into the batch that fixes each behaviour, and drop the rest unless they guard a FAIL.

## How to fix: reproduce first
Every finding comes from reading, not running. Each fix PR should therefore start with a test that fails on `main`:
- F1: an unparsed entry, a key/folder mismatch, and (Windows) a case-only collision.
- F2: `install-hooks --project` on a non-project, and `core.hooksPath` set.
- F16: a same-timestamp swap.
- P1/P2: `""` and `"x\n"` values.
- P3: a `%2` file name.

A fix whose test can't be made to fail first should go back to the review as "not reproduced". Under hard rule 7, a new test that lands with its fix still needs listing under ⚠️ Flags.

## Recommended answers to the seven blocking spec questions
These are suggestions for Dakota; nothing here is decided.
- **S1 `core.hooksPath`:** refuse, with a message naming the hooks folder. Don't write outside the repository's own git directory, since a hook in `.git/hooks` that git never runs would be a silent false sense of safety. Same for `git lfs install --local`.
- **S2 empty strings:** yes, an error: `missing-field` worded "`{field}` is empty". `FieldChecker::stringField` already behaves this way for `loom.toml`.
- **S9 key case:** keys collide when equal ignoring case, for lettering, `paper-key-duplicate` and the `new-paper` existence check.
- **S10 `arxiv_id`:** accept `YYMM.NNNN` (to 1412), `YYMM.NNNNN` (from 1501) and old-style `archive/NNNNNNN`; never a `vN`; a malformed one is `bad-value`.
- **S12 empty `first`:** write the surname braced, `{Garcia Lopez}`, so BibTeX keeps it whole.
- **S13 braces:** count every brace, escaped or not, as BibTeX does. Flag the test change under hard rule 7.
- **S21 append-only:** add "in the same order" to the validation sentence.

## Anything the reviewers missed?
In my pass over the write path (`writeProjectFile`, `EditSession::begin`, the lock, `addedErrors`) and the hook script (`preCommitHook`, `shellQuoted`, `Templates/hooks/pre-commit`), I found nothing new. Quoting, atomic writes and lock ordering all look right. One observation, not a finding: `addedErrors` matches problems by code, file and message. So an edit that fixes one error and adds an identically worded one elsewhere in the same file would pass. That's acceptable by design.
