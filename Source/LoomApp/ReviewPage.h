#pragma once

// The Review tab (Specs/m2-statements-review.md, "The Review tab"): every uncommitted change in the
// project against `HEAD`, as math. The list (left) comes from `git status` and LoomCore's
// reviewList() (LoomCore/ReviewChanges.h), grouped; a statement entry (right) shows its changes in
// plain words, the statement built at `HEAD` and now side by side, the raw diff a click away, and
// **Open in Statements**. **Review never changes a file or git**: git runs only to read (on the Git
// lane), and builds write only under `.loom/` (on the Build lane).

#include "LoomApp/ProjectSnapshot.h"

#include "LoomCore/Build.h"
#include "LoomCore/GitOperations.h"
#include "LoomCore/ReviewChanges.h"

#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <atomic>
#include <memory>
#include <vector>

class QBuffer;
class QLabel;
class QPdfDocument;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace loom {

class AppSettings;
class Lane;
class LoomPdfView;
class ProjectSession;

/// One side of *Before and after*: a statement built at `HEAD` or now, its PDF, or its errors in
/// place of the PDF, or a line saying why there is nothing to show.
class ReviewSide : public QWidget
{
    Q_OBJECT

public:
    /// What the side shows.
    enum class Shows { Message, Pdf, Errors };

    /// @param title The heading: *At HEAD* or *Now*.
    explicit ReviewSide(const QString& title, QWidget* parent = nullptr);
    ~ReviewSide() override;

    /// Shows a line where the PDF goes (*"New: not in the last commit."*).
    void showMessage(const QString& text);
    /// Shows a PDF, read into memory (the file is never held open).
    void showPdf(const QByteArray& bytes);
    /// Shows a build's errors where the PDF goes, one per line as `file:line: message`.
    void showErrors(const QString& heading, const std::vector<Problem>& problems);
    /// Marks the side as building (a note beside its heading); what it shows stays until the result.
    void setBuilding(bool building);

    Shows shows() const { return m_shows; }
    LoomPdfView* view() const { return m_view; }
    QPdfDocument* document() const { return m_document; }
    QLabel* message() const { return m_message; }
    QPlainTextEdit* errors() const { return m_errors; }
    QLabel* state() const { return m_state; }

private:
    Shows m_shows = Shows::Message;
    QLabel* m_state;
    QStackedWidget* m_stack;
    QLabel* m_message;
    QPlainTextEdit* m_errors;
    LoomPdfView* m_view;
    QPdfDocument* m_document;
    std::unique_ptr<QBuffer> m_buffer;
};

/// The tab's page.
class ReviewPage : public QWidget
{
    Q_OBJECT

public:
    /// How long changes are gathered before the list is read again (the spec: "after 300 ms of quiet").
    static constexpr int quietMs = 300;

    /// @param settings  The app's settings: which git and `latexmk`, the timeout, the default mode.
    /// @param gitLane   Where `git status`, `HEAD`'s tree and raw diffs are read.
    /// @param buildLane Where the two builds run (two at once).
    ReviewPage(AppSettings* settings, Lane* gitLane, Lane* buildLane, QWidget* parent = nullptr);
    /// Stops running builds, so the lane's wait for them is short.
    ~ReviewPage() override;

    /// Shows a project, or none. Side effects: forgets the old project's list; reads the new one's.
    void setSession(ProjectSession* session);

    /// Reads the list again once things have been quiet for quietMs (from the snapshot and a new
    /// `git status`). Called for every new snapshot and when the tab is shown.
    void refresh();

    /// The latest list.
    const ReviewList& list() const { return m_list; }
    /// Whether a read of the list is queued or running.
    bool reading() const { return m_reading || m_quiet.isActive(); }
    /// How many builds are running.
    int building() const { return m_building; }
    /// Whether a raw diff is being read.
    bool readingDiff() const { return m_readingDiff; }

    /// Shows the entry with this key (a statement's tag, a paper's folder, a path); false if none.
    bool selectEntry(const QString& key);
    /// The key of the entry shown, or empty.
    QString shownEntry() const { return m_shownKey; }
    /// Sets the build mode and Verbose of *Before and after*, which rebuilds both sides.
    void setMode(BuildMode mode);
    void setVerbose(bool verbose);
    BuildMode mode() const { return m_mode; }
    bool verbose() const { return m_verbose; }
    /// **Raw diff**: replaces the PDFs with `git diff HEAD` of the entry's files.
    void setRawDiff(bool on);
    /// **Open in Statements**: the statement, or the proof whose file is selected in the changes.
    void openInStatements();

