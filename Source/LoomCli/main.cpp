// The `loom` command (Specs/m1-core.md, "The `loom` command"). It knows `--version` and
// `new-project`; the other subcommands arrive with their build issues (#5 onwards).

#include "LoomCore/NewProject.h"
#include "LoomCore/Version.h"

#include <QCoreApplication>
#include <QDir>
#include <QString>
#include <QStringList>

#include <cstdio>

#ifdef Q_OS_WIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {

// Exit codes, from the spec's table.
constexpr int exitDone = 0;
constexpr int exitRefused = 1;
constexpr int exitUsage = 2;
constexpr int exitCantWork = 3;

constexpr auto usage =
    "Usage:\n"
    "  loom --version [--json]\n"
    "  loom new-project <folder> --name <name> [--idea <text>] [--json]";

/// Writes text to a stream as UTF-8.
/// @param stream stdout or stderr.
/// @param text   The text, written as is (add the line break yourself).
void write(std::FILE* stream, const QString& text)
{
    const QByteArray bytes = text.toUtf8();
    std::fwrite(bytes.constData(), 1, static_cast<std::size_t>(bytes.size()), stream);
    std::fflush(stream);
}

/// Writes a JSON string literal: quotes, with `"`, `\` and control characters escaped.
/// @param text The string's value.
/// @returns The JSON text. No side effects.
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

/// Prints how to use `loom` to stderr.
/// @returns exitUsage, so callers can `return printUsage();`.
int printUsage()
{
    write(stderr, QString::fromLatin1(usage) + u'\n');
    return exitUsage;
}

/// Reports a failure: the message on stderr and, with `--json`, the error object on stdout
/// (Specs/m1-core.md, "The `loom` command").
/// @returns `exitCode`, so callers can `return fail(...);`.
int fail(int exitCode, const QString& code, const QString& message, bool json)
{
    write(stderr, message + u'\n');
    if (json)
        write(stdout, QStringLiteral("{\"error\": {\"code\": %1, \"message\": %2}}\n").arg(jsonString(code), jsonString(message)));
    return exitCode;
}

/// Runs `loom new-project <folder> --name <name> [--idea <text>]`.
/// @param args The arguments after `new-project`, with `--json` already taken out.
/// @param json Whether to print JSON.
/// @returns 0 created, 1 refused, 2 wrong usage, 3 git missing or a step failed.
/// Side effects: creates the project (see loom::createProject); prints its path, or the problem.
int runNewProject(const QStringList& args, bool json)
{
    loom::NewProjectRequest request;
    bool haveFolder = false;
    bool haveName = false;
    for (qsizetype i = 0; i < args.size(); ++i) {
        const QString& arg = args[i];
        if (arg == u"--name" || arg == u"--idea") {
            if (i + 1 >= args.size())
                return fail(exitUsage, QStringLiteral("usage"), QStringLiteral("`%1` needs a value.").arg(arg), json);
            (arg == u"--name" ? request.name : request.idea) = args[++i];
            haveName = haveName || arg == u"--name";
        } else if (arg.startsWith(u"--") || haveFolder) {
            return fail(exitUsage, QStringLiteral("usage"),
                QStringLiteral("`loom new-project` doesn't take `%1`.\n").arg(arg) + QString::fromLatin1(usage), json);
        } else {
            request.folder = arg;
            haveFolder = true;
        }
    }
    if (!haveFolder || !haveName)
        return fail(exitUsage, QStringLiteral("usage"),
            QStringLiteral("`loom new-project` needs a folder and `--name`.\n") + QString::fromLatin1(usage), json);
    if (request.name.isEmpty())
        return fail(exitUsage, QStringLiteral("usage"), QStringLiteral("`--name` can't be empty."), json);

    const loom::NewProjectResult result = loom::createProject(request);
    switch (result.outcome) {
    case loom::NewProjectOutcome::Created: {
        const QString path = QDir::toNativeSeparators(result.path);
        write(stdout, json ? QStringLiteral("{\"path\": %1}\n").arg(jsonString(path)) : path + u'\n');
        return exitDone;
    }
    case loom::NewProjectOutcome::Refused:
        return fail(exitRefused, result.code, result.message, json);
    case loom::NewProjectOutcome::CantWork:
        break;
    }
    return fail(exitCantWork, result.code, result.message, json);
}

#ifdef Q_OS_WIN
/// Makes the Windows console show `loom`'s UTF-8 output for as long as `loom` runs, then puts
/// back the code page it had, so the user's console is left as it was.
class Utf8Console {
public:
    Utf8Console() : m_previous(GetConsoleOutputCP()) { SetConsoleOutputCP(CP_UTF8); }
    ~Utf8Console() { SetConsoleOutputCP(m_previous); }
    Utf8Console(const Utf8Console&) = delete;
    Utf8Console& operator=(const Utf8Console&) = delete;

private:
    UINT m_previous;
};
#endif

} // namespace

/// Runs `loom`.
/// @param argc, argv The command line. Arguments are read through QCoreApplication, which on
///                   Windows takes them from the Unicode command line, so non-ASCII names survive.
/// @returns An exit code from the spec's table: 0 done, 1 refused, 2 wrong usage, 3 can't work here.
/// Side effects: those of the subcommand; writes results to stdout and messages to stderr, as UTF-8.
int main(int argc, char* argv[])
{
#ifdef Q_OS_WIN
    const Utf8Console console;
#endif
    const QCoreApplication app(argc, argv);
    QStringList args = QCoreApplication::arguments().mid(1);

    const bool json = args.removeAll(QStringLiteral("--json")) > 0;

    if (args.size() == 1 && args[0] == u"--version") {
        const QString version = QString::fromLatin1(loom::versionString());
        write(stdout, json ? QStringLiteral("{\"version\": \"%1\"}\n").arg(version) : QStringLiteral("loom %1\n").arg(version));
        return exitDone;
    }
    if (!args.isEmpty() && args[0] == u"new-project")
        return runNewProject(args.mid(1), json);

    return printUsage();
}
