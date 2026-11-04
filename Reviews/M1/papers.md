# M1 review: papers

**Scope:** `Source/LoomCore/` Papers.h/.cpp, PaperKeys.h/.cpp, PaperEdits.h/.cpp, Bibliography.h/.cpp; `Tests/LoomCoreTests/` PapersTests.cpp, PaperKeysTests.cpp, PaperEditsTests.cpp, BibliographyTests.cpp, PaperFixture.h.
**Summary:** 62 PASS / 24 NEEDS-CHANGES / 5 FAIL. The two low FAILs not listed below are regexes ending in `$`, which accept a trailing newline (Papers.cpp:352-353, 451). The findings that matter most:
1. **`createPaper` can overwrite an existing `paper.toml`** (PaperEdits.cpp:191-250, FAIL high). It happens when the existing entry doesn't parse, when its `key` disagrees with its folder, or on Windows when the two keys differ only in case (`MacDonald20` / `Macdonald20`). Nothing checks that the target folder is free, so user text is lost.
2. **BibTeX authors with an empty `first`** come out as `{Garcia Lopez}`, without the spec's `Last, First` comma, so BibTeX reads the name as First=Garcia, Last=Lopez (Papers.cpp:613-625, FAIL medium).
3. **Renames are not undone when a later step fails** (PaperEdits.cpp:123-167). On Windows this can happen when a PDF in `local/` is open. The posting-order lettering then comes out permanently wrong.
4. **`build-bib --out` will overwrite any project file** (`loom.toml`, a `paper.toml`), and build-bib skips the "adds no error" check that its header promises (PaperEdits.cpp:357-392).
5. **`arxiv_id` format is never checked** (Papers.cpp:335-418). `"2401.99999v2"` passes, and the arXiv-only entry then gets `eprint = {2401.99999v2v2}`.

## Papers.h / Papers.cpp

### `workTypes`, `readingLevels` (Source/LoomCore/Papers.h:23-30)
**Verdict:** PASS
**Spec:** Papers, `[work]` and `[[reading]]`
**Tests:** covered (every type in PapersTests "Every entry type…").

### `PersonName` (Source/LoomCore/Papers.h:32-37)
**Verdict:** PASS
**Tests:** covered.

### `FieldValue` (Source/LoomCore/Papers.h:39-41)
**Verdict:** PASS
**Tests:** covered.

### `PaperField` (Source/LoomCore/Papers.h:43-59)
**Verdict:** PASS
**Spec:** Papers, "Fields"
**Tests:** covered.

### `NamedField` (Source/LoomCore/Papers.h:61-65)
**Verdict:** PASS
**Tests:** covered.

### `PaperVersion` (Source/LoomCore/Papers.h:67-82)
**Verdict:** PASS
**Tests:** covered.

### `PaperWork` (Source/LoomCore/Papers.h:84-94)
**Verdict:** PASS
**Tests:** covered.

### `ReadingEntry` (Source/LoomCore/Papers.h:96-105)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** The member `FieldLines lines;` (line 104) has no doc comment. Every other member that isn't self-explanatory has one. Readers have to work out that it holds per-field lines for `version-unknown` and `session-missing`.
**Change:** Add `///< The line of each field, for problems found later (version-unknown, session-missing).`
**Tests:** n/a.

### `Renumbering` (Source/LoomCore/Papers.h:107-114)
**Verdict:** PASS
**Tests:** covered.

### `Paper` (Source/LoomCore/Papers.h:116-141)
**Verdict:** PASS
**Tests:** covered.

### `Locator` (Source/LoomCore/Papers.h:143-151)
**Verdict:** PASS
**Tests:** covered.

### `Stub` (Source/LoomCore/Papers.h:153-170)
**Verdict:** PASS
**Tests:** covered.

### `valueKindOf` (Source/LoomCore/Papers.cpp:32-43)
**Verdict:** PASS
**Spec:** Papers: "`year`, `arxiv_version`, `page_count` are integers; `posted` and `date` are dates; the rest are strings"
**Tests:** covered (`wrong-type` for a string year).

### `describeKind` (Source/LoomCore/Papers.cpp:45-55)
**Verdict:** PASS
**Tests:** covered.

### `TypeFields`, `publishedFields` (Source/LoomCore/Papers.cpp:57-79)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Papers, the `[[versions]]` table
**Finding:** The field lists match the spec table exactly. But `publishedFields` has no doc comment of its own: the comment at 57-58 documents the struct. CLAUDE.md asks for a doc comment on every function, covering its parameter and what an unknown type returns (empty lists).
**Change:** Add a doc comment to `publishedFields`: its parameter, that an unknown type gives empty lists, and no side effects.
**Tests:** covered.

### `isBibliographicField` (Source/LoomCore/Papers.cpp:84-96)
**Verdict:** PASS
**Tests:** covered (`jornal` → unknown-field, `school` → field-not-for-type).

### `typeSingular`, `typePlural` (Source/LoomCore/Papers.cpp:98-126)
**Verdict:** PASS
**Tests:** covered for article and book.

### `versionPhrase` (Source/LoomCore/Papers.cpp:128-136)
**Verdict:** PASS
**Tests:** covered ("published version", "edition ed2").

### `isLocalPath` (Source/LoomCore/Papers.cpp:138-143)
**Verdict:** PASS
**Spec:** Papers, "Fields" (`source`), `pdf`
**Tests:** covered (`Downloads/a.pdf`, `published.pdf`).

