// The app shell (Specs/m2-statements-review.md, "The app shell" and "Testing the UI"; Verification,
// "The shell": "Start screen, each row of Opening a folder, tab switching keeps state, shortcuts,
// Settings saved per user, reopening the last project"). Each test drives the real widgets, on
// copies of the demo project (Tests/Support/DemoProject), with the app's settings in a temporary
// folder, so the developer's own are never read or written.

#include "LoomApp/AppSettings.h"
#include "LoomApp/BottomPanel.h"
#include "LoomApp/Lane.h"
#include "LoomApp/MainWindow.h"
#include "LoomApp/NewProjectDialog.h"
#include "LoomApp/ProjectSession.h"
#include "LoomApp/SettingsPage.h"
#include "LoomApp/SetupCheckView.h"
#include "LoomApp/SideBar.h"
#include "LoomApp/StartScreen.h"

#include "DemoProject.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

#include <memory>

using loom::MainWindow;
using loom::Tab;

namespace {

/// The app's folders inside a temporary folder.
loom::AppPaths pathsIn(const QTemporaryDir& temp)
{
    return {temp.filePath(QStringLiteral("AppData/Loom/Loom.ini")), temp.filePath(QStringLiteral("LocalAppData/Loom/projects"))};
}

/// Saves a picture of a widget for people to look at (Specs/m2-statements-review.md, "Testing the
/// UI", "Screenshots": never compared pixel by pixel).
void screenshot(QWidget* widget, const QString& name)
{
    QDir().mkpath(QStringLiteral(LOOM_SCREENSHOTS));
    widget->grab().save(QStringLiteral(LOOM_SCREENSHOTS "/") + name + QStringLiteral(".png"));
}

/// Opens a folder and waits for the window to finish with it.
/// @returns Whether it finished in time.
bool openAndWait(MainWindow& window, const QString& path, MainWindow::OpenMode mode = MainWindow::OpenMode::Interactive)
{
    QSignalSpy finished(&window, &MainWindow::openFinished);
    window.openFolder(path, mode);
    return finished.count() > 0 || finished.wait(30'000);
}

/// The message a refused open shows, or null.
QMessageBox* openMessage(const MainWindow& window)
{
    return window.findChild<QMessageBox*>(QStringLiteral("openMessage"));
}

/// Closes the message a refused open shows, and waits until it's gone.
void closeMessage(MainWindow& window)
{
    QMessageBox* box = openMessage(window);
    QVERIFY(box);
    QTest::mouseClick(box->findChild<QPushButton*>(QStringLiteral("close")), Qt::LeftButton);
    QTRY_VERIFY(!openMessage(window));
}

/// Writes a file, creating its folder. @returns Whether it worked.
bool writeFile(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

} // namespace

/// Qt Test cases for the app shell.
class MainWindowTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void opens();
    void startScreenListsRecentProjects();
    void opensTheDemoProject();
    void openingAFolderInsideAProject();
    void openingAnEmptyFolder();
    void openingAFolderThatIsntAProject();
    void openingAnUnreadableLoomToml();
    void openingAProjectFromANewerLoom();
    void openingAProjectOpenInAnotherWindow();
    void openingAProjectWithErrors();
    void openingAProjectOutsideGit();
    void openingAProjectWithoutGit();
    void newProjectFromTheForm();
    void newProjectRefusalShowsInTheForm();
    void sideBarTooltipsAndHighlight();
    void shortcutsSwitchTabs();
    void switchingTabsKeepsState();
    void settingsAreSavedPerUser();
    void reopensTheLastProjectOnTheLastTab();
    void startsOnTheStartScreenWhenItCant();
    void watchingTheFolderUpdatesTheSnapshot();
    void checkMySetupListsEveryTool();
    void projectStateFileIsOnePerFolder();
    void badges();

private:
    /// A new window on the test's settings, shown.
    std::unique_ptr<MainWindow> newWindow();

    std::unique_ptr<QTemporaryDir> m_temp;
    std::unique_ptr<loom::AppSettings> m_settings;
    QTemporaryDir m_gitHome;
};

void MainWindowTests::initTestCase()
{
    // Every git the app runs gets a test identity and no system config, as DemoProject's does.
    QVERIFY(m_gitHome.isValid());
    const QString config = m_gitHome.filePath(QStringLiteral("gitconfig"));
    QVERIFY(writeFile(config, "[user]\n\tname = Loom Test\n\temail = test@example.com\n"));
    qputenv("GIT_CONFIG_GLOBAL", QDir::toNativeSeparators(config).toLocal8Bit());
    qputenv("GIT_CONFIG_NOSYSTEM", "1");
}

void MainWindowTests::init()
{
    m_temp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_temp->isValid());
    m_settings = std::make_unique<loom::AppSettings>(pathsIn(*m_temp));
}

