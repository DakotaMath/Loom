#pragma once

// The git panel's operations (Specs/m2-statements-review.md, "The git panel"): reading the branch
// and the changed files, a file's diff, committing ticked files through the pre-commit hook, new
// and switched branches, fetch, pull (fast-forward only) and push. Each runs the `git` program
// (Git.h) with `GIT_TERMINAL_PROMPT=0`, so git never waits for a terminal, and M1's timeout
// (`git-timeout`). Each refuses what the spec's table refuses before running anything that
// changes git, and reports every git command it ran, for Output's Git channel. Merges, rebases
// and conflicts are left to a terminal.

#include "LoomCore/Git.h"
#include "LoomCore/Problem.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <vector>

namespace loom {

/// One git command an operation ran, for Output's Git channel ("the time, the folder, the command
/// line, the exit code, how long it took, and its output").
struct GitStep {
    QDateTime started;      ///< When it started, local time.
    QString folder;         ///< Where it ran.
    QStringList arguments;  ///< git's arguments.
    qint64 milliseconds = 0;
    GitRun run;             ///< What it did.
};

/// The text Output's Git channel shows for a step: `In <folder>: <command>`, then the exit code
/// (or that it didn't finish or didn't start) and how long it took, then its stdout and stderr,
/// together cut at 1 MB (the spec: "its output (cut at 1 MB)").
/// @returns The text, without the time (Output adds it). No side effects.
QString gitStepText(const GitStep& step);

/// How a git operation went.
enum class GitOutcome {
    Done,     ///< It did what was asked.
    Refused,  ///< The spec's table refuses it (or git did, as for a refused commit); nothing changed.
    CantWork, ///< Git didn't run, didn't finish (`git-timeout`) or failed; `message` says why.
};

/// The result of an operation that changes git.
struct GitResult {
    GitOutcome outcome = GitOutcome::CantWork;
    /// A stable code for a refusal or failure: `nothing-ticked`, `empty-message`, `detached`,
    /// `in-progress`, `bad-branch-name`, `branch-exists`, `no-such-branch`, `uncommitted-changes`,
    /// `no-remote`, `no-upstream`, `cant-fast-forward`, `nothing-to-push`, `choose-remote`,
    /// `hook-refused`, `locked`, `git-missing`, `git-timeout`, `git-failed`. Empty when done.
    QString code;
    /// What to tell Dakota: the spec's words for a refusal, git's for a failure; for Done, a line
    /// saying what was done (*"Committed 3 files as 1a2b3c4."*).
    QString message;
    /// For a commit the hook refused: each line of its output in `loom validate`'s form
    /// (`<file>:<line>: <severity>: <message>`), as a problem, in order.
    std::vector<Problem> problems;
    /// Git's own output for a refused or failed command (stdout then stderr), to show in the panel.
    QString output;
    /// Every git command run, in order.
    std::vector<GitStep> steps;

    bool done() const { return outcome == GitOutcome::Done; }
};

/// How a changed file differs from `HEAD`, as Review marks it.
enum class FileChange { New, Changed, Deleted };

/// One changed file, from `git status --porcelain=v2 -z --untracked-files=all`.
struct ChangedFile {
    QString path;          ///< Relative to the project folder, with `/`.
    FileChange change = FileChange::Changed;
    bool untracked = false; ///< New and not in git's index: the commit must `git add` it first.
};

/// What the panel shows at the top, and what it may offer (Specs/m2-statements-review.md, "The
/// git panel", "At the top").
struct GitState {
    /// Whether it was read; if not, `code` and `message` say why (`git-missing`, `git-timeout`,
    /// `not-a-repository`, `git-failed`).
    bool ok = false;
    QString code;
    QString message;

    QString branch;         ///< The branch, or empty when `HEAD` is detached.
    QString head;           ///< `HEAD`'s short hash; empty in a repository with no commits.
    bool hasCommits = false;
    QString upstream;       ///< E.g. `origin/main`, or empty if the branch has none.
    int ahead = 0;          ///< Commits on the branch and not its upstream (as of the last fetch).
    int behind = 0;         ///< Commits on its upstream and not the branch.
    /// The operation git is in the middle of: `merge`, `rebase`, `cherry-pick` or `revert`; empty if none.
    QString inProgress;
    QStringList remotes;    ///< `git remote`, in its order.
    QStringList branches;   ///< The local branches, sorted.
    std::vector<ChangedFile> files; ///< Every changed file, sorted by path; ignored files left out.
    std::vector<GitStep> steps;     ///< Every git command run.