### `PaperReader` (struct), `PaperReader::pdfPath` (Source/LoomCore/Papers.cpp:145-158)
**Verdict:** PASS
**Tests:** covered.

### `PaperReader::names` (Source/LoomCore/Papers.cpp:160-193)
**Verdict:** PASS
**Spec:** Papers, `[work]`: "`first` may be empty"
**Tests:** partly covered (a missing `first`). Missing: a list element that isn't a table, an unknown key in a name, a non-ASCII `last` written back.

### `PaperReader::field` (Source/LoomCore/Papers.cpp:195-269)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Papers, "Fields"; Validation `field-source`, `pdf-path`
**Finding:** A source that is a URL but not `https://` is reported as `pdf-path`, for example `source = "http://www.ams.org/mathscinet"` (older MathSciNet and journal links are often `http`). The message reads *"`http://www.ams.org/mathscinet` must be inside this paper's `local/` folder."*, which sends the user the wrong way. The spec only allows `https://`, so rejecting it is right; the wording is the problem.
**Change:** For a source that looks like a URL (`scheme://`) but isn't `https://`, say so, e.g. "`{field}`'s source must be an `https://` link or a path inside `local/`". Keep the code (`field-source`, since `pdf-path` is about paths) or note the choice.
**Tests:** `http://` case missing.

### `PaperReader::wrong` (Source/LoomCore/Papers.cpp:271-277)
**Verdict:** PASS
**Tests:** covered.

### `PaperReader::work` (Source/LoomCore/Papers.cpp:279-331)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Papers, `[work]`; Validation `authors-missing`: "{Key} has no authors (and no editors)."
**Finding:** For a type other than book, the message is "Smith24 has no authors." rather than the spec's text. That's defensible, since editors don't count outside books. But the Verification section asks for the spec's exact messages, so either the code or the spec should change. Missing `[work]` or a non-table `work` is handled. An empty title is reported as `wrong-type` and not `missing-field`, which is fine.
**Change:** Either use the spec's message for every type, or record the two-message variant in the spec (see Spec issues).
**Tests:** covered (both messages).

### `PaperReader::version` (Source/LoomCore/Papers.cpp:333-418)
**Verdict:** FAIL
**Severity:** medium
**Finding (regex anchors, FAIL low):** The patterns at lines 352-353 (`^arxiv-v([1-9][0-9]*)$`, `^ed([2-9]|[1-9][0-9]+)$`) end in `$`. In QRegularExpression (PCRE2) `$` also matches just before a final newline. So `id = "arxiv-v1\n"` (a TOML string with a trailing `\n` escape) passes `version-id`, and line 400 then reads its number. A version ID with a line break is accepted, and formatPaper later writes it as a multi-line string. The input is unusual, but the rule is wrong for it. This was found by review-statements-proofs, relayed by the coordinator, and I confirmed it by reading.
**Change (regex):** Use `QRegularExpression::anchoredPattern(...)` (or end with `\z`) for both patterns.
**Finding (arxiv_id, medium, below):**
**Spec:** Papers, `[[versions]]`: "`arxiv_id` (`YYMM.NNNNN`, or an old-style `math/0601001`)"
**Finding:** The ID, kind, duplicate, `ed<N>` and field-placement rules are all right. But nothing checks the form of `arxiv_id` (or `primary_class`), and Bibliography relies on that form. Scenario: an agent writes `arxiv_id = { value = "2503.00001v2", … }` in `arxiv-v2`, a natural mistake since arXiv URLs carry the version. readPaper reports nothing. `bibEntry` for `cite = "arxiv-v2"` then appends `v2` (Bibliography.cpp:172) and writes `eprint = {2503.00001v2v2}` and `note = {arXiv:2503.00001v2v2}`. For a published citation, `eprint` keeps the version, which the spec says it mustn't have.
**Change:** Validate `arxiv_id` as `^[0-9]{4}\.[0-9]{4,5}$` or old-style `^[a-z-]+(\.[A-Z]{2})?/[0-9]{7}$`, reported as `bad-value` at the field's line. Allow 4-digit numbers too: see Spec issues for 2007–2014 IDs.
**Tests:** missing (an `arxiv_id` with a version suffix, a malformed one).

### `PaperReader::versions` (Source/LoomCore/Papers.cpp:420-437)
**Verdict:** PASS
**Tests:** covered for missing. A non-table `versions` isn't tested.

### `PaperReader::readings` (Source/LoomCore/Papers.cpp:439-472)
**Verdict:** FAIL
**Severity:** low
**Spec:** Papers, `[[reading]]`: `session` is `Sessions/<YYYY-MM-DD>`
**Finding:** `sessionRule` (line 451) ends in `$`, which in QRegularExpression also matches before a final newline. So `session = "Sessions/2026-10-18\n"` passes the shape check that `bad-value` exists for. It is then caught only indirectly, as a `session-missing` warning, because no such folder exists. Relayed from review-statements-proofs and confirmed by reading.
**Change:** `QRegularExpression::anchoredPattern(...)`, or `\z` in place of `$`.
**Tests:** covered (`version-unknown`, bad session). A trailing-newline session is missing.

### `PaperReader::renumberings` (Source/LoomCore/Papers.cpp:474-522)
**Verdict:** PASS
**Spec:** Papers, `[[renumbering]]`
**Tests:** covered (fewer than two labels). Missing: `checked` absent, a non-string label.

### `PaperReader::versionReferences` (Source/LoomCore/Papers.cpp:524-542)
**Verdict:** PASS
**Tests:** covered.

