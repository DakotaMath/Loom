#pragma once

// The Statements tab (Specs/m2-statements-review.md, "The Statements tab" and "Never losing
// text"): the list, the editor's pages, the PDF pane, saving through LoomCore with Dakota's
// confirmations (and compiling after), creating statements and proofs, verbose versions, back and
// forward, find, recovery files and files changed outside Loom. It owns every open file's buffer,
// so unsaved edits survive leaving a statement and switching tabs.

#include "LoomApp/EditBuffer.h"
#include "LoomApp/NewItemDialogs.h"
#include "LoomApp/ProjectSession.h"
#include "LoomApp/ProjectSnapshot.h"

#include "LoomCore/Drafts.h"
#include "LoomCore/EditorSaves.h"

#include <QPointer>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <functional>
#include <map>
#include <memory>
#include <vector>

class QLabel;
class QSettings;
class QSplitter;
class QTabWidget;
class QToolButton;
class QVBoxLayout;

namespace loom {

class AppSettings;
class FilePage;
class Lane;
class PdfPane;
class StatementList;

/// A place in the back and forward history (Specs/m2-statements-review.md, "Back and forward":
/// "the place left (file, page, cursor, scroll)").
struct Place {
    QString tag;           ///< The statement (or stub).
    QString page;          ///< The proof of a proof page; empty for the statement page.
    bool verbose = false;  ///< The proof page showed its verbose version.
    int line = 0;
    int index = 0;
    int scroll = 0;

    /// `tag|page|verbose|line|index|scroll`, as the per-project state file keeps it.
    QString toString() const;
    /// The opposite of toString(); a place with an empty tag if `text` isn't one.
    static Place fromString(const QString& text);
};

/// Where a project file belongs in the tab: the kind of buffer, its ID and its statement.
struct FileRole {
    FileKind kind = FileKind::Statement;
    QString id;  ///< The statement's tag or the proof's ID (a verbose version's proof).
    QString tag; ///< The statement whose pages show it.
};

/// What a project-relative path is to the tab, if anything: `Statements/<tag>.tex`,
/// `Proofs/<tag>/<id>.tex`, `Proofs/<tag>/<id>.verbose.tex`, or a stub `Papers/<Key>/Statements/<tag>.toml`.
/// @returns True and sets `role` if it is one of them. No side effects.
bool fileRole(const QString& path, FileRole& role);

/// The Statements tab.
class StatementsPage : public QWidget
{
    Q_OBJECT

public:
    /// How many places back and forward keep (the spec: up to 100).
    static constexpr int historyLimit = 100;
    /// How long after typing stops a recovery file is written (the spec: one second).
    static constexpr int draftDelayMs = 1000;

    /// @param settings  The app's settings (editor font, compiling); must outlive the page.
    /// @param lane      The Project lane: every read and write of the project runs there.
    /// @param buildLane The Build lane, where the PDF pane's builds run.
    StatementsPage(AppSettings* settings, Lane* lane, Lane* buildLane, QWidget* parent = nullptr);
    /// Keeps the open statement and history in the per-project state file.
    ~StatementsPage() override;

    /// Shows a project, or none. A new project's recovery files are offered back once they're
    /// listed (recoveryOffered()), then the statement open last time is opened again.
    /// Side effects: drops the old project's buffers (their recovery files stay); lists the new
    /// project's drafts on the lane.
    void setSession(ProjectSession* session);
    ProjectSession* session() const { return m_session; }
    /// The latest snapshot, or null.
    std::shared_ptr<const ProjectSnapshot> snapshot() const { return m_snapshot; }
    /// Takes the session's latest snapshot: the list, the pages, and every open file compared
    /// with the disk.
    void snapshotChanged();

    // Opening.
    /// Opens a statement on its statement page, or on a proof's page.
    /// @param record Whether the place left goes on the back history.
    void openStatement(const QString& tag, const QString& page = {}, bool record = true);
    /// Opens a proof on its page.
    void openProof(const QString& proofId);
    /// Opens a paper stub read-only.
    void openStub(const QString& tag);
    /// Opens an ID as typed or shown: a proof, a statement or a stub.
    void openId(const QString& id);
    /// Opens a project file at a line of the file: in raw mode for a front-matter line.
    void showFileLine(const QString& path, int line);
    /// The statement open, or empty.
    const QString& openTag() const { return m_tag; }
    /// The page shown, or null.
    FilePage* currentPage() const;
    /// The page of a proof (or the statement page, for an empty ID), or null.
    FilePage* page(const QString& proofId) const;
    /// The buffer of a file, if it has been opened.
    EditBuffer* buffer(const QString& path) const;
    StatementList* list() const { return m_list; }
    PdfPane* pdfPane() const { return m_pdf; }

