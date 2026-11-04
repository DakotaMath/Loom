# M1 review: statements-proofs

**Scope:** `Source/LoomCore/` StatementsAndProofs, Edits, EditSession, Checks, ProofChecks, Cycles (.h and .cpp); `Tests/LoomCoreTests/` StatementsAndProofsTests, EditsTests, ChecksTests, ProofChecksTests, CyclesTests.
**Summary:** 45 PASS / 14 NEEDS-CHANGES / 4 FAIL (some entries group a few small declarations). What matters most: (1) the readers keep empty strings for required and reference fields without a problem, so `kind = ""`, `id = ""`, `proves = ""`, `level = ""` and, worst, `refuted_by = ""` with `refuted_on` set all pass `loom validate` (StatementsAndProofs.cpp:126-169, 189-203, 236-246); (2) the regexes that check `by`, `origin` and a verbose proof's `fingerprint` end in `$`, which in Qt's PCRE2 also matches before a final newline, so `by = "agent:x\n"` is a valid author (Checks.cpp:159, StatementsAndProofs.cpp:156, 243); (3) the edit, check and refute paths themselves are sound: every write is checked against the project's errors and against outdated checks, Dakota's clicks are explicit arguments, the check history is appended as text, and the verdict table and merge rules match the spec. The gaps beyond that are missing tests and small doc problems.

## StatementsAndProofs.h / StatementsAndProofs.cpp

### `statementKinds`, `proofLevels` (Source/LoomCore/StatementsAndProofs.h:21-28)
**Verdict:** PASS
**Spec:** Statements, Proofs
**Tests:** covered (bad-value messages list them).

### `isProvableKind` (Source/LoomCore/StatementsAndProofs.h:30-33, StatementsAndProofs.cpp:96-99)
**Verdict:** PASS
**Spec:** Statements, "Statuses by kind"; D5
**Tests:** covered indirectly (proof-of-no-status, premise-no-status in EditsTests).

### `Statement`, `Proof`, `VerboseProof` (Source/LoomCore/StatementsAndProofs.h:35-71)
**Verdict:** PASS
**Spec:** Statements, Proofs, D8
**Tests:** covered.

### `FieldLines` and `FieldLines::of` (Source/LoomCore/StatementsAndProofs.h:73-80, StatementsAndProofs.cpp:101-109)
**Verdict:** PASS
**Spec:** Validation ("the line of the offending value, else of its table header, else 1")
**Tests:** covered (`lines.of("stated_using", 0) == 6`, element lines in wrong-type tests).

### `DocumentReading` and its aliases (Source/LoomCore/StatementsAndProofs.h:82-105)
**Verdict:** PASS
**Tests:** covered (`parsed`, `lossless` checked in several cases).

### `parseFrontMatter` (Source/LoomCore/StatementsAndProofs.cpp:29-46)
**Verdict:** PASS
**Spec:** Front matter ("Error line numbers are file lines (the TOML's line plus 1)")
**Tests:** covered (toml-syntax at line 4).

### `finish` (Source/LoomCore/StatementsAndProofs.cpp:49-62)
**Verdict:** PASS
**Spec:** Statements, Proofs ("The body must not be empty")
**Tests:** covered (body-empty for whitespace and for nothing).

### `baseName` (Source/LoomCore/StatementsAndProofs.cpp:65-71)
**Verdict:** PASS
**Tests:** covered indirectly.

### `Writer` (Source/LoomCore/StatementsAndProofs.cpp:74-92)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Front matter, "Canonical form"
**Finding:** The struct has a one-line comment but its three methods (`string`, `list`, `date`) have none; CLAUDE.md asks for a doc comment on every function. The behaviour that matters (an empty string, empty list or invalid date is left out; `list(..., always)` writes `[]`) is only visible by reading the code.
**Change:** One-line doc comment per method, saying an absent value writes nothing.
**Tests:** covered (canonical writer test, minimal statement, `premises = []`).

