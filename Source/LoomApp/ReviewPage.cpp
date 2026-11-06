#include "LoomApp/ReviewPage.h"

#include "LoomApp/AppSettings.h"
#include "LoomApp/Lane.h"
#include "LoomApp/PdfPane.h"
#include "LoomApp/ProjectSession.h"

#include "LoomCore/ProjectContents.h"
#include "LoomCore/Revision.h"

#include <QBuffer>
#include <QButtonGroup>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPdfDocument>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QSyntaxHighlighter>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <optional>
#include <utility>

namespace loom {

namespace {

/// The tree items' roles.
enum Role {
    KeyRole = Qt::UserRole,     ///< An entry's key; on a status change, its statement's tag.
    PathRole = Qt::UserRole + 1, ///< In the changes: the file a line belongs to.
    ProofRole = Qt::UserRole + 2, ///< In the changes: the proof a file belongs to, or empty.
};

/// How the list marks an entry.
QString changeMark(FileChange change)
{
    switch (change) {
    case FileChange::New:
        return QStringLiteral("new");
    case FileChange::Changed:
        return QStringLiteral("changed");
    case FileChange::Deleted:
        return QStringLiteral("deleted");
    }
    return {};
}

/// A status as the lines name it; `none` for no status.
QString statusWord(Status status)
{
    const QString name = statusName(status);
    return name.isEmpty() ? QStringLiteral("none") : name;
}

/// A status change as the list shows it: `4KX7 · title: unchecked → proved ▲` (Specs/m2-statements-
/// review.md, "Rules: Status changes").
QString statusLine(const StatusChange& change)
{
    QString text = change.title.isEmpty() ? change.id : QStringLiteral(u"%1 · %2").arg(change.id, change.title);
    text += QStringLiteral(u": %1 → %2").arg(statusWord(change.before), statusWord(change.after));
    if (change.direction == StatusDirection::Raise)
        text += QStringLiteral(u" ▲");
    else if (change.direction == StatusDirection::Drop)
        text += QStringLiteral(u" ▼");
    return text;
}

/// Whether the app's colours are dark (Windows' dark mode), so marks need lighter colours to read.
bool darkPalette()
{
    return QPalette().color(QPalette::Base).lightness() < 128;
}

/// Green for added lines and raises, red for removed lines and drops, orange for warnings, blue for
/// a diff's hunk headers, each light enough to read on a dark background.
QColor addedColor() { return darkPalette() ? QColor(0x7e, 0xe7, 0x87) : QColor(0x1a, 0x7f, 0x37); }
QColor removedColor() { return darkPalette() ? QColor(0xff, 0x8a, 0x80) : QColor(0xc0, 0x1c, 0x28); }
QColor warningColor() { return darkPalette() ? QColor(0xff, 0xb8, 0x6c) : QColor(0xb3, 0x5c, 0x00); }
QColor hunkColor() { return darkPalette() ? QColor(0x79, 0xc0, 0xff) : QColor(0x1c, 0x5f, 0xb0); }

/// Colours a unified diff: added lines green, removed red, hunk headers blue, file headers bold.
class DiffHighlighter : public QSyntaxHighlighter
{
public:
    using QSyntaxHighlighter::QSyntaxHighlighter;

protected:
    void highlightBlock(const QString& text) override
    {
        QTextCharFormat format;
        if (text.startsWith(u"+++") || text.startsWith(u"---") || text.startsWith(u"diff ") || text.startsWith(u"index ")) {
            format.setFontWeight(QFont::Bold);
        } else if (text.startsWith(u'+')) {
            format.setForeground(addedColor());
        } else if (text.startsWith(u'-')) {
            format.setForeground(removedColor());
        } else if (text.startsWith(u"@@")) {
            format.setForeground(hunkColor());
        } else {
            return;
        }
        setFormat(0, static_cast<int>(text.size()), format);
    }
};

/// A tool button that can be checked, for the bar above *Before and after*.
QToolButton* toggle(const QString& text, const QString& name, QWidget* parent)
{
    auto* button = new QToolButton(parent);
    button->setText(text);
    button->setObjectName(name);
    button->setCheckable(true);
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    return button;
}

/// Runs one git command for Review and notes it for Output's Git channel.
GitRun runLogged(const GitTool& git, const QStringList& arguments, const QString& root, std::vector<GitStep>& steps)
{
    GitStep step;
    step.started = QDateTime::currentDateTime();
    step.folder = root;
    step.arguments = arguments;
    QElapsedTimer timer;
    timer.start();
    step.run = runGit(git, arguments, root);
    step.milliseconds = timer.elapsed();
    steps.push_back(step);
    return step.run;
}

} // namespace

// ---------------------------------------------------------------------------------------------
// ReviewSide

ReviewSide::ReviewSide(const QString& title, QWidget* parent)
    : QWidget(parent)
    , m_state(new QLabel(this))
    , m_stack(new QStackedWidget(this))
    , m_message(new QLabel(m_stack))
    , m_errors(new QPlainTextEdit(m_stack))
    , m_view(new LoomPdfView(m_stack))
    , m_document(new QPdfDocument(this))
{
    auto* heading = new QLabel(title, this);
    QFont font = heading->font();
    font.setBold(true);
    heading->setFont(font);
    m_state->setObjectName(QStringLiteral("sideState"));
    m_state->setForegroundRole(QPalette::PlaceholderText);
    m_message->setObjectName(QStringLiteral("sideMessage"));
    m_message->setWordWrap(true);
    m_message->setAlignment(Qt::AlignCenter);
    m_message->setForegroundRole(QPalette::PlaceholderText);
    m_message->setMargin(16);
    m_errors->setObjectName(QStringLiteral("sideErrors"));
    m_errors->setReadOnly(true);
    m_errors->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_view->setDocument(m_document);
    m_stack->addWidget(m_message);
    m_stack->addWidget(m_errors);
    m_stack->addWidget(m_view);

    auto* top = new QHBoxLayout;
    top->addWidget(heading);
    top->addWidget(m_state, 1);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(top);
    layout->addWidget(m_stack, 1);
    showMessage({});
}

ReviewSide::~ReviewSide()
{
    m_document->close(); // Before the buffer it reads goes.
}

void ReviewSide::showMessage(const QString& text)
{
    m_shows = Shows::Message;
    m_document->close();
    m_buffer.reset();
    m_message->setText(text);
    m_stack->setCurrentWidget(m_message);
}

void ReviewSide::showPdf(const QByteArray& bytes)
{
    // Read into memory, never held open: on Windows an open PDF can't be overwritten by the next build.
    const int scroll = m_view->verticalScrollBar()->value();
    const bool again = m_shows == Shows::Pdf;
    m_shows = Shows::Pdf;
    m_document->close();
    auto buffer = std::make_unique<QBuffer>();
    buffer->setData(bytes);
    buffer->open(QIODevice::ReadOnly);
    m_document->load(buffer.get());
    m_buffer = std::move(buffer);
    m_stack->setCurrentWidget(m_view);
    if (again)
        m_view->verticalScrollBar()->setValue(scroll);
}

void ReviewSide::showErrors(const QString& heading, const std::vector<Problem>& problems)
{
    m_shows = Shows::Errors;
    m_document->close();
    m_buffer.reset();
    QStringList lines{heading};
    for (const Problem& problem : problems) {
        if (problem.severity != Severity::Error)
            continue;
        lines << (problem.file.isEmpty() ? problem.message : QStringLiteral("%1:%2: %3").arg(problem.file, QString::number(problem.line), problem.message));
    }
    m_errors->setPlainText(lines.join(u'\n'));
    m_stack->setCurrentWidget(m_errors);
}

void ReviewSide::setBuilding(bool building)
{
    m_state->setText(building ? QStringLiteral(u"Building…") : QString());
}

// ---------------------------------------------------------------------------------------------
// ReviewPage

/// What a read of the list found, carried from the Git lane.
struct ReviewPage::Read {
    int generation = 0;
    QString problem;
    QString head;
    ReviewList list;
    std::vector<GitStep> steps;
};

/// The contents of `HEAD`'s tree, read once per commit (a commit never changes).
struct ReviewPage::HeadCache {
    QString hash;
    std::shared_ptr<const ProjectContents> contents;
};

ReviewPage::ReviewPage(AppSettings* settings, Lane* gitLane, Lane* buildLane, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_gitLane(gitLane)
    , m_buildLane(buildLane)
    , m_cache(std::make_shared<HeadCache>())
{
    setObjectName(QStringLiteral("reviewPage"));
    m_quiet.setSingleShot(true);
    m_quiet.setInterval(quietMs);
    connect(&m_quiet, &QTimer::timeout, this, &ReviewPage::read);
    m_mode = m_settings->defaultCompileMode();
    createWidgets();
    updateControls();
}

ReviewPage::~ReviewPage()
{
    if (m_stop)
        m_stop->store(true);
}

void ReviewPage::createWidgets()
{
    m_entries = new QTreeWidget(this);
    m_entries->setObjectName(QStringLiteral("reviewEntries"));
    m_entries->setColumnCount(3);
    m_entries->setHeaderHidden(true);
    m_entries->setRootIsDecorated(false);
    m_entries->setIndentation(12);
    m_entries->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_entries->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_entries->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_entries->header()->setStretchLastSection(false);
    connect(m_entries, &QTreeWidget::currentItemChanged, this, [this] { showSelected(); });

    m_right = new QStackedWidget(this);
    m_note = new QLabel(m_right);
    m_note->setObjectName(QStringLiteral("reviewNote"));
    m_note->setWordWrap(true);
    m_note->setAlignment(Qt::AlignCenter);
    m_note->setForegroundRole(QPalette::PlaceholderText);
    m_note->setMargin(24);
    m_right->addWidget(m_note);

    m_entryPage = new QWidget(m_right);
    m_heading = new QLabel(m_entryPage);
    m_heading->setObjectName(QStringLiteral("reviewHeading"));
    m_heading->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont font = m_heading->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.15);
    m_heading->setFont(font);

    m_changes = new QTreeWidget(m_entryPage);
    m_changes->setObjectName(QStringLiteral("reviewChanges"));
    m_changes->setHeaderHidden(true);
    m_changes->setRootIsDecorated(false);
    m_changes->setIndentation(16);

    // The bar above *Before and after*: the mode, Verbose, Link scrolling, Raw diff, Open.
    m_buildBar = new QWidget(m_entryPage);
    auto* modes = new QButtonGroup(this);
    const QString names[3] = {QStringLiteral("Alone"), QStringLiteral("+ Premises"), QStringLiteral("+ Chain")};
    const BuildMode values[3] = {BuildMode::Alone, BuildMode::Premises, BuildMode::Chain};
    auto* bar = new QHBoxLayout(m_buildBar);
    bar->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 3; ++i) {
        m_modeButtons[i] = toggle(names[i], QStringLiteral("reviewMode") + buildModeName(values[i]), m_buildBar);
        modes->addButton(m_modeButtons[i], i);
        bar->addWidget(m_modeButtons[i]);
        connect(m_modeButtons[i], &QToolButton::clicked, this, [this, mode = values[i]] { setMode(mode); });
    }
    modes->setExclusive(true);
    m_verboseButton = toggle(QStringLiteral("Verbose"), QStringLiteral("reviewVerbose"), m_buildBar);
    connect(m_verboseButton, &QToolButton::clicked, this, [this](bool on) { setVerbose(on); });
    bar->addSpacing(12);
    bar->addWidget(m_verboseButton);
    m_linkButton = toggle(QStringLiteral("Link scrolling"), QStringLiteral("reviewLinkScrolling"), m_buildBar);
    m_linkButton->setChecked(true); // "toggle Link scrolling, on by default"
    bar->addWidget(m_linkButton);
    bar->addStretch(1);
    m_rawButton = toggle(QStringLiteral("Raw diff"), QStringLiteral("reviewRawDiff"), m_entryPage);
    connect(m_rawButton, &QToolButton::clicked, this, [this](bool on) { setRawDiff(on); });
    m_openButton = new QPushButton(QStringLiteral("Open in Statements"), m_entryPage);
    m_openButton->setObjectName(QStringLiteral("reviewOpen"));
    connect(m_openButton, &QPushButton::clicked, this, &ReviewPage::openInStatements);

