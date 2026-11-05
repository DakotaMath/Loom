#pragma once

// *Check my setup* (Specs/m2-statements-review.md, "Settings"): finds and runs each tool Loom
// needs and reports its version, or what to install. LoomCore does the checking, by running
// programs only; the app shows it.

#include <QProcessEnvironment>
#include <QString>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace loom {

/// What to check with. Each program is a name looked up on the environment's PATH, or a path
/// (the app's Settings: "A full path overrides PATH, for unusual installs").
struct SetupRequest {
    QString git = QStringLiteral("git");
    QString latexmk = QStringLiteral("latexmk");
    QString synctex = QStringLiteral("synctex");
    /// Always looked up on PATH in the app: `latexmk` finds pdfLaTeX there too.
    QString pdflatex = QStringLiteral("pdflatex");
    /// The commit guard runs `loom` from PATH, so the app leaves this as the name.
    QString loom = QStringLiteral("loom");
    /// The environment the tools run in, and whose PATH they are found on.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    /// How long one version command may run before it's stopped.
    int toolTimeoutMs = 30'000;
    /// How long the test build may run: the app's compile timeout (120 s by default).
    int compileTimeoutMs = 120'000;
    /// Set from another thread to stop early (the window closed); may be null. The items not yet
    /// checked are then left out.
    std::shared_ptr<std::atomic_bool> stop;
};

/// How one line of the check came out.
enum class SetupState {
    Found,   ///< A tick: the version and path.
    Missing, ///< A cross: what to do.
    Warning, ///< Not needed for most things (`synctex`): what is lost without it.
    Skipped, ///< Not checked, because a tool it needs is missing.
};

/// One line of the check.
struct SetupItem {
    /// A stable key: `git`, `git-identity`, `git-lfs`, `loom`, `latexmk`, `pdflatex`, `synctex`,
    /// `test-build`.
    QString key;
    /// What is checked, as the line names it: `git`, `git identity`, `Git LFS`, `loom on PATH`,
    /// `latexmk`, `pdfLaTeX`, `synctex`, `A test build`.
    QString name;
    SetupState state = SetupState::Missing;
    /// For Found: the version the tool printed (its first line, trimmed), or for the identity
    /// `Name <email>`, or for the test build how long it took.
    QString version;
    /// For Found and some failures: the program's absolute path, with `/`.
    QString path;
    /// For Missing, Warning and Skipped: what to do, in plain English.
    QString message;
};

/// Runs the checks in the spec's order: git (2.40 or newer), git's identity, Git LFS, `loom` on
/// PATH, `latexmk -v`, `pdflatex --version`, `synctex help`, and a test build of a two-line
/// document in a temporary folder within the compile timeout.
/// Rules: a program is found by its path, or by PATH (with Windows' executable suffixes); a found
/// program counts if it runs and exits 0, except `synctex`, which counts if it runs at all. The
/// identity is read as a new repository would see it (the global and system config only), as
/// `loom new-project` does. git's identity and Git LFS are Skipped without git; the test build
/// is Skipped without `latexmk` or pdfLaTeX.
/// @param request What to check with.
/// @param onItem  Called with each item as soon as it's checked, on the calling thread; may be empty.
/// @returns Every item checked, in order. Side effects: runs the programs; writes and removes a
///          temporary folder for the test build.
std::vector<SetupItem> checkSetup(const SetupRequest& request, const std::function<void(const SetupItem&)>& onItem = {});

/// Finds a program as QProcess would run it from `environment`.
/// @param program A name to look up on the environment's PATH, or a path (absolute, or with a
///                folder in it); on Windows `.exe` may be left off either.
/// @returns Its absolute path with `/`, or an empty string if it isn't there. Reads the disk only.
QString findProgram(const QString& program, const QProcessEnvironment& environment);

} // namespace loom
