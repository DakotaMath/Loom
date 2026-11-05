#pragma once

// Building a statement or proof to PDF (Specs/m2-statements-review.md, "Building LaTeX" and
// "Building another revision"): the build folder, running `latexmk`, the result and its problems.
// Shared by `loom build` and the app, so agents, tests and Dakota get the same build.

#include "LoomCore/BuildDocument.h"
#include "LoomCore/Git.h"
#include "LoomCore/Problem.h"

#include <QByteArray>
#include <QDateTime>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <atomic>
#include <memory>
#include <vector>

namespace loom {

/// What to build.
struct BuildRequest {
    QString root;              ///< The project folder.
    QString target;            ///< A statement's tag or alias, or a proof ID, as typed.
    BuildMode mode = BuildMode::Alone;
    bool verbose = false;
    /// Empty to build the working tree; else a commit as git reads it (`HEAD`), built from its
    /// tree without touching the working tree (Revision.h).
    QString rev;
    /// How long `latexmk` may run (Specs/m2-statements-review.md: 120 s by default).
    int timeoutMs = 120'000;
    /// The `latexmk` to run: a name looked up on PATH, or a path.
    QString latexmk = QStringLiteral("latexmk");
    /// The environment `latexmk` runs in; Loom adds its own variables to it.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    /// Which git, for `rev`.
    GitTool git;
    /// Set from another thread to stop the build early; may be null.
    std::shared_ptr<std::atomic_bool> stop;
    /// How long to wait for another build of the same folder to finish before giving up.
    int lockWaitMs = 120'000;
    /// Build the preview of the statement `target` (an exact tag), composePreview(), instead of a
    /// build of it; `mode`, `verbose` and `rev` are then ignored.
    bool preview = false;
};

/// How a build went.
enum class BuildOutcome {
    Succeeded, ///< No errors, and a PDF (`loom build` exit 0).
    Failed,    ///< Errors: LaTeX's, or `build-unreadable` (exit 1). `last-good.pdf` is kept.
    Refused,   ///< Nothing to build: `build-stub` or `ref-unknown` (exit 1).
    CantWork,  ///< `latexmk` couldn't run or finish, git failed, a file couldn't be written (exit 3).
    Stopped,   ///< BuildRequest::stop was set; nothing is reported.
};

/// What a build did.
struct BuildResult {
    BuildOutcome outcome = BuildOutcome::CantWork;
    /// For Refused and CantWork: a stable code (`build-stub`, `ref-unknown`, `tex-missing`,
    /// `tex-timeout`, `not-a-project`, `unreadable`, `format-too-new`, `no-commits`, `git-failed`,
    /// `git-missing`, `git-timeout`, `write-failed`, `locked`) and the message.
    QString code;
    QString message;

    QString target;            ///< The target's tag or proof ID, once resolved.
    BuildMode mode = BuildMode::Alone;
    bool verbose = false;
    QString rev;               ///< The commit's full hash; empty for the working tree.
    /// The build folder, project-relative with `/`: `.loom/build/4KX7/premises`.
    QString folder;
    /// `main.pdf`, project-relative with `/`, when this build made one (or reused one); else empty.
    QString pdf;
    /// `last-good.pdf`, project-relative with `/`, when there is one.
    QString lastGoodPdf;
    /// True if the inputs were unchanged since a build without errors, so `latexmk` didn't run.
    bool reused = false;
    /// How long the build took, in seconds.
    double seconds = 0;
    /// The problems: composing's and LaTeX's, errors first, each in file and line order, without
    /// duplicates.
    std::vector<Problem> problems;
    /// The command run, as a person would type it, and what it printed, and `main.log`: for the
    /// app's Output view.
    QString command;
    QByteArray output;
    QByteArray log;

