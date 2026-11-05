#include "LoomCore/TexLog.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <tuple>

namespace loom {

namespace {

/// A file name as the log gives it, made comparable: `\` as `/`, without quotes or a leading `./`.
QString normalisedFile(QString file)
{
    file.replace(u'\\', u'/');
    if (file.size() >= 2 && file.startsWith(u'"') && file.endsWith(u'"'))
        file = file.mid(1, file.size() - 2);
    while (file.startsWith(QStringLiteral("./")))
        file = file.mid(2);
    return file;
}

/// Whether a file is one of the build's own: `main.tex` or a copy under `src/`.
bool isBuildFile(const QString& file)
{
    return file == u"main.tex" || file.startsWith(QStringLiteral("src/"));
}

/// Whether the text after a `(` in the log names a file TeX opened: a path (`./…`, `/…`, `c:/…`)
/// or a name with an extension. Other parentheses (`(badness 10000)`, `(e.g.,`) don't.
bool looksLikeFile(const QString& name)
{
    static const QRegularExpression path(QStringLiteral(R"(^(\.{1,2}/|/|[A-Za-z]:[/\\]))"));
    static const QRegularExpression extension(QStringLiteral(R"(\.[A-Za-z0-9]{1,8}$)"));
    return path.match(name).hasMatch() || extension.match(name).hasMatch();
}

/// The open files, through the log's `(file` … `)` nesting.
struct FileStack {
    /// Each open parenthesis: the file it opened, or an empty string for one that isn't a file.
    QStringList open;

    /// Follows the parentheses of one line. No side effects beyond the stack.
    void follow(const QString& line)
    {
        for (qsizetype j = 0; j < line.size(); ++j) {
            if (line[j] == u')') {
                if (!open.isEmpty())
                    open.removeLast();
                continue;
            }
            if (line[j] != u'(')
                continue;
            qsizetype end = j + 1;
            if (end < line.size() && line[end] == u'"') {
                const qsizetype close = line.indexOf(u'"', end + 1);
                end = close < 0 ? line.size() : close + 1;
            } else {
                while (end < line.size() && !line[end].isSpace() && line[end] != u'(' && line[end] != u')')
                    ++end;
            }
            const QString name = line.mid(j + 1, end - j - 1);
            open << (looksLikeFile(name) ? normalisedFile(name) : QString());
            j = end - 1;
        }
    }

    /// The innermost open file, or an empty string.
    QString current() const
    {
        for (auto it = open.crbegin(); it != open.crend(); ++it) {
            if (!it->isEmpty())
                return *it;
        }
        return {};
    }