void MainWindowTests::cleanup()
{
    m_settings.reset();
    m_temp.reset();
}

std::unique_ptr<MainWindow> MainWindowTests::newWindow()
{
    auto window = std::make_unique<MainWindow>(m_settings.get());
    window->resize(1000, 700);
    window->show();
    return window;
}

void MainWindowTests::opens()
{
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));
    QCOMPARE(window->windowTitle(), QStringLiteral("Loom"));
    QVERIFY(window->onStartScreen());
    // Without a project, the side bar shows only the gear.
    QVERIFY(!window->sideBar()->button(Tab::Statements)->isVisible());
    QVERIFY(!window->sideBar()->button(Tab::Review)->isVisible());
    QVERIFY(window->sideBar()->button(Tab::Settings)->isVisible());
    screenshot(window.get(), QStringLiteral("start-screen-empty"));
}

void MainWindowTests::startScreenListsRecentProjects()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const QString gone = m_temp->filePath(QStringLiteral("Gone"));
    m_settings->addRecentProject(gone, QStringLiteral("Gone project"));
    m_settings->addRecentProject(demo.root(), QStringLiteral("Demo"));

    const std::unique_ptr<MainWindow> window = newWindow();
    QListWidget* list = window->startScreen()->recentList();
    QCOMPARE(list->count(), 2);
    // Newest first, each its name and path.
    QCOMPARE(list->item(0)->data(Qt::UserRole).toString(), demo.root());
    QWidget* first = list->itemWidget(list->item(0));
    QCOMPARE(first->findChild<QLabel*>(QStringLiteral("name"))->text(), QStringLiteral("Demo"));
    QCOMPARE(first->findChild<QLabel*>(QStringLiteral("path"))->text(), QDir::toNativeSeparators(demo.root()));
    QVERIFY(!first->findChild<QPushButton*>(QStringLiteral("removeFromList")));
    // One that no longer exists is greyed, with Remove from list.
    QWidget* second = list->itemWidget(list->item(1));
    QVERIFY(!second->findChild<QLabel*>(QStringLiteral("name"))->isEnabled());
    screenshot(window.get(), QStringLiteral("start-screen-recent"));
    QTest::mouseClick(second->findChild<QPushButton*>(QStringLiteral("removeFromList")), Qt::LeftButton);
    QTRY_COMPARE(list->count(), 1);
    QCOMPARE(m_settings->recentProjects().size(), std::size_t(1));

    // Clicking one that exists opens it.
    QSignalSpy opened(window.get(), &MainWindow::projectOpened);
    QTest::mouseClick(list->viewport(), Qt::LeftButton, {}, list->visualItemRect(list->item(0)).center());
    QVERIFY(opened.count() > 0 || opened.wait(30'000));
    QCOMPARE(window->session()->root(), demo.root());
}

void MainWindowTests::opensTheDemoProject()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openAndWait(*window, demo.root()));
    QVERIFY(window->session());
    QVERIFY(!openMessage(*window));
    QCOMPARE(window->windowTitle(), QStringLiteral("Demo · main · Loom"));
    QCOMPARE(window->currentTab(), Tab::Statements);
    QVERIFY(!window->onStartScreen());
    QVERIFY(window->sideBar()->button(Tab::Statements)->isVisible());
    QVERIFY(window->sideBar()->button(Tab::Statements)->isChecked());
    QCOMPARE(window->findChild<QLabel*>(QStringLiteral("branch"))->text(), QStringLiteral("main"));
    // The demo has no errors, so no bar.
    QCOMPARE(window->session()->snapshot()->errors, 0);
    QVERIFY(window->errorsBar()->isHidden());
    QVERIFY(window->session()->snapshot()->contents.statements.size() >= 10);
    // The lock, and what the app remembers.
    QVERIFY(QFileInfo::exists(demo.root() + QStringLiteral("/.loom/app.lock")));
    QCOMPARE(m_settings->lastProject(), demo.root());
    QCOMPARE(m_settings->recentProjects().front().name, QStringLiteral("Demo"));
    screenshot(window.get(), QStringLiteral("project-open"));
}