    // The parts, for tests.
    QTreeWidget* entries() const { return m_entries; }
    QLabel* note() const { return m_note; }
    QLabel* heading() const { return m_heading; }
    QTreeWidget* changes() const { return m_changes; }
    ReviewSide* beforeSide() const { return m_before; }
    ReviewSide* afterSide() const { return m_after; }
    QPlainTextEdit* rawDiff() const { return m_diff; }
    QToolButton* modeButton(BuildMode mode) const;
    QToolButton* verboseButton() const { return m_verboseButton; }
    QToolButton* linkButton() const { return m_linkButton; }
    QToolButton* rawDiffButton() const { return m_rawButton; }
    QPushButton* openButton() const { return m_openButton; }
    QSplitter* sides() const { return m_sides; }

signals:
    /// The number on the Review badge changed (every entry but the status changes).
    void countChanged(int count);
    /// A list was read and is shown.
    void listShown();
    /// Git commands were run: Output's Git channel logs each one.
    void gitSteps(const std::vector<GitStep>& steps);
    /// A build finished: Output's Build channel logs it.
    void buildFinished(const BuildResult& result);
    /// Both sides' builds of the entry shown have finished.
    void buildsFinished();
    /// A raw diff was read and is shown.
    void diffShown();
    /// **Open in Statements**: open this statement, or this proof when `proof` isn't empty.
    void openRequested(const QString& statement, const QString& proof);

protected:
    /// Reads the list when the tab is shown.
    void showEvent(QShowEvent* event) override;

private:
    struct Read;

    /// Builds the widgets.
    void createWidgets();
    /// Queues the read of the list on the Git lane.
    void read();
    /// Shows a list read, keeping the entry shown.
    void showList(Read read);
    /// Shows the selected entry, or the note.
    void showSelected();
    /// Shows an entry: its changes, its builds or raw diff.
    /// @param rebuild Whether to build both sides (a new entry, a new mode); otherwise only the
    ///                side of now, which LoomCore reuses when nothing changed.
    void showEntry(const ReviewEntry& entry, bool rebuild);
    /// Builds a side of the shown statement on the Build lane.
    void build(ReviewSide* side, bool atHead);
    /// Reads the raw diff of the shown entry's files on the Git lane.
    void readDiff();
    /// Shows the right controls for the shown entry.
    void updateControls();
    /// The shown entry, or null.
    const ReviewEntry* shown() const;
    /// Keeps the two PDF views scrolled together while *Link scrolling* is on.
    void linkScroll(LoomPdfView* from, LoomPdfView* to);

    AppSettings* m_settings;
    Lane* m_gitLane;
    Lane* m_buildLane;
    QPointer<ProjectSession> m_session;
    QTimer m_quiet;
    bool m_reading = false;
    bool m_again = false;
    int m_generation = 0;      ///< Bumped by setSession(): results for an old project are dropped.
    ReviewList m_list;
    QString m_head;            ///< `HEAD`'s hash when the list was read; empty with no commit.
    QString m_problem;         ///< Why there is no list (no repository, no git), or empty.
    /// The tree of `HEAD` read last, kept for the next read; touched on the Git lane only.
    struct HeadCache;
    std::shared_ptr<HeadCache> m_cache;

    QString m_shownKey;
    QString m_shownHead;       ///< The `HEAD` the side at `HEAD` was built from.
    BuildMode m_mode = BuildMode::Alone;
    bool m_verbose = false;
    bool m_raw = false;
    int m_building = 0;
    int m_buildGeneration = 0; ///< Bumped when both sides start again; older results are dropped.
    std::shared_ptr<std::atomic_bool> m_stop;
    bool m_readingDiff = false;
    int m_diffGeneration = 0;
    bool m_syncing = false;

    QTreeWidget* m_entries;
    QStackedWidget* m_right;
    QLabel* m_note;
    QWidget* m_entryPage;
    QLabel* m_heading;
    QTreeWidget* m_changes;
    QWidget* m_buildBar;
    QToolButton* m_modeButtons[3];
    QToolButton* m_verboseButton;
    QToolButton* m_linkButton;
    QToolButton* m_rawButton;
    QPushButton* m_openButton;
    QStackedWidget* m_content;
    QSplitter* m_sides;
    ReviewSide* m_before;
    ReviewSide* m_after;
    QPlainTextEdit* m_diff;
};

} // namespace loom