    bool detached() const { return ok && branch.isEmpty(); }
    /// Whether any change is to a tracked file (what *Switch branch* refuses).
    bool trackedChanges() const;
};

/// Reads the panel's state: `git status --porcelain=v2 -z --branch --untracked-files=all`, the
/// remotes, the local branches, and the files git leaves while a merge, rebase, cherry-pick or
/// revert is in progress (`MERGE_HEAD`, `rebase-merge/`, `rebase-apply/`, `CHERRY_PICK_HEAD`,
/// `REVERT_HEAD` in git's folder).
/// @param git  Which git.
/// @param root The project folder, at the top of its repository.
/// @returns The state. Side effects: runs git (which may refresh its index); changes nothing else.
GitState readGitState(const GitTool& git, const QString& root);

/// Reads the changed files from `git status --porcelain=v2 -z` output. A rename or copy (`2`)
/// lists the new path as new and, for a rename, the old one as deleted; an unmerged path (`u`) as
/// changed; an untracked one (`?`) as new and untracked. Header lines (`#`) and ignored files
/// (`!`) are skipped.
/// @returns The files, sorted by path. No side effects.
std::vector<ChangedFile> parseGitStatus(const QByteArray& output);

/// The text a click on a changed file shows: `git diff HEAD -- <path>`, or, for a new file that
/// git can't diff against `HEAD` (untracked, or in a repository with no commits), the file itself
/// (a binary file says so and gives its size).
/// @param steps The git commands run are added here.
/// @returns The text. Side effects: runs git; reads the file.
QString fileDiff(const GitTool& git, const QString& root, const ChangedFile& file, std::vector<GitStep>& steps);

/// **Commit**: `git add -- <ticked untracked files>`, then `git commit -m <message> -- <ticked
/// paths>`, so only the ticked files go in, through the pre-commit hook.
/// Refuses, before running anything that changes git: nothing ticked (`nothing-ticked`); a
/// message that is only whitespace (`empty-message`); a detached `HEAD` (`detached`: *"Make a
/// branch first: a commit here would be left on no branch."*); a merge, rebase, cherry-pick or
/// revert in progress (`in-progress`). When the commit fails (the hook refused, or git did), the
/// files it added are taken out of the index again (`git reset -q --`, or `git rm --cached -q --`
/// with no commit yet), so a refused commit leaves git's index as it found it; the result is
/// `hook-refused` with the hook's problems when any line of git's output is in `loom validate`'s
/// form, else `git-failed`.
/// @param ticked  The ticked files, as readGitState() listed them.
/// @param message The commit message, as typed.
/// @returns The result. Side effects: runs git; on success, a new commit.
GitResult commitFiles(const GitTool& git, const QString& root, const std::vector<ChangedFile>& ticked, const QString& message);

/// The problems in output in `loom validate`'s form, `<file>:<line>: <severity>: <message>`, one
/// per line; other lines are skipped. `code` is left empty (the form doesn't carry it).
/// @returns The problems, in order. No side effects.
std::vector<Problem> parseProblemLines(const QString& output);

/// **New branch…**: `git switch -c <name>`; uncommitted changes come along, as git does.
/// Refuses a name `git check-ref-format --branch` rejects (`bad-branch-name`), a branch that
/// exists (`branch-exists`), and a merge, rebase, cherry-pick or revert in progress.
/// @returns The result. Side effects: runs git; on success, the new branch, checked out.
GitResult createBranch(const GitTool& git, const QString& root, const QString& name);

/// **Switch branch…**: `git switch <name>`, holding `.loom/write.lock` (Specs/m2-statements-review.md,
/// "Locks": switching changes the working tree, so a save waits for it and it waits for a save).
/// Refuses uncommitted changes to tracked files (`uncommitted-changes`: *"Commit your changes
/// first, or they would come along to {branch}."*), a branch that isn't a local branch
/// (`no-such-branch`), an operation in progress, and a lock not free within `lockWaitMs` (`locked`,
/// M1's message). Unsaved edits in the app are the app's to refuse.
/// @returns The result. Side effects: runs git; creates `.loom/` for the lock; on success, the
/// working tree is the branch's.
GitResult switchBranch(const GitTool& git, const QString& root, const QString& name, int lockWaitMs = 10'000);

/// **Fetch**: `git fetch --prune`. Refuses a repository with no remote (`no-remote`).
/// @returns The result. Side effects: runs git; updates the remote-tracking branches.
GitResult fetchRemotes(const GitTool& git, const QString& root);

/// **Pull**: `git pull --ff-only`, holding `.loom/write.lock`. Refuses no remote, a detached
/// `HEAD`, no upstream (`no-upstream`), an operation in progress, and a lock not free in time.
/// When the pull fails and `HEAD` isn't an ancestor of the upstream, git couldn't fast-forward
/// (`cant-fast-forward`: *"{branch} and its remote have both moved on, so Loom can't fast-forward.
/// Merge them in a terminal."*); telling it that way doesn't depend on git's language. Any other
/// failure (an uncommitted change the pull would overwrite) is git's, as `git-failed`. Unsaved
/// edits in the app are the app's to refuse.
/// @returns The result. Side effects: runs git; on success, the branch moved forward.
GitResult pullFastForward(const GitTool& git, const QString& root, int lockWaitMs = 10'000);

/// What **Push…** would send, shown before anything is pushed.
struct PushPlan {
    /// Whether it can push; if not, `code` and `message` say why (`no-remote`, `detached`,
    /// `nothing-to-push`, or git's failure).
    bool ok = false;
    QString code;
    QString message;
    QString branch;
    QString upstream;     ///< The branch's upstream, or empty: the push then sets one (`-u`).
    QStringList remotes;  ///< The remotes to choose from when there is no upstream.
    QStringList commits;  ///< `git log --oneline` of the commits that will go up, newest first.
    std::vector<GitStep> steps;
};

/// Lists what **Push…** would send: `git log --oneline @{upstream}..HEAD`, or, with no upstream,
/// `git log --oneline HEAD --not --remotes`. Refuses no remote, a detached `HEAD` (a push needs a
/// branch to name), and nothing to push.
/// @returns The plan. Side effects: runs git; changes nothing.
PushPlan planPush(const GitTool& git, const QString& root);

/// **Push {n} commits**: `git push`, or with no upstream `git push -u <remote> <branch>`. Never
/// forces. The same refusals as planPush(), and `choose-remote` when there is no upstream, several
/// remotes and `remote` is empty.
/// @param remote The remote to push to when there is no upstream; empty for the only one.
/// @returns The result. Side effects: runs git; on success, the commits are on the remote.
GitResult pushCommits(const GitTool& git, const QString& root, const QString& remote = {});

} // namespace loom