### `folderName` (Source/LoomCore/Papers.cpp:545-554)
**Verdict:** PASS
**Tests:** indirect.

### `Writer` (Source/LoomCore/Papers.cpp:556-581)
**Verdict:** PASS
**Tests:** covered by the round trips. A multi-line `summary` isn't round-tripped in any test.

### `dateOrFalse` (Source/LoomCore/Papers.cpp:583-587)
**Verdict:** PASS
**Tests:** covered.

### `formatValue` (Source/LoomCore/Papers.cpp:589-602)
**Verdict:** PASS
**Tests:** covered.

### `formatField` (Source/LoomCore/Papers.cpp:604-609)
**Verdict:** PASS
**Tests:** covered.

### `PaperField::text` (Source/LoomCore/Papers.cpp:613-625)
**Verdict:** FAIL
**Severity:** medium
**Spec:** The bibliography: "Authors and editors are `Last, First` joined by ` and `."
**Finding:** When `first` is empty, the name is written as `Last` with no comma (line 623), which departs from the spec's `Last, First`. BibTeX then misreads any surname with a space: it parses a comma-less name as "First von Last". Concrete input: `authors = { value = [{ last = "Garcia Lopez", first = "" }], … }` gives `author = {Garcia Lopez}`, which BibTeX formats as "G. Lopez" and sorts under L. The same happens to an institutional author, e.g. `last = "The Example Consortium"`. The `first` may be empty, as the spec allows. Related edge case: a `last` containing ` and ` (`"Johnson and Johnson"`) is split into two authors by BibTeX. The code deviation is verified by reading. The BibTeX parsing rule is standard but I didn't run BibTeX on it.
**Change:** Keep `text()` for messages, and build the BibTeX name list in Bibliography (`EntryBuilder::author`/`editor`). Write `Last, First`, and with an empty first write `{Last}` (or `Last,`). Brace any `last` that contains ` and `.
**Tests:** missing (empty `first` with a multi-word surname, in BibliographyTests).

### `PaperField::names` (Source/LoomCore/Papers.cpp:627-632)
**Verdict:** PASS
**Tests:** covered.

### `PaperVersion::field`, `PaperVersion::text` (Source/LoomCore/Papers.cpp:634-644)
**Verdict:** PASS
**Tests:** covered.

### `PaperWork::typeName` (Source/LoomCore/Papers.cpp:646-649)
**Verdict:** PASS
**Tests:** covered.

### `Paper::version` (Source/LoomCore/Papers.cpp:651-655)
**Verdict:** PASS
**Tests:** covered.

### `Paper::keyNames` (Source/LoomCore/Papers.cpp:657-663)
**Verdict:** PASS
**Spec:** The key rule: "authors (editors when there are none)"
**Tests:** covered.

### `Paper::readingLevel` (Source/LoomCore/Papers.cpp:665-671)
**Verdict:** PASS
**Tests:** covered (one reading only). Several readings at mixed levels aren't tested.

### `Stub::locator` (Source/LoomCore/Papers.cpp:673-677)
**Verdict:** PASS
**Tests:** covered.

### `isCited` (Source/LoomCore/Papers.cpp:679-683)
**Verdict:** PASS
**Spec:** Papers, stubs; status rules 2–3
**Tests:** covered, including a checked locator in another version.

### `versionFieldOrder` (Source/LoomCore/Papers.cpp:685-699)
**Verdict:** PASS
**Tests:** covered ("Version fields are written in the spec's order").

### `expectedVersionFields` (Source/LoomCore/Papers.cpp:701-708)
**Verdict:** PASS
**Tests:** covered.

### `readPaper` (Source/LoomCore/Papers.cpp:710-754)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The `loom` command, `new-paper`
**Finding:** When `key` is missing, messages name the paper by `folderName(path, 1)`. For a real entry that's right. For a draft, `createPaper` passes the path as typed (LoomCli/PaperCommands.cpp:46), so `loom new-paper draft.toml` names the paper `.`, and `loom new-paper C:/…/Downloads/draft.toml` names it `Downloads`. The refusal for a draft without authors then reads "draft.toml:3: error: . has no authors." (`.` as QFileInfo's fileName of `.` is unverified; `Downloads` is certain.) The test for draft mode uses `Papers/Smith24/paper.toml` as the draft name, which hides this. Everything else checks out: `cite` and `created` are optional only for drafts, `relevance` ≥ 1, and repeats in `tags`/`aliases` are found.
**Change:** Add a `displayName` parameter (or use "the draft" when `keyRequired` is false) for messages about a draft.
**Tests:** missing (draft read under a draft-like name).

### `readStub` (Source/LoomCore/Papers.cpp:756-865)
**Verdict:** FAIL
**Severity:** low
**Spec:** Papers, Stubs: `id` "A tag, equal to the file name"; `kind` "A statement kind"; `extracted_by` "`dakota` or `agent:<model>`"
**Finding:** `id`, `kind` and `extracted_by` are read with `Need::Required`, which accepts `""`. The checks after that are all guarded by `!isEmpty()` (lines 789, 796, 803). So a stub `Papers/SmithJones24/Statements/7RT2.toml` with `id = ""`, `kind = ""` and `extracted_by = ""` shows no problem at all. The spec's rules are broken for those inputs. A stub with `kind = ""` then gets status cited/extracted, since rule 1 doesn't match "". Two smaller issues: a whitespace-only locator `at = "  "` is accepted, and an empty `at` gets its own wording rather than the `missing-field` message format. `readStatement` has the same pattern (StatementsAndProofs.cpp:126-147, not my slice).
**Change:** Use `Need::RequiredNonEmpty` for `id`, `kind`, `extracted_from` (already) and `extracted_by`, or check empties after reading. Trim before testing `at`.
**Tests:** missing (empty `id`, `kind`, `extracted_by`; whitespace `at`).

