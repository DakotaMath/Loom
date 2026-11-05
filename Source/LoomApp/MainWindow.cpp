#include "LoomApp/MainWindow.h"

#include "LoomApp/AppSettings.h"
#include "LoomApp/BottomPanel.h"
#include "LoomApp/Lane.h"
#include "LoomApp/NewProjectDialog.h"
#include "LoomApp/ProjectSession.h"
#include "LoomApp/SettingsPage.h"
#include "LoomApp/SetupCheckView.h"
#include "LoomApp/SideBar.h"
#include "LoomApp/StartScreen.h"
#include "LoomApp/StatementsPage.h"

#include "LoomCore/AppLock.h"
#include "LoomCore/Version.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace loom {

/// What opening a folder found, carried from the Project lane back to the window.
struct MainWindow::Opening {
    FolderInspection folder;
    AppLockOutcome lock = AppLockOutcome::Failed;
    QString lockMessage;
    std::unique_ptr<QLockFile> lockFile;
    std::shared_ptr<const ProjectSnapshot> snapshot;
};

namespace {

/// Whether two folders are the same, as the file system compares names.
bool sameFolder(const QString& a, const QString& b)
{
#ifdef Q_OS_WIN
    return QDir::cleanPath(a).compare(QDir::cleanPath(b), Qt::CaseInsensitive) == 0;
#else
    return QDir::cleanPath(a) == QDir::cleanPath(b);
#endif
}

/// "1 error", "2 errors".
QString count(int n, const char* one, const char* many)
{
    return QStringLiteral("%1 %2").arg(n).arg(QString::fromLatin1(n == 1 ? one : many));
}

/// A page for a tab whose contents come in a later issue: its name and a line.
QWidget* placeholderPage(const QString& objectName, const QString& title, QLabel* note, QWidget* parent)
{
    auto* page = new QWidget(parent);
    page->setObjectName(objectName);
    auto* heading = new QLabel(title, page);
    QFont font = heading->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.3);
    heading->setFont(font);
    note->setParent(page);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 18, 24, 18);
    layout->addWidget(heading);
    layout->addWidget(note);
    layout->addStretch(1);
    return page;
}

/// Shows a file in Explorer, selected; elsewhere opens its folder.
void revealInExplorer(const QString& file)
{
#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), QDir::toNativeSeparators(file)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(file).absolutePath()));
#endif
}

} // namespace

