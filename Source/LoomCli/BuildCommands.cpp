#include "BuildCommands.h"

#include "LoomCore/Build.h"

namespace cli {

namespace {

/// `1 error`, `2 warnings`. No side effects.
QString counted(int count, const QString& one, const QString& many)
{
    return QStringLiteral("%1 %2").arg(count).arg(count == 1 ? one : many);
}

} // namespace

int runBuild(const QStringList& args, const Context& context)
{
    const QString name = QStringLiteral("build");
    int exitCode = exitDone;
    const auto started = start(args, name, {1, {"--mode", "--rev", "--timeout"}, {}, {"--verbose"}}, context, exitCode,
                               QStringLiteral("the ID of a statement or proof"));
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;

    loom::BuildRequest request;
    request.root = root;
    request.target = parsed.positional[0];
    request.verbose = parsed.flags.contains(QStringLiteral("--verbose"));
    if (const std::optional<QString> mode = parsed.value("--mode")) {
        const std::optional<loom::BuildMode> read = loom::buildModeFromName(*mode);
        if (!read)
            return usageError(QStringLiteral("`--mode` is `alone`, `premises` or `chain`, not `%1`.").arg(*mode), context.json);
        request.mode = *read;
    }
    if (const std::optional<QString> rev = parsed.value("--rev")) {
        if (rev->isEmpty())
            return usageError(QStringLiteral("`--rev` needs a commit, such as `HEAD`."), context.json);
        request.rev = *rev;
    }
    if (const std::optional<QString> timeout = parsed.value("--timeout")) {
        bool ok = false;
        const int seconds = timeout->toInt(&ok);
        if (!ok || seconds < 1 || seconds > 86400)
            return usageError(QStringLiteral("`--timeout` is a number of seconds from 1 to 86400, not `%1`.").arg(*timeout), context.json);
        request.timeoutMs = seconds * 1000;
    }
    // A build that waits for another of the same folder waits as long as one may run.
    request.lockWaitMs = request.timeoutMs + 10'000;

    const loom::BuildResult result = loom::buildTarget(request);
    switch (result.outcome) {
    case loom::BuildOutcome::Refused:
        return fail(exitRefused, result.code, result.message, context.json);
    case loom::BuildOutcome::CantWork:
    case loom::BuildOutcome::Stopped:
        return fail(exitCantWork, result.code, result.message, context.json);
    case loom::BuildOutcome::Succeeded:
    case loom::BuildOutcome::Failed:
        break;
    }

    const bool built = result.outcome == loom::BuildOutcome::Succeeded;
    if (context.json) {
        QStringList problems;
        for (const loom::Problem& problem : result.problems)
            problems << jsonProblem(problem);
        write(stdout, QStringLiteral("{\"target\": %1, \"mode\": %2, \"verbose\": %3, \"rev\": %4, \"pdf\": %5, \"built\": %6, "
                                     "\"reused\": %7, \"seconds\": %8, \"problems\": [%9]}\n")
                          .arg(jsonString(result.target), jsonString(loom::buildModeName(result.mode)),
                               result.verbose ? QStringLiteral("true") : QStringLiteral("false"), jsonOptional(result.rev),
                               jsonOptional(result.pdf), built ? QStringLiteral("true") : QStringLiteral("false"),
                               result.reused ? QStringLiteral("true") : QStringLiteral("false"),
                               QString::number(result.seconds, 'f', 1), problems.join(QStringLiteral(", "))));
    } else {
        for (const loom::Problem& problem : result.problems)
            write(stderr, loom::problemLine(problem) + u'\n');
        if (built)
            write(stdout, result.pdf + u'\n');
        else
            write(stdout, QStringLiteral("Not built: %1, %2.\n")
                              .arg(counted(result.errors(), QStringLiteral("error"), QStringLiteral("errors")),
                                   counted(static_cast<int>(result.problems.size()) - result.errors(), QStringLiteral("warning"),
                                           QStringLiteral("warnings"))));
    }
    return built ? exitDone : exitRefused;
}

} // namespace cli