### `formatPaper` (Source/LoomCore/Papers.cpp:867-930)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Papers (canonical form for `new-paper`); the key rule, step 5 (renaming)
**Finding:** Output matches the design's layout and round-trips every lossless entry the tests use. But `rekey` and the renaming done by `new-paper` rewrite an existing, hand-maintained `paper.toml` through this function, which drops every comment. The design's own example (Specs/loom.md, Papers) is full of comments. The spec says comments are dropped only for front matter ("since front matter is data") and is silent for plain-TOML `paper.toml`. Notes a person wrote are lost without warning on a rename. Not an error per the spec, so not a FAIL.
**Change:** Either rename by editing only the `key`/`aliases` lines of the existing text, or have `rekey` refuse (or warn) when the file has comments. Record the choice in the spec.
**Tests:** missing (rekey of an entry with comments).

### `formatStub` (Source/LoomCore/Papers.cpp:932-950)
**Verdict:** PASS
**Tests:** covered (round trip, new-stub exact bytes).

## PaperKeys.h / PaperKeys.cpp

### `replacementOf` (Source/LoomCore/PaperKeys.cpp:10-32)
**Verdict:** PASS
**Spec:** The key rule, step 1 (every listed letter present)
**Tests:** covered for most letters (ß, Æ, ø, Œ, Ł, Đ, Þ, ı). ð, đ, œ, æ and þ in lower case aren't each tested.

### `isSplit` (Source/LoomCore/PaperKeys.cpp:34-40)
**Verdict:** PASS
**Tests:** covered (`'`, `’`, `-`). U+2010/U+2011 aren't tested.

### `isAsciiLetter` (Source/LoomCore/PaperKeys.cpp:42-46)
**Verdict:** PASS
**Tests:** indirect.

### `yearOf` (Source/LoomCore/PaperKeys.cpp:48-58)
**Verdict:** PASS
**Tests:** indirect.

### `arxivNumber` (Source/LoomCore/PaperKeys.cpp:60-66)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** The same helper is copied in three files: PaperKeys.cpp:61-66, PaperEdits.cpp:38-43 and Bibliography.cpp:11-16. `version.id.mid(7).toLongLong()` silently gives 0 for an ID that isn't `arxiv-v<N>`. If the copies drift apart, cite defaults, key years and eprints would disagree on which arXiv version is "newest".
**Change:** Make it one public function (e.g. `PaperVersion::arxivNumber()` in Papers.h) and use it from all three.
**Tests:** indirect.

### `postingDate` (Source/LoomCore/PaperKeys.cpp:68-87)
**Verdict:** PASS
**Spec:** The key rule, step 4 ("the earliest of the arXiv `posted` dates, else the published year")
**Tests:** covered.

### `camelSurname` (Source/LoomCore/PaperKeys.cpp:91-119)
**Verdict:** PASS
**Spec:** The key rule, step 1
**Tests:** covered: every key-table surname plus Æ, Ø, Œ, Đ, Þ, ı and 张. I traced NFKD-then-replace order, combining-mark removal, capitalising only the first letter of a part, and the ASCII filter.

### `keyYear` (Source/LoomCore/PaperKeys.cpp:121-141)
**Verdict:** PASS
**Spec:** The key rule, step 3
**Tests:** covered (published; lowest arXiv; talk date). Missing: a published version without `year` falling back to arXiv.

### `KeyBase` (Source/LoomCore/PaperKeys.h:32-38)
**Verdict:** PASS
**Tests:** covered.

### `baseKey` (Source/LoomCore/PaperKeys.cpp:143-167)
**Verdict:** PASS
**Spec:** The key rule, steps 1–3 and both messages
**Tests:** covered, every key-table row.

### `keyFitsBase` (Source/LoomCore/PaperKeys.cpp:169-174)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The key rule, step 4 ("when a new paper's key equals an existing paper's key (or alias), both get letters")
**Finding:** It answers only whether a key has the right shape for its own base: the base, or the base plus one lowercase letter. It doesn't consider the other papers. PaperKeys.h:47-48 documents it as "Whether `key` follows the rule", but that's true only for a paper alone. Example: an unlettered `SmithJones24` next to `SmithJones24a` (same base) "fits", although step 4 means a shared base is always lettered. Its two callers rely on it as the whole check, so the state isn't reported (`ContentsChecker::papers`, ProjectContents.cpp:294-309, raised by review-status-validation) and isn't repairable (`rekeyPaper`, below). `assignKey` itself handles the case correctly, renaming the unlettered paper to the next letter. How the state arises: a hand-made or restored folder, or an entry edited by hand. `new-paper` and `rekey` don't produce it.
**Change:** Rename or re-document it as a shape check. Add a project-level check: "a bare base key while another paper's key or old key is that base plus a letter". Use it in validation (warning `paper-key-rule`, expected key = the next letter) and in `rekeyPaper`.
**Tests:** covered for shape. The bare-base-next-to-lettered case is missing.

### `postedBefore` (Source/LoomCore/PaperKeys.cpp:176-187)
**Verdict:** PASS
**Tests:** covered (dates, title tie-break).