MainWindow::MainWindow(AppSettings* settings, QWidget* parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_projectLane(new Lane(QStringLiteral("Project"), 1, this))
    , m_buildLane(new Lane(QStringLiteral("Build"), 2, this))
    , m_gitLane(new Lane(QStringLiteral("Git"), 1, this))
{
    setWindowTitle(QStringLiteral("Loom"));
    resize(1200, 800);
    if (const QByteArray geometry = m_settings->windowGeometry(); !geometry.isEmpty())
        restoreGeometry(geometry);

    m_sideBar = new SideBar(this);
    m_sideBar->setProjectTabsVisible(false);
    connect(m_sideBar, &SideBar::tabClicked, this, [this](Tab tab) {
        // Without a project, the gear goes back and forth between Settings and the start screen.
        if (tab == Tab::Settings && !m_session && m_current == Tab::Settings && !onStartScreen())
            showStart();
        else
            showTab(tab);
    });

    m_pages = new QStackedWidget(this);
    m_startScreen = new StartScreen(m_settings, m_pages);
    connect(m_startScreen, &StartScreen::newProjectClicked, this, [this] { newProject(); });
    connect(m_startScreen, &StartScreen::openProjectClicked, this, &MainWindow::chooseFolder);
    connect(m_startScreen, &StartScreen::checkSetupClicked, this, &MainWindow::checkSetup);
    connect(m_startScreen, &StartScreen::recentClicked, this, [this](const QString& path) { openFolder(path); });
    m_pages->addWidget(m_startScreen);

    m_statementsPage = new StatementsPage(m_settings, m_projectLane, m_pages);
    m_tabPages[Tab::Statements] = m_statementsPage;
    m_reviewNote = new QLabel;
    m_tabPages[Tab::Review] = placeholderPage(QStringLiteral("reviewPage"), QStringLiteral("Review"), m_reviewNote, m_pages);
    m_settingsPage = new SettingsPage(m_settings, m_buildLane, m_pages);
    m_tabPages[Tab::Settings] = m_settingsPage;
    for (const auto& [tab, page] : m_tabPages)
        m_pages->addWidget(page);

    // The bar shown when the project has errors.
    m_errorsBar = new QWidget(this);
    m_errorsBar->setObjectName(QStringLiteral("errorsBar"));
    m_errorsBar->setAttribute(Qt::WA_StyledBackground);
    m_errorsBar->setStyleSheet(QStringLiteral("#errorsBar { background: palette(alternate-base); border-bottom: 1px solid palette(mid); }"));
    m_errorsText = new QLabel(m_errorsBar);
    m_errorsText->setObjectName(QStringLiteral("errorsText"));
    m_errorsText->setWordWrap(true);
    auto* showProblems = new QPushButton(QStringLiteral("Show problems"), m_errorsBar);
    showProblems->setObjectName(QStringLiteral("showProblems"));
    connect(showProblems, &QPushButton::clicked, this, [this] {
        m_bottomPanel->show();
        m_bottomPanel->showProblems();
    });
    auto* errorsLayout = new QHBoxLayout(m_errorsBar);
    errorsLayout->setContentsMargins(12, 6, 12, 6);
    errorsLayout->addWidget(m_errorsText, 1);
    errorsLayout->addWidget(showProblems);
    m_errorsBar->hide();

    m_bottomPanel = new BottomPanel(this);
    m_bottomPanel->hide();
    connect(m_bottomPanel, &BottomPanel::problemActivated, this, [this](const QString& file, int line) {
        if (!m_session)
            return;
        showTab(Tab::Statements);
        m_statementsPage->showFileLine(file, line);
    });
    connect(m_statementsPage, &StatementsPage::refusalProblems, this, [this](const std::vector<Problem>& problems) {
        m_bottomPanel->setSaveProblems(problems);
        if (!problems.empty()) {
            m_bottomPanel->show();
            m_bottomPanel->showProblems();
        }
    });
    connect(m_statementsPage, &StatementsPage::outputLine, m_bottomPanel, &BottomPanel::appendOutput);
    connect(m_statementsPage, &StatementsPage::unsavedChanged, this,
            [this](bool any) { m_sideBar->button(Tab::Statements)->setBadgeDot(any); });
    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->addWidget(m_pages);
    m_splitter->addWidget(m_bottomPanel);
    m_splitter->setStretchFactor(0, 4);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setChildrenCollapsible(false);

    auto* right = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);
    rightLayout->addWidget(m_errorsBar);
    rightLayout->addWidget(m_splitter, 1);

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_sideBar);
    layout->addWidget(right, 1);
    setCentralWidget(central);

    createActions();
    createStatusBar();
    for (Lane* lane : {m_projectLane, m_buildLane, m_gitLane})
        connect(lane, &Lane::busyChanged, this, &MainWindow::updateBusy);
    showStart();
}

MainWindow::~MainWindow()
{
    for (SetupCheckView* view : findChildren<SetupCheckView*>())
        view->stop();
    // The page keeps its state before the session goes; the lanes wait for their work before the
    // window's parts go; the session's lock goes with it.
    m_statementsPage->setSession(nullptr);
    delete m_projectLane;
    delete m_buildLane;
    delete m_gitLane;
    delete m_session;
}

