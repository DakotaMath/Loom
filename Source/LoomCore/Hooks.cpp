#include "LoomCore/Hooks.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace loom {

namespace {

/// The first words of the comment that marks a hook as Loom's; see Templates/hooks/pre-commit.
constexpr char loomHookMark[] = "# Written by Loom (`loom install-hooks`)";

/// A failure, as a result.
HooksResult failure(HooksOutcome outcome, const char* code, const QString& message)
{
    HooksResult result;
    result.outcome = outcome;
    result.code = QString::fromLatin1(code);
    result.message = message;
    return result;
}

/// Quotes text for `sh` in single quotes, where nothing is special except `'` itself.
QByteArray shellQuoted(const QString& text)
{
    QByteArray quoted = text.toUtf8();
    quoted.replace("'", "'\\''");
    return '\'' + quoted + '\'';
}

/// Why a git step failed, for a message: its stderr, else its exit code.
QString whyGitFailed(const GitRun& run)
{
    QString why = run.errors.trimmed();
    while (why.endsWith(u'.'))
        why.chop(1);
    if (why.isEmpty())
        why = run.started ? QStringLiteral("it exited with code %1").arg(run.exitCode) : QStringLiteral("it didn't run");
    return why;
}

} // namespace

QByteArray preCommitHook(const QString& loomProgram)
{
    QFile file(QStringLiteral(":/hooks/pre-commit"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QByteArray hook = file.readAll();
    hook.replace("\r\n", "\n");
    // Git's `sh` reads `C:/…` paths, and `/` needs no escaping.
    hook.replace("@LOOM@", shellQuoted(QDir::fromNativeSeparators(loomProgram)));
    return hook;
}

bool isLoomHook(const QByteArray& hook)
{
    return hook.contains(loomHookMark);
}

HooksResult installHooks(const QString& root, const QString& loomProgram, const GitTool& git)
{
    const GitRun where = runGit(git, {QStringLiteral("rev-parse"), QStringLiteral("--git-path"), QStringLiteral("hooks/pre-commit")}, root);
    if (!where.started)
        return failure(HooksOutcome::CantWork, "git-failed",
            QStringLiteral("Loom needs git, and `git` didn't run. Install Git for Windows and try again."));
    if (!where.ok() || where.output.trimmed().isEmpty())
        return failure(HooksOutcome::CantWork, "not-a-repository",
            QStringLiteral("`%1` isn't a git repository, so Loom has nowhere to install its hook.").arg(QDir::toNativeSeparators(root)));

    HooksResult result;
    result.hookPath = QDir::cleanPath(QDir(root).absoluteFilePath(where.output.trimmed()));
    const QString nativeHook = QDir::toNativeSeparators(result.hookPath);

    const QByteArray hook = preCommitHook(loomProgram);
    if (hook.isEmpty())
        return failure(HooksOutcome::CantWork, "write-failed", QStringLiteral("This build of Loom has no pre-commit hook to install."));

    // A hook Loom didn't write is kept, under a name git doesn't run.
    QFile existing(result.hookPath);
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly))
            return failure(HooksOutcome::CantWork, "write-failed", QStringLiteral("Loom couldn't read `%1`.").arg(nativeHook));
        const bool ours = isLoomHook(existing.readAll());
        existing.close();
        if (!ours) {
            const QString moved = result.hookPath + QStringLiteral(".before-loom");
            if (QFileInfo::exists(moved))
                return failure(HooksOutcome::Refused, "hook-backup-exists",
                    QStringLiteral("`%1` wasn't written by Loom, and `%2` already exists, so Loom has nowhere to keep it. "
                                   "Move one of them somewhere else and run `loom install-hooks` again.")
                        .arg(nativeHook, QDir::toNativeSeparators(moved)));
            if (!QFile::rename(result.hookPath, moved))
                return failure(HooksOutcome::CantWork, "write-failed",
                    QStringLiteral("Loom couldn't rename `%1` to `%2`.").arg(nativeHook, QDir::toNativeSeparators(moved)));
            result.movedHook = moved;
        }
    }

    if (!QDir().mkpath(QFileInfo(result.hookPath).absolutePath()))
        return failure(HooksOutcome::CantWork, "write-failed", QStringLiteral("Loom couldn't create the folder for `%1`.").arg(nativeHook));
    QSaveFile file(result.hookPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(hook) != hook.size() || !file.commit())
        return failure(HooksOutcome::CantWork, "write-failed", QStringLiteral("Loom couldn't write `%1`.").arg(nativeHook));
    // Git runs only executable hooks (Windows has no such bit, and Git for Windows runs it anyway).
    QFile::setPermissions(result.hookPath, QFile::permissions(result.hookPath) | QFileDevice::ExeOwner
                                               | QFileDevice::ExeGroup | QFileDevice::ExeOther);

    const QList<QStringList> settings = {
        {QStringLiteral("merge.loom-checks.name"), QStringLiteral("Loom check history")},
        {QStringLiteral("merge.loom-checks.driver"), QStringLiteral("loom merge-checks %O %A %B")},
    };
    for (const QStringList& setting : settings) {
        const GitRun run = runGit(git, QStringList{QStringLiteral("config"), QStringLiteral("--local")} + setting, root);
        if (!run.ok())
            return failure(HooksOutcome::CantWork, "git-failed",
                QStringLiteral("Loom installed its hook, but `git config --local %1` failed: %2.").arg(setting[0], whyGitFailed(run)));
    }

    result.outcome = HooksOutcome::Installed;
    return result;
}

} // namespace loom
