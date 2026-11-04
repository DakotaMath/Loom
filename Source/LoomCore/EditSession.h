#pragma once

// What every change to a project shares (Edits.h, PaperEdits.h): opening the project under the
// write lock, comparing its problems before and after the change, and the results a refusal
// gives. Internal to LoomCore.

#include "LoomCore/Edits.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProjectToml.h"

#include <QLockFile>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>
#include <vector>

namespace loom::edits {

/// A result that failed with `outcome`, `code` and `message`. No side effects.
ChangeResult failure(ChangeOutcome outcome, const QString& code, const QString& message);

/// A refusal (exit 1). No side effects.
ChangeResult refused(const QString& code, const QString& message);

/// A result for a project Loom can't work on (exit 3). No side effects.
ChangeResult cantWork(const QString& code, const QString& message);

/// A project opened for a change: `loom.toml` read, the write lock held, the contents read.
struct Session {
    OpenedProject project;
    ProjectContents contents;
    std::unique_ptr<QLockFile> lock;

    /// Every problem the project shows, for comparing before and after a change.
    std::vector<Problem> problems(const ProjectConfig& config, const std::vector<Problem>& tomlProblems,
                                  const ProjectContents& changed) const
    {
        std::vector<Problem> all = tomlProblems;
        const std::vector<Problem> more = checkContents(config, changed);
        all.insert(all.end(), more.begin(), more.end());
        return all;
    }

    /// Every problem the project shows now.
    std::vector<Problem> problems() const { return problems(project.config, project.problems, contents); }
};

/// Opens a project for a change, in the order that writes nothing to a project Loom mustn't
/// touch: read `loom.toml` (stopping on a newer format), then take the lock (creating `.loom/`),
/// then read everything again, now that no other `loom` can be changing it.
/// @returns Nothing, with `result` set, if the project can't be worked on.
/// Side effects: creates `.loom/` and takes `.loom/write.lock`, held until the session ends.
std::optional<Session> begin(const QString& root, const ChangeOptions& options, ChangeResult& result);

/// The errors in `after` that aren't in `before`. Problems are compared by code, file and message,
/// not line, since an edit moves lines; each problem in `before` excuses one equal one in `after`.
/// No side effects.
std::vector<Problem> addedErrors(const std::vector<Problem>& before, const std::vector<Problem>& after);

/// A refusal listing the errors a change would add.
/// @param what For the message: `create the statement`, … No side effects.
ChangeResult refusedFor(const std::vector<Problem>& problems, const QString& what);

/// Turns IDs as typed into tags of existing statements or stubs.
/// @returns Nothing, with `result` set to the refusal, if one names nothing. No side effects.
std::optional<QStringList> resolveAll(const ProjectContents& contents, const QStringList& typed, ChangeResult& result);

/// A refusal to rewrite a file that rewriting would damage, listing its problems. No side effects.
ChangeResult notEditable(const QString& path, const std::vector<Problem>& problems);

} // namespace loom::edits
