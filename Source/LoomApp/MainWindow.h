#pragma once

// Loom's window (Specs/m2-statements-review.md, "The app shell"): the start screen or a project,
// the side bar of tabs, the status bar, the bottom panel, Settings and the three work lanes. One
// project per window; File → New window opens another.

#include "LoomApp/ProjectSnapshot.h"
#include "LoomApp/Tabs.h"

#include "LoomCore/Build.h"

#include <QMainWindow>

#include <functional>
#include <map>
#include <vector>

class QAction;
class QLabel;
class QProgressBar;
class QSplitter;
class QStackedWidget;
class QToolButton;

namespace loom {

class AppSettings;
class BottomPanel;
class GitPanel;
class Lane;
class ProjectSession;
class ReviewPage;
class SettingsPage;
class SideBar;
class StartScreen;
class StatementsPage;

/// A Loom window.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /// How a folder is being opened.
    enum class OpenMode {
        Interactive, ///< Dakota chose it: a refusal is shown in a message; it opens on Statements.
        Restore,     ///< At start, the last project: it opens on the tab last used; a failure
                     ///< leaves the start screen, with a line in the status bar.
    };

    /// Creates the window, on the start screen. Doesn't show it.
    /// @param settings The settings, shared by every window; must outlive the window.
    /// @param parent   The Qt parent, or nullptr for a top-level window.
    /// Side effects: restores the window geometry the settings remember.
    explicit MainWindow(AppSettings* settings, QWidget* parent = nullptr);
    /// Stops a running *Check my setup*, waits for the lanes, and releases the project's lock.
    ~MainWindow() override;

    /// Opens a folder, as *Opening a folder* says, on the Project lane; the window changes when
    /// it's done (openFinished()). The project already open here stays open if the new one can't be.
    /// @param path The folder.
    /// @param mode How it's being opened.
    void openFolder(const QString& path, OpenMode mode = OpenMode::Interactive);
    /// At start: reopens the last project on the tab last used, if the setting *Reopen the last
    /// project* is on and there is one; otherwise leaves the start screen.
    void restoreLastProject();

    /// Shows a tab's page and highlights its icon. A project tab needs a project.
    void showTab(Tab tab);
    /// The page shown: a tab, or the start screen (onStartScreen()).
    Tab currentTab() const { return m_current; }
    bool onStartScreen() const;

    /// The open project, or null.
    ProjectSession* session() const { return m_session; }

    /// Opens the New project form, filled in if a folder is given.
    void newProject(const QString& parentFolder = {}, const QString& folderName = {});
    /// Asks for a folder and opens it.
    void chooseFolder();
    /// Opens *Check my setup* in a dialog and starts it.
    void checkSetup();
    /// Shows or hides the bottom panel.
    void toggleBottomPanel();
    /// Ctrl+Shift+G, or a click on the branch in the status bar: shows or hides the git panel.
    /// Needs a project.
    void toggleGitPanel();

    // The parts, for tests.
    SideBar* sideBar() const { return m_sideBar; }
    StartScreen* startScreen() const { return m_startScreen; }
    SettingsPage* settingsPage() const { return m_settingsPage; }
    StatementsPage* statementsPage() const { return m_statementsPage; }
    ReviewPage* reviewPage() const { return m_reviewPage; }
    BottomPanel* bottomPanel() const { return m_bottomPanel; }
    GitPanel* gitPanel() const { return m_gitPanel; }
    QWidget* page(Tab tab) const;
    QWidget* errorsBar() const { return m_errorsBar; }
    Lane* projectLane() const { return m_projectLane; }
    Lane* buildLane() const { return m_buildLane; }
    Lane* gitLane() const { return m_gitLane; }

signals:
    /// A project was opened in this window.
    void projectOpened(const QString& root);
    /// An openFolder() finished, whether it opened the folder or not.
    void openFinished();

protected:
    /// Remembers the window's geometry and the project and tab, and stops *Check my setup*.
    void closeEvent(QCloseEvent* event) override;
    /// Re-reads the project when the window is activated, since watchers miss changes on some drives.
    void changeEvent(QEvent* event) override;
    /// A click on the branch in the status bar opens the git panel.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Opening;

    /// Builds the menus and their shortcuts.
    void createActions();
    /// Builds the status bar.
    void createStatusBar();
    /// Takes the result of opening a folder.
    void finishOpening(Opening opening, OpenMode mode);
    /// Tells Dakota why a folder didn't open, with the action the spec gives it, if any.
    void showOpenMessage(const FolderInspection& folder, const QString& message);
    /// Shows the start screen.
    void showStart();
    /// Draws everything that comes from the snapshot: title, status bar, errors bar, Problems.
    void updateFromSnapshot();
    /// Writes a build to Output's Build channel: what was built, how it went, the command and
    /// the full `main.log`.
    void logBuild(const BuildResult& result);
    /// Shows the spinner while any lane is busy.
    void updateBusy();
    /// Asks what to do with unsaved edits before the project closes (Specs/m2-statements-review.md,
    /// "Never losing text": **Save all**, **Keep them for next time**, or **Cancel**), then calls
    /// `proceed` unless Dakota cancelled or a save didn't happen. Without unsaved edits it proceeds
    /// at once.
    void askAboutUnsavedEdits(const QString& why, std::function<void()> proceed);
    /// Enables the Statement menu's actions on the Statements tab only.
    void updateStatementActions();

    AppSettings* m_settings;
    Lane* m_projectLane;
    Lane* m_buildLane;
    Lane* m_gitLane;
    ProjectSession* m_session = nullptr;

    SideBar* m_sideBar;
    QStackedWidget* m_pages;
    StartScreen* m_startScreen;
    SettingsPage* m_settingsPage;
    std::map<Tab, QWidget*> m_tabPages;
    ReviewPage* m_reviewPage;
    QSplitter* m_splitter;
    BottomPanel* m_bottomPanel;
    GitPanel* m_gitPanel;
    QWidget* m_errorsBar;
    QLabel* m_errorsText;

    QLabel* m_branch;
    QLabel* m_buildState;
    QToolButton* m_problemCounts;
    QProgressBar* m_busy;

    std::map<Tab, QAction*> m_tabActions;
    std::vector<QAction*> m_statementActions;
    Tab m_current = Tab::Statements;
    StatementsPage* m_statementsPage;
    /// Dakota answered the question about unsaved edits: the next close or open goes ahead.
    bool m_unsavedAnswered = false;
};

} // namespace loom
