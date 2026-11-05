#include "LoomCore/LatexCommands.h"

#include <QHash>
#include <QRegularExpression>

namespace loom {

const QStringList& validationCommandNames()
{
    static const QStringList names = {QStringLiteral("ref"), QStringLiteral("paperref"), QStringLiteral("cite"),
                                      QStringLiteral("label")};
    return names;
}

qsizetype commentStart(QStringView line)
{
    for (qsizetype at = 0; at < line.size(); ++at) {
        if (line[at] != u'%')
            continue;
        qsizetype slashes = 0;
        while (at - slashes > 0 && line[at - slashes - 1] == u'\\')
            ++slashes;
        if (slashes % 2 == 0)
            return at;
    }
    return line.size();
}

std::vector<LatexCommand> latexCommands(const QByteArray& body, int firstLine, const QStringList& names)
{
    // One expression per set of names, built once per thread: the names are fixed strings of the
    // callers, and the app validates and builds on different threads at once.
    thread_local QHash<QString, QRegularExpression> expressions;
    const QString key = names.join(u'|');
    auto found = expressions.find(key);
    if (found == expressions.end()) {
        QStringList escaped;
        for (const QString& name : names)
            escaped << QRegularExpression::escape(name);
        found = expressions.insert(key, QRegularExpression(
            QStringLiteral(R"(\\(%1)(?![A-Za-z@*])\s*(?:\[[^\]]*\]\s*)*\{([^{}]*)\})").arg(escaped.join(u'|'))));
    }
    const QRegularExpression& command = *found;

    std::vector<LatexCommand> commands;
    const QStringList lines = QString::fromUtf8(body).split(u'\n');
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const QStringView line = QStringView(lines[i]).first(commentStart(lines[i]));
        for (const QRegularExpressionMatch& match : command.globalMatchView(line))
            commands.push_back({match.captured(1), match.captured(2).trimmed(), firstLine + static_cast<int>(i),
                                match.capturedStart(2), match.capturedLength(2)});
    }
    return commands;
}

} // namespace loom
