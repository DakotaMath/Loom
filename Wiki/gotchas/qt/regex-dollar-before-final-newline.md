---
type: gotcha
tags: [qt, regex, validation]
---

# QRegularExpression's `$` also matches before a final line break

> **Summary:** In Qt 6, `QRegularExpression` (PCRE2) lets `$` match at the very end *or* just before a final `\n`, so `^[a-z0-9-]+$` accepts `"energy\n"`. A rule meant to check a whole value must be anchored with `QRegularExpression::anchoredPattern()` (which wraps the pattern in `\A(?:…)\z`), never with `^…$`.

**Last verified:** 2026-11-04 · commit 0ae695e

## Symptom
A value with a trailing line break passes a check written as `^…$`. In TOML a basic string can hold one (`by = "agent:claude\n"`), so on `main` at 0ae695e each of these was valid:

| Value | Rule |
| --- | --- |
| `by = "agent:claude\n"` in a check history, `extracted_by` in a stub | `isCheckAuthor` (Checks.cpp) |
| `id = "energy\n"` for a tag in `loom.toml` | `tag-syntax` (ProjectToml.cpp) |
| `origin = "new\n"` | the origin rule (StatementsAndProofs.cpp) |
| `fingerprint = "3c8e1f0a92d4\n"` in a verbose proof | the fingerprint rule; such a file was then never reported stale |
| `id = "arxiv-v1\n"`, `id = "ed2\n"`, `session = "Sessions/2026-10-18\n"` | the version-id and session rules (Papers.cpp) |

## Cause
PCRE2, like Perl, defines `$` as "end of subject, or before a newline at the end", unless the pattern is compiled with `PCRE2_DOLLAR_ENDONLY`. Qt doesn't set it, and `QRegularExpression` has no option for it. `^` has no such twist.

## Fix
Write the pattern without anchors and anchor it whole:
```cpp
static const QRegularExpression tagSyntax(
    QRegularExpression::anchoredPattern(QStringLiteral("[a-z0-9]+(-[a-z0-9]+)*")));
```
`anchoredPattern()` puts the pattern in a non-capturing group, so alternatives (`new|spec:.+`) stay anchored too, and capture numbers don't change. Writing `\z` at the end by hand works as well. When you add a rule, add a test with a trailing `\n`.

## Dead ends
- **`QRegularExpression::MultilineOption` off:** it already is by default; the final-newline match happens without it.
- **`match(…, QRegularExpression::AnchorAtOffsetMatchOption)`:** anchors only the start.

## See also
- Up: [Gotchas](../README.md)
- [`Specs/m1-core.md`](../../../Specs/m1-core.md): *Validation*
