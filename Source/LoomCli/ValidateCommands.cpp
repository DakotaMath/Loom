#include "ValidateCommands.h"

#include "LoomCore/Hooks.h"
#include "LoomCore/ProjectToml.h"
#include "LoomCore/Validation.h"

#include <QCoreApplication>
#include <QDir>

namespace cli {

namespace {

/// `1 error`, `2 warnings`. No side effects.
QString counted(int count, const QString& one, const QString& many)
{
    return QStringLiteral("%1 %2").arg(count).arg(count == 1 ? one : many);
}

/// One problem as `loom validate --json` gives it (Specs/m1-core.md, "JSON shapes").
QString jsonProblem(const loom::Problem& problem)
{
    return QStringLiteral("{\"severity\": %1, \"code\": %2, \"file\": %3, \"line\": %4, \"message\": %5}")
        .arg(jsonString(problem.severity == loom::Severity::Error ? QStringLiteral("error") : QStringLiteral("warning")),
             jsonString(problem.code), jsonString(problem.file), QString::number(problem.line), jsonString(problem.message));
}

} // namespace

int runValidate(const QStringList& args, const Context& context)
{
    int exitCode = exitDone;
    const auto started = start(args, QStringLiteral("validate"), {0, {}, {}, {QStringLiteral("--staged")}}, context, exitCode);
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;

    loom::ValidationRequest request;
    request.root = root;
    request.staged = parsed.flags.contains(QStringLiteral("--staged"));
    const loom::ValidationResult result = loom::validateProject(request);
    if (result.outcome == loom::ValidationOutcome::CantWork)
        return fail(exitCantWork, result.code, result.message, context.json);

    const int errors = result.errors();
    const int warnings = result.warnings();
    if (context.json) {
        QStringList problems;
        for (const loom::Problem& problem : result.problems)
            problems << jsonProblem(problem);
        write(stdout, QStringLiteral("{\"errors\": %1, \"warnings\": %2, \"problems\": [%3]}\n")
                          .arg(errors).arg(warnings).arg(problems.join(QStringLiteral(", "))));
    } else {
        for (const loom::Problem& problem : result.problems)
            write(stderr, loom::problemLine(problem) + u'\n');
        QString summary;
        if (result.problems.empty())
            summary = QStringLiteral("No problems.");
        else
            summary = counted(errors, QStringLiteral("error"), QStringLiteral("errors")) + QStringLiteral(", ")
                    + counted(warnings, QStringLiteral("warning"), QStringLiteral("warnings")) + u'.';
        if (errors > 0 && request.staged)
            summary += QStringLiteral(" Fix the errors and stage the fixes before committing.");
        write(stdout, summary + u'\n');
    }
    return errors > 0 ? exitRefused : exitDone;
}

int runInstallHooks(const QStringList& args, const Context& context)
{
    int exitCode = exitDone;
    const auto started = start(args, QStringLiteral("install-hooks"), {0, {}, {}, {}}, context, exitCode);
    if (!started)
        return exitCode;
    const QString& root = started->second;

    // A project from a newer Loom is left untouched, like every other write.
    const loom::OpenedProject project = loom::openProject(root);
    if (project.status == loom::OpenStatus::FormatTooNew)
        return fail(exitCantWork, QStringLiteral("format-too-new"), project.message, context.json);

    const loom::HooksResult result = loom::installHooks(root, QCoreApplication::applicationFilePath(), loom::GitTool());
    switch (result.outcome) {
    case loom::HooksOutcome::Installed:
        break;
    case loom::HooksOutcome::Refused:
        return fail(exitRefused, result.code, result.message, context.json);
    case loom::HooksOutcome::CantWork:
        return fail(exitCantWork, result.code, result.message, context.json);
    }

    const QString hook = QDir::toNativeSeparators(result.hookPath);
    const QString moved = QDir::toNativeSeparators(result.movedHook);
    if (context.json) {
        write(stdout, QStringLiteral("{\"hook\": %1, \"moved_hook\": %2, \"merge_driver\": \"loom-checks\"}\n")
                          .arg(jsonString(hook), jsonOptional(moved)));
    } else {
        QString text;
        if (!moved.isEmpty())
            text += QStringLiteral("The pre-commit hook that was there isn't Loom's, so Loom kept it as `%1`; git no longer runs it.\n").arg(moved);
        text += QStringLiteral("Installed the pre-commit hook `%1`: every commit now runs `loom validate --staged`.\n").arg(hook);
        text += QStringLiteral("Registered the `loom-checks` merge driver for check histories.\n");
        write(stdout, text);
    }
    return exitDone;
}

} // namespace cli