    // Edits.
    /// Whether any file has unsaved edits.
    bool hasUnsavedEdits() const;
    /// The files with unsaved edits, project-relative.
    QStringList unsavedPaths() const;
    /// Ctrl+S: saves the page's shown file, then (with *Compile on save*) compiles the PDF.
    void save();
    /// Ctrl+B: compiles the saved files.
    void compile();
    /// Ctrl+J: shows the cursor's place in the PDF (forward SyncTeX).
    void showCursorInPdf();
    /// The latest build's problems, which the pages mark in the margin.
    const std::vector<Problem>& buildProblems() const { return m_buildProblems; }
    /// Takes the latest build's problems. Side effects: re-marks the pages.
    void setBuildProblems(const std::vector<Problem>& problems);
    /// Saves one buffer through LoomCore (Specs/m2-statements-review.md, "Saving"), asking for
    /// Dakota's confirmation when LoomCore says the save needs it, and saving again if he gives
    /// it. A refusal leaves the text in the editor and its recovery file, and lists the problems.
    /// A file already being saved isn't saved a second time meanwhile (`done(false)`); text typed
    /// while a save runs stays as unsaved edits on top of the file it wrote.
    /// @param done Called with whether it saved (false if refused or cancelled).
    void saveBuffer(EditBuffer* buffer, const SaveConfirmations& confirmations = {}, std::function<void(bool)> done = {});
    /// **Save all**: saves every buffer with unsaved edits, one after another, stopping at the
    /// first that doesn't save (which is then shown). @param done Called with whether all saved.
    void saveAll(std::function<void(bool)> done);
    /// **Keep them for next time**: writes every recovery file now. @param done Called once written.
    void writeAllDrafts(std::function<void()> done);
    /// Writes the recovery files of the buffers whose text changed since the last write, and
    /// removes those of buffers with no unsaved edits left. Side effects: queues writes on the lane.
    void writeDrafts();

    /// Ctrl+N and Ctrl+Shift+N: the New statement and New proof forms.
    void newStatement();
    void newProof();
    NewStatementDialog* newStatementDialog() const { return m_newStatement; }
    NewProofDialog* newProofDialog() const { return m_newProof; }

    /// Alt+← and Alt+→.
    void back();
    void forward();
    const std::vector<Place>& backHistory() const { return m_back; }
    const std::vector<Place>& forwardHistory() const { return m_forward; }

    /// Ctrl+F, Ctrl+H, F3 and Shift+F3, on the page shown; Ctrl+F while the PDF has focus finds in
    /// the PDF.
    void find(bool replace);
    void findNext(bool forward);
    /// Ctrl+Shift+F: finds text in every statement's and proof's body.
    void findInProject();
    /// Ctrl+= and Ctrl+-: the editor's font size, kept in Settings.
    void zoomEditor(int delta);

