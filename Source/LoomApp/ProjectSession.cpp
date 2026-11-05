#include "LoomApp/ProjectSession.h"

#include "LoomApp/Lane.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace loom {

ProjectSession::ProjectSession(std::unique_ptr<QLockFile> lock, std::shared_ptr<const ProjectSnapshot> first, Lane* lane,
                               std::function<GitTool()> git, QObject* parent)
    : QObject(parent)
    , m_root(first->root)
    , m_lock(std::move(lock))
    , m_snapshot(std::move(first))
    , m_lane(lane)
    , m_git(std::move(git))
{
    m_gather.setSingleShot(true);
    m_gather.setInterval(gatherMs);
    connect(&m_gather, &QTimer::timeout, this, &ProjectSession::read);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &ProjectSession::refresh);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &ProjectSession::refresh);
    watchFolders();
}

void ProjectSession::refresh()
{
    m_gather.start();
}

void ProjectSession::read()
{
    if (m_reading) {
        m_again = true;
        return;
    }
    m_reading = true;
    m_lane->run(this, [root = m_root, git = m_git()] { return std::make_shared<const ProjectSnapshot>(readSnapshot(root, git)); },
                [this](std::shared_ptr<const ProjectSnapshot> snapshot) {
                    m_reading = false;
                    m_snapshot = std::move(snapshot);
                    watchFolders();
                    emit snapshotChanged();
                    if (m_again) {
                        m_again = false;
                        read();
                    }
                });
}

void ProjectSession::watchFolders()
{
    QStringList wanted = {m_root};
    const auto add = [&](const QString& folder) {
        if (QFileInfo(folder).isDir())
            wanted << folder;
    };
    const auto subfolders = [](const QString& folder) {
        QStringList out;
        for (const QString& name : QDir(folder).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            out << folder + u'/' + name;
        return out;
    };
    add(m_root + QStringLiteral("/Statements"));
    add(m_root + QStringLiteral("/TeX"));
    add(m_root + QStringLiteral("/Proofs"));
    for (const QString& folder : subfolders(m_root + QStringLiteral("/Proofs")))
        add(folder);
    add(m_root + QStringLiteral("/Papers"));
    for (const QString& paper : subfolders(m_root + QStringLiteral("/Papers"))) {
        add(paper);
        add(paper + QStringLiteral("/Statements"));
    }

    const QSet<QString> want(wanted.begin(), wanted.end());
    const QStringList now = m_watcher.directories();
    const QSet<QString> have(now.begin(), now.end());
    QStringList gone;
    for (const QString& folder : have) {
        if (!want.contains(folder))
            gone << folder;
    }
    QStringList fresh;
    for (const QString& folder : wanted) {
        if (!have.contains(folder))
            fresh << folder;
    }
    if (!gone.isEmpty())
        m_watcher.removePaths(gone);
    if (!fresh.isEmpty())
        m_watcher.addPaths(fresh);
}

} // namespace loom
