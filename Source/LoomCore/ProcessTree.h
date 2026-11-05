#pragma once

// Running a program that starts others (latexmk starts pdflatex and bibtex) so that stopping it
// stops all of them (Specs/m2-statements-review.md, "Running `latexmk`": "Loom stops latexmk and
// everything it started (on Windows, through a job object, since killing latexmk alone leaves
// pdflatex running and holding the files)").

#include <QByteArray>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <atomic>
#include <memory>

namespace loom {

/// What to run.
struct ProcessRequest {
    QString program;            ///< A name looked up on PATH, or a path.
    QStringList arguments;
    QString workingDir;         ///< Must exist.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    int timeoutMs = 120'000;    ///< How long before Loom stops it.
    /// Set from another thread to stop the run early (a newer build of the same folder); may be null.
    std::shared_ptr<std::atomic_bool> stop;
};

/// How a run ended.
enum class ProcessEnd {
    Finished,      ///< It exited by itself; `exitCode` says how.
    FailedToStart, ///< It couldn't be started: not installed, or not on PATH.
    TimedOut,      ///< It ran out of time, and Loom stopped it and everything it started.
    Stopped,       ///< `stop` was set, and Loom stopped it and everything it started.
};

/// What a run did.
struct ProcessRun {
    ProcessEnd end = ProcessEnd::FailedToStart;
    int exitCode = -1;
    QByteArray output; ///< stdout and stderr, merged, as written.
};

/// Runs a program and waits for it, stopping it and every process it started if it runs out of
/// time or is asked to stop.
/// Rules: on Windows the program is created suspended, put in a job object that kills its
/// processes when closed, then resumed, so not even its first child can start outside the job;
/// stopping terminates the job. Elsewhere it starts in a session of its own, and stopping kills
/// that process group. When it finishes by itself, anything it left running is stopped too.
/// @returns How it ended. Side effects: whatever the program does.
ProcessRun runProcessTree(const ProcessRequest& request);

} // namespace loom
