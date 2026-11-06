---
type: gotcha
tags: [windows, dates, git-bash, logs]
---

# `TZ=America/Chicago date` in Git Bash gives UTC's date

> **Summary:** On Windows, Git Bash's `date` doesn't know IANA names such as `America/Chicago`, so `TZ=America/Chicago date +%F` silently prints the UTC date: tomorrow's date every evening in Central time. On Windows, use plain `date` (the system clock is in Central time) or ask PowerShell.

**Last verified:** 2026-11-05 · commit 6b59d6e

## Symptom
At 18:41 Central on 2026-11-05, `TZ=America/Chicago date +%F` printed `2026-11-06`. Plain `date` printed `Thu Nov  5 18:41:25 CST 2026`. A session log or `STATUS.md` line dated with the first would carry the wrong date.

## Cause
CLAUDE.md's command is for cloud sessions, which run Linux with the time zone database. Git Bash's `date` has no database for IANA names; it treats an unknown `TZ` as UTC with no warning.

## Fix
- **Linux (cloud sessions, CI):** `TZ=America/Chicago date +%F`, as CLAUDE.md says.
- **Windows:** `date +%F` when the machine is set to Central time, or, to be sure:
  `powershell -NoProfile -Command "[TimeZoneInfo]::ConvertTimeBySystemTimeZoneId([DateTime]::UtcNow,'Central Standard Time').ToString('yyyy-MM-dd')"`

## See also
- Up: [Gotchas](../README.md)
- CLAUDE.md, *Formats*: "Dates are US Central time"
