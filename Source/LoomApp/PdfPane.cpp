#include "LoomApp/PdfPane.h"

#include "LoomApp/AppSettings.h"
#include "LoomApp/Lane.h"
#include "LoomApp/ProjectSession.h"

#include <QBuffer>
#include <QButtonGroup>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QPdfSearchModel>
#include <QProgressBar>
#include <QScreen>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace loom {

namespace {

/// "1 error", "2 errors".
QString errorCount(int n)
{
    return n == 1 ? QStringLiteral("1 error") : QStringLiteral("%1 errors").arg(n);
}

/// The proofs of a statement in a snapshot, in file order.
QStringList proofsOf(const ProjectSnapshot* snapshot, const QString& tag)
{
    QStringList ids;
    if (!snapshot)
        return ids;
    for (const ProofFile& proof : snapshot->contents.proofs) {
        if (proof.folder == tag)
            ids << proof.id;
    }
    return ids;
}

/// A time as the pane shows it: the time of day today, else the date too.
QString shownTime(const QDateTime& time)
{
    const QDateTime local = time.toLocalTime();
    return local.date() == QDate::currentDate() ? local.toString(QStringLiteral("HH:mm"))
                                                : local.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

/// The key under which the per-project state keeps a statement's mode and Verbose toggle.
QString compileKey(const QString& tag)
{
    return QStringLiteral("compile/") + tag;
}

} // namespace

// LoomPdfView

LoomPdfView::LoomPdfView(QWidget* parent)
    : QPdfView(parent)
{
    setObjectName(QStringLiteral("pdfView"));
    setPageMode(PageMode::MultiPage);
    setZoomMode(ZoomMode::FitToWidth);
    // Always there: at Fit width, a scroll bar that comes and goes changes the width the page is
    // fitted to, and at some widths QPdfView lays out for ever (the window hangs).
    // Wiki/gotchas/qt/qpdfview-fit-width-scrollbar-loop.md
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_markTimer.setSingleShot(true);
    m_markTimer.setInterval(PdfPane::markMs);
    connect(&m_markTimer, &QTimer::timeout, this, [this] {
        m_markPage = -1;
        viewport()->update();
    });
}

QRectF LoomPdfView::pageRect(int page) const
{
    const QPdfDocument* doc = document();
    if (!doc || doc->status() != QPdfDocument::Status::Ready || page < 0 || page >= doc->pageCount())
        return {};
    const qreal resolution = QGuiApplication::primaryScreen()->logicalDotsPerInch() / 72.0;
    const QMargins margins = documentMargins();
    const int viewportWidth = viewport()->width();
    // The sizes, as QPdfView computes them.
    const auto sizeOf = [&](int p) {
        if (zoomMode() == ZoomMode::FitToWidth) {
            QSize size = QSizeF(doc->pagePointSize(p) * resolution).toSize();
            if (size.width() <= 0)
                return size;
            const qreal factor = qreal(viewportWidth - margins.left() - margins.right()) / qreal(size.width());
            return size * factor;
        }
        return QSizeF(doc->pagePointSize(p) * resolution * zoomFactor()).toSize();
    };
    int totalWidth = 0;
    int y = margins.top();
    QSize size;
    for (int p = 0; p < doc->pageCount(); ++p) {
        const QSize s = sizeOf(p);
        totalWidth = std::max(totalWidth, s.width());
        if (p < page)
            y += s.height() + pageSpacing();
        if (p == page)
            size = s;
    }
    totalWidth += margins.left() + margins.right();
    const int x = (std::max(totalWidth, viewportWidth) - size.width()) / 2;
    return QRectF(x - horizontalScrollBar()->value(), y - verticalScrollBar()->value(), size.width(), size.height());
}

bool LoomPdfView::pointAt(const QPoint& position, int& page, QPointF& point) const
{
    const QPdfDocument* doc = document();
    if (!doc || doc->status() != QPdfDocument::Status::Ready)
        return false;
    for (int p = 0; p < doc->pageCount(); ++p) {
        const QRectF rect = pageRect(p);
        if (!rect.contains(position))
            continue;
        const qreal scale = rect.width() / doc->pagePointSize(p).width();
        page = p;
        point = QPointF((position.x() - rect.left()) / scale, (position.y() - rect.top()) / scale);
        return true;
    }
    return false;
}

void LoomPdfView::mark(int page, const QRectF& box)
{
    m_markPage = page;
    m_markBox = box;
    m_markTimer.start();
    viewport()->update();
}

void LoomPdfView::paintEvent(QPaintEvent* event)
{
    QPdfView::paintEvent(event);
    if (m_markPage < 0 || !document())
        return;
    const QRectF rect = pageRect(m_markPage);
    if (rect.isEmpty())
        return;
    const qreal scale = rect.width() / document()->pagePointSize(m_markPage).width();
    QRectF box(rect.left() + m_markBox.x() * scale, rect.top() + m_markBox.y() * scale, m_markBox.width() * scale,
               m_markBox.height() * scale);
    if (box.width() < 4)
        box.setWidth(rect.width() / 3);
    if (box.height() < 4)
        box.setHeight(14);
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(230, 120, 0), 2));
    painter.setBrush(QColor(255, 190, 0, 60));
    painter.drawRoundedRect(box.adjusted(-3, -2, 3, 2), 3, 3);
}