void MainWindow::createActions()
{
    QMenu* file = menuBar()->addMenu(QStringLiteral("&File"));
    file->addAction(QStringLiteral("&New project…"), this, [this] { newProject(); });
    QAction* open = file->addAction(QStringLiteral("&Open project…"), this, &MainWindow::chooseFolder);
    open->setShortcut(QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_O)));
    file->addAction(QStringLiteral("New &window"), this, [this] {
        auto* window = new MainWindow(m_settings);
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->show();
    });
    file->addSeparator();
    QAction* settings = file->addAction(QStringLiteral("&Settings"), this, [this] { showTab(Tab::Settings); });
    settings->setShortcut(tabShortcut(Tab::Settings));
    m_tabActions[Tab::Settings] = settings;
    file->addSeparator();
    file->addAction(QStringLiteral("E&xit"), qApp, &QApplication::closeAllWindows);

    QMenu* view = menuBar()->addMenu(QStringLiteral("&View"));
    for (const TabInfo& info : projectTabs()) {
        QAction* action = view->addAction(info.name, this, [this, tab = info.tab] { showTab(tab); });
        action->setShortcut(tabShortcut(info.tab));
        action->setEnabled(false);
        m_tabActions[info.tab] = action;
    }
    view->addSeparator();
    QAction* panel = view->addAction(QStringLiteral("&Bottom panel"), this, &MainWindow::toggleBottomPanel);
    panel->setShortcut(QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_QuoteLeft)));

    // The Statements tab's actions (Specs/m2-statements-review.md, "Shortcuts"), on its tab only.
    QMenu* statement = menuBar()->addMenu(QStringLiteral("&Statement"));
    const auto add = [this, statement](const QString& name, QKeySequence keys, std::function<void()> action) {
        QAction* item = statement->addAction(name, this, std::move(action));
        item->setShortcut(keys);
        item->setEnabled(false);
        m_statementActions.push_back(item);
        return item;
    };
    add(QStringLiteral("&New statement…"), QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_N)),
        [this] { m_statementsPage->newStatement(); });
    add(QStringLiteral("New &proof…"), QKeySequence(QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_N)),
        [this] { m_statementsPage->newProof(); });
    add(QStringLiteral("&Save"), QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_S)), [this] { m_statementsPage->save(); });
    statement->addSeparator();
    add(QStringLiteral("&Back"), QKeySequence(QKeyCombination(Qt::AltModifier, Qt::Key_Left)), [this] { m_statementsPage->back(); });
    add(QStringLiteral("&Forward"), QKeySequence(QKeyCombination(Qt::AltModifier, Qt::Key_Right)), [this] { m_statementsPage->forward(); });
    statement->addSeparator();
    add(QStringLiteral("&Find…"), QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_F)), [this] { m_statementsPage->find(false); });
    add(QStringLiteral("&Replace…"), QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_H)), [this] { m_statementsPage->find(true); });
    add(QStringLiteral("Find &next"), QKeySequence(Qt::Key_F3), [this] { m_statementsPage->findNext(true); });
    add(QStringLiteral("Find pre&vious"), QKeySequence(QKeyCombination(Qt::ShiftModifier, Qt::Key_F3)),
        [this] { m_statementsPage->findNext(false); });
    add(QStringLiteral("Find in the pro&ject…"), QKeySequence(QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_F)),
        [this] { m_statementsPage->findInProject(); });
    statement->addSeparator();
    add(QStringLiteral("&Larger text"), QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_Equal)),
        [this] { m_statementsPage->zoomEditor(1); });
    add(QStringLiteral("S&maller text"), QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_Minus)),
        [this] { m_statementsPage->zoomEditor(-1); });

    QMenu* help = menuBar()->addMenu(QStringLiteral("&Help"));
    help->addAction(QStringLiteral("&About Loom"), this, [this] {
        const std::string_view version = versionString();
        // Every dependency with its licence (Specs/m2-statements-review.md, "Dependencies and
        // toolchain"); later milestones add their rows as they add dependencies.
        const QString text = QStringLiteral(
            "<p><b>Loom %1</b><br>Mathematical research with AI agents.<br>Licensed under the GNU GPL v3.</p>"
            "<table cellspacing='0' cellpadding='3'>"
            "<tr><td>Qt %2 (Core, Gui, Widgets)</td><td>LGPL v3 / GPL v3</td><td>linked</td></tr>"
            "<tr><td>toml++</td><td>MIT</td><td>compiled in</td></tr>"
            "<tr><td>QScintilla 2.14.1</td><td>GPL v3</td><td>compiled in</td></tr>"
            "<tr><td>TeX Live: latexmk, pdfLaTeX, synctex, BibTeX</td><td>LPPL, GPL and others</td><td>separate programs, not linked</td></tr>"
            "<tr><td>Git, Git LFS</td><td>GPL v2, MIT</td><td>separate programs, not linked</td></tr>"
            "</table>")
            .arg(QString::fromUtf8(version.data(), static_cast<qsizetype>(version.size())), QString::fromLatin1(qVersion()));
        auto* box = new QMessageBox(QMessageBox::NoIcon, QStringLiteral("About Loom"), text, QMessageBox::Close, this);
        box->setObjectName(QStringLiteral("aboutLoom"));
        box->setTextFormat(Qt::RichText);
        box->setAttribute(Qt::WA_DeleteOnClose);
        box->open();
    });
}