    /// The innermost open file of the build, or an empty string.
    QString buildFile() const
    {
        for (auto it = open.crbegin(); it != open.crend(); ++it) {
            if (isBuildFile(*it))
                return *it;
        }
        return {};
    }
};

/// Whether a line is one of TeX's context lines after an error: `l.<n> …` or `<…> …`.
bool isContextLine(const QString& line)
{
    static const QRegularExpression context(QStringLiteral(R"(^(l\.\d+|<[^>]*>)( |$))"));
    return context.match(line).hasMatch();
}

/// The control sequence at the end of an error's first context line, for *Undefined control
/// sequence* (`l.10 $\fracc` → `\fracc`); empty if there's none.
QString culprit(const QString& contextLine)
{
    static const QRegularExpression last(QStringLiteral(R"((\\(?:[A-Za-z@]+|.))\s*$)"));
    const QRegularExpressionMatch m = last.match(contextLine);
    return m.hasMatch() ? m.captured(1) : QString();
}

/// Warnings that aren't worth a problem: end-of-run summaries, and hyperref's note on Loom's own
/// outside references (`\loomoutside`), which are deliberately not links.
bool ignoredWarning(const QString& text)
{
    static const QStringList ignored = {
        QStringLiteral("LaTeX Warning: There were undefined references"),
        QStringLiteral("LaTeX Warning: There were multiply-defined labels"),
        QStringLiteral("LaTeX Warning: Label(s) may have changed"),
        QStringLiteral("LaTeX Warning: Citation(s) may have changed"),
        QStringLiteral("LaTeX Font Warning: Some font shapes were not available"),
        // Follows from a `\cite` of nothing in refs.bib, which `tex-cite-undefined` names.
        QStringLiteral("LaTeX Warning: Empty `thebibliography' environment"),
        QStringLiteral("Package hyperref Warning: Suppressing link with empty target"),
    };
    return std::any_of(ignored.begin(), ignored.end(), [&](const QString& start) { return text.startsWith(start); });
}

} // namespace

std::vector<TexLogEntry> parseTexLog(const QByteArray& log)
{
    static const QRegularExpression fileLineError(QStringLiteral(R"(^((?:[A-Za-z]:)?[^:]*\.[A-Za-z0-9]+):(\d+): (.*)$)"));
    static const QRegularExpression bangError(QStringLiteral(R"(^! (.*)$)"));
    static const QRegularExpression contextNumber(QStringLiteral(R"(^l\.(\d+))"));
    static const QRegularExpression warning(QStringLiteral(R"(^(LaTeX|LaTeX Font|Package (\S+)|Class (\S+)) Warning: (.*)$)"));
    static const QRegularExpression inputLine(QStringLiteral(R"(\s*on input line (\d+)\.\s*$)"));
    static const QRegularExpression reference(QStringLiteral(R"(^LaTeX Warning: Reference [`'‘](.*)['’] on page \S+ undefined)"));
    static const QRegularExpression citation(QStringLiteral(R"(^LaTeX Warning: Citation [`'‘](.*)['’] on page \S+ undefined)"));

    QString text = QString::fromUtf8(log);
    text.remove(u'\r');
    const QStringList lines = text.split(u'\n');
    std::vector<TexLogEntry> entries;
    FileStack files;

    qsizetype i = 0;
    while (i < lines.size()) {
        const QString& line = lines[i];

        // --- Errors -------------------------------------------------------------------------------
        const QRegularExpressionMatch fileError = fileLineError.match(line);
        const QRegularExpressionMatch bang = fileError.hasMatch() ? QRegularExpressionMatch() : bangError.match(line);
        if (fileError.hasMatch() || bang.hasMatch()) {
            TexLogEntry entry;
            entry.severity = Severity::Error;
            entry.code = QStringLiteral("tex-error");
            QString message;
            if (fileError.hasMatch()) {
                entry.file = normalisedFile(fileError.captured(1));
                entry.line = fileError.captured(2).toInt();
                message = fileError.captured(3);
            } else {
                entry.file = files.current();
                message = bang.captured(1);
            }
            entry.buildFile = isBuildFile(entry.file) ? entry.file : files.buildFile();

            // Continuation lines: indented, or `(<package>)`, up to a blank or context line.
            qsizetype next = i + 1;
            while (next < lines.size() && !lines[next].isEmpty() && !isContextLine(lines[next])
                   && (lines[next].startsWith(u' ') || lines[next].startsWith(u'('))) {
                message += u' ' + lines[next].trimmed();
                ++next;
            }
            message = message.simplified();
            if (message.startsWith(QStringLiteral("LaTeX Error: ")))
                message = message.mid(13);

            // The context: the first context line names the culprit; `l.<n>` gives the line.
            qsizetype numberAt = -1;
            QString firstContext;
            for (qsizetype k = next; k < lines.size() && k < next + 16; ++k) {
                if (fileLineError.match(lines[k]).hasMatch() || bangError.match(lines[k]).hasMatch())
                    break;
                if (firstContext.isEmpty() && isContextLine(lines[k]))
                    firstContext = lines[k];
                if (const QRegularExpressionMatch n = contextNumber.match(lines[k]); n.hasMatch()) {
                    numberAt = k;
                    if (entry.line == 0)
                        entry.line = n.captured(1).toInt();
                    break;
                }
            }
            if (message == u"Undefined control sequence.") {
                if (const QString cs = culprit(firstContext); !cs.isEmpty())
                    message = QStringLiteral("Undefined control sequence `%1`.").arg(cs);
            }
            entry.message = message;
            entries.push_back(entry);
            // Skip the context (the `l.<n>` line and the one after it, which continues it): it
            // quotes the document, whose parentheses needn't balance.
            i = numberAt >= 0 ? numberAt + 2 : next;
            continue;
        }

        // --- Boxes: not reported, and their quoted text isn't followed --------------------------
        if (line.startsWith(QStringLiteral("Overfull \\")) || line.startsWith(QStringLiteral("Underfull \\"))) {
            ++i;
            while (i < lines.size() && !lines[i].isEmpty())
                ++i;
            continue;
        }

        // --- Warnings -----------------------------------------------------------------------------
        if (const QRegularExpressionMatch w = warning.match(line); w.hasMatch()) {
            QString name = w.captured(1);
            if (!w.captured(2).isEmpty())
                name = w.captured(2);
            else if (!w.captured(3).isEmpty())
                name = w.captured(3);
            else if (name == u"LaTeX Font")
                name = QStringLiteral("Font");
            const QString prefix = u'(' + name + u')';
            QString joined = line;
            qsizetype next = i + 1;
            while (next < lines.size() && lines[next].startsWith(prefix)) {
                joined += u' ' + lines[next].mid(prefix.size()).trimmed();
                ++next;
            }
            joined = joined.simplified();
            i = next;
            if (ignoredWarning(joined))
                continue;

            TexLogEntry entry;
            entry.severity = Severity::Warning;
            entry.file = files.current();
            entry.buildFile = files.buildFile();
            if (const QRegularExpressionMatch n = inputLine.match(joined); n.hasMatch()) {
                entry.line = n.captured(1).toInt();
                joined.truncate(n.capturedStart(0));
            }
            if (const QRegularExpressionMatch r = reference.match(joined); r.hasMatch()) {
                entry.code = QStringLiteral("tex-ref-undefined");
                entry.message = r.captured(1);
            } else if (const QRegularExpressionMatch c = citation.match(joined); c.hasMatch()) {
                entry.code = QStringLiteral("tex-cite-undefined");
                entry.message = c.captured(1);
            } else {
                entry.code = QStringLiteral("tex-warning");
                entry.message = joined;
            }
            entries.push_back(entry);
            continue;
        }

        files.follow(line);
        ++i;
    }
    return entries;
}

void sortUniqueProblems(std::vector<Problem>& problems)
{
    sortProblems(problems);
    std::vector<Problem> unique;
    for (const Problem& p : problems) {
        const bool seen = std::any_of(unique.begin(), unique.end(), [&](const Problem& q) {
            return std::tie(p.severity, p.code, p.file, p.line, p.message) == std::tie(q.severity, q.code, q.file, q.line, q.message);
        });
        if (!seen)
            unique.push_back(p);
    }
    problems = std::move(unique);
}

std::vector<Problem> mapTexLog(const std::vector<TexLogEntry>& entries, const BuildDocument& document)
{
    // latexmk stops after a pass with errors, before the pass that resolves references, so then
    // every `\ref` and `\cite` may read as undefined: those warnings would be noise.
    const bool hasErrors = std::any_of(entries.begin(), entries.end(), [](const TexLogEntry& e) { return e.severity == Severity::Error; });
    std::vector<Problem> problems;
    for (const TexLogEntry& entry : entries) {
        if (hasErrors && (entry.code == u"tex-ref-undefined" || entry.code == u"tex-cite-undefined"))
            continue;
        QString message = entry.message;
        if (entry.code == u"tex-ref-undefined")
            message = QStringLiteral("`\\ref{%1}` points to nothing in this build or the project.").arg(entry.message);
        else if (entry.code == u"tex-cite-undefined")
            message = QStringLiteral("`\\cite{%1}` isn't a paper of this project, or its entry was left out of refs.bib.")
                          .arg(entry.message);

        Problem problem{entry.severity, entry.code, document.targetFile, 1, message};
        // Where it is: a copy, a line of main.tex, or (inside one of TeX's own files) the build
        // file that was open.
        QString file = entry.file;
        int line = entry.line;
        if (!isBuildFile(file)) {
            file = entry.buildFile;
            line = 0;
        }
        if (file.startsWith(QStringLiteral("src/"))) {
            problem.file = file.mid(4);
            problem.line = std::max(1, line);
        } else if (file == u"main.tex") {
            const GeneratedPart part = line >= 1 && line <= static_cast<int>(document.mainLines.size())
                                           ? document.mainLines[static_cast<std::size_t>(line - 1)]
                                           : GeneratedPart{document.target, document.targetFile};
            problem.file = part.file;
            problem.line = 1;
            if (entry.severity == Severity::Error) {
                QString what = message;
                while (what.endsWith(u'.'))
                    what.chop(1);
                problem.code = QStringLiteral("tex-error-generated");
                problem.message = QStringLiteral("LaTeX stopped in the part Loom writes around %1: %2. This is usually a "
                                                 "problem in the preamble or a macro used in the title.")
                                      .arg(part.id, what);
            }
        }
        problems.push_back(problem);
    }
    sortUniqueProblems(problems);
    return problems;
}

} // namespace loom