    m_content = new QStackedWidget(m_entryPage);
    m_sides = new QSplitter(Qt::Horizontal, m_content);
    m_sides->setObjectName(QStringLiteral("reviewSides"));
    m_before = new ReviewSide(QStringLiteral("At HEAD"), m_sides);
    m_before->setObjectName(QStringLiteral("reviewBefore"));
    m_after = new ReviewSide(QStringLiteral("Now"), m_sides);
    m_after->setObjectName(QStringLiteral("reviewAfter"));
    m_sides->addWidget(m_before);
    m_sides->addWidget(m_after);
    m_sides->setChildrenCollapsible(false);
    linkScroll(m_before->view(), m_after->view());
    linkScroll(m_after->view(), m_before->view());
    m_diff = new QPlainTextEdit(m_content);
    m_diff->setObjectName(QStringLiteral("reviewDiff"));
    m_diff->setReadOnly(true);
    m_diff->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_diff->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    new DiffHighlighter(m_diff->document());
    m_content->addWidget(m_sides);
    m_content->addWidget(m_diff);

    auto* top = new QHBoxLayout;
    top->addWidget(m_heading, 1);
    top->addWidget(m_rawButton);
    top->addWidget(m_openButton);
    auto* entryLayout = new QVBoxLayout(m_entryPage);
    entryLayout->setContentsMargins(12, 8, 12, 8);
    entryLayout->addLayout(top);
    entryLayout->addWidget(m_changes);
    entryLayout->addWidget(m_buildBar);
    entryLayout->addWidget(m_content, 1);
    m_right->addWidget(m_entryPage);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_entries);
    splitter->addWidget(m_right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({420, 1000});
    splitter->setChildrenCollapsible(false);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(splitter);
}

