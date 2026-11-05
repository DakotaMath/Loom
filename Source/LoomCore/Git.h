#pragma once

// Running git. Git and Git LFS are separate programs, run with QProcess and never linked
// (Specs/m1-core.md, "Toolchain").

#include <QByteArray>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace loom {

/// Which git to run, and with what environment. Tests point `program` at a stand-in and give
/// `environment` its own git identity.
struct GitTool {
    /// The git program: a name looked up on PATH, or a path.
    QString program = QStringLiteral("git");
    /// The environment git runs in.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    /// How long to wait for one git command before stopping it, unless runGit() is given a time
    /// of its own. Tests make it short.
    int timeoutMs = 120'000;
};

/// What one run of git did.
struct GitRun {
    /// False if git couldn't be started at all (not installed, or not on PATH), crashed or timed out.
    bool started = false;
    /// True if git started but didn't finish in time and Loom stopped it (`started` is then
    /// false). It tells a stuck git apart from a missing one: see gitDidntRunMessage().
    bool timedOut = false;
    int exitCode = -1;
    QString output; ///< stdout, as UTF-8.
    QByteArray outputBytes; ///< stdout as git wrote it, for file contents (`git cat-file`).
    QString errors; ///< stderr, as UTF-8.

    /// Whether git started and exited with code 0.
    bool ok() const { return started && exitCode == 0; }
};

/// Runs git once and waits for it to finish.
/// @param git        Which git, and its environment.
/// @param arguments  The arguments, e.g. `{"init", "-b", "main"}`.
/// @param workingDir The folder git runs in. Must exist.
/// @param timeoutMs  How long to wait before giving up (and killing git); `git.timeoutMs` if not given.
/// @param input      What to write to git's stdin before closing it (`git cat-file --batch`
///                   reads object names there); empty for nothing.
/// @returns What git did. Side effects: whatever the git command does.
GitRun runGit(const GitTool& git, const QStringList& arguments, const QString& workingDir,
              std::optional<int> timeoutMs = std::nullopt, const QByteArray& input = {});

/// A git command as a person would type it, to name it in a message: `git` and the arguments,
/// separated by spaces; an argument that is empty or holds whitespace or a `"` is put in double
/// quotes, with `\"` for a `"` (`git commit --message "Create project Demo"`).
/// @returns The command. No side effects.
QString gitCommand(const QStringList& arguments);

/// What to tell the user when git didn't run or didn't finish (`GitRun::started` is false).
/// For a git that timed out, naming the whole command (gitCommand()), as Specs/m1-core.md, "The
/// `loom` command", A16, words it: *"`{git command}` didn't finish in time, so Loom stopped it.
/// Git may be waiting for something, such as another git's lock or a slow network drive; try
/// again."* Otherwise the spec's message for a missing git (Specs/m1-core.md, "The project
/// folder", step 2): *"Loom needs git, and `git` didn't run. Install Git for Windows and try again."*
/// @param run       The run that failed.
/// @param arguments Its arguments, to name the command.
/// @returns The message. No side effects.
QString gitDidntRunMessage(const GitRun& run, const QStringList& arguments);

/// The error code for a git that didn't run or finish: `git-timeout` if it timed out, else
/// `missingCode` (callers name a missing git differently: `git-missing`, `git-failed`).
/// @returns The code. No side effects.
QString gitDidntRunCode(const GitRun& run, const QString& missingCode);

/// One object of a listing by `git ls-files --stage -z` or `git ls-tree -r -z`.
struct GitListingEntry {
    QString mode;   ///< `100644`, `100755`, `120000` (a link), `160000` (a submodule).
    QByteArray sha; ///< The object's name.
    QString path;   ///< Relative to the folder git ran in, with `/`.
};

/// Reads `git ls-files --stage -z` (`<mode> <object> <stage>\t<path>\0`) or `git ls-tree -r -z`
/// (`<mode> <type> <object>\t<path>\0`). Index entries in a merge conflict (stage 1–3) are left
/// out; git refuses to commit while there are any.
/// @param output The listing, as git wrote it.
/// @param index  True for `ls-files --stage`, false for `ls-tree`.
/// @returns The entries, in git's order. No side effects.
std::vector<GitListingEntry> parseGitListing(const QByteArray& output, bool index);

/// The arguments readGitObjects() runs git with, to name the command in a message.
const QStringList& gitCatFileBatch();

/// Reads objects with `git cat-file --batch`.
/// @param git  Which git.
/// @param root The folder git runs in.
/// @param shas The objects' names.
/// @param run  Set to what git did, so a caller can tell a git that didn't finish in time
///             (`git-timeout`, M1's A16) from one that failed; untouched if there was nothing to read.
/// @returns Their contents, in the same order, or nothing if git failed or any is missing.
/// Side effects: runs git in `root`.
std::optional<std::vector<QByteArray>> readGitObjects(const GitTool& git, const QString& root,
                                                      const std::vector<QByteArray>& shas, GitRun& run);

} // namespace loom
