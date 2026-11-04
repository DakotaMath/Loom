#pragma once

// `loom install-hooks` (Specs/m1-core.md, "The project folder"): the pre-commit hook that runs
// `loom validate --staged`, and the `loom-checks` merge driver for check histories. Git doesn't
// clone hooks or local config, so this runs after every clone as well as in `loom new-project`.

#include "LoomCore/Git.h"

#include <QByteArray>
#include <QString>

namespace loom {

/// How installing the hooks went; each maps to one of `loom`'s exit codes.
enum class HooksOutcome {
    Installed, ///< Exit 0.
    Refused,   ///< Exit 1: a rule refused it, and nothing was written.
    CantWork,  ///< Exit 3: not a git repository, git failed, or the hook couldn't be written.
};

/// The result of installHooks().
struct HooksResult {
    HooksOutcome outcome = HooksOutcome::Refused;
    /// A stable code for `--json`: empty when installed, else one of `not-a-repository`,
    /// `hook-backup-exists`, `write-failed`, `git-failed`.
    QString code;
    /// What went wrong, in plain English; empty when installed.
    QString message;
    /// The hook's path, absolute, with `/`.
    QString hookPath;
    /// Where an earlier hook that Loom didn't write was moved to (absolute, with `/`); empty if
    /// there was none.
    QString movedHook;
};

/// The pre-commit hook's text (Templates/hooks/pre-commit): a `sh` script, since Git for Windows
/// runs hooks with its own `sh`, that runs `loom validate --staged` with the `loom` on PATH, else
/// with `loomProgram`, and refuses the commit with the spec's message if neither runs.
/// @param loomProgram The absolute path of the `loom` installing the hook; empty for none, and
///                    then only PATH is tried.
/// @returns The script, LF line endings. No side effects.
QByteArray preCommitHook(const QString& loomProgram);

/// Whether a hook file is one Loom wrote: it holds the line `# Written by Loom (`loom install-hooks`)...`.
/// No side effects.
bool isLoomHook(const QByteArray& hook);

/// Installs the pre-commit hook and registers the merge driver (Specs/m1-core.md, "`loom
/// install-hooks`").
/// Rules: the hook goes where git looks for it (`git rev-parse --git-path hooks/pre-commit`,
/// normally `.git/hooks/pre-commit`); a hook Loom wrote is replaced, and one it didn't is first
/// renamed `pre-commit.before-loom`, unless that name is taken, which refuses (never lose a
/// user's hook). The repository's own config gets `merge.loom-checks.name = Loom check history`
/// and `merge.loom-checks.driver = loom merge-checks %O %A %B`; the user's global config is
/// never touched.
/// @param root        The project folder, the top of its git repository.
/// @param loomProgram As for preCommitHook().
/// @param git         Which git to run.
/// @returns The outcome. Side effects: writes the hook (executable), may rename an earlier one,
///          and sets two values in the repository's `.git/config`.
HooksResult installHooks(const QString& root, const QString& loomProgram, const GitTool& git);

} // namespace loom
