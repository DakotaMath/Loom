#pragma once

// The project open in a window: its app lock, its latest snapshot, and the watcher that re-reads it
// (Specs/m2-statements-review.md, "The app and LoomCore": "Snapshots" and "Watching the folder").

#include "LoomApp/ProjectSnapshot.h"

#include "LoomCore/Git.h"

#include <QFileSystemWatcher>
#include <QLockFile>
#include <QObject>
#include <QTimer>

#include <functional>
#include <memory>

namespace loom {

class Lane;

/// One open project. Created once the folder is opened and locked; destroyed when the window
/// closes it, which releases `.loom/app.lock`.
class ProjectSession : public QObject
{
    Q_OBJECT

public:
    /// How long changes are gathered before the project is re-read (the spec: 300 ms).
    static constexpr int gatherMs = 300;

    /// @param lock     The project's `.loom/app.lock`, held.
    /// @param first    The snapshot read when it was opened.
    /// @param lane     The Project lane, where re-reading runs.
    /// @param git      Gives the git to read with, each time (the Settings may change it).
    /// @param parent   The Qt parent.
    /// Side effects: starts watching the project's folders.
    ProjectSession(std::unique_ptr<QLockFile> lock, std::shared_ptr<const ProjectSnapshot> first, Lane* lane,
                   std::function<GitTool()> git, QObject* parent = nullptr);

    /// The project folder, absolute, with `/`.
    const QString& root() const { return m_root; }
    /// The latest snapshot; never null.
    std::shared_ptr<const ProjectSnapshot> snapshot() const { return m_snapshot; }

    /// Re-reads the project after the gathering delay; requests made meanwhile are one read.
    /// Called for every change the watcher sees and when the window is activated (watchers miss
    /// changes on some drives). Side effects: queues a read on the Project lane.
    void refresh();

    /// Whether the watcher has seen a change that the latest snapshot doesn't have yet: from the
    /// change until the snapshot read after it is published. Dakota's check buttons wait for it
    /// (Specs/m2-statements-review.md, "Status and checks in the tab": disabled "while the snapshot
    /// is older than the file on disk"). A refresh() asked for without a change seen (the window
    /// activated) doesn't count, or every switch back to Loom would disable them for a moment.
    bool stale() const { return m_changeSeen || m_readingSeen; }

    /// The folders being watched, for tests.
    QStringList watched() const { return m_watcher.directories(); }

signals:
    /// A new snapshot was published.
    void snapshotChanged();
    /// The watcher saw a change: stale() is true until a snapshot read after it is published.
    void becameStale();

private:
    /// Queues the read now.
    void read();
    /// The watcher saw a change: stale() until it is read; then refresh().
    void changeSeen();
    /// Watches the folders the spec lists: the top folder, `Statements/`, `Proofs/` and each
    /// folder in it, `Papers/`, each paper's folder and its `Statements/`, and `TeX/`, adding the
    /// ones that appeared and dropping those that went.
    void watchFolders();

    QString m_root;
    std::unique_ptr<QLockFile> m_lock;
    std::shared_ptr<const ProjectSnapshot> m_snapshot;
    Lane* m_lane;
    std::function<GitTool()> m_git;
    QFileSystemWatcher m_watcher;
    QTimer m_gather;
    bool m_reading = false; ///< A read is queued or running.
    bool m_again = false;   ///< Something changed while it was.
    bool m_changeSeen = false;  ///< The watcher saw a change no read has started on yet.
    bool m_readingSeen = false; ///< The read running started after a change the watcher saw.
};

} // namespace loom
