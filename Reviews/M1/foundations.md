# M1 review: foundations

**Scope:** `Source/LoomCore/` Ids, FrontMatter, FieldReader, TomlFields, TomlWriter, ProjectToml, ProjectFiles, NewProject, Problem, Version, Git, Hooks, Fingerprint (.h and .cpp); `Tests/LoomCoreTests/` IdsTests, FrontMatterTests, ProjectTomlTests, ProjectFilesTests, NewProjectTests, HooksTests, FingerprintTests, TestSupport.h/.cpp, FakeGit.cpp, ProjectFixture.h.
**Summary:** 71 PASS / 24 NEEDS-CHANGES / 3 FAIL (98 entries). What matters most: (1) `installHooks` (Hooks.cpp:63-124, FAIL medium) follows `core.hooksPath` outside the project, so with a global hooks path it renames the user's shared hook and makes every other repository's commits fail. It also never checks that `root` is a Loom project at its repository's top. (2) `problemLine` (Problem.cpp:8-13, FAIL low) chains `QString::arg`, which garbles file names containing `%1`-`%9` in `loom validate` output. (3) `readTags` (ProjectToml.cpp:100, FAIL low) uses a regex whose `$` accepts a tag ending in a newline. The path safety, front matter splitting, UTF-8 validation and fingerprint code is solid; I recomputed the fingerprint test's hashes independently and they match.

## Ids (Ids.h, Ids.cpp)

### `tagAlphabet`, `tagDraws` (Source/LoomCore/Ids.h:14-17)
**Verdict:** PASS
**Spec:** IDs
**Tests:** covered (alphabet compared byte for byte; draw limit tested).

### `isTag` (Source/LoomCore/Ids.cpp:33-42)
**Verdict:** PASS
**Spec:** IDs
**Tests:** covered.

### `normaliseTag` (Source/LoomCore/Ids.cpp:44-60)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** IDs ("lower case is accepted, `O`/`o` is read as `0`, and `I`, `i`, `L`, `l` as `1`")
**Finding:** `QChar::toUpper()` is Unicode case mapping, so some non-ASCII letters turn into ASCII ones: dotless `ı` (U+0131) upper-cases to `I` and is then read as `1`; long `ſ` (U+017F) upper-cases to `S`. So `normaliseTag("ſ4K7") == "S4K7"` and `normaliseTag("ı234") == "1234"`. Harmless on the command line, but `aliasProblem()` uses this function, so the alias `ſ4K7` is refused as "shaped like a tag".
**Change:** Upper-case only ASCII `a`–`z` (e.g. `if (c >= u'a' && c <= u'z') c = QChar(c.unicode() - 32)`), and reject anything else outside the alphabet before mapping.
**Tests:** missing: a non-ASCII letter that upper-cases to ASCII (`ı`, `ſ`).

### `generateTag` (Source/LoomCore/Ids.cpp:62-77)
**Verdict:** PASS
**Spec:** IDs, Generation (uniform per character via `bounded(32)`, all-digit and used tags redrawn, 100 draws)
**Tests:** covered (seeded generator, whole alphabet, collision, give-up).

### `proofLetters` (Source/LoomCore/Ids.cpp:79-89)
**Verdict:** PASS
**Spec:** IDs, Proof IDs
**Tests:** covered through `z` → `aa`, `zz` → `aaa`.

### `proofNumber` (Source/LoomCore/Ids.cpp:91-102)
**Verdict:** PASS
**Spec:** IDs, Proof IDs (overflow guard returns 0, as documented)
**Tests:** covered; the overflow guard isn't tested (a 14+ letter string), minor.

### `ProofIdParts` (Source/LoomCore/Ids.h:51-54)
**Verdict:** PASS

### `splitProofId` (Source/LoomCore/Ids.cpp:104-114)
**Verdict:** PASS
**Spec:** IDs, Proof IDs
**Tests:** covered (`4KX7.a.b`, `4KX.a`, `.a`, upper-case letters).

### `normaliseProofId` (Source/LoomCore/Ids.cpp:116-126)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** IDs
**Finding:** Same Unicode-mapping issue as `normaliseTag` on the letters: `QString::toLower()` maps the Kelvin sign U+212A to ASCII `k`, so `4KX7.\u212A` is accepted as `4KX7.k`.
**Change:** Lower-case ASCII `A`–`Z` only.
**Tests:** missing the non-ASCII case.

### `aliasProblem` (Source/LoomCore/Ids.cpp:128-142)
**Verdict:** PASS
**Spec:** IDs, Aliases. The reading of "shaped like a tag" as "normaliseTag() accepts it" is documented and has a real reason (an alias `thm1` would be read as the tag `THM1` on the command line), but it is broader than the spec's words; see Spec issues.
**Tests:** covered; missing a length test with astral characters (100 vs 101 surrogate pairs), since the code counts code points.

### `inAlphabet`, `isLowerLetters` (Source/LoomCore/Ids.cpp:15-29)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** No doc comments (CLAUDE.md, "Every class and function gets a doc comment"). Every other helper in this slice has one.
**Change:** Add one-line doc comments.
**Tests:** covered indirectly.

