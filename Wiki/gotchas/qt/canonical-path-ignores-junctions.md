---
type: gotcha
tags: [qt, windows, paths, junctions, path-safety]
---

# QFileInfo::canonicalFilePath() doesn't resolve junctions on Windows

> **Summary:** On Windows, Qt 6's `QFileInfo::canonicalFilePath()` leaves NTFS junctions unresolved, both the junction itself and any path through it, so a path check built on it lets `Link/secret.tex` escape the project. Use `std::filesystem::canonical()`, which asks Windows for the final path, as `LoomCore`'s `ProjectFiles.cpp` does.

**Last verified:** 2026-11-03 · commit a0e59db

## Symptom
With `Project/Link` a junction to a folder outside the project (`mklink /J`), Qt 6.11.1 on Windows gives:

| Path | `canonicalFilePath()` | `std::filesystem::canonical()` |
| --- | --- | --- |
| `Project/Link` | `…/Project/Link` | `…/Outside` |
| `Project/Link/secret.tex` | `…/Project/Link/secret.tex` | `…/Outside/secret.tex` |

So a check of "is the canonical path inside the project?" passes for a file that is really outside it. `LoomCoreTests`' *A link that leaves the project is refused* caught it.

## Cause
Qt 6 doesn't treat junctions as symbolic links: for one, `isSymbolicLink()` is false and `isJunction()` is true (seen on Qt 6.11.1), and canonicalising doesn't follow it. On Windows, creating a symbolic link needs Developer Mode or admin rights; a junction needs neither, so it's the link a test (or a user) can always make.

## Fix
Canonicalise with the standard library and convert back:
```cpp
std::error_code error;
const std::filesystem::path canonical =
    std::filesystem::canonical(QFileInfo(path).filesystemAbsoluteFilePath(), error);
const QString result = error ? QString() : QDir::cleanPath(QFileInfo(canonical).absoluteFilePath());
```
MSVC's `canonical()` resolves junctions and symbolic links in every part of the path and returns an ordinary `C:\…` path (no `\\?\` prefix). On Linux it is `realpath`. It fails for a path that doesn't exist, so canonicalise the deepest part that does.

## See also
- Up: [Gotchas](../README.md)
- [`Specs/m1-core.md`](../../../Specs/m1-core.md): *Path safety*