void LoomPdfView::mouseDoubleClickEvent(QMouseEvent* event)
{
    int page = 0;
    QPointF point;
    if (event->button() == Qt::LeftButton && pointAt(event->position().toPoint(), page, point)) {
        emit doubleClicked(page, point);
        return;
    }
    QPdfView::mouseDoubleClickEvent(event);
}

// PdfPane

/// What a build made, carried from the Build lane back to the pane.
struct PdfPane::Built {
    int generation = 0;
    QString key;            ///< The document: target, mode and toggle.
    BuildResult result;
    QString folder;         ///< The build folder, project-relative; empty if there is none.
    QByteArray pdf;         ///< `main.pdf` when it succeeded; else `last-good.pdf`, if any.
    QDateTime lastGoodTime; ///< When `last-good.pdf` was written, when `pdf` is it.
    QByteArray aux;         ///< `main.aux`.
    QStringList inputs;     ///< The project files the build read (its copies), and any it couldn't read.
};

PdfPane::PdfPane(AppSettings* settings, Lane* lane, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_lane(lane)
    , m_modes(new QButtonGroup(this))
    , m_verboseButton(new QToolButton(this))
    , m_compileButton(new QToolButton(this))
    , m_spinner(new QProgressBar(this))
    , m_fitWidth(new QToolButton(this))
    , m_zoomIn(new QToolButton(this))
    , m_zoomOut(new QToolButton(this))
    , m_stateLabel(new QLabel(this))
    , m_lastGoodBar(new QWidget(this))
    , m_lastGoodText(new QLabel(m_lastGoodBar))
    , m_message(new QLabel(this))
    , m_findBar(new QWidget(this))
    , m_findText(new QLineEdit(m_findBar))
    , m_findCount(new QLabel(m_findBar))
    , m_view(new LoomPdfView(this))
    , m_document(new QPdfDocument(this))
    , m_search(new QPdfSearchModel(this))
    , m_dim(new QGraphicsOpacityEffect(this))
{
    setObjectName(QStringLiteral("pdfPane"));

    // Above the PDF: the mode switch, Verbose, Compile, zoom and the build state.
    const struct {
        BuildMode mode;
        const char* name;
        const char* label;
        const char* tip;
    } modes[] = {
        {BuildMode::Alone, "modeAlone", "Alone", "The statement and its proofs"},
        {BuildMode::Premises, "modePremises", "+ Premises", "Also what it is stated using and its proofs' direct premises"},
        {BuildMode::Chain, "modeChain", "+ Chain", "Also everything it rests on, with the proofs their statuses come from"},
    };
    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(6, 4, 6, 4);
    bar->setSpacing(2);
    for (int i = 0; i < 3; ++i) {
        auto* button = new QToolButton(this);
        button->setObjectName(QString::fromLatin1(modes[i].name));
        button->setText(QString::fromLatin1(modes[i].label));
        button->setToolTip(QString::fromLatin1(modes[i].tip));
        button->setCheckable(true);
        button->setAutoRaise(true);
        m_modes->addButton(button, static_cast<int>(modes[i].mode));
        m_modeButtons[i] = button;
        bar->addWidget(button);
    }
    m_modes->setExclusive(true);
    connect(m_modes, &QButtonGroup::idClicked, this, [this](int id) { setMode(static_cast<BuildMode>(id)); });
    bar->addSpacing(8);
    m_verboseButton->setObjectName(QStringLiteral("verbose"));
    m_verboseButton->setText(QStringLiteral("Verbose"));
    m_verboseButton->setToolTip(QStringLiteral("Show each proof's verbose version, where it has one"));
    m_verboseButton->setCheckable(true);
    m_verboseButton->setAutoRaise(true);
    connect(m_verboseButton, &QToolButton::clicked, this, [this](bool on) { setVerbose(on); });
    bar->addWidget(m_verboseButton);
    bar->addSpacing(8);
    m_compileButton->setObjectName(QStringLiteral("compile"));
    m_compileButton->setText(QStringLiteral("Compile"));
    m_compileButton->setToolTip(QStringLiteral("Compile the saved files (Ctrl+B)"));
    connect(m_compileButton, &QToolButton::clicked, this, &PdfPane::compileClicked);
    bar->addWidget(m_compileButton);
    // "While a task runs, the control that started it shows a spinner."
    m_spinner->setObjectName(QStringLiteral("compileSpinner"));
    m_spinner->setRange(0, 0);
    m_spinner->setTextVisible(false);
    m_spinner->setFixedSize(36, 8);
    m_spinner->hide();
    bar->addWidget(m_spinner);
    bar->addStretch(1);
    m_zoomOut->setObjectName(QStringLiteral("zoomOut"));
    m_zoomOut->setText(QStringLiteral("−"));
    m_zoomOut->setToolTip(QStringLiteral("Zoom out"));
    m_zoomOut->setAutoRaise(true);
    m_fitWidth->setObjectName(QStringLiteral("fitWidth"));
    m_fitWidth->setText(QStringLiteral("Fit width"));
    m_fitWidth->setCheckable(true);
    m_fitWidth->setChecked(true); // The view starts fitting the width.
    m_fitWidth->setAutoRaise(true);
    m_zoomIn->setObjectName(QStringLiteral("zoomIn"));
    m_zoomIn->setText(QStringLiteral("+"));
    m_zoomIn->setToolTip(QStringLiteral("Zoom in"));
    m_zoomIn->setAutoRaise(true);
    const auto effectiveZoom = [this] {
        if (m_view->zoomMode() != QPdfView::ZoomMode::FitToWidth || m_document->status() != QPdfDocument::Status::Ready)
            return m_view->zoomFactor();
        const qreal resolution = QGuiApplication::primaryScreen()->logicalDotsPerInch() / 72.0;
        return m_view->pageRect(0).width() / (m_document->pagePointSize(0).width() * resolution);
    };
    connect(m_zoomOut, &QToolButton::clicked, this, [this, effectiveZoom] { setZoom(std::max(0.25, effectiveZoom() / 1.2)); });
    connect(m_zoomIn, &QToolButton::clicked, this, [this, effectiveZoom] { setZoom(std::min(6.0, effectiveZoom() * 1.2)); });
    connect(m_fitWidth, &QToolButton::clicked, this, [this, effectiveZoom](bool on) { setZoom(on ? 0 : effectiveZoom()); });
    bar->addWidget(m_zoomOut);
    bar->addWidget(m_fitWidth);
    bar->addWidget(m_zoomIn);
    bar->addSpacing(8);
    m_stateLabel->setObjectName(QStringLiteral("pdfBuildState"));
    m_stateLabel->setForegroundRole(QPalette::PlaceholderText);
    bar->addWidget(m_stateLabel);

    // "While an error is unfixed the pane shows last-good.pdf, dimmed, under a bar."
    m_lastGoodBar->setObjectName(QStringLiteral("lastGoodBar"));
    m_lastGoodBar->setAttribute(Qt::WA_StyledBackground);
    m_lastGoodBar->setStyleSheet(QStringLiteral("#lastGoodBar { background: palette(alternate-base); border-bottom: 1px solid palette(mid); }"));
    m_lastGoodText->setObjectName(QStringLiteral("lastGoodText"));
    m_lastGoodText->setWordWrap(true);
    auto* lastGoodLayout = new QHBoxLayout(m_lastGoodBar);
    lastGoodLayout->setContentsMargins(10, 5, 10, 5);
    lastGoodLayout->addWidget(m_lastGoodText, 1);
    m_lastGoodBar->hide();

    // Find in the PDF.
    m_findBar->setObjectName(QStringLiteral("pdfFind"));
    m_findText->setObjectName(QStringLiteral("pdfFindText"));
    m_findText->setPlaceholderText(QStringLiteral("Find in the PDF"));
    m_findText->setClearButtonEnabled(true);
    m_findCount->setObjectName(QStringLiteral("pdfFindCount"));
    auto* previous = new QToolButton(m_findBar);
    previous->setText(QStringLiteral("↑"));
    previous->setToolTip(QStringLiteral("Previous (Shift+Enter)"));
    previous->setAutoRaise(true);
    auto* next = new QToolButton(m_findBar);
    next->setText(QStringLiteral("↓"));
    next->setToolTip(QStringLiteral("Next (Enter)"));
    next->setAutoRaise(true);
    auto* close = new QToolButton(m_findBar);
    close->setText(QStringLiteral("×"));
    close->setToolTip(QStringLiteral("Close (Esc)"));
    close->setAutoRaise(true);
    auto* findLayout = new QHBoxLayout(m_findBar);
    findLayout->setContentsMargins(6, 2, 6, 2);
    findLayout->addWidget(m_findText, 1);
    findLayout->addWidget(m_findCount);
    findLayout->addWidget(previous);
    findLayout->addWidget(next);
    findLayout->addWidget(close);
    m_findBar->hide();
    m_search->setDocument(m_document);
    m_view->setSearchModel(m_search);
    const auto step = [this](bool forward) {
        const int count = m_search->rowCount({});
        if (count == 0) {
            // Qt PDF searches page by page as it goes: the first result found will be shown.
            m_findPending = !m_findText->text().isEmpty();
            m_findCount->setText(m_findText->text().isEmpty() ? QString() : QStringLiteral("No matches"));
            return;
        }
        m_findPending = false;
        m_findIndex = forward ? (m_findIndex + 1) % count : (m_findIndex <= 0 ? count - 1 : m_findIndex - 1);
        m_view->setCurrentSearchResultIndex(m_findIndex);
        const QPdfLink link = m_search->resultAtIndex(m_findIndex);
        jumpTo(link.page() + 1, link.location());
        m_findCount->setText(QStringLiteral("%1 of %2").arg(m_findIndex + 1).arg(count));
    };
    connect(m_findText, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_findIndex = -1;
        m_search->setSearchString(text);
        m_findCount->clear();
    });
    connect(m_findText, &QLineEdit::returnPressed, this, [step] {
        step(!(QGuiApplication::keyboardModifiers() & Qt::ShiftModifier));
    });
    connect(m_search, &QPdfSearchModel::countChanged, this, [this, step] {
        if (m_findPending && m_search->rowCount({}) > 0)
            step(true);
    });
    connect(next, &QToolButton::clicked, this, [step] { step(true); });
    connect(previous, &QToolButton::clicked, this, [step] { step(false); });
    connect(close, &QToolButton::clicked, m_findBar, &QWidget::hide);
    auto* escape = new QShortcut(QKeySequence(Qt::Key_Escape), m_findBar, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(escape, &QShortcut::activated, this, [this] {
        m_findBar->hide();
        m_view->setFocus();
    });

    m_message->setObjectName(QStringLiteral("pdfMessage"));
    m_message->setWordWrap(true);
    m_message->setAlignment(Qt::AlignCenter);
    m_message->setForegroundRole(QPalette::PlaceholderText);
    m_message->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_message->setMargin(16);

    m_view->setDocument(m_document);
    m_view->setFocusPolicy(Qt::StrongFocus);
    m_dim->setOpacity(0.4);
    m_dim->setEnabled(false);
    m_view->setGraphicsEffect(m_dim);
    connect(m_view, &LoomPdfView::doubleClicked, this, &PdfPane::inverse);
    connect(m_document, &QPdfDocument::statusChanged, this, [this](QPdfDocument::Status status) {
        if (status != QPdfDocument::Status::Ready)
            return;
        // After the view has laid the document out (it reacts to the same signal).
        QTimer::singleShot(0, this, [this] {
            if (m_document->status() != QPdfDocument::Status::Ready)
                return;
            if (m_pendingKeep) {
                restoreScroll(m_pendingScroll);
            } else {
                jumpTo(std::max(1, m_pendingPage));
            }
            m_pendingKeep = false;
            m_pendingPage = 0;
        });
    });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addLayout(bar);
    layout->addWidget(m_lastGoodBar);
    layout->addWidget(m_findBar);
    layout->addWidget(m_view, 1);
    layout->addWidget(m_message, 1);

    m_quiet.setSingleShot(true);
    m_quiet.setInterval(quietMs);
    connect(&m_quiet, &QTimer::timeout, this, &PdfPane::compile);

    showMessage(QStringLiteral("The compiled PDF of the open statement comes here."));
    updateControls();
}

PdfPane::~PdfPane()
{
    if (m_stop)
        *m_stop = true;
    m_document->close(); // Before the buffer it reads goes.
}

QToolButton* PdfPane::modeButton(BuildMode mode) const
{
    return m_modeButtons[static_cast<int>(mode)];
}

void PdfPane::setSession(ProjectSession* session, QSettings* state)
{
    if (m_stop)
        *m_stop = true;
    m_stop.reset();
    ++m_generation;
    m_building = false;
    m_quiet.stop();
    m_session = session;
    m_state = state;
    m_target.clear();
    m_statement.clear();
    m_stub = false;
    m_result = BuildResult();
    m_hasResult = false;
    m_shownKey.clear();
    m_aux.clear();
    m_goodAux.clear();
    m_goodKey.clear();
    m_shownFolder.clear();
    m_inputs.clear();
    m_written.clear();
    m_compileSnapshot.reset();
    m_proofs.clear();
    m_toldNoSynctex = false;
    m_document->close();
    m_buffer.reset();
    m_dimmed = false;
    m_dim->setEnabled(false);
    m_lastGoodBar->hide();
    m_findBar->hide();
    if (m_state) {
        const double zoom = m_state->value(QStringLiteral("pdf/zoom"), 0.0).toDouble();
        m_fitWidth->setChecked(zoom <= 0);
        if (zoom <= 0) {
            m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
        } else {
            m_view->setZoomMode(QPdfView::ZoomMode::Custom);
            m_view->setZoomFactor(zoom);
        }
    }
    showMessage(session ? QStringLiteral("Choose a statement to see its compiled PDF.")
                        : QStringLiteral("The compiled PDF of the open statement comes here."));
    updateControls();
}

void PdfPane::setTarget(const QString& target, const QString& statement, bool stub)
{
    if (target == m_target && stub == m_stub && statement == m_statement)
        return;
    m_target = target;
    m_statement = statement;
    m_stub = stub;
    m_quiet.stop();
    m_inputs.clear();
    m_proofs.clear();
    // Nothing of the previous target's build applies to this one: its page, nor its problems.
    m_aux.clear();
    emit targetChanged();
    // The mode and toggle are remembered per statement ("remembered per statement").
    m_mode = m_settings->defaultCompileMode();
    m_verbose = false;
    if (m_state && !statement.isEmpty()) {
        const QStringList saved = m_state->value(compileKey(statement)).toString().split(u'|');
        if (const std::optional<BuildMode> mode = buildModeFromName(saved.value(0)))
            m_mode = *mode;
        m_verbose = saved.value(1) == u"1";
    }
    if (target.isEmpty() || stub || !m_session) {
        if (m_stop)
            *m_stop = true;
        ++m_generation;
        m_building = false;
        m_shownKey.clear();
        m_document->close();
        m_buffer.reset();
        m_dimmed = false;
        m_dim->setEnabled(false);
        m_lastGoodBar->hide();
        showMessage(stub ? QStringLiteral("%1 is a result from a paper; it's built only as part of a statement's chain.").arg(target)
                         : QStringLiteral("Choose a statement to see its compiled PDF."));
        updateControls();
        return;
    }
    compile();
}

void PdfPane::setMode(BuildMode mode)
{
    m_mode = mode;
    if (m_state && !m_statement.isEmpty())
        m_state->setValue(compileKey(m_statement), QStringLiteral("%1|%2").arg(buildModeName(m_mode)).arg(m_verbose ? 1 : 0));
    updateControls();
    compile();
}

void PdfPane::setVerbose(bool verbose)
{
    m_verbose = verbose;
    setMode(m_mode);
}

QString PdfPane::documentKey() const
{
    return QStringLiteral("%1|%2|%3").arg(m_target, buildModeName(m_mode)).arg(m_verbose ? 1 : 0);
}

QString PdfPane::folderPath() const
{
    if (!m_session || m_shownFolder.isEmpty())
        return {};
    return m_session->root() + u'/' + m_shownFolder;
}

SyncTexTool PdfPane::synctexTool() const
{
    SyncTexTool tool;
    tool.program = m_settings->synctexProgram();
    return tool;
}

void PdfPane::compileClicked()
{
    if (!m_building)
        compile();
}

void PdfPane::compile()
{
    if (!m_session || m_target.isEmpty() || m_stub)
        return;
    // "A newer build of the same folder stops the running one the same way first."
    if (m_stop)
        *m_stop = true;
    m_stop = std::make_shared<std::atomic_bool>(false);
    m_quiet.stop();
    const int generation = ++m_generation;
    m_building = true;
    m_compileSnapshot = m_session->snapshot();
    m_proofs = proofsOf(m_compileSnapshot.get(), m_statement);

    BuildRequest request;
    request.root = m_session->root();
    request.target = m_target;
    request.mode = m_mode;
    request.verbose = m_verbose;
    request.timeoutMs = m_settings->compileTimeoutSeconds() * 1000;
    request.latexmk = m_settings->latexmkProgram();
    request.git = m_settings->gitTool();
    request.stop = m_stop;
    const QString key = documentKey();
    updateControls();
    emit buildStarted();
    m_lane->run(this,
        [request, key, generation] {
            Built built;
            built.generation = generation;
            built.key = key;
            built.result = buildTarget(request);
            if (built.result.outcome == BuildOutcome::Stopped)
                return built;
            // A build that stopped at a file it couldn't read has no folder in its result, but its
            // folder may hold a last good PDF from before.
            built.folder = built.result.folder;
            if (built.folder.isEmpty() && built.result.outcome == BuildOutcome::Failed && !built.result.target.isEmpty())
                built.folder = buildFolder(built.result.target, request.mode, request.verbose);
            const BuildOutput output = readBuildOutput(request.root, built.folder);
            if (built.result.outcome == BuildOutcome::Succeeded)
                built.pdf = output.pdf;
            if (built.pdf.isEmpty()) {
                built.pdf = output.lastGoodPdf;
                built.lastGoodTime = output.lastGoodTime;
            }
            built.aux = output.aux;
            built.inputs = output.inputs;
            // A file the build couldn't read is watched too, so fixing it compiles again.
            for (const Problem& problem : built.result.problems) {
                if (!built.inputs.contains(problem.file)
                    && (problem.file.startsWith(QStringLiteral("Statements/")) || problem.file.startsWith(QStringLiteral("Proofs/"))))
                    built.inputs << problem.file;
            }
            return built;
        },
        [this](Built built) { finish(std::move(built)); });
}

void PdfPane::finish(Built built)
{
    if (built.generation != m_generation)
        return; // Overtaken by a newer build, or the target changed.
    m_building = false;
    if (built.result.outcome == BuildOutcome::Stopped) {
        updateControls();
        return;
    }
    const bool sameDocument = built.key == m_shownKey && m_document->status() == QPdfDocument::Status::Ready;
    // A reused build of the document shown keeps the time and length of the build it reuses.
    const bool keepTime = built.result.reused && sameDocument && m_hasResult && m_result.outcome == BuildOutcome::Succeeded;
    const double seconds = m_result.seconds;
    m_result = built.result;
    m_hasResult = true;
    m_upToDate = built.result.reused && !keepTime;
    if (keepTime)
        m_result.seconds = seconds;
    else
        m_finished = QDateTime::currentDateTime();
    // What the build read, as Loom knew the files when it started: from the snapshot then, or
    // from Loom's own saves since. A change on disk during the build then differs from this, and
    // the next snapshot compiles again.
    m_inputs.clear();
    for (const QString& path : built.inputs) {
        const auto written = m_written.constFind(path);
        m_inputs.insert(path, written != m_written.cend() ? written->bytes
                                                          : (m_compileSnapshot ? m_compileSnapshot->files.value(path) : QByteArray()));
    }
    m_shownKey = built.key;
    m_shownFolder = built.folder;
    m_aux = built.aux;
    if (m_result.outcome == BuildOutcome::Succeeded) {
        m_goodAux = built.aux;
        m_goodKey = built.key;
    }
    const int errors = m_result.errors();
    if (m_result.outcome == BuildOutcome::Succeeded) {
        m_dimmed = false;
        m_lastGoodBar->hide();
        loadPdf(built.pdf, sameDocument, sameDocument ? 0 : targetPage());
    } else if (!built.pdf.isEmpty()) {
        m_dimmed = true;
        const QString time = shownTime(built.lastGoodTime);
        if (m_result.outcome == BuildOutcome::Failed)
            m_lastGoodText->setText(QStringLiteral("Showing the last good build (%1). %2: fix them to update it.")
                                        .arg(time, errorCount(errors)));
        else
            m_lastGoodText->setText(QStringLiteral("%1 Showing the last good build (%2).").arg(m_result.message, time));
        m_lastGoodBar->show();
        loadPdf(built.pdf, sameDocument, sameDocument ? 0 : targetPage());
    } else {
        // "With no good build yet: the errors only."
        m_dimmed = false;
        m_lastGoodBar->hide();
        m_document->close();
        m_buffer.reset();
        QString text;
        if (m_result.outcome == BuildOutcome::Failed) {
            QStringList lines;
            for (const Problem& problem : m_result.problems) {
                if (problem.severity != Severity::Error)
                    continue;
                lines << QStringLiteral("%1:%2: %3").arg(problem.file).arg(problem.line).arg(problem.message);
                if (lines.size() == 10)
                    break;
            }
            text = QStringLiteral("No PDF yet: %1.\n\n%2").arg(errorCount(errors), lines.join(u'\n'));
        } else {
            text = m_result.message;
        }
        showMessage(text);
    }
    m_dim->setEnabled(m_dimmed);
    updateControls();
    emit buildFinished(m_result);
}

int PdfPane::targetPage() const
{
    // While dimmed, the PDF shown is the last good build's: its own `main.aux`, when the pane
    // has it, says where things are in it.
    const QByteArray& aux = m_dimmed && m_goodKey == m_shownKey && !m_goodAux.isEmpty() ? m_goodAux : m_aux;
    if (const int page = auxLabelPage(aux, m_target); page > 0)
        return page;
    return auxLabelPage(aux, m_statement);
}

void PdfPane::loadPdf(const QByteArray& bytes, bool keepScroll, int page)
{
    m_pendingKeep = keepScroll;
    m_pendingScroll = QPoint(m_view->horizontalScrollBar()->value(), m_view->verticalScrollBar()->value());
    m_pendingPage = page;
    // Read into memory, never held open: on Windows an open PDF can't be overwritten by the next build.
    m_document->close();
    auto buffer = std::make_unique<QBuffer>();
    buffer->setData(bytes);
    buffer->open(QIODevice::ReadOnly);
    m_document->load(buffer.get());
    m_buffer = std::move(buffer);
    m_message->hide();
    m_view->show();
    if (m_findBar->isVisible() && !m_findText->text().isEmpty()) {
        m_findIndex = -1;
        m_search->setSearchString(m_findText->text());
    }
}

void PdfPane::restoreScroll(const QPoint& scroll)
{
    // The view sets its scroll ranges for a new document as it lays it out, which can come after
    // the document is ready; until the range reaches the old place, setting it would be clamped.
    QScrollBar* down = m_view->verticalScrollBar();
    QScrollBar* across = m_view->horizontalScrollBar();
    const auto apply = [down, across, scroll] {
        across->setValue(scroll.x());
        down->setValue(scroll.y());
        return down->value() == scroll.y() && across->value() == scroll.x();
    };
    if (apply())
        return;
    auto connection = std::make_shared<QMetaObject::Connection>();
    *connection = connect(down, &QScrollBar::rangeChanged, this, [apply, connection] {
        if (apply())
            disconnect(*connection);
    });
    // Give up after a moment (the document may be shorter now).
    QTimer::singleShot(1000, this, [connection] { disconnect(*connection); });
}

void PdfPane::showMessage(const QString& text)
{
    m_message->setText(text);
    m_view->hide();
    m_message->show();
}

void PdfPane::jumpTo(int page, const QPointF& point)
{
    if (m_document->status() != QPdfDocument::Status::Ready || page < 1 || page > m_document->pageCount())
        return;
    const QRectF rect = m_view->pageRect(page - 1);
    if (rect.isEmpty())
        return;
    const qreal scale = rect.width() / m_document->pagePointSize(page - 1).width();
    // The page's top for a page; a place a third of the way down the view for a point.
    const int offset = point.isNull() ? m_view->documentMargins().top() : m_view->viewport()->height() / 3;
    QScrollBar* bar = m_view->verticalScrollBar();
    bar->setValue(bar->value() + static_cast<int>(rect.top() + point.y() * scale) - offset);
    if (!point.isNull()) {
        // Zoomed in, the place may be off to the side too.
        QScrollBar* across = m_view->horizontalScrollBar();
        across->setValue(across->value() + static_cast<int>(rect.left() + point.x() * scale) - m_view->viewport()->width() / 4);
    }
}

int PdfPane::currentPage() const
{
    return m_view->pageNavigator()->currentPage();
}

void PdfPane::setZoom(double zoom)
{
    if (zoom <= 0) {
        m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    } else {
        m_view->setZoomMode(QPdfView::ZoomMode::Custom);
        m_view->setZoomFactor(zoom);
    }
    m_fitWidth->setChecked(zoom <= 0);
    if (m_state)
        m_state->setValue(QStringLiteral("pdf/zoom"), zoom);
}

void PdfPane::updateControls()
{
    const bool usable = m_session && !m_target.isEmpty() && !m_stub;
    for (int i = 0; i < 3; ++i) {
        m_modeButtons[i]->setEnabled(usable);
        m_modeButtons[i]->setChecked(static_cast<int>(m_mode) == i);
    }
    m_verboseButton->setEnabled(usable);
    m_verboseButton->setChecked(m_verbose);
    m_compileButton->setEnabled(usable && !m_building);
    m_compileButton->setText(m_building ? QStringLiteral("Compiling…") : QStringLiteral("Compile"));
    m_spinner->setVisible(m_building);
    if (m_building)
        m_stateLabel->setText(QStringLiteral("Building…"));
    else
        m_stateLabel->setText(usable ? stateText() : QString());
}

QString PdfPane::stateText() const
{
    if (m_building)
        return QStringLiteral("Building…");
    if (!m_hasResult)
        return {};
    switch (m_result.outcome) {
    case BuildOutcome::Succeeded:
        if (m_upToDate)
            return QStringLiteral("Up to date");
        return QStringLiteral("Built %1 in %2 s").arg(m_finished.toString(QStringLiteral("HH:mm")), QString::number(m_result.seconds, 'f', 1));
    case BuildOutcome::Failed:
        return errorCount(m_result.errors());
    case BuildOutcome::Refused:
    case BuildOutcome::CantWork:
        return QStringLiteral("Not built");
    case BuildOutcome::Stopped:
        break;
    }
    return {};
}

void PdfPane::noteWritten(const QString& path, const QByteArray& bytes)
{
    if (const auto it = m_inputs.find(path); it != m_inputs.end())
        it.value() = bytes;
    // Until a snapshot has caught up with the write, snapshots still have the bytes from before it.
    const std::shared_ptr<const ProjectSnapshot> snapshot = m_session ? m_session->snapshot() : nullptr;
    m_written.insert(path, Written{bytes, snapshot ? snapshot->files.value(path) : QByteArray()});
}

void PdfPane::snapshotChanged(const std::shared_ptr<const ProjectSnapshot>& snapshot)
{
    if (!snapshot)
        return;
    // A write is caught up with once a snapshot no longer has the bytes from before it.
    for (auto it = m_written.begin(); it != m_written.end();) {
        if (snapshot->files.value(it.key()) != it->before)
            it = m_written.erase(it);
        else
            ++it;
    }
    if (m_target.isEmpty() || m_stub || m_shownKey.isEmpty())
        return;
    // "A file changed on disk that's in the shown build compiles it again (once, after 300 ms of
    // quiet)." A new or removed proof of the statement changes what the build shows, too.
    bool changed = false;
    for (auto it = m_inputs.begin(); it != m_inputs.end(); ++it) {
        if (m_written.contains(it.key()))
            continue; // Loom's own save, not in a snapshot yet.
        const QByteArray now = snapshot->files.value(it.key());
        if (now != it.value()) {
            it.value() = now;
            changed = true;
        }
    }
    if (const QStringList proofs = proofsOf(snapshot.get(), m_statement); proofs != m_proofs) {
        m_proofs = proofs;
        changed = true;
    }
    if (changed)
        m_quiet.start();
}

void PdfPane::showPlace(const QString& file, int line, int column)
{
    if (m_document->status() != QPdfDocument::Status::Ready)
        return;
    const QString folder = folderPath();
    // While dimmed, `main.synctex.gz` is the failed build's, not the PDF's on screen: the
    // statement's page is the best Loom can say.
    if (folder.isEmpty() || !m_inputs.contains(file) || m_dimmed) {
        jumpTo(std::max(1, targetPage()));
        return;
    }
    const SyncTexTool tool = synctexTool();
    const QString key = m_shownKey;
    struct Found {
        SyncTexOutcome outcome;
        SyncTexBox box;
    };
    m_lane->run(this,
        [tool, folder, file, line, column] {
            Found found{SyncTexOutcome::NotFound, {}};
            found.outcome = synctexForward(tool, folder, file, line, column, found.box);
            return found;
        },
        [this, key](Found found) {
            if (key != m_shownKey)
                return;
            if (found.outcome != SyncTexOutcome::Found) {
                // Without synctex, forward search goes to the statement's page.
                if (found.outcome == SyncTexOutcome::Missing && !m_toldNoSynctex) {
                    m_toldNoSynctex = true;
                    emit outputLine(QStringLiteral("Loom couldn't run synctex, so it showed the statement's page instead of the line. "
                                                   "Check my setup says what to install."));
                }
                jumpTo(std::max(1, targetPage()));
                return;
            }
            jumpTo(found.box.page, QPointF(found.box.x, found.box.y));
            m_view->mark(found.box.page - 1, QRectF(found.box.x, found.box.y, found.box.width, found.box.height));
        });
}

void PdfPane::inverse(int page, const QPointF& point)
{
    const QString folder = folderPath();
    if (folder.isEmpty())
        return;
    if (m_dimmed) {
        // `main.synctex.gz` is the failed build's, and would point at the wrong places.
        m_stateLabel->setText(QStringLiteral("This is the last good build; fix the errors to click into the PDF."));
        return;
    }
    const SyncTexTool tool = synctexTool();
    const QString key = m_shownKey;
    struct Found {
        SyncTexOutcome outcome;
        SyncTexPlace place;
    };
    m_lane->run(this,
        [tool, folder, page, point] {
            Found found{SyncTexOutcome::NotFound, {}};
            found.outcome = synctexInverse(tool, folder, page + 1, point.x(), point.y(), found.place);
            return found;
        },
        [this, key](Found found) {
            if (key != m_shownKey)
                return; // Another document is shown now.
            if (found.outcome == SyncTexOutcome::Found) {
                emit placeActivated(found.place.file, found.place.line);
            } else if (found.outcome == SyncTexOutcome::Missing && !m_toldNoSynctex) {
                // "Double-clicking the PDF does nothing and says why once."
                m_toldNoSynctex = true;
                m_stateLabel->setText(QStringLiteral("Without synctex, clicking in the PDF can't find the source."));
                emit outputLine(QStringLiteral("Loom couldn't run synctex, so double-clicking the PDF can't find the source. "
                                               "Check my setup says what to install."));
            }
        });
}

void PdfPane::find()
{
    if (m_view->isHidden())
        return;
    m_findBar->show();
    m_findText->setFocus();
    m_findText->selectAll();
}

} // namespace loom
