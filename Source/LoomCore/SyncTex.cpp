#include "LoomCore/SyncTex.h"

#include "LoomCore/ProcessTree.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>

namespace loom {

namespace {

/// Reads a balanced `{…}` group starting at `at` (which must be `{`).
/// @returns The index just after the closing brace, or -1 if it doesn't close. No side effects.
qsizetype skipGroup(const QByteArray& text, qsizetype at)
{
    if (at >= text.size() || text[at] != '{')
        return -1;
    int depth = 0;
    for (qsizetype i = at; i < text.size(); ++i) {
        if (text[i] == '\\') {
            ++i; // An escaped brace doesn't count.
            continue;
        }
        if (text[i] == '{')
            ++depth;
        else if (text[i] == '}' && --depth == 0)
            return i + 1;
    }
    return -1;
}

/// The value of a `Key:value` line in `synctex`'s first result, or a null string.
QString field(const QStringList& lines, const QString& key)
{
    const QString prefix = key + u':';
    for (const QString& line : lines) {
        if (line.startsWith(prefix))
            return line.mid(prefix.size()).trimmed();
    }
    return {};
}

/// The lines of `synctex`'s first result (between `SyncTeX result begin` and the next
/// `SyncTeX result end` or `Output:` after the first), or none.
QStringList firstResult(const QByteArray& output)
{
    const QStringList all = QString::fromUtf8(output).split(u'\n');
    QStringList lines;
    bool in = false;
    bool sawOutput = false;
    for (QString line : all) {
        if (line.endsWith(u'\r'))
            line.chop(1);
        if (line.startsWith(QStringLiteral("SyncTeX result begin"))) {
            in = true;
            continue;
        }
        if (!in)
            continue;
        if (line.startsWith(QStringLiteral("SyncTeX result end")))
            break;
        if (line.startsWith(QStringLiteral("Output:"))) {
            // A second record starts with its own Output: line.
            if (sawOutput)
                break;
            sawOutput = true;
        }
        lines << line;
    }
    return lines;
}

/// A file's bytes, or none if it can't be read.
QByteArray readBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/// Runs `synctex` in the build folder. @returns The run. Side effects: runs `synctex`.
ProcessRun runSynctex(const SyncTexTool& tool, const QString& buildFolder, const QStringList& arguments)
{
    ProcessRequest request;
    request.program = tool.program;
    request.arguments = arguments;
    request.workingDir = buildFolder;
    request.environment = tool.environment;
    request.timeoutMs = tool.timeoutMs;
    return runProcessTree(request);
}

} // namespace

int auxLabelPage(const QByteArray& aux, const QString& label)
{
    const QByteArray start = "\\newlabel{" + label.toUtf8() + "}";
    qsizetype at = 0;
    while ((at = aux.indexOf(start, at)) >= 0) {
        qsizetype i = at + start.size();
        at = i;
        if (i >= aux.size() || aux[i] != '{')
            continue;
        ++i; // Into the outer group: {<text>}{<page>}…
        i = skipGroup(aux, i);
        if (i < 0 || i >= aux.size() || aux[i] != '{')
            continue;
        const qsizetype close = skipGroup(aux, i);
        if (close < 0)
            continue;
        bool ok = false;
        const int page = aux.mid(i + 1, close - i - 2).trimmed().toInt(&ok);
        if (ok && page > 0)
            return page;
    }
    return 0;
}

std::optional<SyncTexBox> parseSynctexView(const QByteArray& output)
{
    const QStringList lines = firstResult(output);
    bool ok = false;
    SyncTexBox box;
    box.page = field(lines, QStringLiteral("Page")).toInt(&ok);
    if (!ok || box.page <= 0)
        return std::nullopt;
    const double h = field(lines, QStringLiteral("h")).toDouble();
    const double v = field(lines, QStringLiteral("v")).toDouble();
    const double width = field(lines, QStringLiteral("W")).toDouble();
    const double height = field(lines, QStringLiteral("H")).toDouble();
    if (width > 0 && height > 0) {
        box.x = h;
        box.y = v - height;
        box.width = width;
        box.height = height;
    } else {
        box.x = field(lines, QStringLiteral("x")).toDouble();
        box.y = field(lines, QStringLiteral("y")).toDouble();
    }
    return box;
}

std::optional<SyncTexPlace> parseSynctexEdit(const QByteArray& output, const QString& buildFolder)
{
    const QStringList lines = firstResult(output);
    QString input = field(lines, QStringLiteral("Input"));
    bool ok = false;
    const int line = field(lines, QStringLiteral("Line")).toInt(&ok);
    if (input.isEmpty() || !ok || line <= 0)
        return std::nullopt;
    input.replace(u'\\', u'/');
    QString path = QDir::cleanPath(input);
    const QString folder = QDir::cleanPath(QString(buildFolder).replace(u'\\', u'/'));
#ifdef Q_OS_WIN
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    if (QDir::isAbsolutePath(path)) {
        if (!path.startsWith(folder + u'/', cs))
            return std::nullopt;
        path = path.mid(folder.size() + 1);
    }
    if (path == u"main.tex")
        return SyncTexPlace{path, line};
    if (!path.startsWith(QStringLiteral("src/")))
        return std::nullopt;
    path = path.mid(4);
    if (path.isEmpty() || path.startsWith(QStringLiteral("../")))
        return std::nullopt;
    return SyncTexPlace{path, line};
}

std::optional<SyncTexPlace> generatedPlace(const QByteArray& mainTex, int line, const std::function<QByteArray(const QString&)>& copy)
{
    static const QRegularExpression input(QStringLiteral("^\\\\input\\{(src/[^}]+)\\}"));
    const QList<QByteArray> lines = mainTex.split('\n');
    for (int i = line - 1; i >= 0 && i < lines.size() && i <= line + 1; ++i) {
        const QRegularExpressionMatch match = input.match(QString::fromUtf8(lines[i]).trimmed());
        if (!match.hasMatch())
            continue;
        const QString file = match.captured(1);
        const QList<QByteArray> body = copy(file).split('\n');
        int first = 1;
        while (first <= body.size() && body[first - 1].trimmed() == "%")
            ++first;
        return SyncTexPlace{file.mid(4), first};
    }
    return std::nullopt;
}

SyncTexOutcome synctexForward(const SyncTexTool& tool, const QString& buildFolder, const QString& file, int line, int column,
                              SyncTexBox& box)
{
    const ProcessRun run = runSynctex(tool, buildFolder,
                                      {QStringLiteral("view"), QStringLiteral("-i"),
                                       QStringLiteral("%1:%2:src/%3").arg(line).arg(column).arg(file), QStringLiteral("-o"),
                                       QStringLiteral("main.pdf")});
    if (run.end != ProcessEnd::Finished)
        return SyncTexOutcome::Missing;
    const std::optional<SyncTexBox> found = parseSynctexView(run.output);
    if (!found)
        return SyncTexOutcome::NotFound;
    box = *found;
    return SyncTexOutcome::Found;
}

SyncTexOutcome synctexInverse(const SyncTexTool& tool, const QString& buildFolder, int page, double x, double y,
                              SyncTexPlace& place)
{
    const ProcessRun run = runSynctex(tool, buildFolder,
                                      {QStringLiteral("edit"), QStringLiteral("-o"),
                                       QStringLiteral("%1:%2:%3:main.pdf").arg(page).arg(x, 0, 'f', 2).arg(y, 0, 'f', 2)});
    if (run.end != ProcessEnd::Finished)
        return SyncTexOutcome::Missing;
    std::optional<SyncTexPlace> found = parseSynctexEdit(run.output, buildFolder);
    if (found && found->file == u"main.tex") {
        found = generatedPlace(readBytes(buildFolder + QStringLiteral("/main.tex")), found->line,
                               [&](const QString& path) { return readBytes(buildFolder + u'/' + path); });
    }
    if (!found)
        return SyncTexOutcome::NotFound;
    place = *found;
    return SyncTexOutcome::Found;
}

} // namespace loom
