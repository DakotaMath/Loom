#include "LoomCore/EditSession.h"

#include "LoomCore/ProjectFiles.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QLockFile>

namespace loom::edits {

ChangeResult failure(ChangeOutcome outcome, const QString& code, const QString& message)
{
    ChangeResult result;
    result.outcome = outcome;
    result.code = code;
    result.message = message;
    return result;
}

ChangeResult refused(const QString& code, const QString& message)
{
    return failure(ChangeOutcome::Refused, code, message);
}

ChangeResult cantWork(const QString& code, const QString& message)
{
    return failure(ChangeOutcome::CantWork, code, message);
}

/// `<file>:<line>: <severity>: <message>`, as `loom validate` prints a problem.
/// Opens a project for a change, in the order that writes nothing to a project Loom mustn't
/// touch: read `loom.toml` (stopping on a newer format), then take the lock (creating `.loom/`),
/// then read everything again, now that no other `loom` can be changing it.
/// @returns Nothing, with `result` set, if the project can't be worked on.
std::optional<Session> begin(const QString& root, const ChangeOptions& options, ChangeResult& result)
{
    const auto openChecked = [&](OpenedProject& project) {
        project = openProject(root);
        switch (project.status) {
        case OpenStatus::Opened: return true;
        case OpenStatus::NotAProject: result = cantWork(QStringLiteral("not-a-project"), project.message); break;
        case OpenStatus::Unreadable: result = cantWork(QStringLiteral("unreadable"), project.message); break;
        case OpenStatus::FormatTooNew: result = cantWork(QStringLiteral("format-too-new"), project.message); break;
        }
        return false;
    };

    Session session;
    if (!openChecked(session.project))
        return std::nullopt;

    const QString lockRelative = QStringLiteral(".loom/write.lock");
    const std::optional<QString> lockPath = resolveProjectPath(session.project.root, lockRelative);
    if (!lockPath || !QDir().mkpath(QFileInfo(*lockPath).absolutePath())) {
        result = cantWork(QStringLiteral("write-failed"), QStringLiteral("Loom couldn't create `%1`.")
            .arg(QDir::toNativeSeparators(session.project.root + u'/' + lockRelative)));
        return std::nullopt;
    }
    session.lock = std::make_unique<QLockFile>(*lockPath);
    if (!session.lock->tryLock(options.lockWaitMs)) {
        result = cantWork(QStringLiteral("locked"),
            QStringLiteral("Another loom command is changing this project. Try again when it has finished."));
        return std::nullopt;
    }

    if (!openChecked(session.project))
        return std::nullopt;
    ContentsLoading loading = loadContents(session.project.root);
    if (!loading.ok) {
        result = cantWork(QStringLiteral("unreadable"), loading.message);
        return std::nullopt;
    }
    session.contents = std::move(loading.contents);
    return session;
}

/// The errors in `after` that aren't in `before`. Problems are compared by code, file and message,
/// not line, since an edit moves lines; each problem in `before` excuses one equal one in `after`.
std::vector<Problem> addedErrors(const std::vector<Problem>& before, const std::vector<Problem>& after)
{
    QHash<QString, int> count;
    const auto key = [](const Problem& p) { return p.code + u'\x1f' + p.file + u'\x1f' + p.message; };
    for (const Problem& problem : before) {
        if (problem.severity == Severity::Error)
            ++count[key(problem)];
    }
    std::vector<Problem> added;
    for (const Problem& problem : after) {
        if (problem.severity != Severity::Error)
            continue;
        int& left = count[key(problem)];
        if (left > 0)
            --left;
        else
            added.push_back(problem);
    }
    return added;
}

/// A refusal listing the errors a change would add.
ChangeResult refusedFor(const std::vector<Problem>& problems, const QString& what)
{
    QStringList lines{QStringLiteral("Loom didn't %1, because it would add %2:")
                          .arg(what, problems.size() == 1 ? QStringLiteral("this problem") : QStringLiteral("these problems"))};
    for (const Problem& problem : problems)
        lines << problemLine(problem);
    ChangeResult result = refused(problems.front().code, lines.join(u'\n'));
    result.problems = problems;
    return result;
}

/// Turns IDs as typed into tags of existing statements.
/// @returns Nothing, with `result` set to the refusal, if one names nothing.
std::optional<QStringList> resolveAll(const ProjectContents& contents, const QStringList& typed, ChangeResult& result)
{
    QStringList tags;
    for (const QString& ref : typed) {
        const std::optional<QString> tag = resolveStatement(contents, ref);
        if (!tag) {
            result = refused(QStringLiteral("ref-unknown"),
                QStringLiteral("`%1` isn't a statement or paper stub in this project.").arg(ref));
            return std::nullopt;
        }
        tags << *tag;
    }
    return tags;
}

/// A refusal to rewrite a file that rewriting would damage.
ChangeResult notEditable(const QString& path, const std::vector<Problem>& problems)
{
    QStringList lines{QStringLiteral("Loom can't change %1 until these problems are fixed, since rewriting it would lose what it couldn't read:").arg(path)};
    for (const Problem& problem : problems)
        lines << problemLine(problem);
    return refused(QStringLiteral("not-editable"), lines.join(u'\n'));
}

} // namespace loom::edits
