#pragma once

// The New project form (Specs/m2-statements-review.md, "Starting and opening": "the parent folder,
// the folder name, the project's name, and the research idea (several lines, optional). Create runs
// LoomCore's newProject, which does exactly what `loom new-project` does, with M1's checks and
// messages … shown in the form").

#include <QDialog>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace loom {

class AppSettings;
class Lane;

/// The form, as a window-modal dialog. It closes itself once the project is created.
class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    /// @param settings For the git to run.
    /// @param lane     The Project lane, where creating runs.
    /// @param parent   The window.
    NewProjectDialog(AppSettings* settings, Lane* lane, QWidget* parent);

    /// Fills the form in (*"Create a project here"*: the empty folder's parent and name).
    void prefill(const QString& parentFolder, const QString& folderName, const QString& projectName);

    /// The `loom` the new project's commit guard falls back on when `loom` isn't on PATH: the one
    /// beside this program, if it's there.
    static QString loomBesideApp();

    /// Closes the form, unless a project is being created: then Esc, Cancel and the title bar's
    /// close button wait, so a project is never created without being opened.
    void reject() override;

signals:
    /// The project was created; `path` is its folder, absolute, with `/`.
    void created(const QString& path);

private:
    /// Runs createProject() on the Project lane with the form's values.
    void create();
    /// Enables Create when the required fields are filled in.
    void updateCreate();

    AppSettings* m_settings;
    Lane* m_lane;
    QLineEdit* m_parentFolder;
    QLineEdit* m_folderName;
    QLineEdit* m_name;
    QPlainTextEdit* m_idea;
    QLabel* m_message;
    QPushButton* m_create;
    bool m_creating = false;
};

} // namespace loom
