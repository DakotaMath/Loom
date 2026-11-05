#pragma once

// The Statements tab's PDF pane (Specs/m2-statements-review.md, "The PDF pane"): the mode switch,
// the Verbose toggle, Compile, zoom and the build state above the PDF; builds of the open target
// on the Build lane; the PDF read into memory, so the file is never held open; the last good PDF,
// dimmed, while an error is unfixed; SyncTeX both ways; and find in the PDF.

#include "LoomApp/ProjectSnapshot.h"

#include "LoomCore/Build.h"
#include "LoomCore/SyncTex.h"

#include <QDateTime>
#include <QHash>
#include <QPdfView>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QWidget>

#include <atomic>
#include <memory>

class QBuffer;
class QButtonGroup;
class QGraphicsOpacityEffect;
class QLabel;
class QProgressBar;
class QLineEdit;
class QPdfDocument;
class QPdfSearchModel;
class QSettings;
class QToolButton;

namespace loom {

class AppSettings;
class Lane;
class ProjectSession;

/// Qt's PDF view, plus where its pages are (to turn a click into a place on a page and back) and a
/// brief mark on a box (forward search's "marks it briefly").
class LoomPdfView : public QPdfView
{
    Q_OBJECT

public:
    explicit LoomPdfView(QWidget* parent = nullptr);

    /// Where a page is in the viewport, in pixels; empty if there is no such page.
    /// Rule: the layout QPdfView draws (Qt 6.11, multi-page mode): each page is its size in points
    /// × the screen's logical dots per inch / 72 × the zoom (or, fitting the width, scaled to the
    /// viewport's width less the margins), pages one below another from the top margin with the
    /// page spacing between them, each centred in the wider of the viewport and the widest page
    /// with its margins; then moved by the scroll bars. Qt doesn't publish this layout, so it is
    /// recomputed here the same way.
    /// @param page From 0. No side effects.
    QRectF pageRect(int page) const;
    /// The page and point (in PDF points from the page's top-left) under a viewport position.
    /// @returns False if the position isn't on a page. No side effects.
    bool pointAt(const QPoint& position, int& page, QPointF& point) const;
    /// Marks a box (in PDF points on a page from 0) for a moment. Side effects: repaints.
    void mark(int page, const QRectF& box);
    /// Whether a mark is shown, for tests.
    bool marked() const { return m_markPage >= 0; }

signals:
    /// Double-clicked at a place on a page.
    void doubleClicked(int page, const QPointF& point);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    int m_markPage = -1;
    QRectF m_markBox;
    QTimer m_markTimer;
};

/// The pane.
class PdfPane : public QWidget
{
    Q_OBJECT

public:
    /// How long changes on disk are gathered before the shown build compiles again (the spec:
    /// "once, after 300 ms of quiet").
    static constexpr int quietMs = 300;
    /// How long a forward search's mark shows.
    static constexpr int markMs = 1500;

    /// @param settings The app's settings: `latexmk`, `synctex`, the timeout, the default mode.
    /// @param lane     The Build lane, where builds and `synctex` run.
    PdfPane(AppSettings* settings, Lane* lane, QWidget* parent = nullptr);
    /// Stops a running build, so the lane's wait for it is short.
    ~PdfPane() override;

    /// Shows a project's builds, or none.
    /// @param session The project, or null.
    /// @param state   The per-project state file, where each statement's mode and verbose toggle and
    ///                the zoom are kept; may be null. Must outlive the session's use here.
    /// Side effects: stops a running build; clears the PDF.
    void setSession(ProjectSession* session, QSettings* state);

    /// Shows the build of a target: a statement's tag or a proof ID; empty for none. A stub is
    /// shown a note instead (it's built only in a chain). A new target compiles (the build is reused
    /// when nothing changed).
    /// @param target    The target.
    /// @param statement The statement it belongs to (whose mode and verbose toggle are used).
    /// @param stub      Whether `target` is a paper stub.
    void setTarget(const QString& target, const QString& statement, bool stub = false);
    const QString& target() const { return m_target; }

    /// Compiles the shown target (Ctrl+B, Ctrl+S): a running build of it stops first.
    void compile();
    /// **Compile** (Ctrl+B): compiles unless a build is running ("Busy controls": the control
    /// that started a task doesn't start a second one).
    void compileClicked();
    /// Whether a build is running.
    bool building() const { return m_building; }

    /// The mode and Verbose toggle of the shown statement.
    BuildMode mode() const { return m_mode; }
    bool verbose() const { return m_verbose; }
    /// Sets them, remembers them for the statement, and compiles.
    void setMode(BuildMode mode);
    void setVerbose(bool verbose);

    /// Loom itself wrote a file (a save): it isn't a change *on disk* to compile for.
    /// @param path  Project-relative.
    /// @param bytes What was written.
    void noteWritten(const QString& path, const QByteArray& bytes);
    /// A new snapshot: if a file of the shown build changed on disk (or the statement's proofs
    /// changed), compiles once things have been quiet for quietMs.
    void snapshotChanged(const std::shared_ptr<const ProjectSnapshot>& snapshot);

    /// Ctrl+J: shows a place of a project file in the PDF (forward SyncTeX) and marks it; without
    /// `synctex`, or for a file the build doesn't have, shows the open statement's page.
    /// @param file Project-relative. @param line From 1. @param column From 0.
    void showPlace(const QString& file, int line, int column);
    /// Ctrl+F while the PDF has focus: the find bar above the PDF.
    void find();

