#include "DemoProject.h"

#include "LoomCore/Hooks.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace loomtest {

QString copyFolder(const QString& from, const QString& to)
{
    const QDir source(from);
    QDirIterator it(from, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        const QString target = QDir(to).filePath(source.relativeFilePath(file));
        QDir().mkpath(QFileInfo(target).path());
        QFile::remove(target);
        if (!QFile::copy(file, target))
            return QStringLiteral("couldn't copy %1 to %2").arg(file, target);
        // Copies of read-only checkouts stay writable, so tests can edit them.
        QFile(target).setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    }
    return {};
}

DemoProject::DemoProject(const QString& overlay) : m_temp(std::make_unique<QTemporaryDir>())
{
    if (!m_temp->isValid()) {
        m_error = QStringLiteral("couldn't make a temporary folder");
        return;
    }
    m_root = m_temp->filePath(QStringLiteral("Demo"));
    if (const QString error = copyFolder(QStringLiteral(LOOM_DEMO_PROJECT), m_root); !error.isEmpty()) {
        m_error = error;
        return;
    }
    // The kit keeps them without their dots, so they don't act on Loom's own repository.
    for (const char* name : {"gitignore", "gitattributes"}) {
        const QString target = QDir(m_root).filePath(u'.' + QString::fromLatin1(name));
        if (!QFile::copy(QStringLiteral(LOOM_PROJECT_KIT "/") + QString::fromLatin1(name), target)) {
            m_error = QStringLiteral("couldn't copy the kit's %1").arg(QString::fromLatin1(name));
            return;
        }
    }

    // A git identity of the test's own: the developer's settings can't leak in.
    const QString config = m_temp->filePath(QStringLiteral("gitconfig"));
    QFile file(config);
    if (!file.open(QIODevice::WriteOnly) || file.write("[user]\n\tname = Loom Test\n\temail = test@example.com\n") < 0) {
        m_error = QStringLiteral("couldn't write the test's git config");
        return;
    }
    file.close();
    m_git.environment.insert(QStringLiteral("GIT_CONFIG_GLOBAL"), config);
    m_git.environment.insert(QStringLiteral("GIT_CONFIG_NOSYSTEM"), QStringLiteral("1"));
    for (const char* name : {"GIT_AUTHOR_NAME", "GIT_AUTHOR_EMAIL", "GIT_COMMITTER_NAME", "GIT_COMMITTER_EMAIL", "GIT_DIR",
                             "GIT_WORK_TREE", "GIT_INDEX_FILE"})
        m_git.environment.remove(QString::fromLatin1(name));

    const auto git = [&](const QStringList& arguments) {
        const loom::GitRun run = loom::runGit(m_git, arguments, m_root);
        if (!run.ok())
            m_error = QStringLiteral("`%1` failed: %2%3").arg(loom::gitCommand(arguments), run.errors, run.output);
        return run.ok();
    };
    if (!git({QStringLiteral("init"), QStringLiteral("-b"), QStringLiteral("main")}))
        return;
    const loom::HooksResult hooks = loom::installHooks(m_root, QStringLiteral(LOOM_PROGRAM), m_git);
    if (hooks.outcome != loom::HooksOutcome::Installed) {
        m_error = QStringLiteral("installing the hooks failed: %1").arg(hooks.message);
        return;
    }
    if (!git({QStringLiteral("add"), QStringLiteral("-A")}))
        return;
    if (!git({QStringLiteral("commit"), QStringLiteral("--quiet"), QStringLiteral("-m"), QStringLiteral("The demo project")}))
        return;
    if (!overlay.isEmpty())
        m_error = applyOverlay(overlay);
}

QString DemoProject::applyOverlay(const QString& overlay)
{
    const QString folder = QStringLiteral(LOOM_DEMO_EDITS "/") + overlay;
    if (!QFileInfo(folder).isDir())
        return QStringLiteral("there's no overlay %1").arg(folder);
    return copyFolder(folder, m_root);
}

} // namespace loomtest
