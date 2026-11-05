// The `loom` command (Specs/m1-core.md, "The `loom` command"). It knows `--version`, `help`,
// `new-project`, the statement and proof subcommands, the check subcommands, `refute`, `status`,
// `graph`, the paper subcommands, `validate`, `install-hooks` and `build`.

#include "BuildCommands.h"
#include "CheckCommands.h"
#include "Cli.h"
#include "PaperCommands.h"
#include "StatementCommands.h"
#include "StatusCommands.h"
#include "ValidateCommands.h"

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

/// The problem for a word that isn't one of `loom`'s subcommands. No side effects.
QString unknownSubcommand(const QString& subcommand)
{
    return QStringLiteral("`%1` isn't a loom subcommand. `loom help` lists them.").arg(subcommand);
}

/// Runs `loom help [<subcommand>]`: the usage text, or one subcommand's, on stdout, exit 0
/// (Specs/m1-core.md, "The `loom` command", A11). With `--json` it prints `{"usage": "…"}`.
/// @param args The arguments after `help`, with the common options taken out.
/// @returns 0, or 2 for more than one argument or one that isn't a subcommand.
/// Side effects: prints.
int runHelp(const QStringList& args, bool json)
{
    if (args.size() > 1)
        return usageError(QStringLiteral("`loom help` takes at most one subcommand."), json);
    const std::optional<QString> text = args.isEmpty() ? usageText() : subcommandUsage(args[0]);
    if (!text)
        return usageError(unknownSubcommand(args[0]), json);
    write(stdout, json ? QStringLiteral("{\"usage\": %1}\n").arg(jsonString(*text)) : *text + u'\n');
    return exitDone;
}

/// Runs `loom new-project <folder> --name <name> [--idea <text>]`.
/// @param args The arguments after `new-project`, with `--json` already taken out.
/// @param json Whether to print JSON.
/// @returns 0 created, 1 refused, 2 wrong usage, 3 git missing or a step failed.
/// Side effects: creates the project (see loom::createProject); prints its path, or the problem.
int runNewProject(const QStringList& args, bool json)
{
    const QString name = QStringLiteral("new-project");
    QString error;
    const std::optional<Args> parsed =
        parseArgs(args, name, {1, {"--name", "--idea"}, {}, {}}, error, QStringLiteral("the folder to create"));
    if (!parsed)
        return usageError(error, json);
    if (const QString problem = missing(*parsed, name, {"--name"}); !problem.isEmpty())
        return usageError(problem, json);
    loom::NewProjectRequest request;
    request.folder = parsed->positional[0];
    request.name = *parsed->value("--name");
    request.idea = parsed->value("--idea").value_or(QString());
    if (request.name.isEmpty())
        return usageError(QStringLiteral("`--name` can't be empty."), json);

    // The hook falls back on this `loom` when `loom` isn't on PATH.
    request.loomProgram = QCoreApplication::applicationFilePath();
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
    bool projectGiven = false;
    QString error;
    if (!takeCommonOptions(args, context, projectGiven, error))
        return usageError(error, context.json);
    const bool json = context.json;

    if (!args.isEmpty() && args[0] == u"--version") {
        if (args.size() > 1)
            return usageError(QStringLiteral("`loom --version` takes no arguments."), json);
        const QString version = QString::fromLatin1(loom::versionString());
        write(stdout, json ? QStringLiteral("{\"version\": \"%1\"}\n").arg(version) : QStringLiteral("loom %1\n").arg(version));
        return exitDone;
    }
    if (args.isEmpty())
        return usageError(QStringLiteral("`loom` needs a subcommand."), json);

    const QString subcommand = args.takeFirst();
    if (subcommand == u"help")
        return runHelp(args, json);
    if (subcommand == u"new-project") {
        // `--project` names an existing project; the folder to create is new-project's argument.
        if (projectGiven)
            return usageError(QStringLiteral("`loom new-project` doesn't take `--project`: give the folder to create as its first argument."),
                              json);
        return runNewProject(args, json);
    }
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
    if (subcommand == u"validate")
        return runValidate(args, context);
    if (subcommand == u"install-hooks")
        return runInstallHooks(args, context);
    if (subcommand == u"build")
        return runBuild(args, context);
    return usageError(unknownSubcommand(subcommand), json);
}
