---
type: gotcha
tags: [ci, qt, aqtinstall, install-qt-action, windows]
---

# aqtinstall 3.3.0 can't install Qt 6.11 on Windows

> **Summary:** `jurplel/install-qt-action@v4` uses aqtinstall 3.3.0 by default, which fails on every Qt 6.11 release for Windows because Qt changed its repository layout. Install aqtinstall from a pinned commit of its main branch with the action's `aqtsource` input until a fixed release is out.

**Last verified:** 2026-11-03 · commit 7580298

## Symptom
Installing Qt 6.11.x for Windows (`win64_msvc2022_64`) fails before any download:

```
WARNING : Failed to download checksum for the file 'online/qtsdkrepository/windows_x86/desktop/qt6_6111/qt6_6111/Updates.xml'. This may happen on unofficial mirrors.
ERROR   : Failed to locate XML data for Qt version '6.11.1'.
```

`aqt list-qt windows desktop --arch 6.11.1` fails the same way, while 6.10.3 for Windows and 6.11.1 for Linux work.

## Cause
From Qt 6.11, Qt's online repository for Windows has one folder per compiler (`qt6_6111/qt6_6111_msvc2022_64/`) instead of one folder for the version (`qt6_6111/qt6_6111/`). aqtinstall 3.3.0, the latest release on PyPI on 2026-11-03, still looks in the old place. The fix (aqtinstall PR #1000, issue #959) is merged on its main branch but not released; install-qt-action tracks the problem in its issue #374.

## Fix
Pass a pinned commit of aqtinstall to the action, as `.github/workflows/checks.yml` does:

```yaml
- uses: jurplel/install-qt-action@v4
  with:
    version: 6.11.1
    arch: win64_msvc2022_64
    aqtsource: git+https://github.com/miurahr/aqtinstall.git@076e1659807d0b362a3ed684d54c2e9c775eb9c7
```

`076e1659` is the commit install-qt-action's maintainers recommend in their issue #374, and the only later commit on aqtinstall's main branch changes its own CI. When aqtinstall releases a version after 3.3.0, drop `aqtsource` (or set `aqtversion`) and check that Windows CI still installs Qt.

Dakota's machine is not affected: the Qt Online Installer reads the new layout.

## See also
- Up: [Gotchas](../README.md)
- [001: Qt 6](../../decisions/001-qt-6.md)
