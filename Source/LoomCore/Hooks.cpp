#include "LoomCore/Hooks.h"

#include "LoomCore/ProjectToml.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <filesystem>
#include <system_error>

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
        why = run.started    ? QStringLiteral("it exited with code %1").arg(run.exitCode)
            : run.timedOut ? QStringLiteral("it didn't finish in time")
                           : QStringLiteral("it didn't run");
    return why;
}

/// `path`, made absolute against `base`, in the form used to compare it with another path:
/// every link and junction in the part that exists resolved, with `/`. Git and Qt can spell one
/// folder differently (a link, a drive letter's case, a short 8.3 name), and
/// std::filesystem::weakly_canonical() gives the one final path Windows knows.
/// @returns The resolved path; the absolute path as it is if it can't be resolved. No side effects.
QString resolvedPath(const QString& base, const QString& path)
{
    const QString absolute = QDir::cleanPath(QDir(base).absoluteFilePath(path));
    std::error_code error;
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(QFileInfo(absolute).filesystemAbsoluteFilePath(), error);
    if (error)
        return absolute;
    return QDir::cleanPath(QFileInfo(resolved).absoluteFilePath());
}

/// How file names compare: ignoring case on Windows, where the file system does.
constexpr Qt::CaseSensitivity fileNameCase =
#ifdef Q_OS_WIN
    Qt::CaseInsensitive;
#else
    Qt::CaseSensitive;
#endif

} // namespace

QString hooksPathOutsideMessage(const QString& folder)
{
    return QStringLiteral("Git runs hooks from `%1` (`core.hooksPath`), outside this project, so Loom can't install its "
                          "commit guard there. Run `git config --local core.hooksPath .git/hooks` here, then `loom "
                          "install-hooks` again.")
        .arg(QDir::toNativeSeparators(folder));
}

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
    // A1: only a Loom project at the top of its own repository, and only into that repository's
    // own git directory. All three are checked before anything is written.
    const OpenedProject project = openProject(root);
    if (project.status == OpenStatus::NotAProject)
        return failure(HooksOutcome::CantWork, "not-a-project", project.message);

    const QString nativeRoot = QDir::toNativeSeparators(project.root);
    const QStringList topLevel = {QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel")};
    const GitRun top = runGit(git, topLevel, root);
    if (!top.started) {
        HooksResult result = failure(HooksOutcome::CantWork, "git-failed", gitDidntRunMessage(top, topLevel));
        result.code = gitDidntRunCode(top, result.code);
        return result;
    }
    if (!top.ok() || top.output.trimmed().isEmpty())
        return failure(HooksOutcome::CantWork, "not-a-repository",
            QStringLiteral("`%1` isn't a git repository, so Loom has nowhere to install its hook.").arg(nativeRoot));
    if (resolvedPath(root, top.output.trimmed()).compare(resolvedPath(root, QStringLiteral(".")), fileNameCase) != 0)
        return failure(HooksOutcome::CantWork, "hooks-not-top-level",
            QStringLiteral("`%1` isn't the top of its git repository, so its hooks belong to another project. "
                           "Run `loom install-hooks` in a Loom project.").arg(nativeRoot));

    // `--git-path` follows core.hooksPath; `--git-common-dir` is the repository's own git
    // directory, shared by its linked worktrees, where git keeps their hooks.
    const QStringList gitPath = {QStringLiteral("rev-parse"), QStringLiteral("--git-path"), QStringLiteral("hooks/pre-commit")};
    const GitRun where = runGit(git, gitPath, root);
    if (where.timedOut)
        return failure(HooksOutcome::CantWork, "git-timeout", gitDidntRunMessage(where, gitPath));
    const QStringList commonDir = {QStringLiteral("rev-parse"), QStringLiteral("--git-common-dir")};
    const GitRun common = runGit(git, commonDir, root);
    if (common.timedOut)
        return failure(HooksOutcome::CantWork, "git-timeout", gitDidntRunMessage(common, commonDir));
    if (!where.ok() || where.output.trimmed().isEmpty() || !common.ok() || common.output.trimmed().isEmpty())
        return failure(HooksOutcome::CantWork, "git-failed",
            QStringLiteral("Loom couldn't ask git where this repository's hooks are: %1.").arg(whyGitFailed(where.ok() ? common : where)));
    const QString hookFolder = resolvedPath(root, QFileInfo(where.output.trimmed()).path());
    const QString gitFolder = resolvedPath(root, common.output.trimmed());
    if (hookFolder.compare(gitFolder, fileNameCase) != 0 && !hookFolder.startsWith(gitFolder + u'/', fileNameCase))
        return failure(HooksOutcome::CantWork, "hooks-path-outside",
            hooksPathOutsideMessage(QDir::cleanPath(QDir(root).absoluteFilePath(QFileInfo(where.output.trimmed()).path()))));

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
        const QStringList set = QStringList{QStringLiteral("config"), QStringLiteral("--local")} + setting;
        const GitRun run = runGit(git, set, root);
        // A16: a git that hangs is `git-timeout`, wherever it hangs.
        if (run.timedOut)
            return failure(HooksOutcome::CantWork, "git-timeout", gitDidntRunMessage(run, set));
        if (!run.ok())
            return failure(HooksOutcome::CantWork, "git-failed",
                QStringLiteral("Loom installed its hook, but `git config --local %1` failed: %2.").arg(setting[0], whyGitFailed(run)));
    }

    result.outcome = HooksOutcome::Installed;
    return result;
}

} // namespace loom
