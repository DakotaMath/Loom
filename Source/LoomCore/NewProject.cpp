#include "LoomCore/NewProject.h"

#include "LoomCore/Hooks.h"
#include "LoomCore/ProjectFiles.h"
#include "LoomCore/ProjectToml.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace loom {

namespace {

/// Where the project kit (Templates/project/) is in Qt's resources; see Source/LoomCore/CMakeLists.txt.
constexpr auto kitRoot = u":/kit/";

/// A failure, as a result.
NewProjectResult failure(NewProjectOutcome outcome, const char* code, const QString& message, const QString& path)
{
    return {outcome, QString::fromLatin1(code), message, path};
}

/// Reads the project kit from Qt's resources.
/// @returns Pairs of (project-relative path, bytes), sorted by path, with `gitignore` and
///          `gitattributes` renamed to their dotted names and CRLF made LF (Specs/m1-core.md,
///          "The project folder" and "Front matter": "Loom writes LF"). No side effects.
std::vector<std::pair<QString, QByteArray>> readKit()
{
    std::vector<std::pair<QString, QByteArray>> files;
    QDirIterator it(QString::fromUtf16(kitRoot), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString resource = it.next();
        QString relative = resource.mid(QStringView(kitRoot).size());
        // Stored without the dot so they don't act on Loom's own repo.
        if (relative == u"gitignore" || relative == u"gitattributes")
            relative.prepend(u'.');
        QFile file(resource);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        QByteArray bytes = file.readAll();
        bytes.replace("\r\n", "\n");
        files.emplace_back(relative, bytes);
    }
    std::sort(files.begin(), files.end());
    return files;
}

/// The environment for git: the caller's, without the variables that tell git where a repository
/// is, so `git init` makes the repository in the project folder whatever started `loom`.
QProcessEnvironment gitEnvironment(QProcessEnvironment environment)
{
    for (const char* name : {"GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_OBJECT_DIRECTORY",
                             "GIT_ALTERNATE_OBJECT_DIRECTORIES", "GIT_COMMON_DIR"})
        environment.remove(QString::fromLatin1(name));
    return environment;
}

} // namespace

QString gitLfsMissingMessage()
{
    return QStringLiteral("Loom needs Git LFS, and `git lfs` didn't run. Git for Windows includes it; reinstall Git with LFS ticked.");
}

QString gitIdentityMissingMessage()
{
    return QStringLiteral("Git doesn't know your name and email yet, so Loom can't make the first commit. "
                          "Run `git config --global user.name \"…\"` and `git config --global user.email …`, then try again.");
}