### `readStatement` (Source/LoomCore/StatementsAndProofs.cpp:111-173)
**Verdict:** FAIL
**Severity:** medium
**Spec:** Statements (field table; D4 "both or neither"); Validation (`bad-value`, `id-syntax`, `refuted-incomplete`)
**Finding:** Required and reference fields are read with `Need::Required`/`Need::Optional`, which keep an empty string without a problem, and every later check is guarded by `!x.isEmpty()`. Traced inputs:
- `refuted_by = ""` plus `refuted_on = 2026-11-06`: both keys are present, so no `refuted-incomplete` (lines 161-169); `refuted-unknown` in checkContents skips an empty value (ProjectContents.cpp:210); Status's `isRefuted` needs a non-empty `refuted_by` (Status.cpp:94), so the statement isn't false either. `loom validate` is clean while the file records Dakota's acceptance of a refutation that counts for nothing. A later `edit-statement` on that file is refused with a confusing `refuted-incomplete` (the writer drops the empty field), and `refute` says "already refuted by (no proof)".
- `kind = ""`: no `bad-value` (line 147 skips empty); the statement silently has no status.
- `id = ""`: no `id-syntax` or `id-mismatch` (line 140 skips empty), though the spec requires `id` equal to the file name.
- `superseded_by = ""`: silently ignored (ProjectContents.cpp:199 skips empty).
- `title = "   "` passes as "non-empty" (only `""` is caught); a whitespace-only title is empty for any reader.
- `created = 0000-01-01` (reported by review-foundations, confirmed here): FieldReader::date gives an invalid QDate with no problem and `lossless` stays true, so validate is clean; an edit then drops `created` and is refused by `commit()` with a confusing `missing-field`. Same for readProof and readVerboseProof. Fix in FieldReader::date (report the date and set `lost`).
**Change:** Read `id` and `kind` with `Need::RequiredNonEmpty` (or report `bad-value` for an empty value, listing the kinds); for the optional reference fields `superseded_by` and `refuted_by`, report an empty string as `wrong-type` ("a tag" / "a proof ID", not an empty string) and treat it as present for D4. Treat a whitespace-only `title` like an empty one. Add a test per case.
**Tests:** missing: every empty-string case above; `superseded_by`/`refuted_by` of the wrong type.

### `readProof` (Source/LoomCore/StatementsAndProofs.cpp:175-220)
**Verdict:** FAIL
**Severity:** low
**Spec:** Proofs (field table); Validation (`bad-value`, `proof-id-mismatch`)
**Finding:** Same pattern as readStatement. `level = ""`: no `bad-value` (line 202 skips empty); Status gives such a proof nothing and its explanation says "has no level Loom knows" (Status.cpp:258), but validation passes. `id = ""`: no `proof-id-mismatch` (line 199 skips empty). `proves = ""`: no problem from the reader, and checkContents only checks a non-empty `proves` (ProjectContents.cpp:392). The rest (premise-self, premise-duplicate at element lines, the folder check) is right.
**Change:** Read `id`, `proves` and `level` with `Need::RequiredNonEmpty`, or report the empty value as `bad-value` with the allowed list.
**Tests:** missing: empty `id`, `proves`, `level`.

### `readVerboseProof` (Source/LoomCore/StatementsAndProofs.cpp:222-250)
**Verdict:** FAIL
**Severity:** low
**Spec:** Proofs, "Verbose proofs" (D8); Validation (`verbose-stale`)
**Finding:** (1) `fingerprint = ""` and `proof = ""` give no problem (lines 240, 244 skip empty); Validation then skips the stale check for a non-fingerprint (Validation.cpp:453), so such a verbose version is never reported stale. (2) The fingerprint rule `^[0-9a-f]{12}$` (line 243) uses `$`, which in PCRE2 also matches before a final newline: `fingerprint = "3c8e1f0a92d4\n"` passes here, and Validation's strict `isFingerprint` then skips it, so it is never stale either. (3) A `proof` that doesn't match the file is reported as `bad-value` "it must be one of `4KX7.a`", which reads oddly; the spec gives no code, so this is minor.
**Change:** Required non-empty for both fields; use `isFingerprint()` (Fingerprint.h) instead of a second regex, which also fixes the `$` problem.
**Tests:** missing: empty `proof`/`fingerprint`, a fingerprint with a trailing newline, an unknown field in a verbose proof.