void MainWindowTests::openingAFolderInsideAProject()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openAndWait(*window, demo.root() + QStringLiteral("/Statements")));
    QMessageBox* box = openMessage(*window);
    QVERIFY(box);
    QCOMPARE(box->text(), QStringLiteral("This folder is inside the project Demo at %1.").arg(QDir::toNativeSeparators(demo.root())));
    QVERIFY(!window->session());
    screenshot(box, QStringLiteral("open-inside-a-project"));
    QSignalSpy opened(window.get(), &MainWindow::projectOpened);
    QTest::mouseClick(box->findChild<QPushButton*>(QStringLiteral("action")), Qt::LeftButton);
    QVERIFY(opened.count() > 0 || opened.wait(30'000));
    QCOMPARE(window->session()->root(), demo.root());
}

void MainWindowTests::openingAnEmptyFolder()
{
    const QString empty = m_temp->filePath(QStringLiteral("New research"));
    QVERIFY(QDir().mkpath(empty));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openAndWait(*window, empty));
    QMessageBox* box = openMessage(*window);
    QVERIFY(box);
    QCOMPARE(box->text(), QStringLiteral("This folder is empty."));
    // Create a project here: the New project form, filled in.
    QTest::mouseClick(box->findChild<QPushButton*>(QStringLiteral("action")), Qt::LeftButton);
    loom::NewProjectDialog* dialog = nullptr;
    QTRY_VERIFY((dialog = window->findChild<loom::NewProjectDialog*>()));
    QCOMPARE(dialog->findChild<QLineEdit*>(QStringLiteral("parentFolder"))->text(), QDir::toNativeSeparators(m_temp->path()));
    QCOMPARE(dialog->findChild<QLineEdit*>(QStringLiteral("folderName"))->text(), QStringLiteral("New research"));
    QCOMPARE(dialog->findChild<QLineEdit*>(QStringLiteral("projectName"))->text(), QStringLiteral("New research"));
    dialog->reject();
}

void MainWindowTests::openingAFolderThatIsntAProject()
{
    const QString folder = m_temp->filePath(QStringLiteral("Papers I like"));
    QVERIFY(writeFile(folder + QStringLiteral("/notes.txt"), "Read these.\n"));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openAndWait(*window, folder));
    QMessageBox* box = openMessage(*window);
    QVERIFY(box);
    QCOMPARE(box->text(), QStringLiteral("This folder isn't a Loom project: it has no loom.toml. Loom creates projects only in new or empty folders."));
    QVERIFY(!box->findChild<QPushButton*>(QStringLiteral("action")));
    closeMessage(*window);
    QVERIFY(window->onStartScreen());
}

void MainWindowTests::openingAnUnreadableLoomToml()
{
    const QString folder = m_temp->filePath(QStringLiteral("Broken"));
    QVERIFY(writeFile(folder + QStringLiteral("/loom.toml"), "format = 1\nname = \"Broken\nidea = \"x\"\n"));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openAndWait(*window, folder));
    QMessageBox* box = openMessage(*window);
    QVERIFY(box);
    QVERIFY2(box->text().startsWith(QStringLiteral("Loom can't read loom.toml: ")), qPrintable(box->text()));
    QVERIFY2(box->text().endsWith(QStringLiteral(" (line 2). Fix it in a text editor, then open the project again.")), qPrintable(box->text()));
    QCOMPARE(box->findChild<QPushButton*>(QStringLiteral("action"))->text(), QStringLiteral("Show in Explorer"));
    QVERIFY(!window->session());
    QVERIFY(!QFileInfo::exists(folder + QStringLiteral("/.loom"))); // Not opened: nothing written.
    closeMessage(*window);
}

