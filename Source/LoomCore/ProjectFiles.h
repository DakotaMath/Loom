#pragma once

// Every path LoomCore reads or writes goes through this file, so the rule of Specs/m1-core.md,
// "Path safety", is enforced in one place.

#include <QByteArray>
#include <QString>

#include <optional>

namespace loom {

/// Checks the form of a path as it is stored in a project file (Specs/m1-core.md, "Path safety":
/// "Paths stored in files are project-relative with `/`").
/// Rule: non-empty; segments separated by `/`, none of them empty, `.` or `..`; no `\` and no `:`
/// (so no absolute path, no drive letter and no Windows alternate data stream). Refusing every
/// `..`, not only one that leaves the project, matches the `bad-path` message ("written with `/`,
/// without `..`").
/// @param relative The path to check, e.g. `TeX/preamble.tex`.
/// @returns True if the path has the right form. Doesn't look at the disk; no side effects.
bool isProjectPath(const QString& relative);

/// Resolves a project-relative path against the project folder and checks it stays inside
/// (Specs/m1-core.md, "Path safety": "no symlink or junction that resolves outside it (checked on
/// the canonical path)").
/// Rule: `relative` passes isProjectPath(); then the deepest part of the path that exists on disk
/// (the file itself, or the nearest folder above it, for a file not written yet) has a canonical
/// path equal to, or under, the canonical project folder. A link whose target doesn't exist is
/// refused, since writing through it could land anywhere.
/// @param root     The project folder (absolute, or relative to the current folder). Must exist.
/// @param relative The project-relative path, with `/`.
/// @returns The absolute path to use (not canonicalised, so a link inside the project still
///          reads as itself), or nothing if the path is refused. No side effects.
std::optional<QString> resolveProjectPath(const QString& root, const QString& relative);

/// Reads a whole project file.
/// @param root     The project folder.
/// @param relative The file's project-relative path, checked by resolveProjectPath().
/// @returns The bytes, or nothing if the path is refused or the file can't be read. No side effects.
std::optional<QByteArray> readProjectFile(const QString& root, const QString& relative);

/// Writes a whole project file atomically (Specs/m1-core.md, "Front matter": "Writes are atomic:
/// through `QSaveFile`"), creating the folders above it if they're missing.
/// @param root     The project folder.
/// @param relative The file's project-relative path, checked by resolveProjectPath().
/// @param bytes    The file's new content, written as is.
/// @returns An empty string on success, else a plain-English reason for the failure.
/// Side effects: creates folders under `root`; replaces the file, or leaves it untouched on failure.
QString writeProjectFile(const QString& root, const QString& relative, const QByteArray& bytes);

} // namespace loom