### `formatStatement` (Source/LoomCore/StatementsAndProofs.cpp:252-268)
**Verdict:** PASS
**Spec:** Front matter, "Canonical form"; Statements field order
**Tests:** covered (spec example round trip, every field, minimal, escaping, Unicode).

### `formatProof` (Source/LoomCore/StatementsAndProofs.cpp:270-280)
**Verdict:** PASS
**Spec:** Proofs
**Tests:** covered.

### `formatVerboseProof` (Source/LoomCore/StatementsAndProofs.cpp:282-289)
**Verdict:** PASS
**Tests:** covered (spec example round trip).

### `bodyFromText` (Source/LoomCore/StatementsAndProofs.cpp:291-299)
**Verdict:** PASS
**Spec:** Front matter ("When Loom creates a file from a `--body` it adds a final newline if the text has none")
**Tests:** covered (no newline, CRLF, BOM). Invalid UTF-8 isn't checked here, but the callers re-read the bytes and the `not-utf8` error refuses the write; a test of that would be cheap. It inherits `crlfToLf`'s non-idempotence (`\r\r\n` → `\r\n`, so a lone CR is lost on a later rewrite; FrontMatter.cpp:64, foundations' fix); the fingerprint is unaffected.

### Origin rule in `readStatement` (Source/LoomCore/StatementsAndProofs.cpp:156-159)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Statements, `origin`
**Finding:** `^(new|spec:.+|import:.+)$` with `$`: `origin = "new\n"` matches (PCRE2 `$` matches before a final newline), so a value the spec doesn't allow passes. Harmless in practice, but it's the same trap as Checks.cpp:159.
**Change:** `QRegularExpression::anchoredPattern(...)` or end the pattern with `\z`.
**Tests:** missing: an origin with a trailing newline.

### StatementsAndProofsTests.cpp (Tests/LoomCoreTests/StatementsAndProofsTests.cpp:1-374)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Verification, "Front matter round trip", "Canonical writer", "Field validation"
**Finding:** Good coverage of the round trip (verbatim `+++`, Unicode, no final newline, CRLF, BOM, comments dropped), the writer, and each problem with its line. Missing cases: empty strings for `id`, `kind`, `proves`, `level`, `refuted_by`, `superseded_by`, verbose `proof`/`fingerprint` (the FAILs above); `superseded_by`/`refuted_by`/`refuted_on` of the wrong type and the resulting `lossless == false`; an unknown field and `toml-syntax` in a verbose proof; a lone CR in a body; `front-matter-unclosed` through readStatement.
**Change:** Add those cases.
**Tests:** see Finding.

## Edits.h / Edits.cpp

### `ChangeOptions` (Source/LoomCore/Edits.h:24-36)
**Verdict:** PASS
**Spec:** Adding a check ("the current local time"); IDs, Generation; Front matter (10 s lock)
**Tests:** covered (fixed dates, seeded generator, short lock wait).

### `ChangeOutcome`, `ChangeResult` (Source/LoomCore/Edits.h:38-69)
**Verdict:** PASS
**Spec:** The `loom` command, exit codes
**Tests:** covered.

### `NewStatement`, `StatementEdit`, `NewProof`, `ProofEdit`, `NewCheck`, `CheckAdded` (Source/LoomCore/Edits.h:78-173)
**Verdict:** PASS
**Tests:** covered.

### `listed` (Source/LoomCore/Edits.cpp:30-35)
**Verdict:** PASS
**Tests:** covered (refusal naming two proofs). The CLI has its own copy (StatementCommands.cpp uses `listed`); a shared one would avoid drift, but that's cosmetic.

### `commit` (Source/LoomCore/Edits.cpp:42-72)
**Verdict:** PASS
**Spec:** Statements ("Editing a statement whose text is in a checked proof's fingerprint … is refused … unless the caller passes the confirmation"); the `loom` command, exit 1 "Nothing was written"
**Tests:** covered (refusals write nothing; outdates-checks refusal and confirmed edit in ProofChecksTests). It compares only checkContents errors, not Validation's, but the only Validation errors (`check-changed`, `preamble-missing`) can't be introduced by these edits, and body problems are warnings.

### `findTyped` (Source/LoomCore/Edits.cpp:77-86)
**Verdict:** PASS
**Spec:** IDs (forgiving tags, aliases resolved where a person types an ID)
**Tests:** covered (`removePremises = {"zzzz"}` of a reference that names nothing).

### `addRefs` (Source/LoomCore/Edits.cpp:89-103)
**Verdict:** PASS
**Tests:** covered (`already-there` with a lower-case typed tag).

### `removeRefs` (Source/LoomCore/Edits.cpp:106-118)
**Verdict:** PASS
**Tests:** covered.

### `highestProofNumber` (Source/LoomCore/Edits.cpp:122-136)
**Verdict:** PASS
**Spec:** IDs, Proof IDs ("the letter after the highest one in its folder, so a deleted proof's letter isn't reused")
**Tests:** covered (`4KX7.z.checks.toml` gives `4KX7.aa`).

### `addTag` (Source/LoomCore/Edits.cpp:140-186)
**Verdict:** PASS
**Spec:** `loom.toml` (`tags`); the `loom` command, `new-tag`
**Tests:** covered (comments kept, missing final newline, duplicate, syntax, kind, empty title, inline `tags = []` refused). Missing: a CRLF `loom.toml` (the appended lines are LF; TOML and `eol=lf` make that harmless).

### `createStatement` (Source/LoomCore/Edits.cpp:188-225)
**Verdict:** PASS
**Spec:** the `loom` command, `new-statement`; IDs, Generation
**Tests:** covered (layout, alias in `stated_using` resolved to its tag, tag not reused, undeclared tag, bad kind, unknown ref, empty body, nothing written on refusal).

### `editStatement` (Source/LoomCore/Edits.cpp:227-307)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Statements; the `loom` command, `edit-statement`
**Finding:** The logic is right: lossless files only, body kept byte for byte unless replaced, `changed` in spec order, no write when nothing changed, and the outdated-checks guard. Gaps are in tests: no test sets `superseded_by` successfully, none edits a tag that is only a stub (the refusal at lines 238-241), none checks that a title with a line break or an empty title is refused. Also, `superseded_by` can be set to a stub (resolveAll accepts stubs) though the spec says "the tag of another statement"; see Spec issues.
**Change:** Add the tests; settle the stub question in the spec.
**Tests:** missing: as above.

### `createProof` (Source/LoomCore/Edits.cpp:309-341)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** the `loom` command, `new-proof`; Proofs; Cycles
**Finding:** Correct. Missing tests for two refusals the spec makes errors: a proof of a stub (`proof-of-stub`) and a premise that closes a cycle (`premise-cycle`), plus a stub premise being accepted.
**Change:** Add those three cases to "new-proof refuses what the rules refuse".
**Tests:** missing: as above.

### `editProof` (Source/LoomCore/Edits.cpp:343-397)
**Verdict:** PASS
**Spec:** the `loom` command, `edit-proof` ("Changing the body or premises of a checked proof is refused unless `--outdate-checks`")
**Tests:** covered (EditsTests refusals; ProofChecksTests body and premises refused, level edits freely). Setting `level = "failed"` needs no confirmation; see Spec issues.

### `addCheck` (Source/LoomCore/Edits.cpp:399-490)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Checks, "Adding a check", rules 1-3; the `loom` command, `add-check`
**Finding:** The behaviour matches the spec: Dakota's check needs `dakotaConfirmed`; the fingerprint is computed by Loom; an agent must give one and a stale one is refused with the spec's message; the date is `now` cut to the second with its offset; the entry is appended as text so earlier entries stay byte for byte; validity is checked by re-reading the new bytes. What's missing is tests for three refusal paths: `no-fingerprint` (a premise that names nothing), `not-editable` (an existing history that isn't TOML), and a Dakota check whose optional `--fingerprint` is stale. One spec-silent edge: if the last entry is dated after the machine's clock (an entry merged from a machine whose clock runs ahead), every add-check is refused with `check-order` until the clock catches up, and the message doesn't say why; see Spec issues.
**Change:** Add the three tests.
**Tests:** covered for the main paths (ProofChecksTests); missing as above.

