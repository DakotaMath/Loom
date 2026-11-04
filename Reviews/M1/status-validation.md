# M1 review: status-validation

**Scope:** `Source/LoomCore/ProjectContents.h/.cpp`, `Source/LoomCore/Status.h/.cpp`, `Source/LoomCore/Validation.h/.cpp`; `Tests/LoomCoreTests/ProjectContentsTests.cpp`, `StatusTests.cpp`, `ValidationTests.cpp`.
**Summary:** 83 PASS / 17 NEEDS-CHANGES / 1 FAIL. I traced the worked example through `buildGraph()` by hand, and every status, deciding proof, `rests on` list, count and explanation sentence matches the spec. Every validation code in the spec's tables is implemented and appears in the EveryProblem test. The findings that matter most: (1) **FAIL**: `checkAgainstCommit()` checks only that committed checks are still *present*, not that they keep their *order*. Swapping two committed checks with the same date passes `check-order` and `check-changed`, and that swap changes which check counts (Validation.cpp:542-568). (2) `readCommit()` treats any failure of `git rev-parse` as "no commit yet". When that happens, `check-changed` and `deleted-since-commit` are skipped without a word (Validation.cpp:360-361). (3) For a proved statement, the deciding proof is "the proof that brought it into the proved set", which differs from the spec's literal tie-break (J3RW gives J3RW.b; the spec's rule gives J3RW.a). The departure is reasoned and logged, but the spec was never amended (Status.cpp:129-155).

## ProjectContents.h / ProjectContents.cpp

### `StatementFile` (Source/LoomCore/ProjectContents.h:24-28)
**Verdict:** PASS
**Tests:** covered (ProjectContentsTests).

### `ProofFile` (Source/LoomCore/ProjectContents.h:31-36)
**Verdict:** PASS
**Tests:** covered.

### `VerboseFile` (Source/LoomCore/ProjectContents.h:39-43)
**Verdict:** PASS
**Tests:** covered.

### `ChecksFile` (Source/LoomCore/ProjectContents.h:46-50)
**Verdict:** PASS
**Tests:** covered.

### `PaperFile` (Source/LoomCore/ProjectContents.h:53-59)
**Verdict:** PASS
**Tests:** covered (EveryProblem, PaperEditsTests).

### `StubFile` (Source/LoomCore/ProjectContents.h:62-67)
**Verdict:** PASS
**Tests:** covered.

### `ProjectContents` (Source/LoomCore/ProjectContents.h:70-95)
**Verdict:** PASS
**Spec:** The project folder; IDs
**Tests:** covered. The lookups are linear scans. That is fine on its own, but see `premiseCycles` for a caller that uses them inside loops.

### `ProjectContents::statement` (Source/LoomCore/ProjectContents.cpp:534-538)
**Verdict:** PASS
**Tests:** covered indirectly.

### `ProjectContents::stub` (Source/LoomCore/ProjectContents.cpp:540-544)
**Verdict:** PASS
**Tests:** covered indirectly.

### `ProjectContents::paper` (Source/LoomCore/ProjectContents.cpp:546-550)
**Verdict:** PASS
**Tests:** covered indirectly.

### `ProjectContents::paperWithKey` (Source/LoomCore/ProjectContents.cpp:552-561)
**Verdict:** PASS
**Tests:** covered indirectly (paper edits).

### `ProjectContents::proof` (Source/LoomCore/ProjectContents.cpp:563-567)
**Verdict:** PASS
**Tests:** covered indirectly.

### `ProjectContents::checksOf` (Source/LoomCore/ProjectContents.cpp:569-573)
**Verdict:** PASS
**Tests:** covered indirectly.

### `ProjectContents::tagWithAlias` (Source/LoomCore/ProjectContents.cpp:575-586)
**Verdict:** PASS
**Tests:** covered (`resolveStatement(…, "thm:energy-decay")`).

### `ContentsLoading` (Source/LoomCore/ProjectContents.h:98-103)
**Verdict:** PASS
**Tests:** `ok == false` (a file that exists but can't be read) is never tested. That is hard to do portably, so this is noted, not required.

### `loadContents` (Source/LoomCore/ProjectContents.h:105-115, .cpp:588-637)
**Verdict:** PASS
**Spec:** The project folder ("a missing folder means the same as an empty one"); Edge cases (stray file); Path safety
**Tests:** covered: missing folders, `stray-file` in `Statements/`, `Proofs/` and `Proofs/<tag>/`, `bad-path` through a directory link. Not covered: a sub-folder inside `Proofs/<tag>/` and a hidden file such as `Statements/.gitkeep` (both should be `stray-file`).

### `checkContents` (Source/LoomCore/ProjectContents.h:117-130, .cpp:639-666)
**Verdict:** PASS
**Spec:** Validation (IDs and references, Statements, Proofs, Papers and stubs)
**Tests:** covered (ProjectContentsTests "Problems that need several files"; EveryProblem).

### `tagsInUse` (Source/LoomCore/ProjectContents.h:132-136, .cpp:668-690)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** IDs, "Generation" and "Permanent" ("tags are never … reused")
**Finding:** The doc says the result includes "every proof folder's tag", but only folders that hold a readable proof file (`.tex`) or a verbose file count. A folder holding only `X.a.checks.toml`, or only stray files, isn't counted, so a new statement can be given tag `X` and inherit that check history. Tags of statements deleted since the last commit (`deleted-since-commit`) aren't counted either, so a deleted tag can be drawn again. That reuses the tag and also silences the warning.
**Change:** Also add the tag of every `ChecksFile` (`proofId.first(4)`). Either list the folder names directly while loading, or make the doc say exactly which folders count. Raise the HEAD case as a spec question: should generation avoid tags present in `HEAD`?
**Tests:** only one assertion (PaperEditsTests:124). Missing: an alias, a checks-only folder, a proof folder without a statement.

### `resolveStatement` (Source/LoomCore/ProjectContents.h:138-142, .cpp:692-703)
**Verdict:** PASS
**Spec:** IDs (forgiving tags, aliases resolved where a person types an ID)
**Tests:** covered (tags, lower case, alias, unknown). Missing: `O`/`L` read as `0`/`1`, and a stub's tag and alias.

### `resolveProof` (Source/LoomCore/ProjectContents.h:144-147, .cpp:705-711)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** IDs
**Finding:** The doc says it reads the tag part "as resolveStatement() reads the tag part", but `resolveStatement()` also resolves aliases, and this function doesn't (`thm:energy-decay.a` gives nothing). Not resolving aliases is reasonable, because aliases may contain `.`, but the doc misleads.
**Change:** Make the doc say "the tag part as normaliseTag() reads it; aliases aren't resolved".
**Tests:** covered (case-insensitive, unknown letter).

### `entriesOf` (Source/LoomCore/ProjectContents.cpp:22-25)
**Verdict:** PASS
**Tests:** covered indirectly.

### `Node` (Source/LoomCore/ProjectContents.cpp:28-38)
**Verdict:** PASS
**Tests:** n/a.

### `ContentsChecker` (struct and constructor) (Source/LoomCore/ProjectContents.cpp:41-75)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** The constructor has no doc comment, which CLAUDE.md requires. The struct's comment says the indexes keep "checking … linear in the size of the project", but `circles()` calls `premiseCycles()`, which calls the linear `contents.statement()` for each premise, and `keyOf()`/`stubs()` call the linear `contents.paper()`. So checking is O(P·S).
**Change:** Document the constructor. Either pass `nodeByTag` into the cycle search (see `premiseCycles`) or drop the "linear" claim.
**Tests:** n/a.

### `ContentsChecker::aliasesOf` (Source/LoomCore/ProjectContents.cpp:78-84)
**Verdict:** PASS
**Tests:** covered.

### `ContentsChecker::node` (Source/LoomCore/ProjectContents.cpp:87-91)
**Verdict:** PASS
**Tests:** covered.

### `ContentsChecker::add` (Source/LoomCore/ProjectContents.cpp:94-97)
**Verdict:** PASS
**Tests:** covered.

### `ContentsChecker::keyOf` (Source/LoomCore/ProjectContents.cpp:100-104)
**Verdict:** PASS
**Tests:** covered (`proof-of-stub` message).

### `ContentsChecker::reference` (Source/LoomCore/ProjectContents.cpp:110-130)
**Verdict:** PASS
**Spec:** IDs ("an alias in front matter is an error that names the tag"); `ref-unknown`, `ref-alias` (error in front matter), `id-syntax` with "Did you mean"
**Tests:** covered (all three outcomes, for `premises`, `stated_using` and `proves`).

### `ContentsChecker::statedUsing` (Source/LoomCore/ProjectContents.cpp:133-143)
**Verdict:** PASS
**Spec:** Statements, `stated_using` ("not itself"); `stated-using-self`
**Tests:** covered.

### `ContentsChecker::identities` (Source/LoomCore/ProjectContents.cpp:146-180)
**Verdict:** PASS
**Spec:** IDs (one namespace for statements and stubs; aliases unique); `id-duplicate`, `alias-duplicate`
**Tests:** covered for statements. Missing: a stub that reuses a statement's tag, and the same alias on a statement and a stub.

### `ContentsChecker::statements` (Source/LoomCore/ProjectContents.cpp:183-217)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Statements (`superseded_by`: "The tag of another statement"); `superseded-invalid`, `refuted-unknown`, `refuted-no-status`, `tag-unknown`
**Finding:** `superseded_by = "7RT2"`, where 7RT2 is a paper stub, gets no problem: `reference()` finds the stub node, so neither `superseded-invalid` nor anything else fires. The spec says the target is "another statement", but `superseded-invalid` has no wording for "is a paper stub". See Spec issues.
A second problem, from review-statements-proofs and confirmed against this function: the readers keep an empty string as if the field were absent. So `refuted_by = ""` with `refuted_on = 2026-11-06` gives no `refuted-incomplete` (StatementsAndProofs.cpp:161-163 checks only that both keys are *present*). Line 210 then skips `refuted-unknown` because `refutedBy` is empty, and `Status.cpp:94` doesn't count the statement as refuted. The file claims Dakota accepted a refutation, yet `loom validate` is clean and the status is computed as if it weren't refuted. `superseded_by = ""` likewise passes line 199.
**Change:** Once the spec gives the wording, report `superseded-invalid` when the target is a stub (`target->stub != nullptr`), e.g. "…, which is a result from {Key}". For empty values, the right fix is in the readers (report `bad-value` for an empty `refuted_by`, `superseded_by` or `kind`; statements-proofs' slice). Until then, this function can't tell "absent" from "empty", because `Statement` uses empty for absent.
**Tests:** covered: self, missing, cycle, `refuted-unknown`, `refuted-no-status`, `tag-unknown`. Missing: a stub target, `refuted_on` alone on a question (`refuted-no-status` reported on the `refuted_on` line).

### `ContentsChecker::stubs` (Source/LoomCore/ProjectContents.cpp:220-253)
**Verdict:** PASS
**Spec:** Papers, stubs (`paper_premises` same paper; `extracted_from`/locators name versions); `stub-paper-premise`, `version-unknown`
**Tests:** covered in EveryProblem (`5TPP`, `8VRS`). Missing: a stub's `stated_using` problems, and a `paper_premises` entry naming the stub itself.

### `ContentsChecker::papers` (Source/LoomCore/ProjectContents.cpp:256-325)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The key rule (step 4); `paper-key-rule`, `paper-key-mismatch`, `paper-key-duplicate`, `session-missing`, `tag-unknown`
**Finding:** (a) `paper-key-rule` fires only when `!keyFitsBase(key, base)`. Since `keyFitsBase` accepts the base or the base plus any one letter, a project holding both `SmithJones24` (unlettered) and `SmithJones24a` with the same base gets no warning. Step 4 says that once a key is shared, both papers carry letters. (b) When no base key can be made, `base.problem` from PaperKeys.cpp is reported under the `paper-key-rule` code (lines 296-297). That text is one of "…it has no authors or editors.", "…it has no year…" or "Can't make a key from the surname `张`…". The spec gives a single message for `paper-key-rule` ("By the key rule this paper's key is `{expected}`, not `{key}`…"), and these texts don't fit it. The no-names case also repeats `authors-missing`, which already fires for the same paper (EveryProblem: Nobody19 gets both). review-papers and review-cli-build flagged the same thing (see Cross-boundary notes). (c) `paper-key-duplicate` compares names case-sensitively (`folderOfName` is a plain `QHash<QString,…>`), so `MacDonald20` and `Macdonald20` (two keys, or a key and an old key) validate cleanly. On NTFS, which is case-insensitive, two such folders can't coexist, and a Linux clone holding both would collapse into one folder on checkout. BibTeX also treats keys that differ only in case as the same. Unverified on Windows.
**Change:** (a) When the key is the bare base and another paper's key or alias is that base plus a letter, warn that it should be lettered. (b) Skip the no-authors case (it is `authors-missing`'s). For the no-year and non-Latin-surname cases, ask the spec for a second message, or a separate code, rather than reusing `paper-key-rule`. (c) Compare keys and old keys case-insensitively for `paper-key-duplicate`, keyed by `name.toCaseFolded()` (and raise it in the spec: "unique, ignoring case").
**Tests:** covered in EveryProblem (mismatch, duplicate, rule, session). Missing: the lettered and unlettered pair; two keys that differ only in case.

### `ContentsChecker::circles` (Source/LoomCore/ProjectContents.cpp:328-372)
**Verdict:** PASS
**Spec:** Cycles ("reported as an error naming the statements in order"); `premise-cycle`, `stated-using-cycle`, `superseded-invalid` (circle)
**Finding:** I traced the worked example: H8KD → J3RW → H8KD and K6YM → M1PQ → K6YM, on line 4 of `.a`. Self-edges are skipped by `findCycles`, so `premise-self` and `stated-using-self` aren't reported twice.
**Tests:** covered (worked example, the 3-cycle in StatusTests, A1A1/B2B2, E5E5/E6E6).

### `ContentsChecker::proofs` (Source/LoomCore/ProjectContents.cpp:375-437)
**Verdict:** PASS
**Spec:** Proofs; `proof-folder-unknown`, `proof-of-stub`, `proves-mismatch`, `proof-of-no-status`, `premise-no-status`, `ref-unknown`/`ref-alias` in premises, `verbose-orphan`, `checks-orphan`
**Tests:** covered (ProjectContentsTests and EveryProblem). Missing: a premise that is a stub of kind `definition` (`premise-no-status` on a stub).

### `join` (Source/LoomCore/ProjectContents.cpp:441-444)
**Verdict:** PASS
**Tests:** n/a.

### `stray` (Source/LoomCore/ProjectContents.cpp:447-451)
**Verdict:** PASS
**Spec:** `stray-file` message
**Tests:** covered.

### `readFile` (Source/LoomCore/ProjectContents.cpp:455-467)
**Verdict:** PASS
**Spec:** Path safety (`bad-path`); exit 3 for a file Loom can't read
**Tests:** `bad-path` covered. The unreadable case isn't tested (see `ContentsLoading`).

### `lettersOf` (Source/LoomCore/ProjectContents.cpp:470-476)
**Verdict:** PASS
**Tests:** covered indirectly (verbose, checks and proof names; `notes.txt` is stray).

### `loadPapers` (Source/LoomCore/ProjectContents.cpp:479-530)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Papers (folder contents); Path safety
**Finding:** When `paper.toml` exists but `readFile()` refuses it as `bad-path` (a link out of the project), `hasPaper` stays false, so the folder also gets `missing-field` "`paper.toml` is missing; every paper folder needs one". That second message is false, since the file is there. Stubs of a folder without a readable paper are still loaded, which is fine.
**Change:** Track "paper.toml seen" separately from "read", and report `missing-field` only when no `paper.toml` entry exists.
**Tests:** a missing `paper.toml` is covered (PaperEditsTests:169). Not covered: a `paper.toml` that is refused, and stray files inside `Papers/<Key>/Statements/`.

## Status.h / Status.cpp

### `Status` (Source/LoomCore/Status.h:22-33)
**Verdict:** PASS
**Spec:** How status is computed (the ten statuses)
**Tests:** covered.

### `statusName` (Source/LoomCore/Status.h:37, .cpp:343-358)
**Verdict:** PASS
**Tests:** covered through the explanations.

### `statusOrder` (Source/LoomCore/Status.h:41, .cpp:360-366)
**Verdict:** PASS
**Spec:** The `loom status` command's JSON shapes (`counts` order)
**Tests:** covered via the CLI JSON (cli-build's slice).

### `Gives` (Source/LoomCore/Status.h:44-50)
**Verdict:** PASS
**Spec:** What a proof gives
**Tests:** covered.

### `givesName` (Source/LoomCore/Status.h:53, .cpp:368-378)
**Verdict:** PASS
**Spec:** The `loom graph` command's JSON shapes (`gives`)
**Tests:** covered via the CLI.

### `ProofNode` (Source/LoomCore/Status.h:56-65)
**Verdict:** PASS
**Tests:** covered.

### `StatementNode` (Source/LoomCore/Status.h:68-93)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The `loom status` command's JSON shapes ("`paper` is the key for stubs")
**Finding:** `paper` is documented as "its paper's key", but `buildGraph()` fills it with the folder name (`node.paper = file.paperFolder`, Status.cpp:448). `stubSentence()` and the JSON then show the folder. The two differ only when there is also a `paper-key-mismatch` error, so the effect is small, but the doc and the code disagree.
**Change:** Either fill `paper` with the parsed key, falling back to the folder (as `ContentsChecker::keyOf` does), or make the doc say "its paper's folder (its key unless `paper-key-mismatch`)".
**Tests:** covered when folder == key (`7RT2`'s paper).

### `ProjectGraph` (Source/LoomCore/Status.h:96-115)
**Verdict:** PASS
**Tests:** covered.

### `ProjectGraph::statement` (Source/LoomCore/Status.cpp:380-385)
**Verdict:** PASS
**Tests:** covered. The binary search depends on `statements` being sorted, which `buildGraph` does before it is used.

### `ProjectGraph::proof` (Source/LoomCore/Status.cpp:387-391)
**Verdict:** PASS
**Tests:** covered.

### `ProjectGraph::count` (Source/LoomCore/Status.cpp:393-397)
**Verdict:** PASS
**Tests:** covered (the worked example's counts).

### `listed` (Source/LoomCore/Status.cpp:19-24)
**Verdict:** PASS
**Tests:** covered (two resting premises).

### `proofBefore` (Source/LoomCore/Status.cpp:27-36)
**Verdict:** PASS
**Spec:** Tie-breaks ("`z` before `aa`")
**Tests:** `z` before `aa` isn't tested in StatusTests: the levels test only has `a`–`d`. Add a statement with proofs `z` and `aa` that both qualify, and check that `z` decides.

### `givesOf` (Source/LoomCore/Status.cpp:41-52)
**Verdict:** PASS
**Spec:** What a proof gives; Checks rule 7
**Finding:** I traced it: `failed`, or `!supports` (either counting check is `fail`), gives nothing; `complete` gives checked or unchecked; sketch and heuristic come from the level. A `fail` on a sketch gives nothing, as the spec says. The doc states the rule and cites the spec.
**Tests:** covered (worked example D9XF.a/b, W5HC, Dakota's fail).

### `Computation` (class and constructor) (Source/LoomCore/Status.cpp:55-70)
**Verdict:** PASS
**Tests:** covered.

### `Computation::run` (Source/LoomCore/Status.cpp:74-81)
**Verdict:** PASS
**Tests:** covered.

### `Computation::isStub` (Source/LoomCore/Status.cpp:91)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** It has no doc comment, which CLAUDE.md requires of every function. It also relies on `paper` being non-empty for every stub, which is true only because `paperFolder` is never empty.
**Change:** Add a one-line doc: "A paper stub: the only nodes with a `paper`".
**Tests:** covered.

### `Computation::isRefuted` (Source/LoomCore/Status.cpp:94)
**Verdict:** PASS
**Spec:** Rule 4 (both fields set)
**Tests:** covered (`refuted_by` without `refuted_on` stays open). `refuted_on` without `refuted_by` isn't tested.

### `Computation::isCitedTag` (Source/LoomCore/Status.cpp:97-101)
**Verdict:** PASS
**Spec:** Rules 1–2 (a definition stub is never cited)
**Tests:** covered ("a stub of a kind without status has none").

### `Computation::unmet` (Source/LoomCore/Status.cpp:104-113)
**Verdict:** PASS
**Spec:** The proved set ("every premise in the proved set or cited"); Tie-breaks ("in tag order")
**Tests:** covered.

### `Computation::presetStatus` (Source/LoomCore/Status.cpp:117-123)
**Verdict:** PASS
**Spec:** Rules 1–3
**Tests:** covered.

### `Computation::computeProvedSet` (Source/LoomCore/Status.cpp:134-155)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** The proved set; Tie-breaks and explanations ("when several qualify, the one with the fewest premises that are not proved or cited, then the lowest letter")
**Finding:** The proved set itself is right. It is the least fixpoint, grown in rounds against the set as each round began, and I traced it on the worked example: round 1 adds 5HN8, 9D3F (7RT2 cited), G5TZ, J3RW (via b), N7ZE; round 2 adds 4KX7, H8KD, P3QA; F2NS is skipped as refuted; K6YM/M1PQ never enter. The problem is the deciding proof for a *proved* statement, which this function records as "the proof that brought it in". In the final state, J3RW.a (premise H8KD, proved) and J3RW.b (no premises) both qualify with 0 unmet premises, so the spec's literal tie-break picks **J3RW.a**. The code picks **J3RW.b**, and StatusTests:136 and :203 lock that in. The departure is well reasoned (it avoids a circular explanation) and is written up in `Wiki/logs/2026-11/2026-11-04-status.md`, but `Specs/m1-core.md` still says otherwise. CLAUDE.md asks for spec silence or errors to be raised, not settled in code.
**Change:** Keep the code, and ask Dakota to amend the Tie-breaks paragraph: "for a proved statement, the deciding proof is the lowest-letter proof that qualified in the round it entered the proved set". If he prefers the literal rule, change the code and tests instead.
**Tests:** covered (J3RW.b, K6YM.b from outside a cycle).

### `Computation::deciding` (Source/LoomCore/Status.cpp:160-175)
**Verdict:** PASS
**Spec:** Tie-breaks
**Finding:** I traced E8TB: a has unmet {3WQ6, 8CJA} = 2, b has {8CJA} = 1, so b decides. W5HC: the sketch rule picks b.
**Tests:** covered.

### `Computation::decide` (Source/LoomCore/Status.cpp:178-208)
**Verdict:** PASS
**Spec:** Rules 4–10
**Finding:** First match wins, in the spec's order. A refuted statement is `false` before the proved check, and a refuted statement is never in the proved set, so T9GB rests on F2NS. Every complete-checked, unrefuted, non-stub statement with no unmet premises is in the fixpoint, so `restsOn` is never empty for `conditional`.
**Tests:** covered (every worked-example row; levels; the recompute chain).

### `Computation::premiseState` (Source/LoomCore/Status.cpp:211-221)
**Verdict:** PASS
**Spec:** "Loom names them, with their statuses"; example sentence for 6BWT
**Tests:** covered for extracted, unchecked, false, conditional and open. Missing: a premise that doesn't exist ("isn't a statement in this project") and a definition premise ("a definition, which has no status").

### `Computation::lacks` (Source/LoomCore/Status.cpp:225-245)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Tie-breaks and explanations (an unchecked statement says what its deciding proof lacks)
**Finding:** When the proof's fingerprint can't be computed (`proofChecking` gives `fingerprint` empty, e.g. a premise's front matter no longer parses), every agent check has `fingerprint != ""`. `older` is then true, and the sentence says "has no agent pass of this version (it changed after its last agent check)". The proof didn't change: Loom can't compute its fingerprint. Input: 4KX7.a [5HN8], pass, pass, then 5HN8.tex's front matter broken, gives "4KX7 unchecked: 4KX7.a is complete, but has no agent pass of this version (it changed …)".
**Change:** When `c.fingerprint.isEmpty()`, say why there's no fingerprint (proofFingerprint's `problem`), e.g. "4KX7.a's fingerprint can't be computed: …".
**Tests:** covered for every lack in the spec (agent pass, Dakota's pass, a newer pass, Dakota's changes, an agent's needs-changes, old fingerprint). Missing: the no-fingerprint case, and "Dakota's pass only, no agent check".

### `Computation::givesNothing` (Source/LoomCore/Status.cpp:248-259)
**Verdict:** PASS
**Spec:** "An open statement says whether it has no proofs or only proofs that don't support it"; D9XF example
**Tests:** covered (failed, agent's fail, Dakota's fail). The unknown-level branch isn't tested.

### `Computation::explain` (Source/LoomCore/Status.cpp:263-318)
**Verdict:** PASS
**Spec:** "`loom status <id>` explains in one sentence"
**Finding:** All six example sentences in the spec are reproduced exactly (4KX7, 6BWT, 8CJA, B7MP shape, D9XF, F2NS). Dakota's `changes` is quoted in full, `simplified()`, while the spec's example ends with "…" (see Spec issues).
**Tests:** covered (every status, every sentence shape).

### `Computation::sortedPremises` (Source/LoomCore/Status.cpp:321-327)
**Verdict:** PASS
**Tests:** covered.

### `Computation::stubSentence` (Source/LoomCore/Status.cpp:330-338)
**Verdict:** PASS
**Tests:** covered (7RT2, 3WQ6). A stub without a locator for `extracted_from` ("a result of …") isn't tested.

### `premiseCycles` (Source/LoomCore/Status.h:130-136, .cpp:399-412)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Cycles
**Finding:** It is correct: self-premises are left to `premise-self` (`findCycles` skips self-edges), stubs are excluded, and proofs of every level count. Two problems. (a) It calls `contents.statement()` (a linear scan) twice per premise, so it costs O(P·k·S), where the spec expects milliseconds at thousands of statements. (b) It is declared in `Status.h` but used by `ProjectContents.cpp`, which therefore includes `Status.h`, which includes `ProjectContents.h`: a circular dependency between the two modules.
**Change:** Build a `QSet` of statement tags once. Move `premiseCycles` next to `checkContents` (or into `Cycles`) so `ProjectContents` doesn't depend on `Status`.
**Tests:** covered (the worked example's two cycles, a 3-cycle; `premise-self` isn't reported as a cycle in EveryProblem).

### `buildGraph` (Source/LoomCore/Status.h:117-128, .cpp:414-493)
**Verdict:** PASS
**Spec:** How status is computed; Recomputing ("a pure function of the files")
**Finding:** There is no stored status and no setter: status is computed, never claimed. When a tag is duplicated, the statement wins over a stub. Unparsed files are left out, so their dependants become conditional, which is safe. The doc states the rule table and the proved-set rule and cites the spec.
**Tests:** covered (worked example, recompute after check, edit and fail).

## Validation.h / Validation.cpp

### `ValidationRequest` (Source/LoomCore/Validation.h:25-34)
**Verdict:** PASS
**Tests:** covered.

### `ValidationOutcome` (Source/LoomCore/Validation.h:37-40)
**Verdict:** PASS
**Tests:** covered.

### `ValidationResult` (Source/LoomCore/Validation.h:43-57)
**Verdict:** PASS
**Tests:** covered.

### `ValidationResult::errors` / `warnings` (Source/LoomCore/Validation.cpp:388-396)
**Verdict:** PASS
**Tests:** covered.

### `validateProject` (Source/LoomCore/Validation.h:59-68, .cpp:572-621)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** Validation (whole project, or `--staged` from git's index; errors first, then warnings, in file order)
**Finding:** `QTemporaryDir staging;` is built unconditionally (line 577), and the constructor creates a directory. So every plain `loom validate` creates and removes a temporary folder, though the doc says it does so only "with `staged`". The rest is right: it reads HEAD from the real root while checking the staged tree, takes the `--staged` NotAProject message, and runs the checks in order and sorts them.
**Change:** Use `std::optional<QTemporaryDir>` and construct it only when `request.staged`.
**Tests:** covered (worked example, EveryProblem, format-too-new, staged vs working, not-a-repo). Missing: `--staged` in a repository with no commit yet (the first commit through the hook), and `GIT_INDEX_FILE` from `git commit <paths>` (if HooksTests covers it, that's foundations' slice).

### `checkStatuses` (Source/LoomCore/Validation.h:70-76, .cpp:398-460)
**Verdict:** PASS
**Spec:** `refuted-but-proved`, `conjecture-proved`, `premise-false`, `premise-superseded`, `verbose-stale` (all warnings); Edge cases (a refuted statement with a checked proof; a false premise)
**Finding:** I traced the worked example: F2NS.a gives complete-checked with no premises, so `refuted-but-proved`; T9GB.a uses F2NS (false), so `premise-false`; P3QA.a uses N7ZE (superseded by 4KX7), so `premise-superseded`. That is exactly the spec's list. Minor: when `superseded_by` is invalid (itself, or missing), `premise-superseded` still names it ("superseded by ZZZY"), next to the `superseded-invalid` error. That is acceptable.
**Tests:** covered (worked example, EveryProblem).

### `add` (Source/LoomCore/Validation.cpp:25-29)
**Verdict:** PASS
**Tests:** n/a.

### `LatexCommand` (Source/LoomCore/Validation.cpp:34-38)
**Verdict:** PASS
**Tests:** n/a.

### `latexCommands` (Source/LoomCore/Validation.cpp:46-70)
**Verdict:** PASS
**Spec:** IDs and references (`\ref`, `\paperref`, `\cite`, `\label`)
**Finding:** The rules are documented: comments are skipped (`\%` is honoured), the command and its argument must be on one line, optional arguments are skipped, and the name must end there. `\ref*{X}` isn't counted as a reference (the test asserts this). hyperref's `\ref*` does refer to X, but the spec is silent, so I'm only noting it.
**Tests:** covered ("body references: what counts as a reference").

### `BodyChecker` (struct and constructor) (Source/LoomCore/Validation.cpp:73-128)
**Verdict:** PASS
**Tests:** covered.

### `BodyChecker::resolve` (Source/LoomCore/Validation.cpp:132-145)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Spec:** `ref-alias` (warning in a body), `ref-dangling`; `Specs/loom.md`, "`\paperref{<stub-id>}` cites a paper's result"
**Finding:** (a) `\paperref{4KX7}`, where 4KX7 is a statement and not a stub, resolves silently. At M2 build time it can't be expanded into `\cite[…]{Key}`. (b) For `\paperref`, a `\label` doesn't count (correctly), yet the `ref-dangling` message still says "it isn't a tag, an alias, or a `\label` in any body". The spec is silent on both.
**Change:** Raise in the spec. Likely a warning: "`\paperref{4KX7}` names a statement, not a paper's result; use `\ref`". Adjust the `ref-dangling` wording for `\paperref`.
**Tests:** covered for tags, aliases, labels and dangling refs. Missing: `\paperref` to a statement, and to an unknown tag.

### `BodyChecker::body` (Source/LoomCore/Validation.cpp:152-175)
**Verdict:** PASS
**Spec:** `ref-not-listed` (proof refs to its own statement and to kinds without status don't count), `cite-alias`
**Tests:** covered (EveryProblem RN1S; body-references test).

### `knownTopLevelNames` (Source/LoomCore/Validation.cpp:182-195)
**Verdict:** PASS
**Spec:** `unknown-top-level` ("names later milestones use … are skipped"); `Specs/loom.md`, The project folder
**Finding:** The list matches the design's tree, plus `.git` and `.gitignore`.
**Tests:** covered (Junk.txt; Threads/ skipped in EveryProblem).

### `idOfPath` (Source/LoomCore/Validation.cpp:202-216)
**Verdict:** PASS
**Tests:** covered (DE1T; staged deletion).

### `isChecksPath` (Source/LoomCore/Validation.cpp:219-223)
**Verdict:** PASS
**Tests:** covered.

### `GitEntry` (Source/LoomCore/Validation.cpp:228-232)
**Verdict:** PASS
**Tests:** n/a.

### `parseListing` (Source/LoomCore/Validation.cpp:237-250)
**Verdict:** PASS
**Finding:** Both formats parse correctly with `-z`. Conflict stages are skipped, as documented.
**Tests:** covered indirectly (staged and HEAD tests).

### `readObjects` (Source/LoomCore/Validation.cpp:256-283)
**Verdict:** PASS
**Finding:** It parses the `<sha> <type> <size>\n<content>\n` framing. A `missing` header fails cleanly.
**Tests:** covered indirectly.

### `contentNeeded` (Source/LoomCore/Validation.cpp:287-290)
**Verdict:** PASS
**Tests:** covered indirectly.

### `cantWork` (Source/LoomCore/Validation.cpp:293-300)
**Verdict:** PASS
**Tests:** covered.

### `whyGitFailed` (Source/LoomCore/Validation.cpp:303-309)
**Verdict:** PASS
**Tests:** covered indirectly (not-a-repository).

### `copyIndex` (Source/LoomCore/Validation.cpp:313-348)
**Verdict:** PASS
**Spec:** Validation ("with `--staged`, the state in git's index, read with `git ls-files --stage` and `git cat-file`")
**Tests:** covered (staged edit, staged deletion, working-folder-only file ignored, not a repository).

### `readCommit` (Source/LoomCore/Validation.cpp:354-384)
**Verdict:** NEEDS-CHANGES
**Severity:** medium
**Spec:** Checks rule 1 ("Validation compares the file with its version in `HEAD`"); `deleted-since-commit`
**Finding:** `if (!head.ok()) return std::nullopt;` treats *every* failure of `git rev-parse --verify --quiet HEAD^{commit}` as "no commit yet, or not a repository". Exit code 1 (no HEAD yet) is the only failure that should mean that. Any fatal error (exit 128) also lands here, and then `check-changed` and `deleted-since-commit` are skipped without a word: `validate` reports a clean project while the append-only rule went unchecked. Unverified which real situations hit this. Candidates are a repository git refuses to use ("dubious ownership", a frequent Windows case) and a damaged HEAD. With `--staged`, `ls-files` fails first and gives `not-a-repository`, so only plain `loom validate` is affected.
**Change:** Tell the cases apart. First run `git rev-parse --is-inside-work-tree`: if it fails, there's no repository and nothing to compare with. Otherwise, treat rev-parse exit 1 with empty stderr as "no commit yet", and anything else as `git-failed` (exit 3).
**Tests:** "no repository" is covered (ValidationTests:200-202). Missing: no commit yet, and a git failure that isn't "no repository".

### `checkDisk` (Source/LoomCore/Validation.h:90-95, .cpp:491-518)
**Verdict:** PASS
**Spec:** `unknown-top-level`, `check-record-not-found` (warnings; a missing record is a warning until M4)
**Tests:** covered (EveryProblem; staged vs working for `unknown-top-level`).

### `CommittedFiles` (Source/LoomCore/Validation.h:98-103)
**Verdict:** PASS
**Tests:** covered.

### `checkAgainstCommit` (Source/LoomCore/Validation.h:105-112, .cpp:520-570)
**Verdict:** FAIL
**Severity:** low
**Spec:** Checks rule 1 ("No entry is ever edited, removed or **reordered**"), rule 2 ("Ties are allowed; then the later entry in the file is the newer"); Merging two check lists (stable sort, ours first)
**Finding:** `check-changed` matches committed entries against current ones as a multiset (`used[j]`), so it accepts any reordering. `check-order` only catches entries whose dates are out of order. Swapping two committed entries **with the same date** therefore passes both, and because ties are broken by file position, the swap changes which check counts. Concrete input: committed `Proofs/X/X.a.checks.toml` = [A:pass 10:00:00, D:pass 10:05:00, D:needs-changes 10:05:00 (with `changes`)]. The counting Dakota check is needs-changes, so X.a is unchecked. Swap the last two by hand: there's no `check-order` error (equal dates), no `check-duplicate`, and no `check-changed` (every entry is still there). The counting Dakota check is now the pass, and X.a becomes **checked** without a new click from Dakota. Same-second ties are rare, hence low severity, but the hole lets a file edit claim "checked". A correct merge (stable, ours first) always keeps committed entries in their relative order, so checking order is safe.
**Change:** After matching, require the matched indices in the current file to increase with the committed order, i.e. the committed list is a subsequence of the current one. Report the first entry out of place as `check-changed` (or a new wording "was moved"). Also take the line from the matched or nearest current entry rather than `now->reading.lines[i]`, which indexes the *current* file by the *committed* position.
**Tests:** the test "committed checks may move, but not change or go" covers interleaving, removal and editing, but not a reorder of tied entries. Add one.

## Tests

### ProjectContentsTests.cpp (Tests/LoomCoreTests/ProjectContentsTests.cpp:1-157)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** Good coverage of statement and proof cross-file problems with exact messages and lines. Paper and stub cross-file problems are covered only through EveryProblem (ValidationTests).
**Change:** Add these missing cases: a stub that reuses a statement's tag (`id-duplicate` on the stub); an alias shared by a statement and a stub; `superseded_by` naming a stub; a premise that is a definition *stub*; a sub-folder inside `Proofs/<tag>/` and a hidden file in `Statements/` (`stray-file`); `tagsInUse` with aliases and a checks-only folder; `resolveStatement` with `O`/`l` and with a stub alias; a `paper.toml` refused as `bad-path` (one problem, not two).
**Tests:** n/a.

### StatusTests.cpp (Tests/LoomCoreTests/StatusTests.cpp:1-467)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** It covers every worked-example status, the counts, the deciding proofs, `rests on`, cycles, all six spec sentences, the levels ranking, rules 1 and 4, recomputing, and refute. The verdict table is in ProofChecksTests or ChecksTests (statements-proofs' slice).
**Change:** Add these missing cases: `z` before `aa` in the deciding tie-break; `refuted_on` without `refuted_by`; a premise that doesn't exist, and a definition premise (their `premiseState` sentences); the explanation when a fingerprint can't be computed (see `lacks`); an empty project (no statements, all counts 0); a proof filed under a stub or a definition doesn't change any status; "Dakota's pass only, no agent check".
**Tests:** n/a.

### ValidationTests.cpp (Tests/LoomCoreTests/ValidationTests.cpp:1-255)
**Verdict:** NEEDS-CHANGES
**Severity:** low
**Finding:** It covers the worked example's exact list, every spec code (through EveryProblem), format-too-new, staged vs working, not-a-repository, append-only with interleaving, and body-reference parsing.
**Change:** Add these missing cases: two committed same-date checks swapped (should be an error; fails today, see `checkAgainstCommit`); `--staged` with no commit yet; a git failure in `readCommit` that isn't "no repository"; `\paperref` to a statement and to an unknown tag; `ref-alias` in a verbose proof body.
**Tests:** n/a.

## Cross-boundary notes
- `ListAgents` wasn't available and peers couldn't be reached by name, so messages went through the coordinator ("main"), who forwarded them. Messages sent (one relay message):
  - to **review-papers**: `ContentsChecker::papers` (ProjectContents.cpp:294-309) relies on `keyFitsBase()`. An unlettered `SmithJones24` beside `SmithJones24a` with the same base passes without a warning, and `baseKey`'s "no authors or editors" problem surfaces as `paper-key-rule` with wording the spec doesn't list. I asked them to check `keyFitsBase`/`baseKey` against key rule step 4 and the spec's messages.
  - to **review-statements-proofs**: `proofChecking()` gives an empty fingerprint when a premise's front matter doesn't parse. My `lacks()` then explains this as "it changed after its last agent check". I asked whether `ProofChecking` could carry `proofFingerprint`'s `problem` string so status can say why.
- Received (relayed by the coordinator):
  - from **review-statements-proofs**: the readers keep empty strings without a problem. `refuted_by = ""` with `refuted_on` set gives no `refuted-incomplete`; `refuted-unknown` skips the empty value (ProjectContents.cpp:210); Status.cpp:94 doesn't count the statement as refuted, so validate is clean. Empty `superseded_by`/`kind` pass too. Also: `tagsInUse` ignores checks files. **Outcome:** I confirmed the empty-value case against my code (StatementsAndProofs.cpp:161-163 checks only that the keys are present; my line 210 skips empty values; isRefuted needs both). I logged it under `ContentsChecker::statements`, with the fix in the readers. An empty `kind` gives status None in `presetStatus` (not provable), which is safe, and the readers should report it as `bad-value`. The `tagsInUse` point agrees with my own entry, which was already logged.
  - from **review-foundations**: `problemLine` (Problem.cpp:8-13) chains `.arg()`, so a file name from a directory listing that contains `%1`–`%9` prints garbled. Example: `Smith%2C%20Jones.pdf` prints as `Smith1C%20Jones.pdf`. **Outcome:** noted. It affects my `unknown-top-level` (`checkDisk`) and `stray-file` (`loadContents`/`loadPapers`) problems, whose `file` comes straight from the disk. My own messages use single `.arg(a, b)` calls or chain only values Loom controls, except `stray()` (ProjectContents.cpp:450). That builds `.arg(path, folder, holds)` in one call, so it's safe. `checkDisk`'s message uses one `.arg(name)`, also safe. The fix is foundations' (one multi-argument `.arg`).
  - from **review-papers** (and **review-cli-build**): (a) `paper-key-rule` reuses baseKey's failure text, and the no-names case repeats `authors-missing`; (b) `paper-key-duplicate` compares case-sensitively (`MacDonald20` vs `Macdonald20`). **Outcome:** I agree with both; they are logged under `ContentsChecker::papers` as (b) and (c). Unverified on Windows.
  - Replied to the coordinator: my two notes above went in the same relay message, together with an acknowledgement of the empty-string note.
- What I read of others' code to judge mine: `Cycles.cpp` (`findCycles` skips self-edges and gives one cycle per strongly connected component, starting at the smallest ID and closed with its start; my callers rely on both), `Ids.h` (`aliasProblem` rejects anything `normaliseTag` reads as a tag, so `resolveStatement`'s order is safe), `Checks.h` (`CheckStanding.supports` follows rule 7, either counting check), `ProofChecks.h`, `PaperKeys.h`, `StatementsAndProofs.h`, `Papers.h`, `Problem.h`, `ProjectFiles.h`.

## Spec issues
1. **Deciding proof of a proved statement** (How status is computed, Tie-breaks). Read literally, the tie-break picks J3RW.a for J3RW, whose premise H8KD is proved only through J3RW.b: a circular explanation. The code uses "the proof that brought it into the proved set" (J3RW.b) and the build log says why, but the spec wasn't amended. Ask Dakota to adopt the code's rule in the spec.
2. **"What a proof gives" names only the agent's fail.** It says "nothing, if `level = "failed"` or P's counting **agent** check is `fail`", while Checks rule 7 and the verdict table (`A:pass now, D:fail now`: supports no) say *either* counting check. The code follows rule 7. The status paragraph should say "either counting check".
3. **`superseded_by` naming a paper stub.** The field is "The tag of another statement", but `superseded-invalid` has no wording for a stub target, and the code accepts one silently.
4. **`\paperref` to a statement**, which isn't a stub, has no problem code. `Specs/loom.md` says `\paperref{<stub-id>}`.
5. **Tags on stubs.** `loom.toml`'s section says "Every tag used on a statement, stub or paper must be declared", but the stub field table has no `tags` field (and `Stub` has none). One of the two should change.
6. **Quoting Dakota's `changes`.** The B7MP example ends with "…". The code quotes the whole text (whitespace collapsed). The spec should say whether long `changes` are truncated.
7. **Reusing deleted tags.** "Tags are never … reused", but generation avoids only tags "already used in the project", so a tag deleted since the last commit can be drawn again. Should generation also avoid tags in `HEAD`?
8. **The `paper-key-rule` wording when no key can be made** (no authors or editors) isn't in the spec. The code reuses the key-rule problem text under that code.
9. **The append-only rule says "reordered", but validation is described as "every committed entry must still be there, unchanged"** (Checks rule 1 and the merge edge case). The validation sentence should add "in the same order", which a correct merge always keeps.