    /// How many problems are errors. No side effects.
    int errors() const;
};

/// Builds a target (Specs/m2-statements-review.md, "Building LaTeX").
///
/// Rules:
/// - **The folder:** `.loom/build/<target>/<mode>[-verbose]/` for the working tree,
///   `.loom/rev/<hash>/build/<target>/<mode>[-verbose]/` for a commit. Two builds of one folder
///   take turns (`build.lock` in it); builds of different folders run at once. Nothing outside
///   `.loom/` is written, and no write lock is taken.
/// - **The files:** `main.tex`, `loom.sty`, the copies under `src/` (replaced whole on every run)
///   and `refs.bib` (composeBuild()).
/// - **Skipping work:** the inputs' hash is SHA-256 over `loom.sty`, every file written, and the
///   options. If it equals the one in `loom-build.toml`, that build had no errors, and its
///   `main.pdf` is there, the result is that build's (its log read again) and `latexmk` doesn't run.
/// - **Running:** `latexmk -norc -pdf -interaction=nonstopmode -file-line-error -synctex=1
///   main.tex` in the folder, with the variables that stop pdfLaTeX wrapping log lines
///   (`max_print_line.pdflatex=100000`, `error_line.pdflatex=254`, `half_error_line.pdflatex=238`;
///   scoped to pdfLaTeX because BibTeX refuses a `max_print_line` over its buffer size, "bad 3").
///   It is stopped with everything it started when it runs out of time (`tex-timeout`) or `stop`
///   is set; if it can't start, `tex-missing`.
/// - **The result:** errors in the log (or none but no PDF: `tex-no-pdf`) are Failed; otherwise
///   Succeeded, and `main.pdf` is copied to `last-good.pdf`. `loom-build.toml` records the target,
///   mode, verbose, commit, inputs' hash, time, result and seconds.
///
/// - **Previews** (`preview`; Specs/m2-statements-review.md, "IDs in the editor", "Previews") are
///   built in `.loom/build/preview/<tag>/` and kept as `.loom/build/preview/<tag>-<hash>.pdf`,
///   `<hash>` being the first 16 hex digits of the inputs' hash: if that file is there, it is the
///   result at once (`reused`), without taking the folder's lock. A preview without errors is
///   copied there (through a temporary name, so a reader never sees half a file), and the tag's
///   older previews are removed. `pdf` is that file; there is no `last-good.pdf`.
///
/// @returns The result. Side effects: writes under `.loom/`; runs `latexmk` (and, for `rev`, git).
BuildResult buildTarget(const BuildRequest& request);

/// The cached preview of a statement, project-relative: `.loom/build/preview/<tag>-<hash>.pdf`.
/// @param tag  The statement's tag.
/// @param hash The first 16 hex digits of the preview's inputs' hash. No side effects.
QString previewFile(const QString& tag, const QString& hash);

/// What a build left in its folder, for the app to show (Specs/m2-statements-review.md, "The PDF
/// pane").
struct BuildOutput {
    bool read = false;       ///< False if the folder doesn't exist or its lock wasn't free in time.
    QByteArray pdf;          ///< `main.pdf`, or empty.
    QByteArray lastGoodPdf;  ///< `last-good.pdf`, or empty.
    QDateTime lastGoodTime;  ///< When `last-good.pdf` was written.
    QByteArray aux;          ///< `main.aux`, or empty.
    /// The project files the build copied under `src/`, project-relative with `/`, sorted.
    QStringList inputs;
};

/// Reads a build folder's PDFs, `main.aux` and the list of its copies, holding the folder's
/// `build.lock` (as buildTarget() does), so another build of the same folder (an agent's `loom
/// build`) can't replace or remove them halfway through. Reading the bytes is what lets the app
/// show a PDF without holding the file open ("Where the PDF is read from").
/// @param root       The project folder.
/// @param folder     The build folder, project-relative (BuildResult::folder, or buildFolder()).
/// @param lockWaitMs How long to wait for the lock.
/// @returns What was there. Side effects: takes and releases the lock; writes nothing else.
BuildOutput readBuildOutput(const QString& root, const QString& folder, int lockWaitMs = 10'000);

/// The build folder of a target, project-relative: `.loom/build/4KX7/chain-verbose`, or under
/// `.loom/rev/<hash>/build/` for a commit. No side effects.
QString buildFolder(const QString& target, BuildMode mode, bool verbose, const QString& revHash = {});

/// The variables a build adds to `latexmk`'s environment (see buildTarget()). No side effects.
QProcessEnvironment latexmkEnvironment(QProcessEnvironment environment);

} // namespace loom
