// The `loom` command (Specs/m1-core.md, "The `loom` command"). It knows `--version`,
// `new-project`, the statement and proof subcommands, the check subcommands, `refute`, `status`,
// `graph` and the paper subcommands; `validate` and `install-hooks` arrive with #9.

#include "CheckCommands.h"
#include "Cli.h"
#include "PaperCommands.h"
#include "StatementCommands.h"
#include "StatusCommands.h"

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

using namespace cli;

/// Prints how to use `loom` to stderr.
/// @returns exitUsage, so callers can `return printUsage();`.
int printUsage()
{
    write(stderr, usageText() + u'\n');
    return exitUsage;
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
                QStringLiteral("`loom new-project` doesn't take `%1`.\n").arg(arg) + usageText(), json);
        } else {
            request.folder = arg;
            haveFolder = true;
        }
    }
    if (!haveFolder || !haveName)
        return fail(exitUsage, QStringLiteral("usage"),
            QStringLiteral("`loom new-project` needs a folder and `--name`.\n") + usageText(), json);
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

    Context context;
    context.json = args.removeAll(QStringLiteral("--json")) > 0;
    if (const qsizetype at = args.indexOf(QStringLiteral("--project")); at >= 0) {
        if (at + 1 >= args.size())
            return usageError(QStringLiteral("`--project` needs a folder."), context.json);
        context.project = args.takeAt(at + 1);
        args.removeAt(at);
    }
    const bool json = context.json;

    if (args.size() == 1 && args[0] == u"--version") {
        const QString version = QString::fromLatin1(loom::versionString());
        write(stdout, json ? QStringLiteral("{\"version\": \"%1\"}\n").arg(version) : QStringLiteral("loom %1\n").arg(version));
        return exitDone;
    }
    if (args.isEmpty())
        return printUsage();

    const QString subcommand = args.takeFirst();
    if (subcommand == u"new-project")
        return runNewProject(args, json);
    if (subcommand == u"new-tag")
        return runNewTag(args, context);
    if (subcommand == u"new-statement")
        return runNewStatement(args, context);
    if (subcommand == u"edit-statement")
        return runEditStatement(args, context);
    if (subcommand == u"new-proof")
        return runNewProof(args, context);
    if (subcommand == u"edit-proof")
        return runEditProof(args, context);
    if (subcommand == u"fingerprint")
        return runFingerprint(args, context);
    if (subcommand == u"add-check")
        return runAddCheck(args, context);
    if (subcommand == u"merge-checks")
        return runMergeChecks(args, context);
    if (subcommand == u"refute")
        return runRefute(args, context);
    if (subcommand == u"status")
        return runStatus(args, context);
    if (subcommand == u"graph")
        return runGraph(args, context);
    if (subcommand == u"new-paper")
        return runNewPaper(args, context);
    if (subcommand == u"new-stub")
        return runNewStub(args, context);
    if (subcommand == u"rekey")
        return runRekey(args, context);
    if (subcommand == u"build-bib")
        return runBuildBib(args, context);
    return printUsage();
}