void ReviewPage::linkScroll(LoomPdfView* from, LoomPdfView* to)
{
    // Proportionally, since the two PDFs may differ in length.
    connect(from->verticalScrollBar(), &QScrollBar::valueChanged, this, [this, from, to](int value) {
        if (m_syncing || !m_linkButton->isChecked())
            return;
        const int fromMax = from->verticalScrollBar()->maximum();
        QScrollBar* other = to->verticalScrollBar();
        m_syncing = true;
        other->setValue(fromMax > 0 ? static_cast<int>(qint64(value) * other->maximum() / fromMax) : 0);
        m_syncing = false;
    });
}

QToolButton* ReviewPage::modeButton(BuildMode mode) const
{
    return m_modeButtons[static_cast<int>(mode)];
}

void ReviewPage::setSession(ProjectSession* session)
{
    if (m_session)
        disconnect(m_session, nullptr, this, nullptr);
    stopBuilds();
    // Results of the old project's tasks still arrive; each checks this generation and is dropped
    // without touching the counters below.
    ++m_generation;
    ++m_diffGeneration;
    m_session = session;
    m_reading = false;
    m_again = false;
    m_building = 0;
    m_sideBuilds[0] = m_sideBuilds[1] = {};
    m_deferred = false;
    m_deferredRebuild = false;
    m_readingDiff = false;
    m_diffKey.clear();
    m_diff->clear();
    m_list = {};
    m_head.clear();
    m_problem.clear();
    m_shownKey.clear();
    m_shownHead.clear();
    m_cache = std::make_shared<HeadCache>();
    m_entries->clear();
    m_before->showMessage({});
    m_after->showMessage({});
    m_note->setText({});
    m_right->setCurrentWidget(m_note);
    emit countChanged(0);
    if (!m_session)
        return;
    connect(m_session, &ProjectSession::snapshotChanged, this, &ReviewPage::refresh);
    m_quiet.stop();
    read();
}