void MainWindowTests::openingAProjectFromANewerLoom()
{
    const QString folder = m_temp->filePath(QStringLiteral("Future"));
    QVERIFY(writeFile(folder + QStringLiteral("/loom.toml"), "format = 99\nname = \"Future\"\ncreated = 2030-01-01\n"));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openAndWait(*window, folder));
    QMessageBox* box = openMessage(*window);
    QVERIFY(box);
    QCOMPARE(box->text(), QStringLiteral("This project was made by a newer Loom (format 99). Update Loom to open it."));
    QVERIFY(!window->session());
    QVERIFY(!QFileInfo::exists(folder + QStringLiteral("/.loom")));
    closeMessage(*window);
}

void MainWindowTests::openingAProjectOpenInAnotherWindow()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    std::unique_ptr<MainWindow> first = newWindow();
    QVERIFY(openAndWait(*first, demo.root()));
    QVERIFY(first->session());

    {
        const std::unique_ptr<MainWindow> second = newWindow();
        QVERIFY(openAndWait(*second, demo.root()));
        QMessageBox* box = openMessage(*second);
        QVERIFY(box);
        QCOMPARE(box->text(), QStringLiteral("This project is open in another Loom window."));
        QVERIFY(!second->session());
        closeMessage(*second);
    }

    // Once the first window has gone, another can open it.
    first.reset();
    const std::unique_ptr<MainWindow> third = newWindow();
    QVERIFY(openAndWait(*third, demo.root()));
    QVERIFY(third->session());
}

void MainWindowTests::openingAProjectWithErrors()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    // An unknown kind is an error, but the project still opens.
    QVERIFY(writeFile(demo.root() + QStringLiteral("/Statements/M4CD.tex"),
                      "+++\nid = \"M4CD\"\nkind = \"lema\"\ntitle = \"A misspelt kind\"\ncreated = 2026-11-05\n+++\nText.\n"));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openAndWait(*window, demo.root()));
    QVERIFY(window->session());
    const int errors = window->session()->snapshot()->errors;
    QVERIFY(errors >= 1);
    QVERIFY(window->errorsBar()->isVisible());
    QCOMPARE(window->errorsBar()->findChild<QLabel*>(QStringLiteral("errorsText"))->text(),
             QStringLiteral("This project has %1 %2; commits are refused until they're fixed.").arg(errors).arg(errors == 1 ? "error" : "errors"));
    QVERIFY(window->bottomPanel()->isHidden());
    QTest::mouseClick(window->errorsBar()->findChild<QPushButton*>(QStringLiteral("showProblems")), Qt::LeftButton);
    QVERIFY(window->bottomPanel()->isVisible());
    QCOMPARE(window->bottomPanel()->currentWidget(), window->bottomPanel()->problems());
    bool listed = false;
    for (int i = 0; i < window->bottomPanel()->problems()->topLevelItemCount(); ++i)
        listed = listed || window->bottomPanel()->problems()->topLevelItem(i)->text(0) == QStringLiteral("Statements/M4CD.tex");
    QVERIFY(listed);
    screenshot(window.get(), QStringLiteral("project-with-errors"));
}

void MainWindowTests::openingAProjectOutsideGit()
{
    // A copy of the demo's files without a repository.
    const QString copy = m_temp->filePath(QStringLiteral("Loose"));
    QVERIFY(loomtest::copyFolder(QStringLiteral(LOOM_DEMO_PROJECT), copy).isEmpty());
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openAndWait(*window, copy));
    QVERIFY(window->session());
    QCOMPARE(window->session()->snapshot()->gitProblem, QStringLiteral("This project isn't in a git repository"));
    QCOMPARE(window->windowTitle(), QStringLiteral("Demo · Loom"));
    window->showTab(Tab::Review);
    bool said = false;
    for (QLabel* label : window->page(Tab::Review)->findChildren<QLabel*>())
        said = said || label->text() == QStringLiteral("This project isn't in a git repository");
    QVERIFY(said);
}

void MainWindowTests::openingAProjectWithoutGit()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    m_settings->setGitPath(m_temp->filePath(QStringLiteral("no-git-here/git")));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openAndWait(*window, demo.root()));
    QVERIFY(window->session());
    QCOMPARE(window->session()->snapshot()->gitProblem, QStringLiteral("Loom can't find git; Check my setup says what to install."));
    // Validation still ran, without the checks against the last commit.
    QVERIFY(window->session()->snapshot()->unreadable.isEmpty());
}

