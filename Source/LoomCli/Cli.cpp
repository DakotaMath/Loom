#include "Cli.h"

#include "LoomCore/Edits.h"
#include "LoomCore/FrontMatter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_WIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iterator>
#else
#include <iostream>
#include <string>
#include <unistd.h>
#endif

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

QString jsonOptional(const QString& text)
{
    return text.isEmpty() ? QStringLiteral("null") : jsonString(text);
}

QString jsonIndex(const std::optional<qsizetype>& index)
{
    return index ? QString::number(*index) : QStringLiteral("null");
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
        "  loom install-hooks\n"
        "  loom new-tag <tag> --kind equation|technique|other --title <title>\n"
        "  loom new-statement --kind <kind> --title <title> (--body <text> | --body-file <path>)\n"
        "                     [--tag <tag>]... [--stated-using <id>]... [--origin <origin>]\n"
        "  loom edit-statement <id> [--title <title>] [--kind <kind>] [--body-file <path>]\n"
        "                     [--add-tag <tag>]... [--remove-tag <tag>]...\n"
        "                     [--add-stated-using <id>]... [--remove-stated-using <id>]...\n"
        "                     [--superseded-by <id>] [--verified] [--outdate-checks]\n"
        "  loom new-proof <statement-id> --level sketch|heuristic|complete|failed\n"
        "                     (--body <text> | --body-file <path>) [--premise <id>]...\n"
        "  loom edit-proof <proof-id> [--level <level>] [--body-file <path>]\n"
        "                     [--add-premise <id>]... [--remove-premise <id>]... [--verified]\n"
        "                     [--outdate-checks]\n"
        "  loom fingerprint <proof-id>\n"
        "  loom add-check <proof-id> --by dakota|agent:<model> --verdict pass|needs-changes|fail\n"
        "                     [--changes <text> | --changes-file <path>] [--fingerprint <fp>]\n"
        "                     [--record <path>] [--note <path>]\n"
        "  loom merge-checks <base> <ours> <theirs>\n"
        "  loom refute <id> --by <proof-id>\n"
        "  loom status [<id>]\n"
        "  loom graph --json\n"
        "  loom new-paper <draft.toml>\n"
        "  loom new-stub <key> --kind <kind> --gist <text> --version <version-id> --at <locator>\n"
        "                     --by dakota|agent:<model> [--title <title>] [--stated-using <id>]...\n"
        "                     [--paper-premise <id>]...\n"
        "  loom rekey <key>\n"
        "  loom build-bib [--out <path>]\n"
        "  loom validate [--staged]\n"
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

std::optional<Args> parseArgs(const QStringList& args, const QString& name, const ArgSpec& spec, QString& error,
                              const QString& positionalName)
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
        const QString wanted = spec.positionals == 0 ? QStringLiteral("no other arguments")
                             : spec.positionals == 1 ? (positionalName.isEmpty() ? QStringLiteral("an ID") : positionalName)
                                                     : QStringLiteral("%1 arguments").arg(spec.positionals);
        error = QStringLiteral("`loom %1` needs %2.").arg(name, wanted);
        return std::nullopt;
    }
    return parsed;
}