void ReviewPage::refresh()
{
    if (m_session)
        m_quiet.start();
}

void ReviewPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // Builds put off while the tab was hidden run now.
    if (m_deferred) {
        m_deferred = false;
        if (const ReviewEntry* entry = shown())
            showEntry(*entry, std::exchange(m_deferredRebuild, false));
    }
    refresh();
}

void ReviewPage::read()
{
    if (!m_session)
        return;
    if (m_reading) {
        m_again = true;
        return;
    }
    const std::shared_ptr<const ProjectSnapshot> snapshot = m_session->snapshot();
    if (!snapshot->gitProblem.isEmpty()) {
        // No repository, or no git: Review says so, as the git panel does.
        Read read;
        read.generation = m_generation;
        read.problem = snapshot->gitProblem;
        showList(std::move(read));
        return;
    }
    m_reading = true;
    m_gitLane->run(this,
        [git = m_settings->gitTool(), snapshot, cache = m_cache, generation = m_generation] {
            Read read;
            read.generation = generation;
            const QString& root = snapshot->root;
            // "The list (left), from `git status --porcelain=v2 -z --untracked-files=all` (ignored
            // files excluded)".
            // `--no-optional-locks`: git status may otherwise refresh and rewrite `.git/index`, and
            // Review never changes git; it would also hold `index.lock` against an agent's commit.
            const QStringList arguments{QStringLiteral("--no-optional-locks"), QStringLiteral("status"), QStringLiteral("--porcelain=v2"),
                                        QStringLiteral("-z"), QStringLiteral("--untracked-files=all")};
            const GitRun status = runLogged(git, arguments, root, read.steps);
            if (!status.ok()) {
                read.problem = status.started ? QStringLiteral("Loom couldn't read the changed files: %1").arg(status.errors.trimmed())
                                              : gitDidntRunMessage(status, arguments);
                return read;
            }
            const std::vector<ChangedFile> files = parseGitStatus(status.outputBytes);
            // The project at HEAD, read without touching the working tree or the index.
            const RevisionTree tree = readRevision(root, QStringLiteral("HEAD"), git);
            std::shared_ptr<const ProjectContents> before;
            if (tree.outcome == RevisionOutcome::NoCommits) {
                before = std::make_shared<const ProjectContents>(); // "No commits yet: everything is new."
            } else if (tree.outcome != RevisionOutcome::Ready) {
                read.problem = QStringLiteral("Loom couldn't read the last commit: %1").arg(tree.message);
                return read;
            } else if (cache->hash == tree.hash && cache->contents) {
                before = cache->contents;
            } else {
                ContentsLoading loading = loadContents(root + u'/' + tree.folder);
                if (!loading.ok) {
                    read.problem = QStringLiteral("Loom couldn't read the last commit: %1").arg(loading.message);
                    return read;
                }
                before = std::make_shared<const ProjectContents>(std::move(loading.contents));
                cache->hash = tree.hash;
                cache->contents = before;
            }
            read.head = tree.hash;
            read.list = reviewList(files, *before, snapshot->contents);
            return read;
        },
        [this](Read read) {
            // A read for the project open before: its flag was reset when that project closed.
            if (read.generation != m_generation)
                return;
            m_reading = false;
            showList(std::move(read));
            if (m_again) {
                m_again = false;
                m_quiet.start();
            }
        });
}