void MainWindowTests::newProjectFromTheForm()
{
    const std::unique_ptr<MainWindow> window = newWindow();
    QTest::mouseClick(window->startScreen()->findChild<QPushButton*>(QStringLiteral("newProject")), Qt::LeftButton);
    loom::NewProjectDialog* dialog = nullptr;
    QTRY_VERIFY((dialog = window->findChild<loom::NewProjectDialog*>()));
    QPushButton* create = dialog->findChild<QPushButton*>(QStringLiteral("create"));
    auto* parent = dialog->findChild<QLineEdit*>(QStringLiteral("parentFolder"));
    parent->setText(QDir::toNativeSeparators(m_temp->path()));
    // The folder name follows the project's name as it's typed.
    QTest::keyClicks(dialog->findChild<QLineEdit*>(QStringLiteral("projectName")), QStringLiteral("Decay rates"));
    QCOMPARE(dialog->findChild<QLineEdit*>(QStringLiteral("folderName"))->text(), QStringLiteral("Decay rates"));
    dialog->findChild<QPlainTextEdit*>(QStringLiteral("idea"))->setPlainText(QStringLiteral("Find the sharp decay rate."));
    QVERIFY(create->isEnabled());
    screenshot(dialog, QStringLiteral("new-project-form"));

    QSignalSpy opened(window.get(), &MainWindow::projectOpened);
    QTest::mouseClick(create, Qt::LeftButton);
    QVERIFY(opened.count() > 0 || opened.wait(60'000));
    const QString root = m_temp->filePath(QStringLiteral("Decay rates"));
    QCOMPARE(window->session()->root(), root);
    QVERIFY(QFileInfo::exists(root + QStringLiteral("/loom.toml")));
    QVERIFY(QFileInfo::exists(root + QStringLiteral("/.git")));
    QCOMPARE(window->currentTab(), Tab::Statements);
    QCOMPARE(window->windowTitle(), QStringLiteral("Decay rates · main · Loom"));
}

void MainWindowTests::newProjectRefusalShowsInTheForm()
{
    // A folder that isn't empty: M1's refusal, shown in the form, which stays open.
    QVERIFY(writeFile(m_temp->filePath(QStringLiteral("Taken/file.txt")), "x\n"));
    const std::unique_ptr<MainWindow> window = newWindow();
    window->newProject(m_temp->path(), QStringLiteral("Taken"));
    loom::NewProjectDialog* dialog = window->findChild<loom::NewProjectDialog*>();
    QVERIFY(dialog);
    QTest::mouseClick(dialog->findChild<QPushButton*>(QStringLiteral("create")), Qt::LeftButton);
    auto* message = dialog->findChild<QLabel*>(QStringLiteral("message"));
    QTRY_VERIFY_WITH_TIMEOUT(message->isVisible(), 30'000);
    QCOMPARE(message->text(), QStringLiteral("`%1` isn't empty. Loom creates projects only in a new or empty folder.")
                                  .arg(QDir::toNativeSeparators(m_temp->filePath(QStringLiteral("Taken")))));
    QVERIFY(dialog->isVisible());
    QVERIFY(!window->session());
    screenshot(dialog, QStringLiteral("new-project-refused"));
    dialog->reject();
}

void MainWindowTests::sideBarTooltipsAndHighlight()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openAndWait(*window, demo.root()));
    loom::SideBar* bar = window->sideBar();
    // Hovering shows the name, the shortcut and one line.
    // "Ctrl+1" on Windows and Linux; the platform's own words for it elsewhere.
    const auto keys = [](Qt::Key key) { return QKeySequence(QKeyCombination(Qt::ControlModifier, key)).toString(QKeySequence::NativeText); };
    QCOMPARE(bar->button(Tab::Statements)->toolTip(),
             QStringLiteral("<b>Statements</b>&nbsp;&nbsp;%1<br>Edit statements and proofs, with the compiled PDF beside them").arg(keys(Qt::Key_1)));
    QCOMPARE(bar->button(Tab::Review)->toolTip(),
             QStringLiteral("<b>Review</b>&nbsp;&nbsp;%1<br>Your uncommitted changes, as compiled math").arg(keys(Qt::Key_2)));
    QVERIFY(bar->button(Tab::Settings)->toolTip().startsWith(QStringLiteral("<b>Settings</b>&nbsp;&nbsp;%1<br>").arg(keys(Qt::Key_Comma))));
    // The open tab is highlighted, and only it.
    QTest::mouseClick(bar->button(Tab::Review), Qt::LeftButton);
    QCOMPARE(window->currentTab(), Tab::Review);
    QVERIFY(bar->button(Tab::Review)->isChecked());
    QVERIFY(!bar->button(Tab::Statements)->isChecked());
    QVERIFY(!bar->button(Tab::Settings)->isChecked());
    QTest::mouseClick(bar->button(Tab::Settings), Qt::LeftButton);
    QCOMPARE(window->currentTab(), Tab::Settings);
    QVERIFY(bar->button(Tab::Settings)->isChecked());
    QVERIFY(!bar->button(Tab::Review)->isChecked());
    screenshot(bar, QStringLiteral("side-bar"));
}

void MainWindowTests::shortcutsSwitchTabs()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openAndWait(*window, demo.root()));
    window->activateWindow();
    if (!QTest::qWaitForWindowActive(window.get()))
        QSKIP("The window couldn't be made the active window here, and shortcuts need it");
    QTest::keyClick(window.get(), Qt::Key_2, Qt::ControlModifier);
    QCOMPARE(window->currentTab(), Tab::Review);
    QTest::keyClick(window.get(), Qt::Key_1, Qt::ControlModifier);
    QCOMPARE(window->currentTab(), Tab::Statements);
    QTest::keyClick(window.get(), Qt::Key_Comma, Qt::ControlModifier);
    QCOMPARE(window->currentTab(), Tab::Settings);
    QVERIFY(window->sideBar()->button(Tab::Settings)->isChecked());
    // Ctrl+` shows and hides the bottom panel.
    QVERIFY(window->bottomPanel()->isHidden());
    QTest::keyClick(window.get(), Qt::Key_QuoteLeft, Qt::ControlModifier);
    QVERIFY(window->bottomPanel()->isVisible());
    QTest::keyClick(window.get(), Qt::Key_QuoteLeft, Qt::ControlModifier);
    QVERIFY(window->bottomPanel()->isHidden());
}