### `KeyedPaper`, `KeyAssignment` (Source/LoomCore/PaperKeys.h:57-73)
**Verdict:** PASS
**Tests:** covered.

### `assignKey` (Source/LoomCore/PaperKeys.cpp:189-243)
**Verdict:** NEEDS-CHANGES
**Severity:** medium
**Spec:** The key rule, step 4
**Finding:** Lettering logic is right for every spec case I traced: first collision by posting order, third paper takes the next letter, an old key counts as used, exhausting `z`. But every comparison is case-sensitive, even though a key is a folder name and Dakota's file system (NTFS) is case-insensitive. Scenario: `Papers/MacDonald20/` exists (surname "MacDonald"), and a draft by "Macdonald", 2020, has base `Macdonald20`. `assignKey` sees no collision and returns `Macdonald20`. On Windows that is the same folder, and `createPaper` then overwrites the entry (see `createPaper`). Validation's `paper-key-duplicate` is also case-sensitive, so nothing catches it. Unverified on Windows; traced by reading.
**Change:** Compare keys and old keys case-insensitively when deciding a collision. If the bases differ only in case, letter them like a shared key.
**Tests:** missing (case-only collision; `exact` together with existing lettered keys).

## PaperEdits.h / PaperEdits.cpp

### `PaperRenamed`, `PaperChange`, `NewStub`, `BibliographyBuilt` (Source/LoomCore/PaperEdits.h:19-81)
**Verdict:** PASS
**Tests:** covered.

### `paperPath` (Source/LoomCore/PaperEdits.cpp:20-24)
**Verdict:** PASS
**Tests:** covered.

### `othersThan` (Source/LoomCore/PaperEdits.cpp:26-35)
**Verdict:** PASS
**Finding:** It leaves out entries that don't parse. That is part of the `createPaper` overwrite below, which is where the fix belongs.
**Tests:** covered.

### `arxivNumber` (Source/LoomCore/PaperEdits.cpp:37-43)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** This is a copy of PaperKeys.cpp:61-66; see that entry.
**Change:** Use the shared function.
**Tests:** indirect.

### `defaultCite` (Source/LoomCore/PaperEdits.cpp:45-59)
**Verdict:** PASS
**Spec:** Papers, `cite`
**Tests:** covered (published; newest arXiv).

### `Rename` (Source/LoomCore/PaperEdits.cpp:61-67)
**Verdict:** PASS
**Tests:** covered.

### `renameInModel` (Source/LoomCore/PaperEdits.cpp:69-106)
**Verdict:** PASS
**Spec:** The key rule, step 5
**Finding:** It refuses a lossy entry (`notEditable`), so a rename never drops a field. It adds the old key to `aliases` and removes the new key if it was an old one.
**Tests:** covered. Missing: refusing a lossy paper that a new paper's key would rename.

### `movedBy` (Source/LoomCore/PaperEdits.cpp:108-119)
**Verdict:** PASS
**Tests:** indirect (the rename tests end with no errors).

### `carryOut` (Source/LoomCore/PaperEdits.cpp:121-145)
**Verdict:** NEEDS-CHANGES
**Severity:** medium
**Spec:** The key rule, step 4 (letters by posting order the first time); `loom` exit codes
**Finding:** Each rename is undone only if its own `paper.toml` write fails. Renames already done earlier in the list are never undone. Scenario: `loom rekey SmithJones23`, where SmithJones23 (posted 2023) is now published in 2024 and `SmithJones24` exists (posted 2024). The plan is [`SmithJones24` → `SmithJones24b`, `SmithJones23` → `SmithJones24a`]. On Windows, renaming a folder fails while a file in it is open, e.g. `local/arxiv-v1.pdf` in a PDF viewer. The second rename then fails after the first succeeded, and the command exits 3. A retry now sees `SmithJones24b` with old key `SmithJones24`, so the base is taken and the highest letter is `b`. The earlier-posted paper gets `SmithJones24c`. Since lettered keys never change, the rule-4 posting order is permanently wrong. Unverified on Windows (the rename failure); the logic is traced.
**Change:** Keep the list of completed renames and roll them back (in reverse, restoring each old `paper.toml`) when a later step fails. Or check that every source folder can be renamed before starting.
**Tests:** missing (a failing second rename).

### `commitPapers` (Source/LoomCore/PaperEdits.cpp:147-167)
**Verdict:** NEEDS-CHANGES
**Severity:** medium
**Finding:** This is the second half of the `carryOut` problem. The `extra` files (the new `paper.toml` for `new-paper`) are written after the renames. If that write fails, the existing paper stays renamed to `…a`/`…b` with no new paper beside it. A retry then letters the new paper `c`. It is also the natural place for the "target exists" check that `createPaper` lacks.
**Change:** Roll back the renames when an `extra` write fails, and refuse (before any rename) when an `extra` target already exists on disk (case-insensitively).
**Tests:** missing.

### `planRenames` (Source/LoomCore/PaperEdits.cpp:169-187)
**Verdict:** PASS
**Tests:** covered.