void ReviewPage::showList(Read read)
{
    if (!read.steps.empty())
        emit gitSteps(read.steps);
    const bool headMoved = read.head != m_head;
    m_problem = read.problem;
    m_list = read.problem.isEmpty() ? std::move(read.list) : ReviewList();
    m_head = read.head;

    // Keep what was selected: an entry by its key, else nothing.
    const QString selected = m_shownKey;
    {
        const QSignalBlocker blocker(m_entries);
        m_entries->clear();
        QFont bold = m_entries->font();
        bold.setBold(true);
        const auto group = [&](const QString& name) {
            auto* item = new QTreeWidgetItem(m_entries, {name});
            item->setFont(0, bold);
            item->setFlags(Qt::ItemIsEnabled);
            item->setFirstColumnSpanned(true);
            item->setExpanded(true);
            return item;
        };
        if (!m_list.statusChanges.empty()) {
            QTreeWidgetItem* parent = group(QStringLiteral("Status changes"));
            parent->setData(0, Qt::AccessibleDescriptionRole, QStringLiteral("statusChanges"));
            for (const StatusChange& change : m_list.statusChanges) {
                auto* item = new QTreeWidgetItem(parent, {statusLine(change)});
                item->setFirstColumnSpanned(true);
                item->setData(0, KeyRole, change.id);
                item->setToolTip(0, item->text(0));
                if (change.direction == StatusDirection::Drop)
                    item->setForeground(0, removedColor());
                else if (change.direction == StatusDirection::Raise)
                    item->setForeground(0, addedColor());
            }
        }
        QTreeWidgetItem* parent = nullptr;
        std::optional<ReviewGroup> last;
        for (const ReviewEntry& entry : m_list.entries) {
            if (!last || *last != entry.group) {
                parent = group(reviewGroupName(entry.group));
                last = entry.group;
            }
            auto* item = new QTreeWidgetItem(parent, {entry.name, changeMark(entry.change), entry.summary});
            item->setData(0, KeyRole, entry.key);
            item->setToolTip(0, entry.name);
            item->setForeground(1, QPalette().color(QPalette::PlaceholderText));
            item->setForeground(2, QPalette().color(QPalette::PlaceholderText));
        }
        m_entries->expandAll();
    }

    if (!m_problem.isEmpty()) {
        m_note->setText(m_problem);
        m_right->setCurrentWidget(m_note);
        m_shownKey.clear();
        stopBuilds();
    } else if (m_list.entries.empty() && m_list.statusChanges.empty()) {
        m_note->setText(QStringLiteral("No uncommitted changes: the project is as it was at the last commit."));
        m_right->setCurrentWidget(m_note);
        m_shownKey.clear();
        stopBuilds();
    } else if (!selected.isEmpty() && selectEntry(selected)) {
        // The entry is still there: its changes again, its side of now built again (LoomCore
        // reuses the build when nothing changed), and the side at HEAD if HEAD moved.
        if (const ReviewEntry* entry = shown())
            showEntry(*entry, headMoved);
    } else {
        m_shownKey.clear();
        stopBuilds();
        m_note->setText(QStringLiteral("Choose a change on the left to see it compiled before and after."));
        m_right->setCurrentWidget(m_note);
    }
    emit countChanged(m_list.count());
    emit listShown();
}

