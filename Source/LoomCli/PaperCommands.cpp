#include "PaperCommands.h"

#include "LoomCore/PaperEdits.h"

#include <QDir>
#include <QFileInfo>

namespace cli {

namespace {

/// The renames as a JSON array of `{"from", "to"}`. No side effects.
QString jsonRenamed(const std::vector<loom::PaperRenamed>& renamed)
{
    QStringList items;
    for (const loom::PaperRenamed& rename : renamed)
        items << QStringLiteral("{\"from\": %1, \"to\": %2}").arg(jsonString(rename.from), jsonString(rename.to));
    return u'[' + items.join(u", ") + u']';
}

/// Says on stderr which papers were renamed, so a person sees it whatever stdout is for.
void tellRenamed(const std::vector<loom::PaperRenamed>& renamed, const QString& except = {})
{
    for (const loom::PaperRenamed& rename : renamed) {
        if (rename.to != except)
            write(stderr, QStringLiteral("Renamed %1 to %2: it shares its key with another paper, so both have letters now.\n")
                              .arg(rename.from, rename.to));
    }
}

} // namespace

int runNewPaper(const QStringList& args, const Context& context)
{
    int exitCode = exitDone;
    const auto started = start(args, QStringLiteral("new-paper"), {1, {}, {}, {}}, context, exitCode,
                               QStringLiteral("a draft paper.toml"));
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;
    const QString& draftPath = parsed.positional[0];
    const std::optional<QByteArray> draft = readTextFile(draftPath, context.json, exitCode);
    if (!draft)
        return exitCode;

    const loom::PaperChange result = loom::createPaper(root, *draft, QDir::fromNativeSeparators(draftPath));
    if (result.change.outcome == loom::ChangeOutcome::Done)
        tellRenamed(result.renamed);
    return report(result.change, context.json, result.change.id,
                  QStringLiteral("\"key\": %1, \"path\": %2, \"renamed\": %3")
                      .arg(jsonString(result.change.id), jsonString(result.change.path), jsonRenamed(result.renamed)));
}

int runNewStub(const QStringList& args, const Context& context)
{
    const QString name = QStringLiteral("new-stub");
    int exitCode = exitDone;
    const auto started = start(args, name,
        {1, {"--kind", "--gist", "--version", "--at", "--by", "--title"}, {"--stated-using", "--paper-premise"}, {}},
        context, exitCode, QStringLiteral("a paper's key"));
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;
    if (const QString problem = missing(parsed, name, {"--kind", "--gist", "--version", "--at", "--by"}); !problem.isEmpty())
        return usageError(problem, context.json);

    loom::NewStub request;
    request.paper = parsed.positional[0];
    request.kind = *parsed.value("--kind");
    request.gist = *parsed.value("--gist");
    request.version = *parsed.value("--version");
    request.at = *parsed.value("--at");
    request.by = *parsed.value("--by");
    request.title = parsed.value("--title").value_or(QString());
    request.statedUsing = parsed.repeated.value("--stated-using");
    request.paperPremises = parsed.repeated.value("--paper-premise");
    const loom::ChangeResult result = loom::createStub(root, request);
    return report(result, context.json, result.id,
                  QStringLiteral("\"id\": %1, \"path\": %2").arg(jsonString(result.id), jsonString(result.path)));
}

int runRekey(const QStringList& args, const Context& context)
{
    int exitCode = exitDone;
    const auto started = start(args, QStringLiteral("rekey"), {1, {}, {}, {}}, context, exitCode, QStringLiteral("a paper's key"));
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;

    const loom::PaperChange result = loom::rekeyPaper(root, parsed.positional[0]);
    const QString to = result.change.id;
    const QString from = result.renamed.empty() ? to : result.renamed.back().from;
    if (result.change.outcome == loom::ChangeOutcome::Done)
        tellRenamed(result.renamed, to);
    const QString text = result.renamed.empty() ? QStringLiteral("%1: already right.").arg(to) : QStringLiteral("%1 → %2").arg(from, to);
    return report(result.change, context.json, text,
                  QStringLiteral("\"from\": %1, \"to\": %2, \"renamed\": %3")
                      .arg(jsonString(from), jsonString(to), jsonRenamed(result.renamed)));
}

int runBuildBib(const QStringList& args, const Context& context)
{
    int exitCode = exitDone;
    const auto started = start(args, QStringLiteral("build-bib"), {0, {"--out"}, {}, {}}, context, exitCode);
    if (!started)
        return exitCode;
    const auto& [parsed, root] = *started;

    // `--out` is typed relative to where the person is; LoomCore takes project paths.
    QString out;
    if (const std::optional<QString> typed = parsed.value("--out")) {
        const QString absolute = QFileInfo(*typed).absoluteFilePath();
        out = QDir(root).relativeFilePath(absolute);
        if (out.startsWith(u"../") || out == u".." || QFileInfo(out).isAbsolute())
            return fail(exitCantWork, QStringLiteral("bad-path"),
                QStringLiteral("`%1` must be inside the project.").arg(QDir::toNativeSeparators(*typed)), context.json);
    }

    const loom::BibliographyBuilt built = loom::buildBibliography(root, out);
    if (built.change.outcome != loom::ChangeOutcome::Done)
        return report(built.change, context.json, {}, {});
    QStringList problems;
    for (const loom::Problem& problem : built.problems) {
        const QString severity = problem.severity == loom::Severity::Error ? QStringLiteral("error") : QStringLiteral("warning");
        write(stderr, QStringLiteral("%1:%2: %3: %4\n").arg(problem.file).arg(problem.line).arg(severity, problem.message));
        problems << QStringLiteral("{\"severity\": %1, \"code\": %2, \"file\": %3, \"line\": %4, \"message\": %5}")
                        .arg(jsonString(severity), jsonString(problem.code), jsonString(problem.file))
                        .arg(problem.line)
                        .arg(jsonString(problem.message));
    }
    const QString path = QDir::toNativeSeparators(QDir(root).filePath(built.change.path));
    return report(built.change, context.json, path,
                  QStringLiteral("\"path\": %1, \"problems\": [%2]").arg(jsonString(path), problems.join(u", ")));
}

} // namespace cli