    /// The latest build's result; hasResult() says whether there has been one.
    const BuildResult& result() const { return m_result; }
    bool hasResult() const { return m_hasResult; }
    /// The build state as the status bar shows it: *"Built 14:02 in 1.8 s"*, *"2 errors"*,
    /// *"Building…"*, *"Not built"*, *"Up to date"* (reused from an earlier session's build), or
    /// empty before any build.
    QString stateText() const;
    /// Whether the PDF shown is the last good one, dimmed.
    bool dimmed() const { return m_dimmed; }
    /// The page shown at the top, from 0.
    int currentPage() const;

    // The parts, for tests.
    LoomPdfView* view() const { return m_view; }
    QPdfDocument* document() const { return m_document; }
    QToolButton* modeButton(BuildMode mode) const;
    QToolButton* verboseButton() const { return m_verboseButton; }
    QToolButton* compileButton() const { return m_compileButton; }
    QLabel* stateLabel() const { return m_stateLabel; }
    QWidget* lastGoodBar() const { return m_lastGoodBar; }
    QLabel* messageLabel() const { return m_message; }
    QWidget* findBar() const { return m_findBar; }

signals:
    /// A build started.
    void buildStarted();
    /// A build finished (not one that was stopped or overtaken by a newer one).
    void buildFinished(const BuildResult& result);
    /// Double-clicking the PDF found this place (inverse SyncTeX): open it.
    void placeActivated(const QString& file, int line);
    /// A line for Output's Loom channel (what SyncTeX couldn't do).
    void outputLine(const QString& text);
    /// The target changed: the problems of the build shown so far aren't the open statement's.
    void targetChanged();

private:
    struct Built;

    /// Shows what a build made.
    void finish(Built built);
    /// Loads PDF bytes into the view, keeping the scroll position when `keepScroll`; else opening
    /// at `page` (from 1; 0 for the first).
    void loadPdf(const QByteArray& bytes, bool keepScroll, int page);
    /// Puts the scroll bars back where they were, once the view's new layout allows it.
    void restoreScroll(const QPoint& scroll);
    /// Shows a message where the PDF goes (no PDF).
    void showMessage(const QString& text);
    /// Updates the toolbar from the mode, toggle and build state.
    void updateControls();
    /// The key of the shown document: target, mode and toggle.
    QString documentKey() const;
    /// Jumps to a page (from 1) and point (PDF points), keeping the zoom.
    void jumpTo(int page, const QPointF& point = {});
    /// Applies a zoom: 0 fits the width; else a factor. Remembered per project.
    void setZoom(double zoom);
    /// Double-click: inverse SyncTeX on the lane.
    void inverse(int page, const QPointF& point);
    /// The build folder of the shown document, absolute.
    QString folderPath() const;
    /// The SyncTeX tool from the settings.
    SyncTexTool synctexTool() const;
    /// The statement's or proof's page from main.aux, or 0.
    int targetPage() const;

    AppSettings* m_settings;
    Lane* m_lane;
    QPointer<ProjectSession> m_session;
    QSettings* m_state = nullptr;

    QString m_target;
    QString m_statement;
    bool m_stub = false;
    BuildMode m_mode = BuildMode::Alone;
    bool m_verbose = false;

    BuildResult m_result;
    bool m_hasResult = false;
    QDateTime m_finished;    ///< When the latest build that ran latexmk finished.
    bool m_upToDate = false; ///< The latest build was reused from before this session's builds.
    QString m_shownKey;      ///< The document the PDF shown is of.
    QByteArray m_aux;        ///< Its `main.aux`.
    bool m_dimmed = false;
    bool m_building = false;
    int m_generation = 0;    ///< Builds started; a result of an older one is dropped.
    std::shared_ptr<std::atomic_bool> m_stop;
    /// The project files the shown build read (statements, proofs, verbose versions) with their
    /// bytes then, to see a change on disk; and the target statement's proofs then.
    QHash<QString, QByteArray> m_inputs;
    /// Loom's own saves that no snapshot has caught up with yet: the bytes written, and the
    /// snapshots' bytes from before.
    struct Written {
        QByteArray bytes;
        QByteArray before;
    };
    QHash<QString, Written> m_written;
    std::shared_ptr<const ProjectSnapshot> m_compileSnapshot; ///< The snapshot when the latest build started.
    QString m_shownFolder;   ///< The build folder of the PDF shown, project-relative.
    QByteArray m_goodAux;    ///< `main.aux` of the latest build without errors, of document m_goodKey.
    QString m_goodKey;
    QStringList m_proofs;
    QTimer m_quiet;
    bool m_toldNoSynctex = false;

    QToolButton* m_modeButtons[3];
    QButtonGroup* m_modes;
    QToolButton* m_verboseButton;
    QToolButton* m_compileButton;
    QProgressBar* m_spinner;
    QToolButton* m_fitWidth;
    QToolButton* m_zoomIn;
    QToolButton* m_zoomOut;
    QLabel* m_stateLabel;
    QWidget* m_lastGoodBar;
    QLabel* m_lastGoodText;
    QLabel* m_message;
    QWidget* m_findBar;
    QLineEdit* m_findText;
    QLabel* m_findCount;
    LoomPdfView* m_view;
    QPdfDocument* m_document;
    QPdfSearchModel* m_search;
    QGraphicsOpacityEffect* m_dim;
    std::unique_ptr<QBuffer> m_buffer;
    int m_findIndex = -1;
    bool m_findPending = false; ///< Enter was pressed before the search found anything.
    /// What loadPdf() does once the document is ready: restore the scroll, or jump to a page.
    int m_pendingPage = 0;
    QPoint m_pendingScroll;
    bool m_pendingKeep = false;
};

} // namespace loom
