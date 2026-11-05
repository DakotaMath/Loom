#include "Cli.h"

#include "LoomCore/Edits.h"
#include "LoomCore/FrontMatter.h"
#include "LoomCore/ProjectToml.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <vector>

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

namespace {

/// The error object `--json` prints for a failure (Specs/m1-core.md, "The `loom` command"),
/// with its line break. No side effects.
QString jsonError(const QString& code, const QString& message)
{
    return QStringLiteral("{\"error\": {\"code\": %1, \"message\": %2}}\n").arg(jsonString(code), jsonString(message));
}

/// One entry of the usage text.
struct UsageEntry {
    QString subcommand; ///< As typed after `loom`.
    QString lines;      ///< Its lines of the usage text, each ending in a line break.
};

/// The usage text, one entry per subcommand, in the order `loom help` lists them. No side effects.
const std::vector<UsageEntry>& usageEntries()
{
    static const std::vector<UsageEntry> entries = {
        {QStringLiteral("help"), QStringLiteral("  loom help [<subcommand>]\n")},
        {QStringLiteral("--version"), QStringLiteral("  loom --version [--json]\n")},
        {QStringLiteral("new-project"), QStringLiteral("  loom new-project <folder> --name <name> [--idea <text>] [--json]\n")},
        {QStringLiteral("install-hooks"), QStringLiteral("  loom install-hooks\n")},
        {QStringLiteral("new-tag"), QStringLiteral("  loom new-tag <tag> --kind equation|technique|other --title <title>\n")},
        {QStringLiteral("new-statement"),
         QStringLiteral("  loom new-statement --kind <kind> --title <title> (--body <text> | --body-file <path>)\n"
                        "                     [--tag <tag>]... [--stated-using <id>]... [--origin <origin>]\n")},
        {QStringLiteral("edit-statement"),
         QStringLiteral("  loom edit-statement <id> [--title <title>] [--kind <kind>] [--body-file <path>]\n"
                        "                     [--add-tag <tag>]... [--remove-tag <tag>]...\n"
                        "                     [--add-stated-using <id>]... [--remove-stated-using <id>]...\n"
                        "                     [--superseded-by <id>] [--verified] [--outdate-checks]\n")},
        {QStringLiteral("new-proof"),
         QStringLiteral("  loom new-proof <statement-id> --level sketch|heuristic|complete|failed\n"
                        "                     (--body <text> | --body-file <path>) [--premise <id>]...\n")},
        {QStringLiteral("edit-proof"),
         QStringLiteral("  loom edit-proof <proof-id> [--level <level>] [--body-file <path>]\n"
                        "                     [--add-premise <id>]... [--remove-premise <id>]... [--verified]\n"
                        "                     [--outdate-checks]\n")},
        {QStringLiteral("fingerprint"), QStringLiteral("  loom fingerprint <proof-id>\n")},
        {QStringLiteral("add-check"),
         QStringLiteral("  loom add-check <proof-id> --by dakota|agent:<model> --verdict pass|needs-changes|fail\n"
                        "                     [--changes <text> | --changes-file <path>] [--fingerprint <fp>]\n"
                        "                     [--record <path>] [--note <path>]\n")},
        {QStringLiteral("merge-checks"), QStringLiteral("  loom merge-checks <base> <ours> <theirs>\n")},
        {QStringLiteral("refute"), QStringLiteral("  loom refute <id> --by <proof-id>\n")},
        {QStringLiteral("status"), QStringLiteral("  loom status [<id>]\n")},
        {QStringLiteral("graph"), QStringLiteral("  loom graph --json\n")},
        {QStringLiteral("new-paper"), QStringLiteral("  loom new-paper <draft.toml>\n")},
        {QStringLiteral("new-stub"),
         QStringLiteral("  loom new-stub <key> --kind <kind> --gist <text> --version <version-id> --at <locator>\n"
                        "                     --by dakota|agent:<model> [--title <title>] [--stated-using <id>]...\n"
                        "                     [--paper-premise <id>]...\n")},
        {QStringLiteral("rekey"), QStringLiteral("  loom rekey <key>\n")},
        {QStringLiteral("build-bib"), QStringLiteral("  loom build-bib [--out <path>]\n")},
        {QStringLiteral("validate"), QStringLiteral("  loom validate [--staged]\n")},
        {QStringLiteral("build"),
         QStringLiteral("  loom build <id> [--mode alone|premises|chain] [--verbose] [--rev <commit>]\n"
                        "                     [--timeout <seconds>]\n")},
    };
    return entries;
}

/// The last line of every usage text. No side effects.
QString commonOptionsLine()
{
    return QStringLiteral("Every subcommand that works in a project takes --project <folder> and --json.");
}

} // namespace