void MainWindowTests::switchingTabsKeepsState()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    window->resize(900, 360); // Small, so the Settings page scrolls.
    QVERIFY(openAndWait(*window, demo.root()));
    window->showTab(Tab::Settings);
    QWidget* settings = window->page(Tab::Settings);
    QScrollBar* scroll = settings->findChild<QScrollArea*>()->verticalScrollBar();
    QTRY_VERIFY(scroll->maximum() > 0);
    scroll->setValue(scroll->maximum() / 2);
    const int where = scroll->value();
    auto* latexmk = settings->findChild<QLineEdit*>(QStringLiteral("latexmkPath"));
    latexmk->setText(QStringLiteral("half-typed"));
    latexmk->setCursorPosition(4);

    window->showTab(Tab::Review);
    window->showTab(Tab::Statements);
    window->showTab(Tab::Settings);
    // The same page, as it was.
    QCOMPARE(window->page(Tab::Settings), settings);
    QCOMPARE(scroll->value(), where);
    QCOMPARE(latexmk->text(), QStringLiteral("half-typed"));
    QCOMPARE(latexmk->cursorPosition(), 4);
}

void MainWindowTests::settingsAreSavedPerUser()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openAndWait(*window, demo.root()));
    window->showTab(Tab::Settings);
    QWidget* page = window->page(Tab::Settings);
    screenshot(window.get(), QStringLiteral("settings"));

    auto* theme = page->findChild<QComboBox*>(QStringLiteral("theme"));
    theme->setCurrentIndex(theme->findText(QStringLiteral("Dark")));
    auto* timeout = page->findChild<QSpinBox*>(QStringLiteral("compileTimeout"));
    QCOMPARE(timeout->value(), 120);
    timeout->setValue(300);
    auto* compileOnSave = page->findChild<QCheckBox*>(QStringLiteral("compileOnSave"));
    QVERIFY(compileOnSave->isChecked());
    compileOnSave->setChecked(false);
    auto* mode = page->findChild<QComboBox*>(QStringLiteral("defaultCompileMode"));
    mode->setCurrentIndex(mode->findText(QStringLiteral("With the chain")));
    auto* reopen = page->findChild<QCheckBox*>(QStringLiteral("reopenLastProject"));
    reopen->setChecked(false);
    auto* git = page->findChild<QLineEdit*>(QStringLiteral("gitPath"));
    git->setText(QStringLiteral("C:/Tools/git/bin/git.exe"));
    emit git->editingFinished();
    // 10–1800 seconds.
    timeout->setValue(5);
    QCOMPARE(timeout->value(), 10);
    timeout->setValue(300);
    theme->setCurrentIndex(theme->findText(QStringLiteral("Follow Windows")));
    theme->setCurrentIndex(theme->findText(QStringLiteral("Dark")));

    // Another reader of the same file, as the next start of Loom would be, sees them all.
    const loom::AppSettings again(pathsIn(*m_temp));
    QCOMPARE(again.theme(), loom::Theme::Dark);
    QCOMPARE(again.compileTimeoutSeconds(), 300);
    QCOMPARE(again.compileOnSave(), false);
    QCOMPARE(again.defaultCompileMode(), loom::BuildMode::Chain);
    QCOMPARE(again.reopenLastProject(), false);
    QCOMPARE(again.gitPath(), QStringLiteral("C:/Tools/git/bin/git.exe"));
    QVERIFY(QFileInfo::exists(m_temp->filePath(QStringLiteral("AppData/Loom/Loom.ini"))));

    // Never in the project: git sees nothing new or changed there (`.loom/` is ignored).
    const loom::GitRun status = loom::runGit(demo.git(), {QStringLiteral("status"), QStringLiteral("--porcelain")}, demo.root());
    QVERIFY(status.ok());
    QCOMPARE(status.output.trimmed(), QString());
    m_settings->setTheme(loom::Theme::FollowWindows);
    m_settings->applyAppearance();
}

