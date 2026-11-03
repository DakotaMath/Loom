---
type: log
tags: [m1, build, new-project, paths]
---

# 2026-11-03: `loom new-project`, opening a project, path safety

> **Summary:** Built #4: `LoomCore` creates a project folder from the compiled-in project kit with its first commit, opens a project by reading `loom.toml` with every problem the spec lists for it, and checks every path it reads or writes stays inside the project; `loom new-project` exposes it. All 34 `ctest` tests pass on Windows.

**Session model:** Opus 5.5
**Issues and PRs:** #4, PR #14

## Done
- `LoomCore`: `ProjectFiles` (path safety, atomic writes), `ProjectToml` (`loom.toml`'s fields, its problems, Loom's own writer, `openProject`), `TomlWriter` (basic and multi-line strings, dates, arrays), `Git` (runs git with `QProcess`), `NewProject` (`createProject`), `Problem`.
- The project kit in `Templates/project/` (`CLAUDE.md`, `TeX/preamble.tex`, `TeX/macros.tex`, `gitignore`, `gitattributes`), compiled into `LoomCore` as a Qt resource.
- `loom new-project <folder> --name <name> [--idea <text>] [--json]`, with exit codes 0–3, the JSON error shape, arguments read as Unicode and UTF-8 output on the Windows console.
- Tests: Catch2 for path safety, `loom.toml` (every problem code with its line and message, the spec's example read and written byte for byte), and creation (exact files and contents, one commit, branch `main`, LFS set up, each refusal and stop, and a failing git step); a stand-in git (`loom-fake-git`) for the failures a real git can't show on demand; a CMake script that checks the `loom` program's exit codes and output.
- Wiki: [gotcha: `canonicalFilePath()` ignores junctions](../../gotchas/qt/canonical-path-ignores-junctions.md).

## Learned
- Qt's `QFileInfo::canonicalFilePath()` doesn't resolve NTFS junctions on Windows, so path safety uses `std::filesystem::canonical()` ([gotcha](../../gotchas/qt/canonical-path-ignores-junctions.md)). The link test caught it on its first run on Windows.
- To ask for the identity a new repository will see, run `git config --get user.name` with `GIT_DIR` pointing at the new folder's (not yet existing) `.git`: git then reads only system and global config, never that of a repository the folder sits in. A test covers a folder inside a repo with its own identity.
- Building on this machine without the Qt Online Installer: `aqt install-qt windows desktop 6.11.1 win64_msvc2022_64 --archives qtbase` (aqtinstall from the commit CI pins) is enough for `LoomCore`, `loom` and the tests, and takes about 15 seconds.
- Catch2 prints `{?}` for a `QString` unless it has a `StringMaker`; `Tests/LoomCoreTests/TestSupport.h` has one.

## Next
- Dakota reviews PR #14 and does its Windows check.
- Then #5 (statements, proofs, IDs). Its front matter writer can reuse `TomlWriter`, and its problems `Problem`.

## Waiting on Dakota
- PR #14: review, the Windows check, and the spec points it raises (where the project kit is compiled in, the order of folder creation and git checks, error codes for `new-project`, `CLAUDE.md` in the kit).