QString jsonProblem(const loom::Problem& problem)
{
    return QStringLiteral("{\"severity\": %1, \"code\": %2, \"file\": %3, \"line\": %4, \"message\": %5}")
        .arg(jsonString(problem.severity == loom::Severity::Error ? QStringLiteral("error") : QStringLiteral("warning")),
             jsonString(problem.code), jsonString(problem.file), QString::number(problem.line), jsonString(problem.message));
}

int fail(int exitCode, const QString& code, const QString& message, bool json)
{
    write(stderr, message + u'\n');
    if (json)
        write(stdout, jsonError(code, message));
    return exitCode;
}

QString usageText()
{
    QString text = QStringLiteral("Usage:\n");
    for (const UsageEntry& entry : usageEntries())
        text += entry.lines;
    return text + commonOptionsLine();
}

std::optional<QString> subcommandUsage(const QString& subcommand)
{
    for (const UsageEntry& entry : usageEntries()) {
        if (entry.subcommand == subcommand)
            return QStringLiteral("Usage:\n") + entry.lines + commonOptionsLine();
    }
    return std::nullopt;
}

int usageError(const QString& problem, bool json)
{
    write(stderr, problem + u'\n' + usageText() + u'\n');
    if (json)
        write(stdout, jsonError(QStringLiteral("usage"), problem));
    return exitUsage;
}

const QSet<QString>& valuelessOptions()
{
    static const QSet<QString> options = {QStringLiteral("--json"), QStringLiteral("--version"), QStringLiteral("--staged"),
                                          QStringLiteral("--verified"), QStringLiteral("--outdate-checks"),
                                          QStringLiteral("--verbose")};
    return options;
}

bool takeCommonOptions(QStringList& args, Context& context, bool& projectGiven, QString& error)
{
    QStringList rest;
    projectGiven = false;
    error.clear();
    // After a problem it reads on, so that a `--json` later on still gets the error as JSON.
    for (qsizetype i = 0; i < args.size(); ++i) {
        const QString& arg = args[i];
        if (arg == u"--json") {
            context.json = true;
        } else if (arg == u"--project") {
            if (i + 1 >= args.size()) {
                if (error.isEmpty())
                    error = QStringLiteral("`--project` needs a folder.");
                break;
            }
            if (projectGiven && error.isEmpty())
                error = QStringLiteral("`--project` can be given only once.");
            context.project = args[++i];
            projectGiven = true;
        } else {
            rest << arg;
            // The argument after an option that takes one is its value, whatever it looks like;
            // the subcommand's own parseArgs() then decides whether it knows the option.
            if (arg.startsWith(u"--") && !valuelessOptions().contains(arg) && i + 1 < args.size())
                rest << args[++i];
        }
    }
    args = rest;
    return error.isEmpty();
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
                .arg(QDir::toNativeSeparators(path), QString::number(*bad)), json);
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

std::optional<loom::ProjectContents> readProject(const QString& root, bool json, int& exitCode)
{
    exitCode = exitCantWork;
    const loom::OpenedProject project = loom::openProject(root);
    switch (project.status) {
    case loom::OpenStatus::Opened: break;
    case loom::OpenStatus::NotAProject: fail(exitCantWork, QStringLiteral("not-a-project"), project.message, json); return std::nullopt;
    case loom::OpenStatus::Unreadable: fail(exitCantWork, QStringLiteral("unreadable"), project.message, json); return std::nullopt;
    case loom::OpenStatus::FormatTooNew: fail(exitCantWork, QStringLiteral("format-too-new"), project.message, json); return std::nullopt;
    }
    loom::ContentsLoading loading = loom::loadContents(project.root);
    if (!loading.ok) {
        fail(exitCantWork, QStringLiteral("unreadable"), loading.message, json);
        return std::nullopt;
    }
    exitCode = exitDone;
    return std::move(loading.contents);
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