void MainWindowTests::reopensTheLastProjectOnTheLastTab()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    {
        const std::unique_ptr<MainWindow> window = newWindow();
        QVERIFY(openAndWait(*window, demo.root()));
        window->showTab(Tab::Review);
        window->close();
    }
    QCOMPARE(m_settings->lastProject(), demo.root());
    QCOMPARE(m_settings->lastTab(), QStringLiteral("review"));

    const std::unique_ptr<MainWindow> window = newWindow();
    QSignalSpy finished(window.get(), &MainWindow::openFinished);
    window->restoreLastProject();
    QVERIFY(finished.wait(30'000));
    QVERIFY(window->session());
    QCOMPARE(window->session()->root(), demo.root());
    QCOMPARE(window->currentTab(), Tab::Review);
    QVERIFY(window->sideBar()->button(Tab::Review)->isChecked());
}

void MainWindowTests::startsOnTheStartScreenWhenItCant()
{
    // The setting off: the start screen, whatever was open last.
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    m_settings->setLastProject(demo.root());
    m_settings->setReopenLastProject(false);
    {
        const std::unique_ptr<MainWindow> window = newWindow();
        window->restoreLastProject();
        QVERIFY(window->projectLane()->waitUntilIdle());
        QVERIFY(window->onStartScreen());
        QVERIFY(!window->session());
    }
    // The last project gone: the start screen, quietly, with a line in the status bar.
    m_settings->setReopenLastProject(true);
    m_settings->setLastProject(m_temp->filePath(QStringLiteral("Gone")));
    const std::unique_ptr<MainWindow> window = newWindow();
    QSignalSpy finished(window.get(), &MainWindow::openFinished);
    window->restoreLastProject();
    QVERIFY(finished.wait(30'000));
    QVERIFY(window->onStartScreen());
    QVERIFY(!openMessage(*window));
    QVERIFY(window->statusBar()->currentMessage().startsWith(QStringLiteral("Loom couldn't reopen ")));
}

void MainWindowTests::watchingTheFolderUpdatesTheSnapshot()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openAndWait(*window, demo.root()));
    loom::ProjectSession* session = window->session();
    const std::size_t before = session->snapshot()->contents.statements.size();
    QVERIFY(session->watched().contains(demo.root() + QStringLiteral("/Statements")));
    QSignalSpy changed(session, &loom::ProjectSession::snapshotChanged);
    // Another program (an agent's `loom`, an editor) adds a statement.
    QVERIFY(writeFile(demo.root() + QStringLiteral("/Statements/M4CD.tex"),
                      "+++\nid = \"M4CD\"\nkind = \"remark\"\ntitle = \"A remark added outside Loom\"\ncreated = 2026-11-05\n+++\nText.\n"));
    QTRY_VERIFY_WITH_TIMEOUT(session->snapshot()->contents.statements.size() == before + 1, 15'000);
    QVERIFY(changed.count() >= 1);
}

void MainWindowTests::checkMySetupListsEveryTool()
{
    // Missing TeX tools, so the test build is skipped and this stays quick; the checks themselves
    // are LoomCore's (Tests/LoomCoreTests/SetupCheckTests.cpp).
    m_settings->setLatexmkPath(m_temp->filePath(QStringLiteral("none/latexmk")));
    m_settings->setSynctexPath(m_temp->filePath(QStringLiteral("none/synctex")));
    const std::unique_ptr<MainWindow> window = newWindow();
    window->checkSetup();
    auto* dialog = window->findChild<QDialog*>(QStringLiteral("setupCheckDialog"));
    QVERIFY(dialog);
    auto* view = dialog->findChild<loom::SetupCheckView*>();
    QSignalSpy finished(view, &loom::SetupCheckView::finished);
    QVERIFY(view->running() || finished.count() > 0);
    QVERIFY(finished.count() > 0 || finished.wait(60'000));
    QTreeWidget* results = view->results();
    QStringList keys;
    for (int i = 0; i < results->topLevelItemCount(); ++i)
        keys << results->topLevelItem(i)->data(0, Qt::UserRole).toString();
    QCOMPARE(keys, QStringList({"git", "git-identity", "git-lfs", "loom", "latexmk", "pdflatex", "synctex", "test-build"}));
    QCOMPARE(results->topLevelItem(4)->text(1),
             QStringLiteral("Install TeX Live (it includes latexmk and Perl). With MiKTeX, also install Perl."));
    QCOMPARE(results->topLevelItem(7)->text(1), QStringLiteral("Needs latexmk and pdfLaTeX."));
    screenshot(dialog, QStringLiteral("check-my-setup"));
    dialog->reject();
}

void MainWindowTests::projectStateFileIsOnePerFolder()
{
    // Specs/m2-statements-review.md, "What the app remembers": `<12 hex characters of the SHA-256
    // of the project's canonical path>.ini`, outside the project.
    const loom::AppPaths paths = pathsIn(*m_temp);
    const QString folder = m_temp->filePath(QStringLiteral("Some project"));
    QVERIFY(QDir().mkpath(folder + QStringLiteral("/Statements")));
    const QString file = loom::projectStateFile(paths, folder);
    QVERIFY(file.startsWith(paths.projectStateFolder + u'/'));
    QVERIFY(QRegularExpression(QStringLiteral("^[0-9a-f]{12}\\.ini$")).match(QFileInfo(file).fileName()).hasMatch());
    // The same folder, however it's spelt, has the same file; another folder has another.
    QCOMPARE(loom::projectStateFile(paths, folder + QStringLiteral("/Statements/..")), file);
#ifdef Q_OS_WIN
    QCOMPARE(loom::projectStateFile(paths, folder.toUpper()), file);
#endif
    QVERIFY(loom::projectStateFile(paths, m_temp->path()) != file);
}

void MainWindowTests::badges()
{
    loom::SideBar bar;
    loom::SideBarButton* review = bar.button(Tab::Review);
    QCOMPARE(review->badgeCount(), 0);
    review->setBadgeCount(3);
    QCOMPARE(review->badgeCount(), 3);
    bar.button(Tab::Statements)->setBadgeDot(true);
    QVERIFY(bar.button(Tab::Statements)->badgeDot());
    bar.resize(loom::SideBar::barWidth, 400);
    screenshot(&bar, QStringLiteral("side-bar-badges"));
    review->setBadgeCount(0);
    QCOMPARE(review->badgeCount(), 0);
}

QTEST_MAIN(MainWindowTests)
#include "MainWindowTests.moc"