### `refuteStatement` (Source/LoomCore/Edits.cpp:492-535)
**Verdict:** PASS
**Spec:** Statements, D4; the `loom` command, `refute` *(Dakota)*
**Tests:** covered in StatusTests.cpp:427-463 (not confirmed, typed IDs, already refuted, unknown proof, a definition, a stub). A statement can be refuted by one of its own proofs (`refute 4KX7 --by 4KX7.a`); see Spec issues.

## EditSession.h / EditSession.cpp

### `failure`, `refused`, `cantWork` (Source/LoomCore/EditSession.cpp:12-29)
**Verdict:** PASS
**Tests:** covered.

### `Session` (Source/LoomCore/EditSession.h:30-48)
**Verdict:** PASS
**Tests:** covered indirectly.

### `begin` (Source/LoomCore/EditSession.cpp:31-76)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Front matter ("takes the lock `.loom/write.lock` first and waits up to 10 seconds"); Edge cases ("A project from a newer Loom: every command exits 3 without writing")
**Finding:** Correct order (open, refuse a newer format before creating `.loom/`, lock, re-read). Line 31 is a stray doc line, "`<file>:<line>: <severity>: <message>`, as `loom validate` prints a problem.", left above the function's real comment: it describes something else.
**Change:** Delete line 31.
**Tests:** covered (no project, newer format writes nothing, lock held).