### `createPaper` (Source/LoomCore/PaperEdits.cpp:191-250)
**Verdict:** FAIL
**Severity:** high
**Spec:** CLAUDE.md "Never lose user text"; `new-paper` "creates `Papers/<Key>/paper.toml`"
**Finding:** Nothing checks that `Papers/<Key>/paper.toml` is free before writing it. `writeProjectFile` replaces whatever is there, and the "adds no error" check doesn't catch it, because the old reading stays in the model with the same problems. Three traced scenarios overwrite a person's entry:
1. `Papers/SmithJones24/paper.toml` has a TOML syntax error (a hand edit in progress). `othersThan` drops unparsed entries, so a draft by Smith and Jones, published 2024, is assigned `SmithJones24`. The old reading's `toml-syntax` error appears both before and after, so nothing is "added", and the file is replaced.
2. `Papers/SmithJones24/paper.toml` parses but its `key` is `SmithJones2` (typo), or `key` is missing. The others list holds `SmithJones2`, the new paper gets `SmithJones24`, the `paper-key-mismatch` error is unchanged before and after, and the file is replaced.
3. On Windows, the case-only collision under `assignKey` (`Macdonald20` over `MacDonald20`). Unverified on Windows.

In each case Dakota's entry, with its verified dates, is gone. It survives only if it was committed.
**Change:** Before writing, refuse (exit 1) when any folder in `Papers/` matches the new key case-insensitively, whether or not its entry parses: "`Papers/<Key>/` already exists; …". Do the same for every rename target. `carryOut` checks only with `QFileInfo::exists` and only at rename time, after other renames may already be done.
**Tests:** missing (all three scenarios).

### `createStub` (Source/LoomCore/PaperEdits.cpp:252-298)
**Verdict:** PASS
**Spec:** `new-stub`; Papers, stubs; IDs (generation)
**Finding:** The locator is always `checked_against_pdf = false`, so status can't be claimed. Kind, version, `by`, title and `paper_premises` are validated through the "adds no error" check. The tag avoids every tag and alias in use.
**Tests:** covered (exact bytes, refusals). Missing: an unknown kind, and a title with a line break.

### `rekeyPaper` (Source/LoomCore/PaperEdits.cpp:300-355)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The key rule, step 5; D9
**Finding:** Logic is right: lossy entries are refused, "already right" when the key fits and matches its folder, and `\cite` is never rewritten. The comment at line 331 says a fitting key that disagrees with its folder "is put back with its folder's name". The code does the opposite: it renames the folder to the key (`renameInModel(…, file->folder, paper.key, …)`).
A second gap: line 326 treats any key that `keyFitsBase` accepts as "already right". So `loom rekey SmithJones24`, with `SmithJones24a` beside it, does nothing, though step 4 needs the bare key lettered (see `keyFitsBase`). Even if validation warned, the command it names couldn't repair the state.
**Change:** Fix the comment to "the folder is renamed to match the key". Treat a bare base key as not right when another paper holds the base plus a letter (as a key or an old key), and give it the next letter via `assignKey`.
**Tests:** covered for the main paths. Missing: a key that fits but whose folder differs, and refusing a lossy entry.

