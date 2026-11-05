#pragma once

// The app's work lanes (Specs/m2-statements-review.md, "The app and LoomCore", "Threads": "The UI
// thread never waits on a file, git or TeX. Work runs in three lanes, each a worker thread with a
// queue"): Project (one task at a time, in order), Build (two at once) and Git (one at a time).

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QThreadPool>

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace loom {

/// A queue of work run on worker threads, at most `threads` tasks at once, each started in the
/// order it was queued. Results come back on the UI thread.
class Lane : public QObject
{
    Q_OBJECT

public:
    /// @param name    The lane's name, for its threads (`Project`, `Build`, `Git`).
    /// @param threads How many tasks may run at once.
    /// @param parent  The Qt parent.
    Lane(const QString& name, int threads, QObject* parent = nullptr);
    /// Waits for every task queued or running, whose results are then dropped.
    ~Lane() override;

    /// Queues `work` to run on the lane, then `done(result)` on this object's (the UI) thread.
    /// `done` isn't called if `context` has gone by then. Both must be copyable.
    /// @param context The object `done` belongs to.
    /// @param work    Runs on a worker thread; returns the result. Must not touch widgets.
    /// @param done    Takes the result, on the UI thread.
    /// Side effects: whatever `work` and `done` do; busy() is true until `done` has run.
    template <typename Work, typename Done>
    void run(QObject* context, Work work, Done done)
    {
        using Result = std::invoke_result_t<Work&>;
        auto result = std::make_shared<std::optional<Result>>();
        QPointer<QObject> guard(context);
        begin();
        m_pool.start([this, work, done, result, guard]() mutable {
            result->emplace(work());
            QMetaObject::invokeMethod(this, [this, done, result, guard]() mutable {
                if (guard)
                    done(std::move(**result));
                finish();
            }, Qt::QueuedConnection);
        });
    }

    /// Whether any task is queued, running, or waiting for its `done`.
    bool busy() const { return m_tasks > 0; }

    /// Waits until every task queued so far has run, keeping the UI's events flowing so their
    /// `done`s run too. For tests and for closing a window.
    /// @param timeoutMs How long to wait at most.
    /// @returns Whether the lane is idle. Side effects: processes events.
    bool waitUntilIdle(int timeoutMs = 30'000);

signals:
    /// busy() changed.
    void busyChanged(bool busy);

private:
    /// A task was queued.
    void begin();
    /// A task's `done` ran.
    void finish();

    QThreadPool m_pool;
    int m_tasks = 0; ///< Touched on the UI thread only.
};

} // namespace loom