### `addedErrors` (Source/LoomCore/EditSession.cpp:80-99)
**Verdict:** PASS
**Tests:** covered ("An edit may fix a problem without fixing every other one").

### `refusedFor` (Source/LoomCore/EditSession.cpp:102-111)
**Verdict:** PASS
**Tests:** covered (exact refusal text in EditsTests).

### `resolveAll` (Source/LoomCore/EditSession.cpp:115-128)
**Verdict:** PASS
**Tests:** covered.

### `notEditable` (Source/LoomCore/EditSession.cpp:131-137)
**Verdict:** PASS
**Tests:** covered ("Loom won't rewrite a file it couldn't fully read").

### EditsTests.cpp (Tests/LoomCoreTests/EditsTests.cpp:1-432)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Verification, "CLI" and the `loom` command table
**Finding:** Covers the layout, refusals writing nothing, CRLF rewrite, lossless guard, lock and format. Missing: `superseded_by` set successfully; editing a stub-only tag refused; a title with a line break or empty title refused; a body that isn't UTF-8 refused; `proof-of-stub` and `premise-cycle` refusals in createProof; a stub as a premise accepted; `addTag` on a CRLF `loom.toml`.
**Change:** Add those cases.
**Tests:** see Finding.

## Checks.h / Checks.cpp

### `checkVerdicts`, `Check` (Source/LoomCore/Checks.h:19-37)
**Verdict:** PASS
**Spec:** Checks field table; D10
**Tests:** covered.

### `sameCheck` (Source/LoomCore/Checks.h:39-42, Checks.cpp:150-155)
**Verdict:** PASS
**Spec:** Merging ("entries are equal when all their fields are equal")
**Tests:** covered (duplicates, round trip).

### `isCheckAuthor` (Source/LoomCore/Checks.h:44-46, Checks.cpp:157-161)
**Verdict:** FAIL
**Severity:** low
**Spec:** Checks, `by` ("`agent:<model>` with `<model>` matching `[a-z0-9][a-z0-9.-]*`")
**Finding:** `^agent:[a-z0-9][a-z0-9.-]*$`: Qt's QRegularExpression is PCRE2 without DOLLAR_ENDONLY, so `$` also matches just before a final `\n`. `by = "agent:claude\n"` is accepted as a valid author (no `check-by`), and `byAgent()` counts it as an agent's check. `add-check --by $'agent:x\n'` would write it too.
**Change:** `QRegularExpression::anchoredPattern(QStringLiteral("agent:[a-z0-9][a-z0-9.-]*"))` (or end with `\z`).
**Tests:** missing: a `by` with a trailing newline.