### IdsTests.cpp (Tests/LoomCoreTests/IdsTests.cpp:1-116)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Verification, "IDs"
**Finding:** Covers every row the spec's Verification asks for. Missing: non-ASCII characters that case-map to ASCII (`ı`, `ſ`, Kelvin sign) in `normaliseTag`/`normaliseProofId`; alias length with surrogate pairs; `proofNumber` overflow.
**Change:** Add those cases (they will fail until the two functions above are fixed; list under Flags if done in the same PR).
**Tests:** n/a

## Front matter (FrontMatter.h, FrontMatter.cpp)

### `FrontMatterSplit` (Source/LoomCore/FrontMatter.h:19-32)
**Verdict:** PASS

### `lineAt` (Source/LoomCore/FrontMatter.cpp:11-14)
**Verdict:** PASS

### `firstInvalidUtf8` (Source/LoomCore/FrontMatter.cpp:18-62)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Validation, `not-utf8` ("the first bad byte is at offset {n}")
**Finding:** RFC 3629 ranges are right (overlongs, surrogates, > U+10FFFF, C0/C1/F5+ all refused). But a sequence cut off at the end reports `i + k`, which is the file size: for the 3-byte file `x\xE2\x82` the message says "the first bad byte is at offset 3", and there is no byte 3. The test pins this value (`== 3`).
**Change:** Report the lead byte (`return i;`) for a truncated sequence, as is done for a bad second byte; update the test (Flags).
**Tests:** covered, but the test asserts the past-the-end offset.

### `crlfToLf` (Source/LoomCore/FrontMatter.cpp:64-67)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Front matter, "Line endings" ("When Loom rewrites such a file, the whole file comes out LF")
**Finding:** One pass of `replace("\r\n", "\n")` isn't idempotent. A body containing `\r\r\n` (a lone CR before a CRLF, as produced by some broken editors or old-Mac text pasted into a Windows file) becomes `\r\n`, so when Loom rewrites the file the result still contains a CR LF pair, contrary to "the whole file comes out LF". The next read turns that into `\n`, so the lone CR is lost on the second rewrite (a change to body bytes the user never made). The fingerprint applies `crlfToLf` again (Fingerprint.cpp:23), so it sees `\n` where the body Loom holds is `\r\n`; the hash is stable across rewrites, so no check is wrongly outdated.
**Change:** Decide in the spec what `\r\r\n` means; the simplest consistent rule is to apply the replacement until no CRLF remains (or treat a CR run before LF as one line ending). Then add a test.
**Tests:** missing `\r\r\n`.

### `isBlankBody` (Source/LoomCore/FrontMatter.cpp:69-72)
**Verdict:** PASS
**Spec:** Statements ("only whitespace counts as empty")
**Tests:** covered.

### `splitFrontMatter` (Source/LoomCore/FrontMatter.cpp:74-122)
**Verdict:** PASS
**Spec:** Front matter (UTF-8 first, BOM skipped with warning, CRLF as LF, first line exactly `+++`, first closing `+++` wins, body after its line ending, file line numbers); Edge cases (`+++` in body)
**Tests:** covered: verbatim `+++`, CRLF, lone CR, Unicode, BOM, missing/unclosed, closing at EOF, non-UTF-8 line and offset.

### `joinFrontMatter` (Source/LoomCore/FrontMatter.cpp:124-127)
**Verdict:** PASS
**Spec:** Front matter (writes LF, no BOM)
**Tests:** covered (BOM round trip).

### FrontMatterTests.cpp (Tests/LoomCoreTests/FrontMatterTests.cpp:1-126)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Verification, "Front matter round trip"
**Finding:** Good coverage. Missing: BOM and CRLF together; empty front matter (`+++\n+++\nBody`); `\r\r\n`; a non-UTF-8 byte in a CRLF file (the line number is counted on the raw bytes, which is right, but untested).
**Change:** Add those cases.
**Tests:** n/a

## FieldReader (FieldReader.h, FieldReader.cpp)

### `quotedList` (Source/LoomCore/FieldReader.cpp:12-18)
**Verdict:** PASS