NewProjectResult createProject(const NewProjectRequest& request)
{
    const QString path = QDir::cleanPath(QFileInfo(request.folder).absoluteFilePath());
    const QString native = QDir::toNativeSeparators(path);
    const QFileInfo info(path);

    // Step 1: a new or empty folder, in a folder that exists.
    if (request.name.isEmpty())
        return failure(NewProjectOutcome::Refused, "name-empty", QStringLiteral("A project needs a name."), path);
    if (info.exists() && !info.isDir())
        return failure(NewProjectOutcome::Refused, "not-a-folder",
            QStringLiteral("`%1` is a file. Loom creates projects only in a new or empty folder.").arg(native), path);
    if (info.exists() && !QDir(path).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
        return failure(NewProjectOutcome::Refused, "folder-not-empty",
            QStringLiteral("`%1` isn't empty. Loom creates projects only in a new or empty folder.").arg(native), path);
    const QString parent = info.absolutePath();
    if (!info.exists() && !QFileInfo(parent).isDir())
        return failure(NewProjectOutcome::CantWork, "parent-missing",
            QStringLiteral("`%1` doesn't exist, so Loom can't create `%2` in it. Create it first, or choose another place.")
                .arg(QDir::toNativeSeparators(parent), native), path);

    // Step 2: git, Git LFS and git's identity, before writing anything.
    GitTool git = request.git;
    git.environment = gitEnvironment(git.environment);
    const QString runIn = info.exists() ? path : parent;
    // A git that didn't finish in time is stuck, not missing, and says so; it must never pass for
    // "not set" either (core.hooksPath below).
    const auto timedOut = [&](const GitRun& run, const QStringList& arguments) {
        return failure(NewProjectOutcome::CantWork, "git-timeout", gitDidntRunMessage(run, arguments), path);
    };
    const QStringList version = {QStringLiteral("--version")};
    if (const GitRun run = runGit(git, version, runIn); !run.ok())
        return run.timedOut ? timedOut(run, version)
                            : failure(NewProjectOutcome::CantWork, "git-missing", gitDidntRunMessage(run, version), path);
    const QStringList lfsVersion = {QStringLiteral("lfs"), QStringLiteral("version")};
    if (const GitRun run = runGit(git, lfsVersion, runIn); !run.ok())
        return run.timedOut ? timedOut(run, lfsVersion)
                            : failure(NewProjectOutcome::CantWork, "git-lfs-missing", gitLfsMissingMessage(), path);

    // Ask for the identity the new repository will see: pointing GIT_DIR at its (not yet
    // existing) .git folder makes git read the system and global config only, never the config
    // of a repository the folder happens to sit in.
    GitTool identityGit = git;
    identityGit.environment.insert(QStringLiteral("GIT_DIR"), path + QStringLiteral("/.git"));
    for (const char* key : {"user.name", "user.email"}) {
        const QStringList get = {QStringLiteral("config"), QStringLiteral("--get"), QString::fromLatin1(key)};
        const GitRun run = runGit(identityGit, get, runIn);
        if (run.timedOut)
            return timedOut(run, get);
        if (!run.ok() || run.output.trimmed().isEmpty())
            return failure(NewProjectOutcome::CantWork, "git-identity-missing", gitIdentityMissingMessage(), path);
    }

    // A1: with core.hooksPath in the global (or system) config, `git lfs install --local` and the
    // commit guard would both write their hooks into that folder, which other repositories share.
    // install-hooks' message can't serve here: its way out (`git config --local …` in the
    // repository) needs a repository, and there isn't one yet.
    const QStringList getHooksPath = {QStringLiteral("config"), QStringLiteral("--get"), QStringLiteral("--type=path"),
                                      QStringLiteral("core.hooksPath")};
    const GitRun hooksPath = runGit(identityGit, getHooksPath, runIn);
    if (hooksPath.timedOut)
        return timedOut(hooksPath, getHooksPath);
    if (hooksPath.ok() && !hooksPath.output.trimmed().isEmpty())
        return failure(NewProjectOutcome::CantWork, "hooks-path-outside",
            QStringLiteral("Git runs hooks from `%1` (`core.hooksPath` in your global git config), so a new project's commit "
                           "guard and Git LFS hooks would go there. Unset it with `git config --global --unset "
                           "core.hooksPath`, then try again.")
                .arg(QDir::toNativeSeparators(hooksPath.output.trimmed())), path);

    const std::vector<std::pair<QString, QByteArray>> kit = readKit();
    if (kit.empty())
        return failure(NewProjectOutcome::CantWork, "kit-missing",
            QStringLiteral("This build of Loom has no project kit, so it can't create projects."), path);

    // Step 3: the folder and its files. From here on, a failure leaves what was made in place.
    const QString leftInPlace = QStringLiteral(" Loom left what it made in `%1` for you to look at.").arg(native);
    if (!info.exists() && !QDir().mkdir(path))
        return failure(NewProjectOutcome::CantWork, "write-failed",
            QStringLiteral("Loom couldn't create the folder `%1`.").arg(native), path);

    ProjectConfig config;
    config.name = request.name;
    config.idea = request.idea;
    config.created = request.created;
    config.preamble = {QStringLiteral("TeX/preamble.tex"), QStringLiteral("TeX/macros.tex")};
    QString writeError = writeProjectFile(path, QStringLiteral("loom.toml"), formatProjectToml(config));
    for (auto it = kit.begin(); writeError.isEmpty() && it != kit.end(); ++it)
        writeError = writeProjectFile(path, it->first, it->second);
    if (!writeError.isEmpty())
        return failure(NewProjectOutcome::CantWork, "write-failed", writeError + leftInPlace, path);

    // Steps 4 and 5: the repository, Loom's hook and merge driver, and the first commit.
    const QList<QStringList> steps = {
        {QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main")},
        {QStringLiteral("lfs"), QStringLiteral("install"), QStringLiteral("--local")},
        {}, // loom install-hooks
        {QStringLiteral("add"), QStringLiteral("--all")},
        {QStringLiteral("commit"), QStringLiteral("--quiet"), QStringLiteral("--message"), QStringLiteral("Create project ") + request.name},
    };
    for (const QStringList& step : steps) {
        if (step.isEmpty()) {
            const HooksResult hooks = installHooks(path, request.loomProgram, git);
            if (hooks.code == u"git-timeout")
                return failure(NewProjectOutcome::CantWork, "git-timeout", hooks.message + leftInPlace, path);
            if (hooks.outcome != HooksOutcome::Installed)
                return failure(NewProjectOutcome::CantWork, "hooks-failed",
                    QStringLiteral("Loom wrote the project files, but `loom install-hooks` failed: %1").arg(hooks.message) + leftInPlace, path);
            continue;
        }
        const GitRun run = runGit(git, step, path);
        if (run.ok())
            continue;
        // A16: a git that hangs is `git-timeout`, at every step, naming the whole command.
        if (run.timedOut)
            return failure(NewProjectOutcome::CantWork, "git-timeout", gitDidntRunMessage(run, step) + leftInPlace, path);
        QString why = run.errors.trimmed();
        while (why.endsWith(u'.'))
            why.chop(1);
        if (why.isEmpty())
            why = run.started ? QStringLiteral("it exited with code %1").arg(run.exitCode) : QStringLiteral("it didn't run");
        const QString command = step.first() == u"commit" ? QStringLiteral("git commit") : QStringLiteral("git ") + step.join(u' ');
        return failure(NewProjectOutcome::CantWork, "git-failed",
            QStringLiteral("Loom wrote the project files, but `%1` failed: %2.").arg(command, why) + leftInPlace, path);
    }

    return {NewProjectOutcome::Created, {}, {}, path};
}

} // namespace loom
