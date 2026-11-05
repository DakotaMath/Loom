#pragma once

// The start screen (Specs/m2-statements-review.md, "Starting and opening": "New project, Open
// project… and Recent projects (the last 10, newest first, each its name and path; one that no
// longer exists is greyed, with Remove from list)"; and *Check my setup*).

#include <QWidget>

class QListWidget;

namespace loom {

class AppSettings;

/// What a window shows with no project open.
class StartScreen : public QWidget
{
    Q_OBJECT

public:
    /// @param settings Where the recent projects are; the list follows its changes.
    /// @param parent   The Qt parent.
    explicit StartScreen(AppSettings* settings, QWidget* parent = nullptr);

    /// The list of recent projects (`recentProjects`); each item holds its path in Qt::UserRole.
    QListWidget* recentList() const { return m_recent; }

signals:
    void newProjectClicked();
    void openProjectClicked();
    void checkSetupClicked();
    /// A recent project that exists was clicked.
    void recentClicked(const QString& path);

private:
    /// Rebuilds the recent list from the settings.
    void fillRecent();

    AppSettings* m_settings;
    QListWidget* m_recent;
};

} // namespace loom
