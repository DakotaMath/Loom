---
type: log
tags: [m1, fix, checks, fingerprints]
---

# 2026-11-04: The proof's level is in its fingerprint

> **Summary:** Did Dakota's decision from the M1 final review: a proof's `level` is now in its fingerprint (`loom-fingerprint-v2`, a `level` section after the header), so changing the level of a checked proof outdates its checks and `edit-proof --level` needs `--outdate-checks`, as a body change does. All 270 `ctest` tests pass on Windows.

**Session model:** Opus 5.5
**Issues and PRs:** no issue (Dakota's decision in the M1 final review); #29

## Done
- Spec (`Specs/m1-core.md`, marked A18): the fingerprint recipe gains `level\n<length>\n<level>\n` after a `loom-fingerprint-v2\n` header; `level` leaves "What isn't in it", with the reason; D3, the Proofs `level` row and the `edit-proof` row say a level change of a checked proof outdates its checks, and that `failed` on a checked proof needs both of Dakota's clicks.
- `LoomCore`: `FingerprintInput::level`, written by `fingerprintBytes()`; `proofFingerprint()` passes the proof's level. `editProof()` needed no new logic: its outdated-checks rule already compares fingerprints. The `outdates-checks` message now says "what its checks read would change" instead of "the text", since the level isn't text.
- Tests: the known byte string and hashes (computed again with Python's hashlib, which also reproduced the old v1 hash from the old bytes), a level change alone changes the fingerprint, a checked sketch raised to `complete` is no longer checked (status unchecked, not proved), `edit-proof --level` on a checked proof is refused without the confirmation (Catch2 and the CLI script), and `failed` on a checked proof asks for both clicks. The new tests failed on `main` first.
- Fixtures: every check (and verbose proof) whose fingerprint matched its proof under v1 now carries the v2 fingerprint, computed by `loom fingerprint`; deliberately stale ones (AH3K.a, CK1A.a, CK2A.a, K3MA's verbose versions) were left as they were. The worked example's statuses and sentences are unchanged.

## Next
- Dakota reviews the PR; the amendments table's A18 row comes from the parallel fix-final PR.

## Learned
- The shared Qt folder on this machine was pruned today (`syncqt.exe` and other tools gone), so a fresh `cmake --preset windows` fails at `find_package(Qt6)`. A private copy of Qt (`aqt install-qt … --archives qtbase`), or one whose CMake files skip the missing tools, gets past it. This is about one machine, not the repo.
