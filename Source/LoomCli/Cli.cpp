#include "Cli.h"

#include "LoomCore/Edits.h"
#include "LoomCore/FrontMatter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace cli {

void write(std::FILE* stream, const QString& text)
{
    const QByteArray bytes = text.toUtf8();
    std::fwrite(bytes.constData(), 1, static_cast<std::size_t>(bytes.size()), stream);
    std::fflush(stream);
}

QString jsonString(const QString& text)
{
    QString out = QStringLiteral("\"");
    for (const QChar c : text) {
        if (c == u'"' || c == u'\\')
            out += u'\\' + QString(c);
        else if (c == u'\n')
            out += u"\\n";
        else if (c.unicode() < 0x20)
            out += u"\\u" + QString::number(c.unicode(), 16).rightJustified(4, u'0');
        else
            out += c;
    }
    return out + u'"';
}

QString jsonStringArray(const QStringList& items)
{
    QStringList quoted;
    for (const QString& item : items)
        quoted << jsonString(item);
    return u'[' + quoted.join(u", ") + u']';
}

int fail(int exitCode, const QString& code, const QString& message, bool json)
{
    write(stderr, message + u'\n');
    if (json)
        write(stdout, QStringLiteral("{\"error\": {\"code\": %1, \"message\": %2}}\n").arg(jsonString(code), jsonString(message)));
    return exitCode;
}

QString usageText()
{
    return QStringLiteral(
        "Usage:\n"
        "  loom --version [--json]\n"
        "  loom new-project <folder> --name <name> [--idea <text>] [--json]\n"
        "  loom new-tag <tag> --kind equation|technique|other --title <title>\n"
        "  loom new-statement --kind <kind> --title <title> (--body <text> | --body-file <path>)\n"
        "                     [--tag <tag>]... [--stated-using <id>]... [--origin <origin>]\n"
        "  loom edit-statement <id> [--title <title>] [--kind <kind>] [--body-file <path>]\n"
        "                     [--add-tag <tag>]... [--remove-tag <tag>]...\n"
        "                     [--add-stated-using <id>]... [--remove-stated-using <id>]...\n"
        "                     [--superseded-by <id>] [--verified]\n"
        "  loom new-proof <statement-id> --level sketch|heuristic|complete|failed\n"
        "                     (--body <text> | --body-file <path>) [--premise <id>]...\n"
        "  loom edit-proof <proof-id> [--level <level>] [--body-file <path>]\n"
        "                     [--add-premise <id>]... [--remove-premise <id>]... [--verified]\n"
        "Every subcommand that works in a project takes --project <folder> and --json.");
}

int usageError(const QString& problem, bool json)
{
    return fail(exitUsage, QStringLiteral("usage"), problem + u'\n' + usageText(), json);
}

std::optional<QString> Args::value(const QString& option) const
{
    if (const auto it = single.constFind(option); it != single.constEnd())
        return *it;
    return std::nullopt;
}

std::optional<Args> parseArgs(const QStringList& args, const QString& name, const ArgSpec& spec, QString& error)
{
    Args parsed;
    for (qsizetype i = 0; i < args.size(); ++i) {
        const QString& arg = args[i];
        if (spec.flags.contains(arg)) {
            parsed.flags.insert(arg);
        } else if (spec.single.contains(arg) || spec.repeated.contains(arg)) {
            if (i + 1 >= args.size()) {
                error = QStringLiteral("`%1` needs a value.").arg(arg);
                return std::nullopt;
            }
            const QString& value = args[++i];
            if (spec.repeated.contains(arg)) {
                parsed.repeated[arg] << value;
            } else if (parsed.single.contains(arg)) {
                error = QStringLiteral("`%1` can be given only once.").arg(arg);
                return std::nullopt;
            } else {
                parsed.single.insert(arg, value);
            }
        } else if (arg.startsWith(u"--") || parsed.positional.size() >= spec.positionals) {
            error = QStringLiteral("`loom %1` doesn't take `%2`.").arg(name, arg);
            return std::nullopt;
        } else {
            parsed.positional << arg;
        }
    }
    if (parsed.positional.size() != spec.positionals) {
        error = QStringLiteral("`loom %1` needs %2.").arg(name, spec.positionals == 1 ? QStringLiteral("an ID") : QStringLiteral("no other arguments"));
        return std::nullopt;
    }
    return parsed;
}

std::optional<QString> findProject(const Context& context, QString& message)
{
    if (!context.project.isEmpty())
        return QFileInfo(context.project).absoluteFilePath();
    QDir folder(QDir::currentPath());
    while (true) {
        if (QFileInfo(folder.filePath(QStringLiteral("loom.toml"))).isFile())
            return folder.absolutePath();
        if (!folder.cdUp())
            break;
    }
    message = QStringLiteral("There's no Loom project here: no loom.toml in `%1` or any folder above it. "
                             "Run loom inside a project, or give --project <folder>.")
                  .arg(QDir::toNativeSeparators(QDir::currentPath()));
    return std::nullopt;
}

std::optional<QByteArray> readBody(const Args& args, bool json, int& exitCode)
{
    const std::optional<QString> body = args.value(QStringLiteral("--body"));
    const std::optional<QString> bodyFile = args.value(QStringLiteral("--body-file"));
    if (body)
        return body->toUtf8();
    if (!bodyFile)
        return QByteArray();

    QFile file(*bodyFile);
    if (!file.open(QIODevice::ReadOnly)) {
        exitCode = fail(exitCantWork, QStringLiteral("unreadable"),
            QStringLiteral("Loom couldn't read `%1`.").arg(QDir::toNativeSeparators(*bodyFile)), json);
        return std::nullopt;
    }
    const QByteArray bytes = file.readAll();
    if (const std::optional<qsizetype> bad = loom::firstInvalidUtf8(bytes)) {
        exitCode = fail(exitUsage, QStringLiteral("not-utf8"),
            QStringLiteral("`%1` isn't valid UTF-8 (the first bad byte is at offset %2). Save it as UTF-8.")
                .arg(QDir::toNativeSeparators(*bodyFile)).arg(*bad), json);
        return std::nullopt;
    }
    return bytes;
}

int report(const loom::ChangeResult& result, bool json, const QString& text, const QString& jsonFields)
{
    switch (result.outcome) {
    case loom::ChangeOutcome::Done:
        write(stdout, json ? u'{' + jsonFields + QStringLiteral("}\n") : text + u'\n');
        return exitDone;
    case loom::ChangeOutcome::Refused:
        return fail(exitRefused, result.code, result.message, json);
    case loom::ChangeOutcome::CantWork:
        break;
    }
    return fail(exitCantWork, result.code, result.message, json);
}

} // namespace cli