### `CheckLines`, `ChecksReading` (Source/LoomCore/Checks.h:48-74)
**Verdict:** PASS
**Tests:** covered (header and field lines).

### `EntryReader::string` (Source/LoomCore/Checks.cpp:33-56)
**Verdict:** PASS
**Tests:** covered (empty `changes`); whitespace-only not tested.

### `EntryReader::date` (Source/LoomCore/Checks.cpp:59-81)
**Verdict:** PASS
**Spec:** D1 (offset date-time to the second); Checks, `date`
**Tests:** covered (no offset, missing). Missing: fractions of a second, a plain date.

### `EntryReader::path` (Source/LoomCore/Checks.cpp:84-89)
**Verdict:** PASS
**Spec:** Checks, `record` and `note`; Path safety
**Tests:** covered (`../notes.md`). The spec's `Records/…` or `Reviews/…` prefix for `record` isn't checked; see Spec issues.

### `readEntry` (Source/LoomCore/Checks.cpp:93-139)
**Verdict:** PASS
**Spec:** Checks rule 3; Validation, Checks table
**Tests:** covered (one problem file with every check code and its line).

### `olderDate` (Source/LoomCore/Checks.cpp:143-146)
**Verdict:** PASS
**Spec:** Checks rule 2
**Tests:** covered (ours-first ties).

### `readChecks` (Source/LoomCore/Checks.cpp:163-234)
**Verdict:** PASS
**Spec:** Checks ("plain TOML, one array of tables `checks`, nothing else"); rule 2; Validation
**Tests:** covered (empty file, not TOML, not UTF-8, wrong shape, duplicates, order).

### `formatCheck` (Source/LoomCore/Checks.cpp:236-252)
**Verdict:** PASS
**Spec:** Checks ("Loom writes it as a multi-line string when it has line breaks")
**Tests:** covered (exact bytes, odd `changes` round trip, `Z` offset).

### `formatChecks` (Source/LoomCore/Checks.cpp:254-263)
**Verdict:** PASS
**Tests:** covered.

### `CheckStanding`, `standingOf` (Source/LoomCore/Checks.h:94-116, Checks.cpp:265-291)
**Verdict:** PASS
**Spec:** Checks rules 2, 4-7; verdict table
**Tests:** covered (every row of the verdict table, counting indexes, equal dates, instants across offsets, no fingerprint). The doc states the rules and cites the spec, as CLAUDE.md requires.

### `ChecksMerge`, `mergeChecks` (Source/LoomCore/Checks.h:118-137, Checks.cpp:293-335)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Checks, "Merging two check lists" steps 1-4
**Finding:** Correct: ours, then theirs' new entries, then base entries put back, stable sort by date so ours win ties; it refuses not only unparseable input (step 1) but also lossy or undated input, which is the safe reading. Missing tests: an undated entry refused; a base entry that one side *edited* (the original comes back and the edited one stays as a new entry, which is what rule 1 implies, but nothing pins it).
**Change:** Add the two tests.
**Tests:** covered: interleaving, once-only duplicates, ours-first ties, restore, empty base, unreadable side.

### ChecksTests.cpp (Tests/LoomCoreTests/ChecksTests.cpp:1-341)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Verification, "Verdict table", "Check merge"
**Finding:** Every verdict-table row is a test, as the spec requires, plus the invalid row. Missing: `by` with a trailing newline (FAIL above); a date with fractions of a second and a plain date; a whitespace-only `changes`; an element of `checks` that isn't a table; the merge cases under mergeChecks.
**Change:** Add those cases.
**Tests:** see Finding.

## ProofChecks.h / ProofChecks.cpp

### `proofFingerprint` (Source/LoomCore/ProofChecks.h:16-23, ProofChecks.cpp:7-45)
**Verdict:** PASS
**Spec:** Checks, "The fingerprint"; D3
**Tests:** covered (matches a hand-built FingerprintInput; unknown premise says why). It uses the folder's statement, not `proves`; a mismatch is `proves-mismatch` anyway. Missing: a stub premise in a project (`premise <tag>\n0\n\n`), so that the stub path here is pinned, not only in FingerprintTests.

