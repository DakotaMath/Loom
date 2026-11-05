#pragma once

// Opening a folder and reading a project for the app (Specs/m2-statements-review.md, "Starting and
// opening" and "The app and LoomCore", "Snapshots"). Both read through LoomCore and run on the
// Project lane; neither touches a widget.

#include "LoomCore/Git.h"
#include "LoomCore/Problem.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProjectToml.h"
#include "LoomCore/Status.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QString>

#include <vector>

namespace loom {

/// What a folder is, as the table *Opening a folder* sorts them.
enum class FolderKind {
    Project,        ///< Has a `loom.toml` that reads (its problems are the snapshot's).
    InsideProject,  ///< A folder above it has `loom.toml`.
    Empty,          ///< Empty: a project can be created there.
    NotAProject,    ///< No `loom.toml`, and not empty.
    TomlUnreadable, ///< `loom.toml` doesn't parse, or can't be read.
    FormatTooNew,   ///< A newer Loom made it.
    Missing,        ///< The folder doesn't exist (a recent project that has gone).
};

/// What inspectFolder() found.
struct FolderInspection {
    FolderKind kind = FolderKind::Missing;
    /// The folder looked at, absolute, with `/`.
    QString folder;
    /// For Project: the folder; for InsideProject: the project above it.
    QString projectRoot;
    /// For Project and InsideProject: the project's name (the folder's name if `loom.toml`
    /// doesn't say).
    QString projectName;
    /// For every kind but Project: the spec's message, in plain English.
    QString message;
};

/// Sorts a folder into the rows of *Opening a folder*, in the table's order: a folder with
/// `loom.toml` (which reads, doesn't parse, or is from a newer Loom), one inside a project, an
/// empty one, any other.
/// Rule for "doesn't parse": `loom.toml`'s problems include `toml-syntax` or `not-utf8`, which stop
/// it being read at all; any other problem in it is a validation problem, and the project opens.
/// @param path The folder.
/// @returns What it is. Reads `loom.toml` files and lists the folder; writes nothing.
FolderInspection inspectFolder(const QString& path);

/// An immutable picture of the project, which every tab draws from (Specs/m2-statements-review.md,
/// "Snapshots": "the project's contents, graph, statuses and validation problems").
struct ProjectSnapshot {
    QString root;               ///< The project folder, absolute, with `/`.
    ProjectConfig config;       ///< `loom.toml`.
    ProjectContents contents;   ///< Every statement, proof and paper.
    ProjectGraph graph;         ///< With every computed status.
    /// The bytes of every statement, proof, verbose proof and paper stub file, by project-relative
    /// path, as read for this snapshot: what the editor opens, and compares with what it loaded to
    /// see a file changed outside Loom (Specs/m2-statements-review.md, "Never losing text").
    QHash<QString, QByteArray> files;
    /// When each of `files` was last modified, for sorting the list by *recently changed*.
    QHash<QString, QDateTime> modified;
    /// Every validation problem, as `loom validate` sorts them; plus, if the project couldn't be
    /// validated at all, nothing and `unreadable` says why.
    std::vector<Problem> problems;
    int errors = 0;
    int warnings = 0;
    /// Why the project couldn't be read or validated in full (a file Loom can't read), or empty.
    QString unreadable;
    /// The current branch, or empty (detached `HEAD`, no repository, no git).
    QString branch;
    /// Empty if git works and the project is in a repository; else what Review and the git panel
    /// say (*"This project isn't in a git repository"*, or *"Loom can't find git; Check my setup
    /// says what to install."*).
    QString gitProblem;
};

/// Reads a project into a snapshot: `loom.toml`, the contents and graph (loadContents(),
/// buildGraph()), the bytes and times of the statement, proof and stub files, the validation problems (validateProject(), skipping the checks against the
/// last commit when git can't read it), and the branch (`git symbolic-ref --short -q HEAD`).
/// @param root The project folder.
/// @param git  Which git.
/// @returns The snapshot. Side effects: runs git; writes nothing.
ProjectSnapshot readSnapshot(const QString& root, const GitTool& git);

} // namespace loom