std::optional<QString> findProject(const Context& context, QString& message)
{
    if (!context.project.isEmpty()) {
        // As openProject() decides: a loom.toml that is a link or can't be read is still the
        // project's, and gets its own message when it is opened.
        const QString folder = QDir::cleanPath(QFileInfo(context.project).absoluteFilePath());
        const QFileInfo toml(folder + QStringLiteral("/loom.toml"));
        if (toml.exists() || toml.isSymbolicLink())
            return folder;
        message = QStringLiteral("`%1` isn't a Loom project: it has no loom.toml.").arg(QDir::toNativeSeparators(folder));
        return std::nullopt;
    }
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

QString listed(const QStringList& items)
{
    if (items.size() < 2)
        return items.join(QString());
    return items.first(items.size() - 1).join(u", ") + QStringLiteral(" and ") + items.last();
}

bool inputIsTerminal()
{
#ifdef Q_OS_WIN
    // _isatty() is true for any character device, `NUL` included; only a console has a mode.
    DWORD mode = 0;
    return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

std::optional<QString> readTerminalLine()
{
    QString line;
#ifdef Q_OS_WIN
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    wchar_t buffer[256];
    while (!line.endsWith(u'\n')) {
        DWORD read = 0;
        if (!ReadConsoleW(input, buffer, static_cast<DWORD>(std::size(buffer)), &read, nullptr) || read == 0)
            break;
        line += QString::fromWCharArray(buffer, static_cast<qsizetype>(read));
        if (line.contains(QChar(0x1a))) // Ctrl+Z, the console's end of input.
            return std::nullopt;
    }
    if (line.isEmpty())
        return std::nullopt;
#else
    std::string text;
    if (!std::getline(std::cin, text))
        return std::nullopt;
    line = QString::fromUtf8(text);
#endif
    while (line.endsWith(u'\n') || line.endsWith(u'\r'))
        line.chop(1);
    return line;
}

bool requireTerminal(const QString& what, bool json, int& exitCode)
{
    if (inputIsTerminal())
        return true;
    exitCode = fail(exitRefused, QStringLiteral("dakota-only"), QStringLiteral("Only Dakota can %1, and only at a terminal.").arg(what), json);
    return false;
}

bool askDakota(const QString& what, bool json, int& exitCode)
{
    write(stderr, QStringLiteral("This is Dakota's decision: %1. Type yes to go ahead: ").arg(what));
    const std::optional<QString> answer = readTerminalLine();
    if (answer && answer->trimmed() == u"yes")
        return true;
    exitCode = fail(exitRefused, QStringLiteral("not-confirmed"), QStringLiteral("Nothing was done: the answer wasn't yes."), json);
    return false;
}

std::optional<QByteArray> readTextFile(const QString& path, bool json, int& exitCode)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        exitCode = fail(exitCantWork, QStringLiteral("unreadable"),
            QStringLiteral("Loom couldn't read `%1`.").arg(QDir::toNativeSeparators(path)), json);
        return std::nullopt;
    }
    QByteArray bytes = file.readAll();
    if (const std::optional<qsizetype> bad = loom::firstInvalidUtf8(bytes)) {
        exitCode = fail(exitUsage, QStringLiteral("not-utf8"),
            QStringLiteral("`%1` isn't valid UTF-8 (the first bad byte is at offset %2). Save it as UTF-8.")
                .arg(QDir::toNativeSeparators(path)).arg(*bad), json);
        return std::nullopt;
    }
    if (bytes.startsWith("\xEF\xBB\xBF"))
        bytes.remove(0, 3);
    return bytes;
}

std::optional<QByteArray> readBody(const Args& args, bool json, int& exitCode)
{
    const std::optional<QString> body = args.value(QStringLiteral("--body"));
    const std::optional<QString> bodyFile = args.value(QStringLiteral("--body-file"));
    if (body)
        return body->toUtf8();
    if (!bodyFile)
        return QByteArray();
    return readTextFile(*bodyFile, json, exitCode);
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

std::optional<std::pair<Args, QString>> start(const QStringList& args, const QString& name, const ArgSpec& spec,
                                              const Context& context, int& exitCode, const QString& positionalName)
{
    QString error;
    std::optional<Args> parsed = parseArgs(args, name, spec, error, positionalName);
    if (!parsed) {
        exitCode = usageError(error, context.json);
        return std::nullopt;
    }
    std::optional<QString> root = findProject(context, error);
    if (!root) {
        exitCode = fail(exitCantWork, QStringLiteral("not-a-project"), error, context.json);
        return std::nullopt;
    }
    return std::pair{std::move(*parsed), *root};
}

QString missing(const Args& args, const QString& name, const QStringList& required)
{
    for (const QString& option : required) {
        if (!args.value(option))
            return QStringLiteral("`loom %1` needs `%2`.").arg(name, option);
    }
    return {};
}

} // namespace cli