void MainWindow::createStatusBar()
{
    m_branch = new QLabel(this);
    m_branch->setObjectName(QStringLiteral("branch"));
    m_buildState = new QLabel(this);
    m_buildState->setObjectName(QStringLiteral("buildState"));
    m_problemCounts = new QToolButton(this);
    m_problemCounts->setObjectName(QStringLiteral("problemCounts"));
    m_problemCounts->setAutoRaise(true);
    m_problemCounts->setToolTip(QStringLiteral("Show the Problems panel"));
    connect(m_problemCounts, &QToolButton::clicked, this, [this] {
        m_bottomPanel->show();
        m_bottomPanel->showProblems();
    });
    m_busy = new QProgressBar(this);
    m_busy->setObjectName(QStringLiteral("busy"));
    m_busy->setRange(0, 0);
    m_busy->setTextVisible(false);
    m_busy->setFixedWidth(80);
    m_busy->setMaximumHeight(12);
    m_busy->hide();
    // No frames around the items: Windows' style draws one even around an empty label.
    statusBar()->setStyleSheet(QStringLiteral("QStatusBar::item { border: none; }"));
    m_buildState->hide(); // The build's state comes with compiling (#36).
    statusBar()->addWidget(m_branch);
    statusBar()->addWidget(m_buildState);
    statusBar()->addPermanentWidget(m_problemCounts);
    statusBar()->addPermanentWidget(m_busy);
    m_problemCounts->hide();
}

QWidget* MainWindow::page(Tab tab) const
{
    return m_tabPages.at(tab);
}

bool MainWindow::onStartScreen() const
{
    return m_pages->currentWidget() == m_startScreen;
}

void MainWindow::showStart()
{
    m_pages->setCurrentWidget(m_startScreen);
    m_sideBar->clearCurrent();
    updateStatementActions();
}

void MainWindow::showTab(Tab tab)
{
    if (tab != Tab::Settings && !m_session)
        return;
    m_current = tab;
    m_pages->setCurrentWidget(m_tabPages.at(tab));
    m_sideBar->setCurrent(tab);
    if (m_session)
        m_settings->setLastTab(tabInfo(tab).key);
    updateStatementActions();
}

void MainWindow::updateStatementActions()
{
    const bool on = m_session && !onStartScreen() && m_current == Tab::Statements;
    for (QAction* action : m_statementActions)
        action->setEnabled(on);
}

void MainWindow::askAboutUnsavedEdits(const QString& why, std::function<void()> proceed)
{
    if (!m_statementsPage->hasUnsavedEdits()) {
        proceed();
        return;
    }
    const int files = static_cast<int>(m_statementsPage->unsavedPaths().size());
    auto* box = new QMessageBox(QMessageBox::Question, QStringLiteral("Unsaved edits"),
                                QStringLiteral("%1 unsaved edits. %2")
                                    .arg(files == 1 ? QStringLiteral("1 file has") : QStringLiteral("%1 files have").arg(files), why),
                                QMessageBox::NoButton, this);
    box->setObjectName(QStringLiteral("unsavedEditsMessage"));
    box->setAttribute(Qt::WA_DeleteOnClose);
    QPushButton* saveAll = box->addButton(QStringLiteral("Save all"), QMessageBox::AcceptRole);
    saveAll->setObjectName(QStringLiteral("saveAll"));
    QPushButton* keep = box->addButton(QStringLiteral("Keep them for next time"), QMessageBox::DestructiveRole);
    keep->setObjectName(QStringLiteral("keep"));
    QPushButton* cancel = box->addButton(QMessageBox::Cancel);
    cancel->setObjectName(QStringLiteral("cancel"));
    box->setDefaultButton(cancel);
    box->setEscapeButton(cancel);
    connect(box, &QMessageBox::finished, this, [this, box, saveAll, keep, proceed] {
        if (box->clickedButton() == saveAll) {
            m_statementsPage->saveAll([this, proceed](bool ok) {
                if (ok)
                    proceed();
                else
                    showTab(Tab::Statements);
            });
        } else if (box->clickedButton() == keep) {
            // The recovery files stay, written now, and are offered back next time.
            m_statementsPage->writeAllDrafts(proceed);
        }
    });
    box->open();
    cancel->setFocus();
}

void MainWindow::toggleBottomPanel()
{
    m_bottomPanel->setVisible(!m_bottomPanel->isVisible());
}