bool ReviewPage::selectEntry(const QString& key)
{
    for (QTreeWidgetItemIterator it(m_entries); *it; ++it) {
        QTreeWidgetItem* item = *it;
        // Entries only: a status change's key is its statement's tag too, but it isn't an entry.
        if (item->parent() && !item->text(1).isEmpty() && item->data(0, KeyRole).toString() == key) {
            const QSignalBlocker blocker(m_entries);
            m_entries->setCurrentItem(item);
            m_shownKey = key;
            return true;
        }
    }
    return false;
}

const ReviewEntry* ReviewPage::shown() const
{
    for (const ReviewEntry& entry : m_list.entries)
        if (entry.key == m_shownKey)
            return &entry;
    return nullptr;
}

void ReviewPage::showSelected()
{
    QTreeWidgetItem* item = m_entries->currentItem();
    if (!item || !item->parent())
        return;
    const QString key = item->data(0, KeyRole).toString();
    if (item->text(1).isEmpty()) {
        // A status change: its statement's entry, if it has one.
        if (selectEntry(key)) {
            if (const ReviewEntry* entry = shown())
                showEntry(*entry, true);
        }
        return;
    }
    m_shownKey = key;
    if (const ReviewEntry* entry = shown())
        showEntry(*entry, true);
}

void ReviewPage::showEntry(const ReviewEntry& entry, bool rebuild)
{
    m_right->setCurrentWidget(m_entryPage);
    if (entry.key != m_diffKey) {
        // Another entry: a diff still being read for the one before is dropped, and its text goes.
        m_diffKey = entry.key;
        ++m_diffGeneration;
        m_readingDiff = false;
        m_diff->clear();
    }
    m_heading->setText(QStringLiteral(u"%1 — %2").arg(entry.name, changeMark(entry.change)));

    // 1. Changes in plain words, one line each, under the file they belong to; a selected proof's
    // file is what **Open in Statements** opens.
    const QString selectedPath = m_changes->currentItem() ? m_changes->currentItem()->data(0, PathRole).toString() : QString();
    m_changes->clear();
    QFont bold = m_changes->font();
    bold.setBold(true);
    QTreeWidgetItem* reselect = nullptr;
    for (const FileChanges& file : entry.details) {
        auto* heading = new QTreeWidgetItem(m_changes, {file.label});
        heading->setFont(0, bold);
        heading->setData(0, PathRole, file.path);
        heading->setData(0, ProofRole, file.proofId);
        heading->setToolTip(0, file.path);
        if (file.path == selectedPath)
            reselect = heading;
        for (const ChangeLine& line : file.lines) {
            auto* item = new QTreeWidgetItem(heading, {line.text});
            item->setData(0, PathRole, file.path);
            item->setData(0, ProofRole, file.proofId);
            item->setToolTip(0, line.text);
            if (line.warning)
                item->setForeground(0, warningColor());
        }
    }
    m_changes->expandAll();
    // As tall as its lines, up to a point: the PDFs below get the rest.
    int rows = 0;
    for (QTreeWidgetItemIterator it(m_changes); *it; ++it)
        ++rows;
    const int row = m_changes->fontMetrics().height() + 4;
    m_changes->setFixedHeight(qMin(rows * row + 2 * m_changes->frameWidth() + 6, 200));
    if (reselect)
        m_changes->setCurrentItem(reselect);
    m_changes->setVisible(!entry.details.empty());

    updateControls();
    if (!isVisible()) {
        // The tab is hidden: building now would only hold the Build lane, which the Statements
        // tab's builds share. It happens when the tab is shown (showEvent()).
        m_deferred = true;
        m_deferredRebuild = m_deferredRebuild || rebuild;
        return;
    }
    if (entry.group != ReviewGroup::Statements || m_raw) {
        stopBuilds(); // The PDFs aren't shown: their builds aren't needed.
        m_content->setCurrentWidget(m_diff);
        readDiff();
        return;
    }
    m_content->setCurrentWidget(m_sides);
    // 2. Before and after. Both start again for a new entry, mode or HEAD; a result of an older
    // build is then dropped. Otherwise only the side of now builds again.
    if (rebuild || m_shownHead != m_head || !m_stop) {
        stopBuilds();
        m_stop = std::make_shared<std::atomic_bool>(false);
        build(m_before, true);
    }
    build(m_after, false);
}

