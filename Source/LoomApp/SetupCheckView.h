#pragma once

// *Check my setup*'s results (Specs/m2-statements-review.md, "Settings": "checks, in order, and shows
// one line each: a tick with the version and path, or a cross with what to do"). LoomCore's
// checkSetup() does the checking, on the Build lane; this shows each line as it arrives.

#include "LoomCore/SetupCheck.h"

#include <QWidget>

#include <atomic>
#include <memory>

class QPushButton;
class QTreeWidget;

namespace loom {

class AppSettings;
class Lane;

/// The button and the list of results.
class SetupCheckView : public QWidget
{
    Q_OBJECT

public:
    /// @param settings For the tools' paths and the compile timeout.
    /// @param lane     The lane the check runs on (the Build lane: its longest part is a build).
    /// @param parent   The Qt parent.
    SetupCheckView(AppSettings* settings, Lane* lane, QWidget* parent = nullptr);
    /// Stops a check that's running.
    ~SetupCheckView() override;

    /// Starts the check, unless one is running. Side effects: clears the list; runs the tools.
    void start();
    /// Asks a running check to stop (the window is closing).
    void stop();
    /// Whether a check is running.
    bool running() const { return m_running; }
    /// The list of results: one top-level item per line, its key in Qt::UserRole.
    QTreeWidget* results() const { return m_results; }

signals:
    /// A check finished (or was stopped).
    void finished();

private:
    /// Adds one line.
    void add(const SetupItem& item);

    AppSettings* m_settings;
    Lane* m_lane;
    QPushButton* m_button;
    QTreeWidget* m_results;
    std::shared_ptr<std::atomic_bool> m_stop;
    bool m_running = false;
};

} // namespace loom
