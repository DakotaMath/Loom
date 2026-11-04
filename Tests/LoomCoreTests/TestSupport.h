#pragma once

// Helpers shared by LoomCore's tests: files in temporary folders, and git with an identity of
// the test's own (Specs/m1-core.md, "Toolchain": "Tests that make git repos set a git identity first").

#include "LoomCore/Git.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <catch2/catch_tostring.hpp>

/// Shows QString values in Catch2's failure messages, as UTF-8; without it Catch2 prints `{?}`.
template <>
struct Catch::StringMaker<QString> {
    /// @returns The value in double quotes, so empty strings and edge spaces show. No side effects.
    static std::string convert(const QString& value) { return '"' + value.toStdString() + '"'; }
};
/// Shows QStringList values in Catch2's failure messages, as UTF-8.
template <>
struct Catch::StringMaker<QStringList> {
    /// @returns The elements joined by `, ` in brackets. No side effects.
    static std::string convert(const QStringList& value) { return '[' + value.join(u", ").toStdString() + ']'; }
};

namespace loomtest {

/// Writes a file, creating the folders above it.
/// @param path  The file's absolute path.
/// @param bytes Its content, written as is.
/// Side effects: creates folders and the file; fails the current test if it can't.
void writeFile(const QString& path, const QByteArray& bytes);

/// Reads a whole file.
/// @param path The file's absolute path.
/// @returns Its bytes; fails the current test if it can't be read.
QByteArray readFile(const QString& path);

/// Lists the files under a folder, skipping `.git`.
/// @param folder The folder.
/// @returns Paths relative to `folder`, with `/`, sorted. No side effects.
QStringList filesUnder(const QString& folder);

/// A git that runs with the given global config file and no system config, so the developer's
/// own identity and settings can't leak into a test.
/// @param globalConfig The file to use as git's global config. Created holding a test identity
///                     when `withIdentity` is true, else created empty.
/// @param withIdentity Whether git should know a user name and email.
/// @returns The git to pass to LoomCore. Side effects: writes `globalConfig`.
loom::GitTool testGit(const QString& globalConfig, bool withIdentity = true);

/// Runs git in a folder and returns its stdout, trimmed.
/// @param git        From testGit().
/// @param arguments  git's arguments.
/// @param workingDir Where to run it.
/// @returns stdout, trimmed; fails the current test if git exits with an error.
QString gitOutput(const loom::GitTool& git, const QStringList& arguments, const QString& workingDir);

/// Makes `link` a directory link to `target`: a symbolic link, or on Windows, when symbolic links
/// need privileges the test doesn't have, a junction.
/// @returns False if neither could be made (the caller should SKIP). Side effects: creates `link`.
bool makeDirectoryLink(const QString& target, const QString& link);

} // namespace loomtest
