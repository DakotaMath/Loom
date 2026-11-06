#pragma once

// The git panel (Specs/m2-statements-review.md, "The git panel"): a panel on the right, opened
// from every tab, that runs git only on a click, through LoomCore's git operations
// (LoomCore/GitOperations.h), on the Git lane. At the top the branch and how far it is ahead and
// behind; then the changed files with tick boxes and a diff on click; the commit message and
// **Commit**; **Run checks**, **New branch…**, **Switch branch…**, **Fetch**, **Pull** and
// **Push…**. Every git command it runs goes to Output's Git channel (gitSteps()).

#include "LoomCore/GitOperations.h"
#include "LoomCore/Problem.h"

#include <QWidget>

#include <functional>
#include <memory>
#include <set>
#include <vector>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSettings;
class QTreeWidget;
class QTreeWidgetItem;

namespace loom {

class AppSettings;
class Lane;
class ProjectSession;
class StatementsPage;

/// The git panel.
class GitPanel : public QWidget
{
    Q_OBJECT

public:
    /// The panel's buttons.
    enum class Button { Commit, RunChecks, NewBranch, SwitchBranch, Fetch, Pull, Push };

    /// How long a first line of the commit message may be before the panel warns (the spec: 72).
    static constexpr int firstLineLimit = 72;

    /// @param settings     The app's settings: which git to run, and where the project state is kept.
    /// @param gitLane      Where git runs, one command at a time.
    /// @param projectLane  Where *Run checks* validates the project.
    /// @param parent       The Qt parent.
    GitPanel(AppSettings* settings, Lane* gitLane, Lane* projectLane, QWidget* parent = nullptr);
    ~GitPanel() override;

    /// Shows a project, or none. The page tells it which files have unsaved edits and saves them.
    /// Side effects: forgets the old project's state; restores the new one's unfinished commit
    /// message; reads the git state if the panel is shown.
    void setSession(ProjectSession* session, StatementsPage* page);

    /// Reads the git state again on the Git lane (only while the panel is shown; it reads when
    /// shown). Reads asked for while one runs become one more read after it.
    void refresh();

    // The operations, as their buttons run them.
    /// **Commit** the ticked files with the message, asking first about ticked files with unsaved edits.
    void commit();
    /// **Run checks**: validates the project as `loom validate` does; the problems go to Problems.
    void runChecks();
    /// **New branch…**: asks for a name, then makes the branch.
    void newBranch();
    /// **Switch branch…**: refused with unsaved edits; otherwise asks which branch, then switches.
    void switchBranch();
    /// **Fetch**.
    void fetch();
    /// **Pull** (fast-forward only); refused with unsaved edits.
    void pull();
    /// **Push…**: lists the commits that will go up, then pushes on **Push {n} commits**.
    void push();

    /// The latest state read.
    const GitState& state() const { return m_state; }
    /// Whether an operation (or a read) is running.
    bool busy() const { return m_running > 0; }

    // The parts, for tests.
    QLabel* header() const { return m_header; }
    QTreeWidget* files() const { return m_files; }
    QPlainTextEdit* diff() const { return m_diff; }
    QPlainTextEdit* message() const { return m_message; }
    QLabel* messageWarning() const { return m_messageWarning; }
    QLabel* result() const { return m_result; }
    QPlainTextEdit* gitOutput() const { return m_gitOutput; }
    QPushButton* button(Button button) const;
    /// Ticks or unticks a changed file. @returns Whether it is listed.
    bool setTicked(const QString& path, bool ticked);

signals:
    /// Git commands were run: Output's Git channel logs each one.
    void gitSteps(const std::vector<GitStep>& steps);
    /// A commit was refused by Loom's commit guard with these problems, or a later commit cleared
    /// them (empty): they go to the Problems panel.
    void commitProblems(const std::vector<Problem>& problems);
    /// *Run checks* finished with these problems: they go to the Problems panel.
    void checksFinished(const std::vector<Problem>& problems);
    /// An operation changed git or the working tree: the project is re-read.
    void repositoryChanged();
    /// A state read finished and is shown.
    void stateShown();
    /// An operation finished, whatever its outcome, and its result is shown.
    void operationFinished();

protected:
    /// Reads the state when the panel is shown.
    void showEvent(QShowEvent* event) override;

private:
    /// Builds the widgets.
    void createWidgets();
    /// Shows the latest state: the header, the buttons, the files (keeping ticks and selection).
    void showState();
    /// Shows which buttons apply, and enables them unless an operation runs.
    void updateButtons();
    /// Shows an operation's outcome under the buttons, and git's output when it failed.
    void showResult(const GitResult& result);
    /// Shows a line under the buttons, with no git output.
    void showMessage(const QString& message);
    /// Runs an operation on the Git lane, then shows its result, logs its steps, and re-reads.
    /// @param work Runs on the lane; returns the result.
    /// @param after Called on the UI thread with the result, before the re-read; may be empty.
    void runOperation(std::function<GitResult()> work, std::function<void(const GitResult&)> after = {});
    /// The ticked files of the latest state.
    std::vector<ChangedFile> tickedFiles() const;
    /// Commits the ticked files now, without asking about unsaved edits.
    void commitNow();
    /// Saves these files' unsaved edits one after another, then calls `done` with whether all saved.
    void saveFiles(QStringList paths, std::function<void(bool)> done);
    /// Shows the diff of the selected file, read on the Git lane.
    void showDiff();
    /// Warns about a first line over 72 characters, and keeps the message for next time.
    void messageChanged();
    /// Why switching or pulling is refused by the app itself (*"Save or discard your unsaved edits
    /// first."*), or empty.
    QString unsavedRefusal() const;
    /// The git to run, from the settings.
    GitTool git() const;
    /// The project folder, or empty.
    QString root() const;
    /// Starts or ends a task on the lane, for busy() and the buttons.
    void setRunning(int delta);

    AppSettings* m_settings;
    Lane* m_gitLane;
    Lane* m_projectLane;
    ProjectSession* m_session = nullptr;
    StatementsPage* m_page = nullptr;
    std::unique_ptr<QSettings> m_projectState;
    GitState m_state;
    /// Files Dakota unticked, by path; every other changed file is ticked (the spec: "all ticked by default").
    std::set<QString> m_unticked;
    int m_running = 0;
    bool m_reading = false;
    bool m_readAgain = false;
    int m_generation = 0; ///< Bumped by setSession(), so results for an old project are dropped.

    QLabel* m_header;
    QWidget* m_operations;
    QPushButton* m_buttons[7];
    QProgressBar* m_busy;
    QLabel* m_result;
    QPlainTextEdit* m_gitOutput;
    QLabel* m_filesLabel;
    QTreeWidget* m_files;
    QPlainTextEdit* m_diff;
    QPlainTextEdit* m_message;
    QLabel* m_messageWarning;
};

} // namespace loom
