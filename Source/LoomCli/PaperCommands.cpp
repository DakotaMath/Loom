#include "PaperCommands.h"

#include "LoomCore/PaperEdits.h"
#include "LoomCore/Problem.h"

#include <QDir>
#include <QFileInfo>

#include <filesystem>
#include <system_error>

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

/// Says on stderr which of the draft's verified and checked dates Loom took out (S16, A16), so a
/// person sees it whatever stdout is for: the entry's own `verified` is left out, since it can't
/// be `false`, and the others are set to false. Says nothing when there are none.
void tellUnclicked(const QStringList& unclicked)
{
    QStringList setToFalse = unclicked;
    if (setToFalse.removeAll(QStringLiteral("verified")) > 0)
        write(stderr, QStringLiteral("Left out `verified`, since verifying is Dakota's click.\n"));
    if (!setToFalse.isEmpty())
        write(stderr, QStringLiteral("Set to false, since verifying is Dakota's click: %1.\n").arg(listed(setToFalse)));
}

/// `path` as the file system resolves it, absolute with `/`: every link, junction and 8.3 short
/// name in the part that exists replaced by what it names; the rest (a file not written yet, and
/// any folders above it that don't exist) kept as typed. std::filesystem::weakly_canonical()
/// does this on Windows too, where QFileInfo::canonicalFilePath() leaves junctions alone
/// (Wiki/gotchas/qt/canonical-path-ignores-junctions.md).
/// @param path Absolute, or relative to the current folder.
/// @returns The resolved path; the absolute path as typed if it can't be resolved. No side effects.
QString resolvedPath(const QString& path)
{
    const QFileInfo info(path);
    std::error_code error;
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(info.filesystemAbsoluteFilePath(), error);
    if (error)
        return QDir::cleanPath(info.absoluteFilePath());
    return QDir::cleanPath(QFileInfo(resolved).absoluteFilePath());
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
    if (result.change.outcome == loom::ChangeOutcome::Done) {
        tellRenamed(result.renamed);
        tellUnclicked(result.unclicked);
    }
    return report(result.change, context.json, result.change.id,
                  QStringLiteral("\"key\": %1, \"path\": %2, \"renamed\": %3, \"unclicked\": %4")
                      .arg(jsonString(result.change.id), jsonString(result.change.path), jsonRenamed(result.renamed),
                           jsonStringArray(result.unclicked)));
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

    // `--out` is typed relative to where the person is; LoomCore takes project paths. Both are
    // resolved first, so a link, a junction or a short name leading into a folder Loom keeps its
    // files in is seen for what it is, and LoomCore's guard applies (M2).
    QString out;
    if (const std::optional<QString> typed = parsed.value("--out")) {
        out = QDir(resolvedPath(root)).relativeFilePath(resolvedPath(*typed));
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
        // Each line is filled in by one .arg() call, so a `%2` in a file name is never replaced
        // by a later call.
        write(stderr, loom::problemLine(problem) + u'\n');
        problems << QStringLiteral("{\"severity\": %1, \"code\": %2, \"file\": %3, \"line\": %4, \"message\": %5}")
                        .arg(jsonString(severity), jsonString(problem.code), jsonString(problem.file),
                             QString::number(problem.line), jsonString(problem.message));
    }
    // A person is told where the file is; the JSON gives the project path, as every other
    // subcommand's `path` does (A23).
    const QString path = QDir::toNativeSeparators(QDir(root).filePath(built.change.path));
    return report(built.change, context.json, path,
                  QStringLiteral("\"path\": %1, \"problems\": [%2]").arg(jsonString(built.change.path), problems.join(u", ")));
}

} // namespace cli