    // What the pages ask for.
    /// A buffer's text or fields changed: the recovery file is written a second later.
    void bufferEdited(EditBuffer* buffer);
    void keepRecovered(EditBuffer* buffer);
    void discardRecovered(EditBuffer* buffer);
    void compare(EditBuffer* buffer);
    void keepMine(EditBuffer* buffer);
    void takeTheirs(EditBuffer* buffer);
    void restore(EditBuffer* buffer);
    void closeDeleted(EditBuffer* buffer);
    void markVerboseUpToDate(EditBuffer* buffer);
    /// **Write a verbose version**: an empty verbose buffer for the proof, saved as a new file.
    void writeVerbose(const QString& proofId);
    /// Dakota chose `failed` in a proof's level list: asks, then saves (or puts `previous` back).
    void chooseFailed(EditBuffer* buffer, const QString& previous);
    /// + of stated using (`statedUsing`) or premises (`premises`): asks for an ID and adds it.
    void addReference(EditBuffer* buffer, const QString& field);
    /// Shows a buffer's refused-save problems in the Problems panel.
    void showRefusalProblems(EditBuffer* buffer);
    /// Switches a buffer between the form and raw text; says why on the page if it can't.
    void setRaw(EditBuffer* buffer, bool raw);
    /// The app's settings.
    AppSettings* settings() const { return m_settings; }

signals:
    /// Whether any file has unsaved edits changed (the side bar's dot).
    void unsavedChanged(bool any);
    /// A refused save's problems, for the Problems panel (empty: none).
    void refusalProblems(const std::vector<Problem>& problems);
    /// A line for Output's Loom channel: what was written, refused and why, or reloaded.
    void outputLine(const QString& text);
    /// The project's recovery files have been listed and offered back.
    void recoveryOffered();
    /// A file was saved, or a save didn't happen.
    void saved(const QString& path);
    void saveFailed(const QString& path);
    /// A statement or proof was created and opened.
    void created(const QString& id);

protected:
    /// Back and forward from the mouse's buttons, anywhere in the tab.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// The buffer of a file, created and loaded from the snapshot if it isn't open yet.
    EditBuffer* ensureBuffer(const QString& path, FileKind kind, const QString& id);
    /// Rebuilds the pages of the open statement, selecting `select` (a proof ID, or empty).
    void rebuildPages(const QString& select);
    /// The proof IDs whose pages the open statement has: from the snapshot, plus any open buffer
    /// of a proof of it with unsaved edits (a file deleted under its edits keeps its page).
    QStringList pageIds() const;
    /// Compares every open buffer with the snapshot's files (Specs/m2-statements-review.md, "A file
    /// changed outside Loom").
    void checkOutside();
    /// Re-reads every page.
    void refreshPages();
    /// Tells the PDF pane what the tab shows: the statement on its page, the proof on a proof's.
    void updatePdfTarget();
    /// Updates the dots and unsavedChanged().
    void updateUnsaved();
    /// Lists the recovery files and offers them back.
    void offerDrafts(const std::vector<Draft>& drafts);
    /// The bar that offers back the text of a New statement or New proof form.
    void showFormDrafts();
    /// Handles a save's result.
    /// @param sent The text the save sent: edits typed since stay as unsaved edits.
    void finishSave(EditBuffer* buffer, const QByteArray& sent, const ChangeResult& result,
                    const SaveConfirmations& confirmations, std::function<void(bool)> done);
    /// Removes a buffer's recovery file, on the lane.
    void dropDraft(EditBuffer* buffer);
    /// Where the tab is now.
    Place currentPlace() const;
    /// Goes to a place without recording it.
    void goTo(const Place& place);
    /// Keeps every page's cursor in its buffer.
    void storeViews();
    /// Writes the open statement, history and cursors to the per-project state file.
    void saveState();
    /// Applies the editor font from the settings to every editor.
    void applyEditorFont();
    /// Writes the recovery files of the open New statement and New proof forms.
    void writeFormDrafts();
    /// Opens a form for a form draft, filled in.
    void openFormDraft(const Draft& draft);
    /// Runs a New statement's or New proof's Create.
    void createFromDialog(bool statement);

    AppSettings* m_settings;
    Lane* m_lane;
    QPointer<ProjectSession> m_session;
    std::shared_ptr<const ProjectSnapshot> m_snapshot;
    std::map<QString, std::unique_ptr<EditBuffer>> m_buffers;
    std::unique_ptr<QSettings> m_state;

    StatementList* m_list;
    QSplitter* m_splitter;
    QWidget* m_editorArea;
    QVBoxLayout* m_formDraftsLayout;
    QWidget* m_formDraftsBar;
    QLabel* m_empty;
    QTabWidget* m_tabs;
    QToolButton* m_newProofButton;
    PdfPane* m_pdf;
    std::vector<Problem> m_buildProblems;

    QString m_tag;          ///< The statement (or stub) open.
    bool m_stub = false;    ///< The open tag is a paper stub, read-only.
    std::vector<Place> m_back;
    std::vector<Place> m_forward;
    QTimer m_draftTimer;
    QTimer m_formDraftTimer;
    std::vector<Draft> m_formDrafts;
    bool m_unsaved = false;
    QString m_pendingOpen;  ///< A statement or proof just created, opened when the snapshot has it.
    QSet<QString> m_saving; ///< Files with a save on the lane: one at a time ("Busy controls").
    QPointer<NewStatementDialog> m_newStatement;
    QPointer<NewProofDialog> m_newProof;
    std::shared_ptr<const ProjectSnapshot> m_dialogSnapshot; ///< Kept alive for an open form.
};

} // namespace loom