### `parsePlainToml` (Source/LoomCore/FieldReader.cpp:20-37)
**Verdict:** PASS
**Spec:** Validation, `not-utf8`, `toml-syntax`
**Tests:** covered through PapersTests and ChecksTests (other reviewers' files).

### `FieldReader` (struct) (Source/LoomCore/FieldReader.h:32-76)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** The header includes `StatementsAndProofs.h` only to get `FieldLines`, so the paper readers (Papers.cpp) depend on the statements header through a "shared" reader. Fragile layering, not a bug.
**Change:** Move `FieldLines` into `FieldReader.h` (or `TomlFields.h`) and have `StatementsAndProofs.h` include it.
**Tests:** no test file of its own; exercised by StatementsAndProofsTests and PapersTests.

### `FieldReader::FieldReader` (Source/LoomCore/FieldReader.cpp:39-42)
**Verdict:** PASS

### `FieldReader::unknownFields` (Source/LoomCore/FieldReader.cpp:44-49)
**Verdict:** PASS
**Spec:** Front matter, "Unknown fields are errors"

### `FieldReader::string` (Source/LoomCore/FieldReader.cpp:51-71)
**Verdict:** PASS
**Spec:** Validation, `missing-field`, `wrong-type`. Keeping an empty required string (reported but kept) is right for "rewriting loses nothing".
**Tests:** covered via the statement tests.

### `FieldReader::list` (Source/LoomCore/FieldReader.cpp:73-104)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Validation ("`<line>` is the line of the offending value, else of its table header, else 1")
**Finding:** Unlike `string`, `date` and `integer`, `list` has no `missingLine` parameter: a missing required list is always reported at line 1. Right today (the only required list is a proof's `premises`, in front matter, where line 1 is the opening `+++`), but wrong as soon as a required list sits in a nested table of a plain TOML file.
**Change:** Add `int missingLine = 1` as the other readers have.
**Tests:** covered for front matter.

### `FieldReader::date` (Source/LoomCore/FieldReader.cpp:106-124)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Front matter, canonical form; "never lose user text"
**Finding:** (Partly unverified: assumes toml++ accepts year `0000`, which TOML 1.0's `4DIGIT` year allows.) `created = 0000-01-01` parses, but `QDate(0, 1, 1)` is invalid (Qt has no year 0). The reader returns the invalid date with no problem and without setting `lost`, so the document counts as lossless; the statement/proof writer (StatementsAndProofs.cpp:87-91) skips invalid dates, so the next rewrite silently deletes `created`, and the file then fails `missing-field`.
**Change:** If the resulting `QDate` is invalid, report `wrong-type`/`bad-value` and set `lost = true`.
**Tests:** missing.

### `FieldReader::integer` (Source/LoomCore/FieldReader.cpp:126-143)
**Verdict:** PASS

### `FieldReader::badValue` (Source/LoomCore/FieldReader.cpp:145-148)
**Verdict:** PASS

### `FieldReader::repeats` (Source/LoomCore/FieldReader.cpp:150-158)
**Verdict:** PASS
**Finding (note):** element indexes line up with `lines.elements` because both skip non-string elements.

### `FieldReader::tagSyntax` (Source/LoomCore/FieldReader.cpp:160-166)
**Verdict:** PASS
**Spec:** Validation, `id-syntax`; Edge cases ("in a file: `id-syntax`, with the corrected tag suggested")

### `FieldReader::aliases` (Source/LoomCore/FieldReader.cpp:168-176)
**Verdict:** PASS
**Spec:** Validation, `alias-invalid`

## TomlFields (TomlFields.h, TomlFields.cpp)

### `fromUtf8` (Source/LoomCore/TomlFields.cpp:7-10)
**Verdict:** PASS

### `lineOf` (Source/LoomCore/TomlFields.cpp:12-15)
**Verdict:** PASS

### `describeToml` (Source/LoomCore/TomlFields.cpp:17-33)
**Verdict:** PASS

### `Need` (Source/LoomCore/TomlFields.h:31)
**Verdict:** PASS

### `FieldChecker` (struct) (Source/LoomCore/TomlFields.h:34-71)
**Verdict:** PASS

### `FieldChecker::add` (Source/LoomCore/TomlFields.cpp:35-38)
**Verdict:** PASS

### `FieldChecker::unknownFields` (Source/LoomCore/TomlFields.cpp:40-50)
**Verdict:** PASS
**Tests:** covered (ProjectTomlTests, line of the key).

### `FieldChecker::missingField` (Source/LoomCore/TomlFields.cpp:52-55)
**Verdict:** PASS

### `FieldChecker::wrongType` (Source/LoomCore/TomlFields.cpp:57-60)
**Verdict:** PASS

### `FieldChecker::stringField` (Source/LoomCore/TomlFields.cpp:62-77)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** Duplicates `FieldReader::string` with a different rule: here an empty `RequiredNonEmpty` value is reported and dropped (returns nothing); there it is reported and kept. For `loom.toml` this is harmless today because Loom never rewrites `loom.toml` from the parsed config (`addTag` appends text), but two readers of the same thing with different loss behaviour is a trap for whoever adds the next `loom.toml` edit.
**Change:** Keep one string reader (keep-and-report), and use it from ProjectToml.cpp.
**Tests:** covered (`name = ""` in ProjectTomlTests).

## TomlWriter (TomlWriter.h, TomlWriter.cpp)

### `escapeBasic` (Source/LoomCore/TomlWriter.cpp:15-36)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Front matter, canonical form ("escaping `"`, `\` and control characters")
**Finding:** The code is right (all C0 controls and DEL escaped; surrogate pairs pass through in order). But no test anywhere writes a control character other than tab, CR and LF (`\u0001`, `\u001F`, DEL), so the `\uXXXX` branch is untested; the spec's Verification asks for "strings escaped".
**Change:** Add a round-trip test with `\x01`, `\x1F`, `\x7F`, `\b`, `\f`.
**Tests:** partly covered (quotes, backslash, tab, CRLF in ProjectTomlTests).

### `tomlBasicString` (Source/LoomCore/TomlWriter.cpp:40-43)
**Verdict:** PASS

### `tomlMultilineString` (Source/LoomCore/TomlWriter.cpp:45-52)
**Verdict:** PASS
**Finding (note):** adds a final newline, so `idea = "x"` would come back as `"x\n"` if `loom.toml` were ever rewritten from its config; only new-project uses it today.
**Tests:** covered (`"""` inside, CRLF).

### `tomlMultilineStringAsIs` (Source/LoomCore/TomlWriter.cpp:54-57)
**Verdict:** PASS
**Tests:** covered through ChecksTests (other reviewer).

### `tomlOffsetDateTime` (Source/LoomCore/TomlWriter.cpp:59-67)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** D1, Checks (`date`: offset date-time to the second)
**Finding:** Correct for every real offset; an offset with seconds (only historical zones) would be truncated to minutes, irrelevant for "now". No direct test of the `Z` branch or a positive offset (`+05:30`); the only coverage is check files written at the test machine's local offset.
**Change:** Add a test with `QDateTime(…, QTimeZone::fromSecondsAheadOfUtc(0))`, `+19800` and `-21600`.
**Tests:** missing as above.

### `tomlKey` (Source/LoomCore/TomlWriter.cpp:69-75)
**Verdict:** PASS
**Tests:** bare keys covered via paper tests; a quoted key is untested (minor).

### `tomlDate` (Source/LoomCore/TomlWriter.cpp:77-80)
**Verdict:** PASS

### `tomlStringArray` (Source/LoomCore/TomlWriter.cpp:82-89)
**Verdict:** PASS

## ProjectToml (ProjectToml.h, ProjectToml.cpp)

### `projectFormat`, `ProjectTag`, `ProjectConfig`, `ProjectTomlReading` (Source/LoomCore/ProjectToml.h:17-45)
**Verdict:** PASS
**Spec:** `loom.toml`, D7

### `makeChecker` (Source/LoomCore/ProjectToml.cpp:25-28)
**Verdict:** PASS

### `readPreamble` (Source/LoomCore/ProjectToml.cpp:31-86)
**Verdict:** PASS
**Spec:** `loom.toml` (`tex.preamble`), Path safety, Validation (`bad-path`, `preamble-missing`)
**Tests:** covered (missing, `..`, backslash, not `.tex`, folder named `.tex`, empty list). A preamble entry that is a link leaving the project (the `resolveProjectPath` branch at line 74) is untested here, but resolveProjectPath itself is.

### `readTags` (Source/LoomCore/ProjectToml.cpp:89-138)
**Verdict:** FAIL
**Severity:** low
**Spec:** `loom.toml` (`tags`: `id` matches `[a-z0-9]+(-[a-z0-9]+)*`), Validation (`tag-syntax`)
**Finding:** (Raised by review-statements-proofs; confirmed by reading.) The pattern at line 100 is `^[a-z0-9]+(-[a-z0-9]+)*$`, and in PCRE2, which QRegularExpression uses without `DOLLAR_ENDONLY`, `$` also matches just before a final newline. So `id = "energy\n"` (a TOML escape) passes `tag-syntax` and is declared as a tag holding a newline, which the spec's rule forbids. Duplicate and kind checks are fine.
**Change:** `QRegularExpression(QRegularExpression::anchoredPattern(u"[a-z0-9]+(-[a-z0-9]+)*"))`, or end the pattern with `\z`.
**Tests:** missing: `id = "energy\n"` must give `tag-syntax`.

### `readProjectToml` (Source/LoomCore/ProjectToml.cpp:142-209)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Validation, `not-utf8`
**Finding:** Parses with `toml::parse` directly (line 149) instead of `parsePlainToml`, so a `loom.toml` with an invalid UTF-8 byte (e.g. a Latin-1 `é` in `name`) is reported as `toml-syntax` with toml++'s wording, not `not-utf8` with the spec's message and offset, unlike every other plain TOML file. Everything else (format first, `format-too-new` replaces all other problems, unknown fields, sorting) matches the spec.
**Change:** Use `parsePlainToml(bytes, checker)`.
**Tests:** missing a non-UTF-8 `loom.toml`.

### `formatProjectToml` (Source/LoomCore/ProjectToml.cpp:211-225)
**Verdict:** PASS
**Spec:** `loom.toml` (the example's layout, byte for byte)
**Tests:** covered (spec example reproduced exactly; round trip with quotes, backslashes, tab, Unicode, `"""`).

### `OpenStatus`, `OpenedProject` (Source/LoomCore/ProjectToml.h:66-84)
**Verdict:** PASS

### `openProject` (Source/LoomCore/ProjectToml.cpp:227-257)
**Verdict:** PASS
**Spec:** The project folder; `format-too-new` ("every command exits 3 without writing")
**Tests:** covered for NotAProject, Opened, FormatTooNew; `Unreadable` (e.g. `loom.toml` a link leaving the project, or a folder) untested.

### ProjectTomlTests.cpp (Tests/LoomCoreTests/ProjectTomlTests.cpp:1-357)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Verification, "Field validation", "Canonical writer"
**Finding:** Thorough, with exact messages and line numbers. Missing: a non-UTF-8 `loom.toml`; a CRLF `loom.toml`; `OpenStatus::Unreadable`; a preamble path through a link that leaves the project.
**Change:** Add those cases.
**Tests:** n/a

## ProjectFiles (ProjectFiles.h, ProjectFiles.cpp)

### `isInside` (Source/LoomCore/ProjectFiles.cpp:18-29)
**Verdict:** PASS
**Finding (note):** the prefix test adds `/`, so `C:/Proj2` is not "inside" `C:/Proj`; case-insensitive on Windows only.

### `canonicalPath` (Source/LoomCore/ProjectFiles.cpp:37-44)
**Verdict:** PASS
**Spec:** Path safety ("checked on the canonical path"; junctions resolved via `std::filesystem::canonical`, as the wiki gotcha says)

### `isProjectPath` (Source/LoomCore/ProjectFiles.cpp:48-58)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Path safety
**Finding:** (Unverified: needs Windows.) Refuses `\`, `:`, empty, `.` and `..` segments, which covers absolute paths, drive letters and streams. It accepts Windows reserved device names (`Records/con.md`, `Notes/nul.txt`) and segments ending in `.` or space (`Records/job.md.`), which Win32 silently rewrites: `Records/job.md.` names the same file as `Records/job.md`, and `con.md` may name the console device rather than a file in the project. None of these escapes the project, and tags can't hit a device name (Crockford's alphabet has no `O`, `U`, `L`), so the exposure is limited to hand-written paths (`record`, `note`, `pdf`, `session`, preamble).
**Change:** Also refuse segments that end in `.` or space and the reserved names `CON PRN AUX NUL COM0-9 LPT0-9` (with or without an extension), on every OS, so a project is portable.
**Tests:** missing those cases.

### `resolveProjectPath` (Source/LoomCore/ProjectFiles.cpp:60-91)
**Verdict:** PASS
**Spec:** Path safety
**Finding (note):** the deepest existing ancestor is canonicalised and compared with the canonical root; a dangling link at any level is refused. I tried `...` segments, links inside the project, a project reached through a link, and junction roots: all safe because the comparison is on canonical paths. The check-then-use race is inherent and acceptable for a single-user tool.
**Tests:** covered (`..`, absolute, link out, link in, project through a link); a dangling link is untested.

### `readProjectFile` (Source/LoomCore/ProjectFiles.cpp:93-102)
**Verdict:** PASS

### `writeProjectFile` (Source/LoomCore/ProjectFiles.cpp:104-120)
**Verdict:** PASS
**Spec:** Front matter, "Writes are atomic: through `QSaveFile`"; Path safety
**Finding (note):** a short write or failed commit leaves the old file untouched (QSaveFile cancels in its destructor); `setDirectWriteFallback` stays off, so it never writes in place.
**Tests:** covered.

### ProjectFilesTests.cpp (Tests/LoomCoreTests/ProjectFilesTests.cpp:1-120)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Verification, "Path safety"
**Finding:** Covers the spec's three rows (`..`, absolute paths, links leaving the project, with a junction fallback on Windows). Missing: a dangling link (the `isSymbolicLink() || isJunction()` branch at line 73); a case-changed path on Windows; reserved names, once refused.
**Change:** Add the dangling-link case at least.
**Tests:** n/a

## NewProject (NewProject.h, NewProject.cpp)

### `NewProjectRequest`, `NewProjectOutcome`, `NewProjectResult` (Source/LoomCore/NewProject.h:13-48)
**Verdict:** PASS

### `failure` (Source/LoomCore/NewProject.cpp:22-25)
**Verdict:** PASS

### `readKit` (Source/LoomCore/NewProject.cpp:31-50)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The project folder ("Files it creates")
**Finding:** A kit resource that can't be opened is skipped (`continue`, lines 41-43), and `createProject` only fails when the whole kit is empty. A build with one broken resource would create a project without, say, `.gitattributes` and report `Created`, quietly losing `eol=lf` and the merge driver mapping. Unlikely with compiled-in resources, but the failure mode is silent.
**Change:** Return an error (or an empty kit) if any listed resource can't be read, so `kit-missing` fires.
**Tests:** covered for the normal case (exact files and bytes).

### `gitEnvironment` (Source/LoomCore/NewProject.cpp:54-60)
**Verdict:** PASS
**Tests:** covered (`GIT_DIR` pointing at an outer repository is ignored).

### `createProject` (Source/LoomCore/NewProject.cpp:64-161)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The project folder, steps 1-6; Edge cases ("`loom new-project` without git identity: stops before writing anything")
**Finding:** Matches the spec's steps, and orders them better than the spec's numbered list (it creates the folder after the git checks, as the edge case requires). Three smaller points:
1. The identity check (lines 99-107) asks `git config --get user.name/user.email` with `GIT_DIR` pointed at the not-yet-existing repo. That misses identity git would actually use for the commit: `GIT_AUTHOR_*`/`GIT_COMMITTER_*`/`EMAIL` environment variables, and `includeIf "gitdir:…"` sections that will match the new repo once it exists. A user whose identity lives only there is refused with "Git doesn't know your name and email yet" although the commit would succeed.
2. `git lfs install --local` (step 4, from the spec) honours `core.hooksPath` the same way `installHooks` does (see below); with a global hooks path it writes LFS's hooks into the shared folder. That's a spec-level issue (see Spec issues).
3. Lines 150-154 duplicate `whyGitFailed` from Hooks.cpp.
**Change:** (1) Accept the identity if the environment variables are set, or check with `git var GIT_COMMITTER_IDENT` run after `git init` but before writing files, or say in the doc comment that only config is consulted. (3) Share one helper.
**Tests:** covered: exact files and bytes, one commit, author, branch, LFS filter, every refusal, no git, no LFS, no identity, outer repo identity ignored, a failing git step leaves files. Missing: the `hooks-failed` path; a name with `"`, `\` or a newline (loom.toml escaping plus commit message).

### NewProjectTests.cpp (Tests/LoomCoreTests/NewProjectTests.cpp:1-310)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Verification, "New project layout"
**Finding:** Covers every item the spec's Verification row lists. Missing: `hooks-failed`; a project name with quotes, backslash or newline; identity from environment variables (documents the behaviour either way).
**Change:** Add those cases.
**Tests:** n/a

## Problem (Problem.h, Problem.cpp)

### `Severity`, `Problem` (Source/LoomCore/Problem.h:10-24)
**Verdict:** PASS

### `problemLine` (Source/LoomCore/Problem.cpp:8-13)
**Verdict:** FAIL
**Severity:** low
**Spec:** Validation (`<file>:<line>: <severity>: <message>`)
**Finding:** Chained `QString::arg()` calls rescan text already substituted. `.arg(problem.file)` puts the file name into the string; the next `.arg(problem.line)` replaces the lowest-numbered `%n` marker everywhere, including inside the file name. Traced: a stray file at the project top named `Smith%2C%20Jones.pdf` (a common URL-encoded download name) gives the `unknown-top-level` problem; `problemLine` first yields `Smith%2C%20Jones.pdf:%2: %3: %4`, then `.arg(1)` replaces both `%2`s → `Smith1C%20Jones.pdf:1: %3: %4`, so `loom validate` prints a file name that doesn't exist. A name containing `%1` is worse (`x%1.tex` → `x7.tex:error: <message>: %4`).
**Change:** One multi-argument call: `QStringLiteral("%1:%2: %3: %4").arg(problem.file, QString::number(problem.line), severityText, problem.message)`.
**Tests:** missing: a problem whose file name contains `%2`.

### `sortProblems` (Source/LoomCore/Problem.cpp:15-20)
**Verdict:** PASS
**Spec:** Validation ("errors first, then warnings, each group in file order")

## Version (Version.h, Version.cpp)

### `versionString` (Source/LoomCore/Version.cpp:5-9)
**Verdict:** PASS
**Spec:** The `loom` command (`--version`)

## Git (Git.h, Git.cpp)

### `GitTool` (Source/LoomCore/Git.h:15-20)
**Verdict:** PASS

### `GitRun` (Source/LoomCore/Git.h:23-33)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** `started` is false not only when git couldn't start but also when it crashed or timed out (as documented). Callers read it as "git isn't installed": `installHooks` (Hooks.cpp:66-68) answers a timeout or crash with "Loom needs git, and `git` didn't run. Install Git for Windows and try again.", which sends the user the wrong way.
**Change:** Split it into `started` and `finished` (or add `timedOut`), and word the messages by case.
**Tests:** n/a

### `runGit` (Source/LoomCore/Git.cpp:7-36)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Toolchain ("Git, Git LFS: run as programs (`QProcess`)")
**Finding:** Correct use of QProcess (stdin written then closed, pipes drained by `waitForFinished`, git killed on timeout). Unverified, Windows only: with `program = "git"` QProcess hands the bare name to `CreateProcess`, whose search order puts the directory of the calling process's current folder before `PATH`. `loom` usually runs with its current folder inside the project, so a `git.exe` or `git.bat` at a project's top (e.g. in a cloned project) could be run instead of Git for Windows.
**Change:** Resolve the program once with `QStandardPaths::findExecutable("git")` when it isn't already absolute, and fail as "git missing" if that finds nothing.
**Tests:** covered indirectly; the lookup can only be checked on Windows.

## Hooks (Hooks.h, Hooks.cpp; template Templates/hooks/pre-commit)

### `HooksOutcome`, `HooksResult` (Source/LoomCore/Hooks.h:15-34)
**Verdict:** PASS

### `failure` (Source/LoomCore/Hooks.cpp:16-23)
**Verdict:** PASS

### `shellQuoted` (Source/LoomCore/Hooks.cpp:26-31)
**Verdict:** PASS
**Tests:** covered (a quote in the path).

### `whyGitFailed` (Source/LoomCore/Hooks.cpp:34-42)
**Verdict:** PASS
**Finding (note):** duplicated inline in NewProject.cpp:150-154.

### `preCommitHook` (Source/LoomCore/Hooks.cpp:46-56)
**Verdict:** PASS
**Spec:** `loom install-hooks` (PATH first, then the installing `loom`; the spec's refusal message, word for word)
**Tests:** covered. The `C:/…` path with Git for Windows' `sh` (`[ -x "C:/…/loom.exe" ]`) can only be checked on Windows.

### `isLoomHook` (Source/LoomCore/Hooks.cpp:58-61)
**Verdict:** PASS

### `installHooks` (Source/LoomCore/Hooks.cpp:63-124)
**Verdict:** FAIL
**Severity:** medium
**Spec:** The project folder, `loom install-hooks` ("writes `.git/hooks/pre-commit`"; "the user's global git config is never touched")
**Finding:** The hook path comes from `git rev-parse --git-path hooks/pre-commit` (line 65), which honours `core.hooksPath`. I checked with git 2.55 in a scratch repo: with `core.hooksPath=/c/SharedHooks`, it prints `C:/SharedHooks/pre-commit`. So for a user with a global `core.hooksPath` (common with company-wide or tool-managed hooks), `loom install-hooks`, and `loom new-project` through it:
- renames the user's shared `pre-commit` to `pre-commit.before-loom` (line 95), switching off that hook for every repository on the machine;
- writes Loom's hook into the shared folder (line 104), so every other repository's commits now run `loom validate --staged`, which finds no project and exits 3, so all commits outside Loom are refused.

This writes outside the project folder and outside `.git`, and changes every repository's behaviour, which the spec's "never touched" intent rules out. The doc comment says the choice ("where git looks for it") is deliberate, but the spec says `.git/hooks/pre-commit`.

Second (raised by review-cli-build; confirmed by reading): `installHooks` never checks that `root` is a Loom project or the top of its repository. (a) `loom install-hooks --project <a git repo without loom.toml>` renames that repo's own `pre-commit`, installs Loom's hook and sets the merge driver, and every later commit there is refused (`loom validate --staged` finds no project and exits 3). (b) If `root` is a subfolder of a larger repository, `rev-parse --git-path` (run in `root`) names the outer repository's hooks, and the hook then runs `loom validate --staged` from the outer top level, where there's no `loom.toml`, so every commit in the outer repository is refused.

Smaller: the return value of `QFile::setPermissions` (line 108) is ignored. On Linux/macOS a hook without the executable bit is skipped by git with only a hint, so the commit guard would be off without an error.
**Change:** Before anything else, refuse unless `root/loom.toml` exists and `git rev-parse --show-toplevel` (canonicalised) equals `root` (canonicalised): "`…` isn't the top of a Loom project's git repository". Then resolve the hook path; if it is outside the repository's own git directory (`git rev-parse --absolute-git-dir`), refuse with a plain message ("Git is set to run hooks from `…` (core.hooksPath), so Loom's hook in this project wouldn't run. …") instead of writing there; the spec needs to say what Loom does then (see Spec issues). Check `setPermissions` and fail `write-failed` if it returns false on non-Windows.
**Tests:** missing: `core.hooksPath` set to a folder outside the repository (set it through `GIT_CONFIG_GLOBAL` in `testGit`'s file); a repository without `loom.toml`; a project folder that is a subfolder of a larger repository; `setPermissions` failure.

### HooksTests.cpp (Tests/LoomCoreTests/HooksTests.cpp:1-149)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Verification, "New project layout" (hook and merge driver), "Commit guard"
**Finding:** Good: hook text, rewrite of Loom's own hook, foreign hook kept, backup in the way refused, not a repository, and an end-to-end commit refused then accepted. Missing: `core.hooksPath` (above); an existing hook that can't be read; `Repository::hook()` has no doc comment.
**Change:** Add the `core.hooksPath`, no-`loom.toml` and subfolder cases with the fix.
**Tests:** n/a

## Fingerprint (Fingerprint.h, Fingerprint.cpp)

### `FingerprintPremise`, `FingerprintInput` (Source/LoomCore/Fingerprint.h:16-32)
**Verdict:** PASS
**Spec:** D3, Checks, "The fingerprint"

### `appendSection` (Source/LoomCore/Fingerprint.cpp:14-17)
**Verdict:** PASS
**Spec:** "The fingerprint", step 2 (label line, byte length line, text, `\n`)

### `normaliseForFingerprint` (Source/LoomCore/Fingerprint.cpp:21-44)
**Verdict:** PASS
**Spec:** "The fingerprint", step 1 (CRLF → LF, trailing spaces/tabs per line, final newlines; nothing else). See `crlfToLf` for the `\r\r\n` note.
**Tests:** covered.

### `fingerprintBytes` (Source/LoomCore/Fingerprint.cpp:46-60)
**Verdict:** PASS
**Spec:** "The fingerprint", step 2 (premises by tag in byte order; a stub is `premise <tag>\n0\n\n`)
**Tests:** covered.

### `fingerprint` (Source/LoomCore/Fingerprint.cpp:62-66)
**Verdict:** PASS
**Spec:** step 3, D2 (12 lowercase hex characters of SHA-256)
**Tests:** covered. I recomputed both expected hashes (`d84640b94eb8`, `3e67474a0e36`) and the lengths (72, 10, 16, 29) independently with Python's hashlib from the spec's description; they match.

### `isFingerprint` (Source/LoomCore/Fingerprint.cpp:68-72)
**Verdict:** PASS

### FingerprintTests.cpp (Tests/LoomCoreTests/FingerprintTests.cpp:1-97)
**Verdict:** PASS
**Spec:** Verification, "Fingerprint" (known byte string and hash; each input changes it; line-end whitespace, CRLF and premise order don't)
**Tests:** covers every item of the Verification row.

## Test support

### TestSupport.h / TestSupport.cpp (Tests/LoomCoreTests/TestSupport.h:1-62, TestSupport.cpp:1-80)
**Verdict:** PASS
**Finding (note):** `testGit` isolates the developer's config (`GIT_CONFIG_GLOBAL`, `GIT_CONFIG_NOSYSTEM`, author/committer variables removed), so a developer's own `core.hooksPath` can't leak in, which is also why the `installHooks` problem above never showed in tests.

### FakeGit.cpp (Tests/LoomCoreTests/FakeGit.cpp:1-39)
**Verdict:** PASS

### ProjectFixture.h (Tests/LoomCoreTests/ProjectFixture.h:1-54)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** The `ProjectFixture` struct and its constructor have no doc comments (CLAUDE.md, "Every class and function gets a doc comment"); the file comment describes it, and its methods are documented.
**Change:** Add a doc comment to the struct and the constructor.
**Tests:** n/a

## Cross-boundary notes
- **Sent (via the coordinator) to review-statements-proofs:** I confirmed their `$` finding. Separately, `FieldReader::date` returns an invalid `QDate` for `0000-01-01` without reporting it, and their writer (StatementsAndProofs.cpp:87-91) then drops `created` on rewrite. I also gave them the `crlfToLf` `
` note.
- **Sent to review-cli-build:** I agreed with their install-hooks finding (logged in `installHooks`), described the `core.hooksPath` problem, and pointed out that `problemLine` garbles `%n` in file names in `loom validate` text output.
- **Sent to review-status-validation:** `problemLine` affects their `unknown-top-level` and `stray-file` problems; the fix is on my side.
- **Received from review-statements-proofs (relayed):** the `tagSyntax` regex at ProjectToml.cpp:100 ends in `$`, which in PCRE2 also matches before a final newline, so `"x
"` passes. Confirmed by reading; logged as a FAIL (low) on `readTags`.
- **Received from review-cli-build (relayed):** `installHooks` doesn't check that `root` holds `loom.toml` or is the git top level, so `install-hooks` on a non-Loom repository, or on a project in a subfolder of a larger repository, installs a hook that refuses every commit there. Confirmed by reading (Hooks.cpp:65 runs `rev-parse` in `root` with no such guard). Folded into the `installHooks` FAIL with a suggested guard.

## Spec issues
1. **`core.hooksPath` (The project folder, `loom install-hooks` and step 4).** The spec says the hook goes in `.git/hooks/pre-commit` and that global config is never touched, but is silent on a user whose git runs hooks from somewhere else. The code follows `core.hooksPath` and writes outside the project (see `installHooks`); writing to `.git/hooks` instead would leave a hook git never runs. The spec should choose: refuse with a message, or write to `.git/hooks` and warn that the commit guard is off. The same applies to `git lfs install --local` in step 4, which (unverified) also installs its hooks wherever `core.hooksPath` points.
2. **CRLF rule is not idempotent (Front matter, "Line endings"; "The fingerprint", step 1).** "Every CRLF counts as LF" applied once to `\r\r\n` leaves a CR LF pair, so "the whole file comes out LF" isn't true for that input and the body bytes change on the second rewrite. The spec should say "repeat until no CRLF is left" or define a lone CR before LF.
3. **"Shaped like a tag" (IDs, Aliases) is undefined.** The code reads it as "the command line would read it as a tag", which refuses common LaTeX labels such as `thm1`, `def1`, `lem2`, `eq12` (each is four characters that normalise to a tag). That's defensible (otherwise `loom status thm1` is ambiguous), but the spec should say it, since imports (later milestones) will meet such labels and need a rule.
4. **Step order in `new-project` (The project folder, steps 1-2).** Step 1 says "Creates `<folder>` if it doesn't exist" before step 2's git checks, but the edge case says new-project "stops before writing anything" without a git identity. The code follows the edge case (it creates the folder after the checks); the steps should be reworded to match.
