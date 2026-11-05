---
type: how-to
tags: [validate, problems, pre-commit, checks, m1]
---

# How to fix what `loom validate` reports

> **Summary:** Run `loom validate` (or read what the refused commit printed), find each problem's code in the tables below, and fix the file and line it names; errors block a commit, warnings don't. With `--json` every problem carries its `code`. After a clone, run `loom install-hooks` so commits are checked.

**Last verified:** 2026-11-04 · commit 5626c88

## Before you start
- A Loom project (a folder with `loom.toml`), and `loom` on `PATH`.
- After cloning a project, run `loom install-hooks` once: git doesn't clone hooks, so until then nothing checks your commits. `loom new-project` does this itself.

## Steps
1. In the project, run:
   ```
   loom validate
   ```
   Each problem is one line on stderr, `<file>:<line>: <severity>: <message>`, errors first, then warnings, each in file order; then a count line. Exit code 0 means no errors (warnings allowed), 1 means errors, 3 means Loom couldn't check the project at all (no `loom.toml`, a newer format, an unreadable file, no `git`, or git failing to read the last commit, e.g. in a repository it refuses to use for "dubious ownership").
2. If a commit was refused, the hook ran `loom validate --staged`, which checks what git has **staged**, not the files on disk. Fix the file, then `git add` it again before committing.
3. Look up each problem below by what the message says (or by `code` in `loom validate --json`), and fix it. Change Loom's files with `loom` commands where one exists (`loom edit-statement`, `loom edit-proof`, `loom new-tag`, `loom rekey`, …); a plain edit is fine as long as `loom validate` accepts it.
4. Run `loom validate` again until it reports no errors.

