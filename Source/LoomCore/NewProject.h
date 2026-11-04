#pragma once

// Creating a project: what `loom new-project` does (Specs/m1-core.md, "The project folder").

#include "LoomCore/Git.h"

#include <QDate>
#include <QString>

namespace loom {

/// What to create.
struct NewProjectRequest {
    /// The project folder: absolute, or relative to the current folder. It must be new or empty,
    /// and the folder above it must exist.
    QString folder;
    /// The project's name, for `loom.toml` and the first commit's message. Must not be empty.
    QString name;
    /// The research idea; empty means none.
    QString idea;
    /// The `created` date in `loom.toml`.
    QDate created = QDate::currentDate();
    /// Which git to run.
    GitTool git;
    /// The absolute path of the `loom` creating the project, for the pre-commit hook to fall back
    /// on when `loom` isn't on PATH (see installHooks()); empty for none.
    QString loomProgram;
};

/// How creating a project went; each maps to one of `loom`'s exit codes.
enum class NewProjectOutcome {
    Created,  ///< Exit 0.
    Refused,  ///< Exit 1: a rule refused it, and nothing was written.
    CantWork, ///< Exit 3: git, Git LFS or git's identity is missing, or a write or git step failed.
};

/// The result of createProject().
struct NewProjectResult {
    NewProjectOutcome outcome = NewProjectOutcome::Refused;
    /// A stable code for `--json`: empty when created, else one of `folder-not-empty`,
    /// `not-a-folder`, `parent-missing`, `name-empty`, `git-missing`, `git-lfs-missing`,
    /// `git-identity-missing`, `kit-missing`, `write-failed`, `git-failed`, `hooks-failed`.
    QString code;
    /// What went wrong, in plain English; empty when created.
    QString message;
    /// The project folder, absolute, with `/`.
    QString path;
};

/// Creates a project (Specs/m1-core.md, "The project folder", steps 1 to 6).
/// In order: refuses a non-empty folder, a file, or a folder whose parent doesn't exist; checks
/// that `git` and `git lfs` run and that git knows the user's name and email, as the new repo
/// will see them; only then creates the folder, writes `loom.toml` and the project kit (with LF
/// line endings, `gitignore` and `gitattributes` renamed to `.gitignore` and `.gitattributes`),
/// runs `git init -b main`, `git lfs install --local` and installHooks(), and commits everything
/// as `Create project <name>`, through the pre-commit hook like any commit.
/// @param request What to create.
/// @returns The outcome. On any failure after the checks, what was made is left in place and
///          `message` says which step failed.
/// Side effects: creates the folder and its files and a git repository in it, with Loom's hook
/// and merge driver; runs git, and through the hook, `loom validate --staged`. Git's
/// repository-location variables (`GIT_DIR`, `GIT_WORK_TREE`, …) are removed from git's
/// environment, so the new repository can't land anywhere else.
NewProjectResult createProject(const NewProjectRequest& request);

} // namespace loom
