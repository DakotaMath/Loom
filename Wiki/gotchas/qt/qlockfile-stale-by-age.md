---
type: gotcha
tags: [qt, qlockfile, locks, windows]
---

# QLockFile treats a lock older than 30 seconds as stale, even while its owner runs

> **Summary:** By default a `QLockFile` held longer than its stale time (30 s) can be removed by another process that tries to take it, even though the holder is still running. For a lock held for a window's whole life (`.loom/app.lock`) or a long build, call `setStaleLockTime(0)`, so only a dead owner makes it stale. In tests that fake a crashed owner's lock file, leave the host line empty: on Windows QLockFile spells the machine name in capitals, not as `QSysInfo::machineHostName()` does.

**Last verified:** 2026-11-05 · commit d81778a

## Symptom
- A second Loom window opens a project that another window still has open, once the first has had it for more than 30 seconds (on Linux; on Windows the held file can't be deleted, so it happens less).
- A test that writes a lock file for a process that has ended expects `tryLock()` to take it over, and it doesn't.

## Cause
- `QLockFile::isApparentlyStale()` returns true if the owner process isn't running, **or** if the file's age is over `staleLockTime()` (30 000 ms by default), whatever the owner is doing.
- The owner check applies only when the lock file's host line matches this machine (or is empty). On Windows QLockFile writes the name from `GetComputerName`, in capitals (`MYPC`), while `QSysInfo::machineHostName()` gives `MyPc`, so a test that writes the latter makes the lock look like another machine's, which is never stale by owner.

## Fix
- `lock.setStaleLockTime(0)` before `tryLock()`, as `LoomCore`'s `takeAppLock()` and the build's `build.lock` do.
- In a test, write the lock file as `<pid>\n<app name>\n\n`: an empty host line means this machine.

## See also
- Up: [Gotchas](../README.md)
- `Source/LoomCore/AppLock.cpp`, `Tests/LoomCoreTests/AppLockTests.cpp`