### Files and syntax
| Code | Sev. | What it means | How to fix |
| --- | --- | --- | --- |
| `not-utf8` | error | The file has bytes that aren't UTF-8, often from an editor saving in a Windows code page. | Re-save the file as UTF-8. The message gives the offset of the first bad byte. |
| `bom` | warning | The file starts with a byte-order mark. Harmless; Loom drops it when it next writes the file. | Nothing, or re-save as "UTF-8" rather than "UTF-8 with BOM". |
| `toml-syntax` | error | The TOML (front matter, `loom.toml`, `paper.toml`, a stub or a checks file) doesn't parse. | Fix the syntax at the line given: usually an unclosed `"`, a missing `=`, or a string spanning lines. |
| `front-matter-missing` | error | A statement, proof or verbose proof doesn't start with a `+++` line. | Put the front matter back at the very top, between two `+++` lines. |
| `front-matter-unclosed` | error | The front matter opens with `+++` but no later line is exactly `+++`. | Add the closing `+++` line (nothing else on it, no spaces). |
| `body-empty` | error | Nothing but whitespace below the front matter. | Write the statement or proof. A failed proof's body says what was tried and why it breaks. |
| `unknown-field` | error | A field Loom doesn't know, usually a misspelling. The message lists the right fields. | Rename it to the field you meant, or remove it; front matter holds data, prose goes in the body. |
| `missing-field` | error | A required field is absent (or a paper folder has no `paper.toml`), or a field is present but empty (`refuted_by = ""`, or only spaces: `title = "   "`): an empty value counts as a missing one, except in free text such as `differences` or a name's `first`. | Add the field, or give the empty one a value. An optional field with nothing to say is left out, never written as `""`. The spec's tables (`Specs/m1-core.md`) say what each file needs. |
| `wrong-type` | error | A field has the wrong kind of value: a string where a list belongs, a date without a time, … | Write the value in the type the message names, e.g. `tags = ["model-system"]`, `date = 2026-11-04T15:32:10-06:00`. |
| `bad-value` | error | The value isn't one of the allowed ones (a misspelt `kind`, `level`, …), or a date is in the year 0 (`0000-01-01`), which Loom can't hold; a check dated so never counts. | Use one of the values the message lists. |
| `bad-path` | error | A path in a file is absolute, uses `\`, or has `..`; or a file resolves outside the project through a link. | Write it project-relative with `/` (`Records/2026-11/job-0142.md`). Move linked files into the project. |
| `stray-file` | error | A file sits where Loom keeps only its own files (`Statements/`, a proof folder, a paper folder). | Move it out (notes belong in a paper's `notes.md` or outside these folders), or delete an editor backup like `4KX7.tex~`. |
| `unknown-top-level` | warning | A file or folder at the top of the project that no milestone uses. Names later milestones use (`Threads/`, `Tasks/`, …) are skipped. | Move it into a folder Loom knows, or remove it. Scratch files belong in `.loom/`, which git ignores. |
| `deleted-since-commit` | warning | A statement, proof or stub was in the last commit and its file is gone. Tags are permanent. | Restore it (`git restore <file>`). Retire a statement with `superseded_by`, a proof with `level = "failed"` (that is Dakota's click: `loom edit-proof <id> --level failed` at a terminal). |

### The project (`loom.toml`)
| Code | Sev. | What it means | How to fix |
| --- | --- | --- | --- |
| `format-too-new` | stops | A newer Loom made this project; this Loom won't read or write it. | Update Loom. |
| `preamble-missing` | error | A file in `tex.preamble` doesn't exist. | Create it, or take it out of the list. |
| `tag-syntax` | error | A tag ID in `[[tags]]` isn't lowercase words joined by single hyphens. | Rename it, e.g. `energy-estimates`, and update the statements that use it. |
| `tag-duplicate` | error | Two `[[tags]]` entries have the same `id`. | Remove or rename one. |
| `tag-unknown` | error | A statement, stub or paper uses a tag `loom.toml` doesn't declare. | `loom new-tag <tag> --kind … --title …`, or fix the spelling. |

### IDs and references
| Code | Sev. | What it means | How to fix |
| --- | --- | --- | --- |
| `id-syntax` | error | Something written where a tag belongs isn't one: tags are four characters from `0–9` and `A–Z` without I, L, O, U. | Write the tag the message suggests (an `O` is usually `0`, an `I` or `L` is `1`). |
| `id-mismatch` | error | A file's `id` isn't its file name. | Make them agree. Tags never change after merging, so usually the `id` field is the one to fix. |
| `id-duplicate` | error | Two files claim the same tag. | Keep the tag on one; the other needs its own (create it again with `loom new-statement`). |
| `alias-invalid` | error | An alias has whitespace or `{}[],"`, looks like a tag (four characters the command line would read as one, such as `thm1` or `eq12`), or is over 100 characters. | Change or drop it. |
| `alias-duplicate` | error | Two statements or stubs share an alias (or an alias is also a tag). In `paper.toml`: an old key in `aliases` is the paper's own key, or another of its old keys, in another case (`smithjones24` beside `key = "SmithJones24"`); keys compare ignoring case. | Keep it on the one it names. In `paper.toml`, delete the extra spelling from `aliases`. |
| `ref-unknown` | error | Front matter names a tag no statement or stub has. | Fix the tag, or create the statement first. |
| `ref-alias` | error in front matter, warning in a body | An old name (alias) is used where the tag should be. | Write the tag the message gives. In a body it still works, but the tag is clearer. |
| `ref-not-listed` | warning | The body `\ref`s or `\paperref`s a statement that `premises` (a proof) or `stated_using` (a statement) doesn't list. A proof's refs to the statement it proves, and to definitions and other statements without a status, don't count. | If the proof relies on it, add it: `loom edit-proof <id> --add-premise <tag>` (or `--add-stated-using` for a statement). Otherwise rephrase the reference. |
| `ref-dangling` | warning | `\ref{…}` points at nothing: not a tag, an alias, or a `\label` in any body. Or `\paperref{…}` points at no paper stub (a `\label` doesn't count for it). | Fix the label, or add the `\label` it means. For `\paperref`, write the stub's tag. |
| `paperref-not-stub` | error | `\paperref{…}` names a statement of this project (by tag or alias). `\paperref` cites a paper's result, so it must name a stub. | Write `\ref{<tag>}` for a statement of the project; keep `\paperref` for stubs. |
| `cite-alias` | warning | `\cite{…}` uses a paper's old key. It still works, through the alias. Or it spells a key (or old key) in another case, `\cite{smithjones24}` for `SmithJones24`: the same paper to Loom, but BibTeX matches keys exactly, so it wouldn't be found. | Write the current key the message gives, when you're next editing the text (it changes the fingerprint, so do it before checks, not after). |

### Statements
| Code | Sev. | What it means | How to fix |
| --- | --- | --- | --- |
| `stated-using-self` | error | A statement lists itself in `stated_using`. | Remove it. |
| `stated-using-cycle` | error | `stated_using` goes round in a circle. | Break the circle: one of them doesn't really need the other to be stated. |
| `superseded-invalid` | error | `superseded_by` names nothing, itself, a paper stub, or leads round in a circle. | Point it at the statement of the project that replaces this one. A paper's result isn't one: state it as a statement first. |
| `refuted-incomplete` | error | Only one of `refuted_by` and `refuted_on` is set. | Refuting is Dakota's click: `loom refute <id> --by <proof-id>` at a terminal sets both. Remove a half-set pair. |
| `refuted-unknown` | error | `refuted_by` names a proof that doesn't exist. | Fix the proof ID. |
| `refuted-self` | error | `refuted_by` names one of the statement's own proofs, which argues for it, not against it. | Name a proof of another statement, such as one stating its negation. `loom refute` refuses this too. |
| `refuted-no-status` | error | A definition, question, remark or example is marked refuted. | Remove `refuted_by` and `refuted_on`; only provable kinds can be false. |
| `refuted-but-proved` | warning | The statement is marked false, yet one of its complete, checked proofs, whose premises all hold, would prove it. | One of them is wrong: look for the mistake in the proof or in the refutation, then fail the proof's check or reconsider the refutation. |
| `conjecture-proved` | warning | A conjecture is now proved. | `loom edit-statement <id> --kind theorem` (or lemma, …) when you're ready. |

### Proofs
| Code | Sev. | What it means | How to fix |
| --- | --- | --- | --- |
| `proof-folder-unknown` | error | `Proofs/<tag>/` has no statement `<tag>`. | Move the proofs to the right folder, or restore the statement. |
| `proof-id-mismatch` | error | A proof's `id` isn't its file name, or doesn't start with its folder's tag. | Make the `id` equal the file name without `.tex`. |
| `proves-mismatch` | error | A proof's `proves` isn't the folder it sits in. | Fix `proves`, or move the file to the folder of the statement it proves. |
| `proof-of-no-status` | error | A proof of a definition, question, remark or example. | Only lemmas, propositions, theorems, corollaries and conjectures have proofs. Change the statement's kind, or move the argument into its body. |
| `proof-of-stub` | error | A proof of a paper's result. Stubs are cited, not proved here. | If you need your own proof, state the result as a statement of the project and prove that. |
| `premise-self` | error | A proof lists the statement it proves as a premise. | Remove it. |
| `premise-duplicate` | error | A premise is listed twice. | Remove the repeat. |
| `premise-no-status` | error | A premise is a definition (or another kind without a status). | Move it to the statement's `stated_using`. |
| `premise-cycle` | error | Premises go round in a circle: no statement in it can be proved through it. | Prove one statement in the circle without the next. For an equivalence, keep one statement whose proof covers both directions. |
| `premise-false` | warning | A premise has been refuted, so this proof proves nothing. | Find another route, or ask Dakota to mark the proof `failed` (his click). |
| `premise-superseded` | warning | A premise has been superseded. | Use the statement that replaced it (`loom edit-proof --remove-premise … --add-premise …`); the proof's checks then need renewing. |
| `verbose-orphan` | error | A verbose proof whose proof doesn't exist. | Rename it to match its proof, or remove it. |
| `verbose-stale` | warning | The proof changed after its verbose version was written. | Rewrite the verbose version from the proof as it is now. |
| `checks-orphan` | error | A check history whose proof doesn't exist. | Restore the proof. Checks are never deleted. |

### Checks
| Code | Sev. | What it means | How to fix |
| --- | --- | --- | --- |
| `check-by` | error | `by` isn't `dakota` or `agent:<model>`. | Fix the author. Add checks with `loom add-check`, which writes them right. |
| `check-verdict` | error | The verdict isn't `pass`, `needs-changes` or `fail`. | Use one of the three. |
| `check-changes-missing` | error | Dakota's `needs-changes` doesn't say what to change. | Add `changes`; `loom add-check --by dakota --verdict needs-changes` asks for it. |
| `check-changes-unexpected` | error | `changes` on a check that isn't `needs-changes`. | Remove `changes`. |
| `check-fingerprint` | error | The fingerprint isn't 12 characters from `0–9` and `a–f`. | Take it from `loom fingerprint <proof-id>`, or let `loom add-check` fill it in. |
| `check-record-missing` | error | An agent's check has no `record`. | Every agent check links its reasoning: give `--record`. |
| `check-record-not-found` | warning | A check's `record` names a file that doesn't exist (normal before M4 writes records). | Commit the record, or fix the path. |
| `check-order` | error | A check is dated before the one above it. | Checks are append-only and in date order. If you added one by hand, move it to its place; never edit the dates of committed checks. |
| `check-duplicate` | error | The same entry appears twice. | Remove the later copy (only if it isn't committed yet). |
| `check-changed` | error | A check that is in the last commit was edited, removed or moved. Committed checks keep their order relative to each other, even when two have the same date; new ones may sit between them, as a merge interleaves. | Restore the file (`git restore <file>`) and add a new check instead. |

### Papers and stubs
| Code | Sev. | What it means | How to fix |
| --- | --- | --- | --- |
| `paper-key-mismatch` | error | `key` in `paper.toml` isn't the folder's name. | `loom rekey <key>` renames both together. |
| `paper-key-rule` | warning | The key isn't what the key rule gives (the authors or the year changed), or it is unlettered while another paper has the same base with a letter (`SmithJones24` beside `SmithJones24a`: a shared base is always lettered), or it differs from the rule's key only in case (`smithjones24a`; `rekey` keeps the letter and fixes the spelling). Also when no key can be made: the paper has no year, or a surname has no Latin letters. | `loom rekey <key>`. The old key stays as an alias, so `\cite` keeps working. With no key to make, add the year, or write the surname in Latin letters as the paper prints it. |
| `paper-key-duplicate` | error | Two papers share a key or an old key. Keys that differ only in case (`MacDonald20`, `Macdonald20`) are the same key: they're folder names, and Windows ignores case in those. | Rekey one of them; `loom new-paper` letters shared keys (`SmithJones24a`, `…b`) itself. |
| `field-shape` | error | A bibliographic value isn't `{ value = …, source = "…", verified = … }`. | Write it in that shape, with where it came from. |
| `field-source` | error | A field has no `source`, or its source is a link that isn't `https://` (an old `http://` MathSciNet link). | Add the URL or the path inside `local/` it was read from; give an `https://` link. |
| `field-verified` | error | `verified` isn't a date or `false`. | Agents write `false`; a date is Dakota's click. |
| `field-not-for-type` | error | A field that this version's kind and the work's type don't have. | Remove it, or fix the work's `type`. |
| `field-expected` | warning | A field this type of work usually has is missing. | Add it if the work has one; ignore it if not (many series have no volume). |
| `version-id` | error | A version's `id` doesn't fit its kind: `arxiv-v<N>`, `published` or `ed<N>`. | Rename the version, and the places that name it (`cite`, readings, stub locators). |
| `version-duplicate` | error | Two versions have the same `id`. | Rename or merge them. |
| `arxiv-version-mismatch` | error | `arxiv-v<N>`'s `arxiv_version` isn't N. | Make them agree. An `arxiv_id` with its version on the end (`2401.99999v2`) is `bad-value`: the version goes in `arxiv_version`, the identifier without it. |
| `cite-unknown` | error | `cite` names a version the paper doesn't have. | Point it at one of the paper's versions. |
| `version-unknown` | error | A reading or a stub names a version the paper doesn't have. | Fix the version, or add it to the paper. |
| `session-missing` | warning | A reading links a session folder that doesn't exist. | Commit the session, or fix the link. |
| `pdf-path` | error | A version's `pdf` isn't inside the paper's `local/`. | Move the PDF to `Papers/<Key>/local/` and point there. |
| `authors-missing` | error | The work has no authors (and no editors). | Add them; for an edited book with no authors, add `editors`. |
| `stub-locator-missing` | error | A stub has no locator for the version it was extracted from. | Add it to `[locators]`: where in that version the result is. |
| `stub-paper-premise` | error | `paper_premises` names something that isn't a stub of the same paper. | Keep only the same paper's stubs there. Our own statements go in premises of our proofs. |
| `stub-gist-empty` | error | A stub has no gist. | Write the result in our own words. |

## Check it worked
- `loom validate` ends with `No problems.`, or lists only warnings you've decided to keep, and exits with code 0.
- `git commit` goes through without the hook refusing it.

## If it goes wrong
- **The commit is refused, but `loom validate` finds no errors:** the fix isn't staged. `git add` the fixed files; the hook checks what's staged (`loom validate --staged` shows the same).
- **"Loom's checks couldn't run because `loom` wasn't found":** the hook can't find `loom`. Put Loom's folder on `PATH`, or run `loom install-hooks` again so the hook points at this copy of `loom`.
- **`loom install-hooks` says a hook is already kept as `pre-commit.before-loom`:** another tool installed its own pre-commit hook while one was already kept. Merge the two by hand or move one out of `.git/hooks/`, then run `loom install-hooks` again.
- **A problem you're sure is wrong:** note the command, the file and the message, and raise it with Dakota; don't edit around Loom's checks.

## See also
- Up: [How-to](README.md)
- [`Specs/m1-core.md`](../../Specs/m1-core.md), *Validation*: the rules and messages this page explains