void ReviewPage::stopBuilds()
{
    if (m_stop)
        m_stop->store(true);
    m_stop.reset();
    ++m_buildGeneration;
}

void ReviewPage::updateControls()
{
    const ReviewEntry* entry = shown();
    const bool statement = entry && entry->group == ReviewGroup::Statements;
    // Other entries show only the raw diff, so the toggle and the build bar are for statements.
    m_buildBar->setVisible(statement && !m_raw);
    m_rawButton->setVisible(statement);
    m_rawButton->setChecked(m_raw);
    m_openButton->setVisible(statement);
    m_openButton->setEnabled(statement && entry->after);
    for (int i = 0; i < 3; ++i)
        m_modeButtons[i]->setChecked(static_cast<int>(m_mode) == i);
    m_verboseButton->setChecked(m_verbose);
}

void ReviewPage::setMode(BuildMode mode)
{
    m_mode = mode;
    updateControls();
    if (const ReviewEntry* entry = shown(); entry && entry->group == ReviewGroup::Statements && !m_raw)
        showEntry(*entry, true); // "the mode switch rebuilds both"
}

void ReviewPage::setVerbose(bool verbose)
{
    m_verbose = verbose;
    updateControls();
    if (const ReviewEntry* entry = shown(); entry && entry->group == ReviewGroup::Statements && !m_raw)
        showEntry(*entry, true);
}

void ReviewPage::setRawDiff(bool on)
{
    m_raw = on;
    if (const ReviewEntry* entry = shown())
        showEntry(*entry, true);
    else
        updateControls();
}

void ReviewPage::build(ReviewSide* side, bool atHead)
{
    const ReviewEntry* entry = shown();
    if (!entry || !m_session)
        return;
    const bool exists = atHead ? entry->before : entry->after;
    if (atHead)
        m_shownHead = m_head;
    if (!exists) {
        side->setBuilding(false);
        side->showMessage(atHead ? QStringLiteral("New: not in the last commit.") : QStringLiteral("Deleted: not in the working tree."));
        return;
    }
    if (atHead && m_head.isEmpty()) {
        side->setBuilding(false);
        side->showMessage(QStringLiteral("New: not in the last commit."));
        return;
    }
    // One build per side at a time: a request while this side's build of the same generation
    // runs becomes one more build after it, so snapshots in a row don't queue a build each.
    SideBuild& state = m_sideBuilds[atHead ? 0 : 1];
    if (state.running == m_buildGeneration) {
        state.again = true;
        return;
    }
    state.running = m_buildGeneration;
    state.again = false;
    BuildRequest request;
    request.root = m_session->root();
    request.target = entry->key;
    request.mode = m_mode;
    request.verbose = m_verbose;
    request.rev = atHead ? m_head : QString();
    request.timeoutMs = m_settings->compileTimeoutSeconds() * 1000;
    request.latexmk = m_settings->latexmkProgram();
    request.git = m_settings->gitTool();
    request.stop = m_stop;
    if (side->shows() != ReviewSide::Shows::Pdf)
        side->showMessage(QStringLiteral(u"Building…"));
    side->setBuilding(true);
    ++m_building;
    const int generation = m_buildGeneration;
    const int session = m_generation;
    const QString key = entry->key;
    struct Built {
        BuildResult result;
        BuildOutput output;
    };
    m_buildLane->run(this,
        [request] {
            Built built;
            built.result = buildTarget(request);
            if (!built.result.folder.isEmpty() && built.result.outcome == BuildOutcome::Succeeded)
                built.output = readBuildOutput(request.root, built.result.folder);
            return built;
        },
        [this, side, atHead, generation, session, key](Built built) {
            if (session != m_generation)
                return; // The project open before: its counters were reset when it closed.
            --m_building;
            SideBuild& state = m_sideBuilds[atHead ? 0 : 1];
            const bool again = state.running == generation && state.again;
            if (state.running == generation)
                state = {};
            const bool current = generation == m_buildGeneration && key == m_shownKey;
            if (current) {
                side->setBuilding(false);
                switch (built.result.outcome) {
                case BuildOutcome::Succeeded:
                    if (built.output.pdf.isEmpty())
                        side->showMessage(QStringLiteral("Loom couldn't read the PDF this build made."));
                    else
                        side->showPdf(built.output.pdf);
                    break;
                case BuildOutcome::Failed:
                    // "A side that doesn't compile shows its errors in place of the PDF while the
                    // other still shows."
                    side->showErrors(built.result.errors() == 1 ? QStringLiteral("It doesn't compile: 1 error.")
                                                                : QStringLiteral("It doesn't compile: %1 errors.").arg(built.result.errors()),
                                     built.result.problems);
                    break;
                case BuildOutcome::Refused:
                case BuildOutcome::CantWork:
                    side->showMessage(built.result.message);
                    break;
                case BuildOutcome::Stopped:
                    break;
                }
            }
            if (built.result.outcome != BuildOutcome::Stopped)
                emit buildFinished(built.result);
            if (again && current)
                build(side, atHead);
            if (m_building == 0)
                emit buildsFinished();
        });
}