### `buildBibliography` (Source/LoomCore/PaperEdits.cpp:357-392)
**Verdict:** NEEDS-CHANGES
**Severity:** medium
**Spec:** The bibliography ("Output goes to `.loom/build/refs.bib` unless `--out` says otherwise"); PaperEdits.h header ("writes only if the change adds no error")
**Finding:** `--out` is checked only for path safety. `loom build-bib --out loom.toml` (or `--out Papers/SmithJones24/paper.toml`, or a statement's `.tex`) replaces that Loom-owned file with BibTeX, without a refusal. That breaks the PaperEdits.h header's promise that each change "writes only if the change adds no error to the project". `--out Statements/refs.bib` likewise adds a `stray-file` error. Smaller issue: entries that parse but lost a field are passed on, though `formatBibliography` documents "each read without losing a field". The skip message "Loom can't read …" is also wrong for a parsed entry with no `key`.
**Change:** Refuse an `--out` inside `Statements/`, `Proofs/` or `Papers/`, or naming `loom.toml`. Refuse an existing file that isn't `.bib`, or run the added-errors check. Skip, with a problem, entries that aren't lossless, or update the doc.
**Tests:** covered for default, `TeX/refs.bib` and `../`. Missing: `--out` onto an existing project file, and a skipped unreadable entry.

## Bibliography.h / Bibliography.cpp

### `BibEntry`, `Bibliography` (Source/LoomCore/Bibliography.h:17-47)
**Verdict:** PASS
**Tests:** covered.

### `arxivNumber` (Source/LoomCore/Bibliography.cpp:10-16)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** This is the third copy (see PaperKeys.cpp:60-66).
**Change:** Use the shared function.
**Tests:** indirect.

### `EntryBuilder` and `used`, `text`, `field`, `version`, `author`, `editor`, `title`, `mrZblUrl` (Source/LoomCore/Bibliography.cpp:18-87)
**Verdict:** PASS
**Spec:** The bibliography (fields only when they have a value; unverified fields listed)
**Finding:** Correct apart from the name formatting it inherits from `PaperField::text` (FAIL above).
**Tests:** covered.

### `EntryBuilder::eprint` (Source/LoomCore/Bibliography.cpp:89-107)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** "The `eprint` of a published version is the arXiv identifier **without** a version"
**Finding:** It writes `arxiv_id` as stored. The spec promise holds only if `arxiv_id` never carries a version, and the reader doesn't enforce that (see `PaperReader::version`).
**Change:** Either validate `arxiv_id` in the reader (preferred) or strip a trailing `v<N>` here.
**Tests:** covered for a clean ID.

### `escapeBibValue` (Source/LoomCore/Bibliography.cpp:112-127)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The bibliography: "`&`, `%` and `#` that aren't already escaped and aren't inside `$…$` are escaped"
**Finding:** "Already escaped" means only "the previous character is `\`". So in `A\\% B` (a LaTeX line break then `%`), the `%` counts as escaped and is left raw, and it comments out the rest of the line, closing brace included, in the `.bbl`. `$$…$$` toggles math twice at once, so its contents are treated as text. Both are rare in titles.
**Change:** Count the run of backslashes before the character (odd means escaped). Treat `$$` as one delimiter.
**Tests:** covered for the basic cases. `\\%` and `$$` are missing.

### `bracesBalanced` (Source/LoomCore/Bibliography.cpp:129-141)
**Verdict:** NEEDS-CHANGES
**Severity:** medium
**Spec:** "a value with unbalanced braces is an error and its entry is skipped"
**Finding:** `\{` and `\}` are skipped when counting, and a test (BibliographyTests.cpp:192) fixes that behaviour. But BibTeX's own parser counts every brace, backslash or not. A title like `On the set \{x > 0` passes this check and is written as `title = {On the set \{x > 0},`. BibTeX then sees an unclosed brace, and the rest of `refs.bib` breaks: exactly what the skip rule exists to prevent. A value ending in `\` (`foo\`) gives `{foo\}`. The behaviour of BibTeX here is from its documentation, not run (unverified).
**Change:** Count every `{` and `}`, escaped or not. Reject a value ending in an odd run of backslashes.
**Tests:** BibliographyTests.cpp:192 encodes the current behaviour. A test case should assert the opposite (with ⚠️ Flags, per CLAUDE.md rule 7).

### `bibEntry` (Source/LoomCore/Bibliography.cpp:143-254)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The bibliography: "Every field written whose `verified` is `false` is listed in a warning"
**Finding:** Entry types and field order match the spec for every type: published article (including `month`/`publisher`/`issn` after `doi`), arXiv-only `@misc`, book with `series_volume` → `volume` and the editor fallback, incollection/inproceedings, theses, techreport, and talk/notes/other. But line 155-156 lists `work.type` as an unverified field "used", although `type` is never written as a field. A paper whose written fields are all verified, with only `type` unverified, warns "refs.bib uses unverified fields: X type".
**Change:** Don't list `type`, or document it as a deliberate addition in the spec.
**Tests:** covered for the exact outputs. Missing: `cite` naming no version, and the `type`-only warning.

### `formatBibliography` (Source/LoomCore/Bibliography.cpp:256-280)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The bibliography ("in key order")
**Finding:** Problems are filed under `Papers/<key>/paper.toml`, built from the key. For an entry whose folder differs from its key (`paper-key-mismatch`), that names a file that doesn't exist. The caller has the real path. The codes `bib-entry-skipped` and `bib-unverified` aren't in the spec (see Spec issues).
**Change:** Take the paper's path with each paper (e.g. `std::vector<std::pair<QString, Paper>>`).
**Tests:** covered (order, warning text, skipped entry).

## Tests

### `PaperFixture.h` (Tests/LoomCoreTests/PaperFixture.h:1-119)
**Verdict:** PASS
**Tests:** n/a. Helpers are documented and match the design's example.

### `PapersTests.cpp` (Tests/LoomCoreTests/PapersTests.cpp:1-343)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** Good coverage of field and version problems with lines, every type's round trip, and stub problems. Missing cases:
- a paper.toml or stub with CRLF line endings, and with a BOM;
- non-ASCII author names written back by `formatPaper`;
- a multi-line `summary`/`differences` round trip;
- a stub with `id = ""`, `kind = ""`, `extracted_by = ""`, or a whitespace `at`;
- `[[renumbering]]` without `checked`, and a non-string label;
- a names element that isn't a table, or has an unknown key;
- an `http://` source;
- `versions` or `work` that isn't a table;
- a malformed `arxiv_id`;
- a draft read under a draft-like name.
**Change:** Add those cases.
**Tests:** n/a.

### `PaperKeysTests.cpp` (Tests/LoomCoreTests/PaperKeysTests.cpp:1-168)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** Every key-table row, lettering by posting order, the third paper, and old keys are covered. Missing cases:
- letters exhausted (`z`);
- an unlettered paper with the base alongside lettered ones (renamed to the next letter);
- a case-only collision (`MacDonald`/`Macdonald`);
- a published version without `year` falling back to arXiv;
- the U+2010/U+2011 hyphens;
- lower-case `ð`, `đ`, `œ`, `æ`, `þ`.
**Change:** Add those cases.
**Tests:** n/a.

### `PaperEditsTests.cpp` (Tests/LoomCoreTests/PaperEditsTests.cpp:1-470)
**Verdict:** NEEDS-CHANGES
**Severity:** medium
**Finding:** The happy paths and main refusals are covered. Missing cases include the most important one, the overwrite:
- `new-paper` when `Papers/<Key>/paper.toml` exists but doesn't parse, or has a mismatched or missing key (must refuse and leave the file untouched);
- a rename that fails midway (rollback);
- `rekey` refusing a lossy entry;
- `rekey` of a fitting key whose folder differs;
- `new-paper` that must rename a lossy existing entry (refused);
- `build-bib --out` onto an existing project file;
- `build-bib` with an unreadable entry (`bib-entry-skipped`);
- `new-stub` with an unknown kind or a multi-line title.
**Change:** Add those cases; the first is the regression test for the FAIL.
**Tests:** n/a.

### `BibliographyTests.cpp` (Tests/LoomCoreTests/BibliographyTests.cpp:1-237)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** Exact output for every type, escaping and warnings is covered, as the spec's Verification asks. Missing cases:
- an empty `first` with a multi-word surname;
- `\{`, which the test currently asserts is balanced;
- a trailing backslash;
- `\\%`;
- `cite` naming no version;
- an arXiv-only paper whose `arxiv_id` carries a version;
- the `type`-only unverified warning;
- a talk with a date but no venue.
**Change:** Add those cases. Change the `\{` assertion with ⚠️ Flags when the code is fixed.
**Tests:** n/a.

## Cross-boundary notes
- **Received, from review-status-validation (via the coordinator):** `ContentsChecker::papers` (ProjectContents.cpp:294-309) warns `paper-key-rule` only when `!keyFitsBase(key, base)`. So an unlettered `SmithJones24` beside `SmithJones24a` gets no warning, though step 4 letters both. Confirmed: `keyFitsBase` is only a shape check, and `rekeyPaper` (PaperEdits.cpp:326) has the same blind spot, so `loom rekey` couldn't fix the state either. Logged as NEEDS-CHANGES (low) under `keyFitsBase`, and added to `rekeyPaper`. `assignKey` itself handles the case correctly.
- **Later:** both notes below were sent to the coordinator ("main") for forwarding, once it said direct messages by name don't work in this session.
- **To review-statements-proofs (first attempt not delivered):** "readStatement accepts `id = ""` and `kind = ""` silently (StatementsAndProofs.cpp:126-127, 139-147): Need::Required takes "", and the checks are guarded by `!isEmpty()`. readStub has the same gap (logged in papers.md); readProof's `id` (:189) may too." The SendMessage call failed: "No agent named 'review-statements-proofs' is reachable". ListAgents couldn't be loaded with ToolSearch, so I couldn't find the right address. Consolidator: please carry this over.
- **To review-status-validation (not delivered, same reason):** "`paper-key-duplicate` (ProjectContents.cpp:280-290) compares keys case-sensitively. `MacDonald20` and `Macdonald20` both validate, but they're one folder on NTFS, and createPaper then overwrites the first entry (logged here as a FAIL)." Consolidator: please carry this over.
- **Received, from review-statements-proofs (via the coordinator):** Papers.cpp:352, 353 and 451 use regexes ending in `$`, which also matches before a final newline. Confirmed and logged as FAIL (low) under `PaperReader::version` and `PaperReader::readings`. No other regex in my slice.
- **Received, from review-cli-build (via the coordinator):** `Tests/Projects/EveryProblem-expected.txt` shows `paper-key-rule` on `Papers/Nobody19` with "Can't make a key for this paper: it has no authors or editors.", next to an `authors-missing` error for the same paper. The **text** comes from my `baseKey` (PaperKeys.cpp:146-147). It's right there: `new-paper` and `rekey` need a reason to refuse. But the **emission** as a validation problem is in ProjectContents.cpp:294-297 (`add("paper-key-rule", …, base.problem, Severity::Warning)`), which is status-validation's code. My judgement: when `baseKey` fails because there are no names, validation should stay silent, since `authors-missing` already says it. When it fails for the year or a non-Latin surname, the spec's paper-key-rule message doesn't fit either, so the spec needs a second message or a separate code. Replied to the coordinator asking to forward this to review-status-validation.
- Read but not judged: `FieldReader::string` (`Need::Required` accepts ""), `writeProjectFile` (no overwrite guard, by design), `checkContents` (keeps the old reading's problems, which is why the added-errors check misses the overwrite), `LoomCli/PaperCommands.cpp` (passes the draft path as typed; converts `--out` to a project path).

## Spec issues
1. **Key equality and case.** The key rule compares keys exactly ("equals an existing paper's key"), but keys are folder names and Windows folders are case-insensitive. The spec should say keys collide when they are equal ignoring case, for lettering and for `paper-key-duplicate`.
2. **arXiv identifiers.** `arxiv_id` is specified as `YYMM.NNNNN`, but IDs from April 2007 to December 2014 are `YYMM.NNNN` (e.g. `0704.0001`). The spec also names no problem code for a malformed `arxiv_id` (presumably `bad-value`), and doesn't say the stored ID must not carry `v<N>`.
3. **Escaping in `url` and `doi`.** "In values, `&`, `%` and `#` … are escaped" applies to every field, so `url = {https://example.org/a\%20b}` and fragment `\#` end up in links. Many BibTeX styles pass `url`/`doi` verbatim to `\url`/`\doi`. The spec should exempt them, or say how they're handled.
4. **Brace balance.** The spec doesn't say whether `\{`/`\}` count. BibTeX's parser counts them, so they should.
5. **An empty `first`.** "`Last, First`" leaves the empty-first case to the reader. The spec should say the name is written `{Last}` (or `Last,`).
6. **`authors-missing` message.** The spec gives one message ("…has no authors (and no editors).") for every type; the code uses "…has no authors." for non-books.
7. **Comments in `paper.toml`.** Comments are dropped on rewrite only for front matter. Rekeying rewrites a plain-TOML `paper.toml` and drops its comments, and the spec is silent. The design's own example has comments.
8. **Dakota's clicks in a `new-paper` draft.** A draft may carry `verified = <date>` on fields, a top-level `verified`, or `checked = <date>` in renumberings. `new-paper` (not a Dakota command) writes them as given. Setting `verified` is Dakota's click. The spec should say whether `new-paper` refuses or resets them (plain edits can do the same, but the command is an easy route).
9. **Codes outside the spec.** `bib-entry-skipped` and `bib-unverified` are used for `--json` but aren't in the spec's tables.