void MainWindow::openFolder(const QString& path, OpenMode mode)
{
    const QString folder = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (m_session && sameFolder(m_session->root(), folder)) {
        // Already open here: nothing to open, and taking its lock again would fail.
        if (onStartScreen())
            showTab(Tab::Statements);
        emit openFinished();
        return;
    }
    if (m_session && m_statementsPage->hasUnsavedEdits() && !m_unsavedAnswered) {
        // One project per window: the open one's unsaved edits are asked about first.
        askAboutUnsavedEdits(QStringLiteral("What should Loom do with them before it opens another project?"), [this, path, mode] {
            m_unsavedAnswered = true;
            openFolder(path, mode);
        });
        return;
    }
    const GitTool git = m_settings->gitTool();
    m_projectLane->run(this,
        [folder, git] {
            Opening opening;
            opening.folder = inspectFolder(folder);
            if (opening.folder.kind != FolderKind::Project)
                return opening;
            AppLockResult lock = takeAppLock(opening.folder.projectRoot);
            opening.lock = lock.outcome;
            opening.lockMessage = lock.message;
            opening.lockFile = std::move(lock.lock);
            if (opening.lock == AppLockOutcome::Taken)
                opening.snapshot = std::make_shared<const ProjectSnapshot>(readSnapshot(opening.folder.projectRoot, git));
            return opening;
        },
        [this, mode](Opening opening) { finishOpening(std::move(opening), mode); });
}

void MainWindow::restoreLastProject()
{
    const QString last = m_settings->lastProject();
    if (m_settings->reopenLastProject() && !last.isEmpty())
        openFolder(last, OpenMode::Restore);
}