### `ProofChecking`, `proofChecking` (Source/LoomCore/ProofChecks.h:25-38, ProofChecks.cpp:47-55)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** How status is computed, "Tie-breaks and explanations" (an unchecked statement "says what its deciding proof lacks")
**Finding:** Raised by review-status-validation; I confirmed it here. Line 50 calls `proofFingerprint(contents, id)` without its `problem` out-parameter, so when the fingerprint can't be computed (a premise whose front matter doesn't parse, a premise that names nothing, a missing statement) the reason is thrown away and only an empty `fingerprint` survives. Status's `lacks()` (Status.cpp:236-240) then explains it as "has no agent pass of this version (it changed after its last agent check)", which is wrong: nothing changed, Loom just can't read a file.
**Change:** Add `QString fingerprintProblem;` to `ProofChecking` (empty when the fingerprint was computed), fill it from proofFingerprint's `problem`, and let Status use it in the explanation.
**Tests:** missing: proofChecking on a proof whose premise doesn't parse, checking `fingerprintProblem`.

### `outdatedChecks` (Source/LoomCore/ProofChecks.h:40-48, ProofChecks.cpp:57-66)
**Verdict:** PASS
**Spec:** Statements ("Editing a statement whose text is in a checked proof's fingerprint … is refused"); the `loom` command, `edit-proof`
**Tests:** covered (proved statement, premise, proof body, premises, whitespace-only edit allowed, unchecked proof free, several proofs named). The doc states the rule and cites the spec. Cost is quadratic in proofs (linear lookups per proof), fine at M1's sizes.

### ProofChecksTests.cpp (Tests/LoomCoreTests/ProofChecksTests.cpp:1-332)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Checks, "Adding a check"; Statements
**Finding:** Strong: append keeps hand-written entries byte for byte (comment, odd spacing, literal string, no final newline), refusals write nothing, the outdated-checks guard in every direction. Missing: `no-fingerprint`; `not-editable` for an existing history that isn't TOML; Dakota's check with a stale `--fingerprint`; a stub premise's fingerprint in a project.
**Change:** Add those cases.
**Tests:** see Finding.

## Cycles.h / Cycles.cpp

### `findCycles` (Source/LoomCore/Cycles.h:1-22, Cycles.cpp:12-91)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** How status is computed, "Cycles"; Validation (`premise-cycle`, `stated-using-cycle`, `superseded-invalid`)
**Finding:** Tarjan plus a shortest way back from the smallest ID is correct, and the output doesn't depend on edge order. Two things: (1) The recursion is a `std::function` calling itself once per node on the DFS path. A chain of premises a few thousand long (the spec's "thousands of statements") could exhaust Windows' default 1 MB main-thread stack in a Debug build. Unverified: I didn't measure the frame size. (2) The header comment (Cycles.h:3) says "premises in #7", but premises now use this function (Status.cpp `premiseCycles`), so it's out of date. Also, "byte order" in the doc is really QString's UTF-16 order; that's the same for ASCII tags.
**Change:** Make the DFS iterative with an explicit stack, or add a test with a 10,000-node chain that runs on Windows CI; update the header comment.
**Tests:** see CyclesTests.

### CyclesTests.cpp (Tests/LoomCoreTests/CyclesTests.cpp:1-40)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** Covers none, self-loop, start at smallest, separate groups, shortest way. Missing: one group with two circles (only one is reported, as the doc says), a successor that isn't a key, the same graph given in another key order, and a long chain (depth).
**Change:** Add those cases.
**Tests:** see Finding.

