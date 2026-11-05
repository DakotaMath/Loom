#pragma once

// A project as it is at a commit, read without touching the working tree or the index
// (Specs/m2-statements-review.md, "Building another revision"), so Review can build a statement as
// it was at `HEAD` beside how it is now.

#include "LoomCore/Git.h"
#include "LoomCore/ProjectContents.h"

#include <QString>
#include <QStringList>

namespace loom {

/// How reading a revision went.
enum class RevisionOutcome {
    Ready,     ///< The commit's files are in `folder`.
    NoCommits, ///< The repository has no commit yet, so everything is new.
    CantWork,  ///< Git failed, the commit doesn't exist, or a file couldn't be written; `message` says which.
};

/// The result of readRevision().
struct RevisionTree {
    RevisionOutcome outcome = RevisionOutcome::CantWork;
    /// For CantWork: `git-missing`, `git-timeout`, `git-failed` or `write-failed`.
    QString code;
    QString message;
    /// The commit's full hash.
    QString hash;
    /// `.loom/rev/<hash>/tree`, project-relative, with `/`.
    QString folder;
    /// True if the tree was already there and was used as it was.
    bool reused = false;
};

/// Writes the files Loom reads of a commit under `.loom/rev/<hash>/tree/` (Specs/m2-statements-
/// review.md, "Building another revision").
/// Rules: the commit is resolved with `git rev-parse --verify <rev>^{commit}`; a repository with no
/// commit yet (`HEAD` doesn't resolve, silently) is NoCommits. The files are `loom.toml`, the
/// preamble files it names, and everything under `Statements/`, `Proofs/`, `Papers/*/paper.toml`
/// and `Papers/*/Statements/`, read with `git ls-tree -r -z` and `git cat-file --batch`; links and
/// submodules are skipped. They are written to a temporary folder beside `tree/`, which is renamed
/// to `tree/` once complete, so a half-written tree is never used. A commit never changes, so an
/// existing `tree/` is used as it is. Each use is noted (`.loom/rev/<hash>/used`) for
/// pruneRevisions().
/// @param root The project folder, at the top of its repository.
/// @param rev  A commit as git reads it: `HEAD`, a hash, a branch.
/// @param git  Which git.
/// @returns The tree. Side effects: runs git; writes under `.loom/rev/`.
RevisionTree readRevision(const QString& root, const QString& rev, const GitTool& git);

/// Removes the trees and builds of all but the most recently used commits (Specs/m2-statements-
/// review.md: "When the project opens, Loom keeps the trees of the 5 most recently used commits").
/// @param keep How many to keep.
/// @returns The hashes removed. Side effects: deletes folders under `.loom/rev/`.
QStringList pruneRevisions(const QString& root, int keep = 5);

/// Removes the working-tree build folders of statements and proofs that no longer exist
/// (Specs/m2-statements-review.md, "The build folder", *Cleaning*): every `.loom/build/<target>/`
/// whose target isn't a statement tag or proof ID of `contents`. Other entries (`preview/`, files)
/// stay.
/// @returns The targets removed. Side effects: deletes folders under `.loom/build/`.
QStringList cleanBuildFolders(const QString& root, const ProjectContents& contents);

} // namespace loom