void ReviewPage::readDiff()
{
    const ReviewEntry* entry = shown();
    if (!entry || !m_session)
        return;
    m_readingDiff = true;
    const int generation = ++m_diffGeneration;
    const int session = m_generation;
    if (m_diff->document()->isEmpty())
        m_diff->setPlainText(QStringLiteral(u"Reading the diff…"));
    m_gitLane->run(this,
        [git = m_settings->gitTool(), root = m_session->root(), files = entry->files, hasHead = !m_head.isEmpty()] {
            std::vector<GitStep> steps;
            QStringList parts;
            for (const ChangedFile& file : files) {
                QString text = fileDiff(git, root, file, steps);
                // "a binary file says so and shows its size before and after"
                static const QRegularExpression binary(QStringLiteral("^Binary files .* differ$"), QRegularExpression::MultilineOption);
                if (binary.match(text).hasMatch()) {
                    QString before = QStringLiteral("none");
                    if (hasHead && file.change != FileChange::New) {
                        const GitRun size = runLogged(git, {QStringLiteral("cat-file"), QStringLiteral("-s"), QStringLiteral("HEAD:") + file.path}, root, steps);
                        if (size.ok())
                            before = QStringLiteral("%1 bytes").arg(size.output.trimmed());
                    }
                    const QFileInfo now(root + u'/' + file.path);
                    text = QStringLiteral("%1: a binary file, %2 at HEAD, %3 now.\n")
                               .arg(file.path, before, now.exists() ? QStringLiteral("%1 bytes").arg(now.size()) : QStringLiteral("none"));
                }
                parts << text;
            }
            return std::make_pair(parts.join(u'\n'), std::move(steps));
        },
        [this, generation, session](std::pair<QString, std::vector<GitStep>> diff) {
            if (session != m_generation)
                return; // The project open before: its commands aren't this one's Output.
            if (!diff.second.empty())
                emit gitSteps(diff.second);
            if (generation != m_diffGeneration)
                return;
            m_readingDiff = false;
            m_diff->setPlainText(diff.first);
            emit diffShown();
        });
}

void ReviewPage::openInStatements()
{
    const ReviewEntry* entry = shown();
    if (!entry || entry->group != ReviewGroup::Statements)
        return;
    QString proof;
    if (QTreeWidgetItem* item = m_changes->currentItem())
        proof = item->data(0, ProofRole).toString();
    // A proof that is gone opens its statement instead.
    if (!proof.isEmpty() && m_session && !m_session->snapshot()->contents.proof(proof))
        proof.clear();
    if (!entry->after && proof.isEmpty())
        return;
    emit openRequested(entry->key, proof);
}

} // namespace loom