void MainWindow::finishOpening(Opening opening, OpenMode mode)
{
    QString refusal;
    if (opening.folder.kind != FolderKind::Project)
        refusal = opening.folder.message;
    else if (opening.lock != AppLockOutcome::Taken)
        refusal = opening.lockMessage;
    if (!refusal.isEmpty()) {
        // The project open here stays, so its unsaved edits will be asked about again.
        m_unsavedAnswered = false;
        if (mode == OpenMode::Restore)
            statusBar()->showMessage(QStringLiteral("Loom couldn't reopen %1: %2").arg(QDir::toNativeSeparators(opening.folder.folder), refusal), 15'000);
        else
            showOpenMessage(opening.folder, refusal);
        emit openFinished();
        return;
    }

    // Opening another project in a window that has one: the old one closes, releasing its lock.
    // Its unsaved edits were asked about before this open started (openFolder()).
    m_unsavedAnswered = false;
    m_statementsPage->setSession(nullptr);
    delete m_session;
    m_session = new ProjectSession(std::move(opening.lockFile), opening.snapshot, m_projectLane,
                                   [settings = m_settings] { return settings->gitTool(); }, this);
    connect(m_session, &ProjectSession::snapshotChanged, this, &MainWindow::updateFromSnapshot);

    const ProjectSnapshot& snapshot = *opening.snapshot;
    const QString name = snapshot.config.name.isEmpty() ? QFileInfo(snapshot.root).fileName() : snapshot.config.name;
    Tab tab = Tab::Statements;
    if (mode == OpenMode::Restore)
        tabFromKey(m_settings->lastTab(), tab);
    m_settings->addRecentProject(snapshot.root, name);
    m_settings->setLastProject(snapshot.root);

    m_sideBar->setProjectTabsVisible(true);
    for (const TabInfo& info : projectTabs())
        m_tabActions.at(info.tab)->setEnabled(true);
    m_statementsPage->setSession(m_session);
    showTab(tab);
    updateFromSnapshot();
    emit projectOpened(snapshot.root);
    emit openFinished();
}

void MainWindow::showOpenMessage(const FolderInspection& folder, const QString& message)
{
    const bool helpful = folder.kind == FolderKind::InsideProject || folder.kind == FolderKind::Empty;
    auto* box = new QMessageBox(helpful ? QMessageBox::Information : QMessageBox::Warning, QStringLiteral("Open project"), message,
                                QMessageBox::NoButton, this);
    box->setObjectName(QStringLiteral("openMessage"));
    box->setTextFormat(Qt::PlainText);
    box->setAttribute(Qt::WA_DeleteOnClose);
    QPushButton* action = nullptr;
    switch (folder.kind) {
    case FolderKind::InsideProject:
        action = box->addButton(QStringLiteral("Open that project"), QMessageBox::AcceptRole);
        connect(action, &QPushButton::clicked, this, [this, root = folder.projectRoot] { openFolder(root); });
        break;
    case FolderKind::Empty:
        action = box->addButton(QStringLiteral("Create a project here"), QMessageBox::AcceptRole);
        connect(action, &QPushButton::clicked, this, [this, where = folder.folder] {
            const QFileInfo info(where);
            newProject(info.absolutePath(), info.fileName());
        });
        break;
    case FolderKind::TomlUnreadable:
        action = box->addButton(QStringLiteral("Show in Explorer"), QMessageBox::ActionRole);
        connect(action, &QPushButton::clicked, this, [file = folder.folder + QStringLiteral("/loom.toml")] { revealInExplorer(file); });
        break;
    case FolderKind::Project:
    case FolderKind::NotAProject:
    case FolderKind::FormatTooNew:
    case FolderKind::Missing:
        break;
    }
    if (action)
        action->setObjectName(QStringLiteral("action"));
    QPushButton* close = box->addButton(action ? QMessageBox::Cancel : QMessageBox::Close);
    close->setObjectName(QStringLiteral("close"));
    box->setDefaultButton(action ? action : close);
    box->open();
}

void MainWindow::newProject(const QString& parentFolder, const QString& folderName)
{
    auto* dialog = new NewProjectDialog(m_settings, m_projectLane, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    if (!folderName.isEmpty())
        dialog->prefill(parentFolder, folderName, folderName);
    connect(dialog, &NewProjectDialog::created, this, [this](const QString& path) { openFolder(path); });
    dialog->open();
}

void MainWindow::chooseFolder()
{
    const QString start = m_session ? QFileInfo(m_session->root()).absolutePath() : QString();
    const QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("Open project"), start);
    if (!folder.isEmpty())
        openFolder(folder);
}

void MainWindow::checkSetup()
{
    auto* dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("setupCheckDialog"));
    dialog->setWindowTitle(QStringLiteral("Check my setup"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(640, 420);
    auto* view = new SetupCheckView(m_settings, m_buildLane, dialog);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(view, 1);
    layout->addWidget(buttons);
    dialog->open();
    view->start();
}

void MainWindow::updateFromSnapshot()
{
    if (!m_session)
        return;
    const std::shared_ptr<const ProjectSnapshot> snapshot = m_session->snapshot();
    const QString name = snapshot->config.name.isEmpty() ? QFileInfo(snapshot->root).fileName() : snapshot->config.name;
    setWindowTitle(snapshot->branch.isEmpty() ? QStringLiteral("%1 · Loom").arg(name)
                                              : QStringLiteral("%1 · %2 · Loom").arg(name, snapshot->branch));
    m_branch->setText(snapshot->branch);
    m_branch->setVisible(!snapshot->branch.isEmpty());

    QStringList counts;
    if (snapshot->errors > 0)
        counts << count(snapshot->errors, "error", "errors");
    if (snapshot->warnings > 0)
        counts << count(snapshot->warnings, "warning", "warnings");
    m_problemCounts->setText(counts.isEmpty() ? QStringLiteral("No problems") : counts.join(QStringLiteral(", ")));
    m_problemCounts->show();
    m_bottomPanel->setProblems(snapshot->problems);

    if (!snapshot->unreadable.isEmpty()) {
        m_errorsText->setText(snapshot->unreadable);
        m_errorsBar->show();
    } else if (snapshot->errors > 0) {
        m_errorsText->setText(QStringLiteral("This project has %1; commits are refused until they're fixed.")
                                  .arg(count(snapshot->errors, "error", "errors")));
        m_errorsBar->show();
    } else {
        m_errorsBar->hide();
    }

    m_reviewNote->setText(snapshot->gitProblem.isEmpty() ? QStringLiteral("Your uncommitted changes, compiled before and after, come next.")
                                                         : snapshot->gitProblem);
}

void MainWindow::updateBusy()
{
    m_busy->setVisible(m_projectLane->busy() || m_buildLane->busy() || m_gitLane->busy());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_session && m_statementsPage->hasUnsavedEdits() && !m_unsavedAnswered) {
        event->ignore();
        askAboutUnsavedEdits(QStringLiteral("What should Loom do with them before it closes?"), [this] {
            m_unsavedAnswered = true;
            close();
        });
        return;
    }
    m_unsavedAnswered = false;
    m_settings->setWindowGeometry(saveGeometry());
    if (m_session) {
        m_settings->setLastProject(m_session->root());
        m_settings->setLastTab(tabInfo(m_current).key);
    }
    for (SetupCheckView* view : findChildren<SetupCheckView*>())
        view->stop();
    QMainWindow::closeEvent(event);
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::ActivationChange && isActiveWindow() && m_session)
        m_session->refresh();
    QMainWindow::changeEvent(event);
}

} // namespace loom