## Cross-boundary notes
- **To review-papers (not delivered):** Papers.cpp:352, 353, 451 (`arxivId`, `editionId`, `sessionRule`) end their patterns in `$`, which in PCRE2 matches before a final `\n`, so e.g. `id = "arxiv-v1\n"` passes. Same issue as Checks.cpp:159 here. The send failed: "No agent named 'review-papers' is reachable", and ListAgents wasn't available to find the right name. Please relay.
- **To review-foundations (not delivered):** ProjectToml.cpp:100 `tagSyntax` `^[a-z0-9]+(-[a-z0-9]+)*$` accepts `"energy\n"` for the same reason. The send failed the same way.
- **To review-status-validation (not delivered):** empty `refuted_by`/`superseded_by`/`kind` pass checkContents (ProjectContents.cpp:199, 209-215) and Status (Status.cpp:94); I log the fix on the reader side (readStatement FAIL above). Also FYI: `tagsInUse` (ProjectContents.cpp:668-690) counts proof and verbose folders but not checks files' tags. That's harmless today because createProof's `highestProofNumber` counts checks files. The send failed the same way.
- **From review-foundations (relayed by the coordinator):** (1) They confirmed the `ProjectToml.cpp:100` `$` finding and logged it as FAIL (low). (2) `FieldReader::date` (FieldReader.cpp:106-124) turns `created = 0000-01-01` into an invalid QDate (QDate has no year 0) without a problem and without setting `lost`. I checked my side. readStatement, readProof and readVerboseProof all report nothing and keep `lossless` true, so `loom validate` accepts the file; I added this to the readStatement FAIL. `formatStatement`'s `date()` (StatementsAndProofs.cpp:87-91) then leaves `created` out. Edits don't delete it silently: `commit()` re-reads the new bytes, finds `missing-field created`, and refuses as an added error, though the refusal is confusing. So the fix belongs in the reader: report the out-of-range date and set `lost`, which makes the file `not-editable`. The writer needn't refuse; that would duplicate the guard. (3) `crlfToLf` (FrontMatter.cpp:64) isn't idempotent: `\r\r\n` reads as `\r\n`, so each rewrite drops one more CR. I traced it through my code. The body Loom keeps loses the lone CR on the second rewrite, which breaks "byte for byte apart from CRLF becoming LF". The fingerprint is unaffected, because normaliseForFingerprint runs on the already-converted body and gives the same text both times, so no check is outdated. bodyFromText shares the behaviour; I noted it there. The fix is foundations' (`crlfToLf`).
- **From review-status-validation (relayed by the coordinator):** when a premise's front matter doesn't parse, `proofChecking()` leaves the fingerprint empty and Status's `lacks()` (Status.cpp:236-240) gives a wrong reason, "it changed after its last agent check". I confirmed it: `ProofChecks.cpp:50` drops proofFingerprint's `problem`. Logged as NEEDS-CHANGES on `proofChecking`: carry the problem string in `ProofChecking`.
- **From review-papers (relayed by the coordinator):** they independently confirmed the readStatement empty-string finding for `id` and `kind`.
- No other messages received.

## Spec issues
- **`superseded_by` and stubs.** The table says "The tag of another statement", but both checkContents and `edit-statement --superseded-by` accept a paper stub. The spec should say whether a stub may supersede a statement (plausible: "it was already in a paper").
- **Setting `level = "failed"`.** Checks, Verdicts, `fail`: "Loom doesn't set `level = "failed"` itself; that's Dakota's click". But `edit-proof --level` isn't marked *(Dakota)* and the code lets anyone set it. The spec should say whether `--level failed` is a Dakota click.
- **`record` prefix.** The Checks table says `record` is "(`Records/…` or `Reviews/…`)", but no validation code exists for another prefix, so the code accepts any project path. Either add a code or drop the parenthesis as a rule.
- **Refuting by its own proof.** `refute 4KX7 --by 4KX7.a` is accepted; the spec doesn't say a refuting proof must not be a proof of the statement itself.
- **Clock skew and `check-order`.** add-check dates entries with the local clock; if the history's last entry (merged from another machine) is later than now, add-check is refused by `check-order` with no explanation. The spec is silent; it could say to refuse with a clearer message, or clamp to the last entry's date.
- **Empty strings.** The spec says optional fields are "left out when empty or absent" for writing, but doesn't say that an empty string in a required or reference field is an error when reading. The readStatement and readProof FAILs assume it is (`kind` "one of …", `refuted_by` "a proof ID").
