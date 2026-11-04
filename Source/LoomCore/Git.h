#pragma once

// Running git. Git and Git LFS are separate programs, run with QProcess and never linked
// (Specs/m1-core.md, "Toolchain").

#include <QByteArray>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace loom {

/// Which git to run, and with what environment. Tests point `program` at a stand-in and give
/// `environment` its own git identity.
struct GitTool {
    /// The git program: a name looked up on PATH, or a path.
    QString program = QStringLiteral("git");
    /// The environment git runs in.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
};

/// What one run of git did.
struct GitRun {
    /// False if git couldn't be started at all (not installed, or not on PATH), crashed or timed out.
    bool started = false;
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
/// @param timeoutMs  How long to wait before giving up (and killing git).
/// @param input      What to write to git's stdin before closing it (`git cat-file --batch`
///                   reads object names there); empty for nothing.
/// @returns What git did. Side effects: whatever the git command does.
GitRun runGit(const GitTool& git, const QStringList& arguments, const QString& workingDir, int timeoutMs = 120'000,
              const QByteArray& input = {});

} // namespace loom
