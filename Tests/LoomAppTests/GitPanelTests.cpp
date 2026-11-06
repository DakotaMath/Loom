// The git panel (Specs/m2-statements-review.md, "The git panel"; Verification, "Git panel":
// "Commit, refused commit with clickable problems, switch refused, push listing"). Each test drives
// the real widgets on a copy of the demo project (Tests/Support/DemoProject), whose commit guard
// is this build's `loom`. What each operation does and refuses is LoomCore's, tested there
// (GitOperationsTests); these check the wiring: the panel shows the state, runs git only on a
// click, asks before pushing, shows refusals, and sends problems and git's commands where the
// spec says.

#include "LoomApp/AppSettings.h"
#include "LoomApp/BottomPanel.h"
#include "LoomApp/FilePage.h"
#include "LoomApp/GitPanel.h"
#include "LoomApp/Lane.h"
#include "LoomApp/LatexEditor.h"
#include "LoomApp/MainWindow.h"
#include "LoomApp/StatementsPage.h"

#include "DemoProject.h"

#include <QAction>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

#include <memory>

using loom::GitPanel;
using loom::MainWindow;
using loom::OutputChannel;
using Button = loom::GitPanel::Button;

namespace {

/// The app's folders inside a temporary folder.
loom::AppPaths pathsIn(const QTemporaryDir& temp)
{
    return {temp.filePath(QStringLiteral("AppData/Loom/Loom.ini")), temp.filePath(QStringLiteral("LocalAppData/Loom/projects"))};
}

/// Saves a picture of a widget for people to look at (never compared pixel by pixel).
void screenshot(QWidget* widget, const QString& name)
{
    QDir().mkpath(QStringLiteral(LOOM_SCREENSHOTS));
    widget->grab().save(QStringLiteral(LOOM_SCREENSHOTS "/") + name + QStringLiteral(".png"));
}

/// Reads a file. @returns Its bytes, or empty if it can't be read.
QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/// Writes a file, creating its folder. @returns Whether it worked.
bool writeFile(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

/// Runs git in a folder. @returns Its stdout, trimmed, or `<failed>` if it failed.
QString git(const loomtest::DemoProject& demo, const QStringList& arguments, const QString& folder = {})
{
    const loom::GitRun run = loom::runGit(demo.git(), arguments, folder.isEmpty() ? demo.root() : folder);
    return run.ok() ? run.output.trimmed() : QStringLiteral("<failed: %1>").arg(run.errors.trimmed());
}

/// The paths the panel lists.
QStringList listed(const GitPanel* panel)
{
    QStringList paths;
    for (int i = 0; i < panel->files()->topLevelItemCount(); ++i)
        paths << panel->files()->topLevelItem(i)->text(0);
    return paths;
}

/// A dialog of the window's, by object name, or null.
QDialog* dialog(const QWidget& window, const QString& name)
{
    return window.findChild<QDialog*>(name);
}

/// Clicks a button of a dialog and waits for the dialog to go.
void answer(QWidget& window, const QString& name, const QString& button)
{
    QTRY_VERIFY(dialog(window, name));
    QPushButton* clicked = dialog(window, name)->findChild<QPushButton*>(button);
    QVERIFY2(clicked, qPrintable(button));
    QVERIFY(clicked->isEnabled());
    QTest::mouseClick(clicked, Qt::LeftButton);
    QTRY_VERIFY(!dialog(window, name));
}

/// Clicks a button of the panel and waits for its operation to finish.
bool click(GitPanel* panel, Button button)
{
    QSignalSpy finished(panel, &GitPanel::operationFinished);
    QPushButton* b = panel->button(button);
    if (!b->isVisible() || !b->isEnabled())
        return false;
    QTest::mouseClick(b, Qt::LeftButton);
    return finished.count() > 0 || finished.wait(60'000);
}

} // namespace

/// Qt Test cases for the git panel.
class GitPanelTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void opensFromEveryTabWithTheBranch();
    void commitsOnlyTheTickedFiles();
    void aRefusedCommitListsClickableProblems();
    void tickedFilesWithUnsavedEditsAskFirst();
    void switchingIsRefusedWithChanges();
    void runChecksListsProblems();
    void pushListsTheCommitsFirst();

private:
    /// A new window on the test's settings, shown, with the demo open and the git panel shown.
    std::unique_ptr<MainWindow> openDemo(const QString& root);

    std::unique_ptr<QTemporaryDir> m_temp;
    std::unique_ptr<loom::AppSettings> m_settings;
    QTemporaryDir m_gitHome;
};

void GitPanelTests::initTestCase()
{
    QVERIFY(m_gitHome.isValid());
    const QString config = m_gitHome.filePath(QStringLiteral("gitconfig"));
    QVERIFY(writeFile(config, "[user]\n\tname = Loom Test\n\temail = test@example.com\n"));
    qputenv("GIT_CONFIG_GLOBAL", QDir::toNativeSeparators(config).toLocal8Bit());
    qputenv("GIT_CONFIG_NOSYSTEM", "1");
}

void GitPanelTests::init()
{
    m_temp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_temp->isValid());
    m_settings = std::make_unique<loom::AppSettings>(pathsIn(*m_temp));
    m_settings->setCompileOnSave(false);
}

void GitPanelTests::cleanup()
{
    m_settings.reset();
    m_temp.reset();
}

std::unique_ptr<MainWindow> GitPanelTests::openDemo(const QString& root)
{
    auto window = std::make_unique<MainWindow>(m_settings.get());
    window->resize(1400, 900);
    window->show();
    QSignalSpy offered(window->statementsPage(), &loom::StatementsPage::recoveryOffered);
    QSignalSpy finished(window.get(), &MainWindow::openFinished);
    window->openFolder(root);
    if ((finished.count() == 0 && !finished.wait(30'000)) || (offered.count() == 0 && !offered.wait(30'000)))
        return nullptr;
    QSignalSpy shown(window->gitPanel(), &GitPanel::stateShown);
    window->toggleGitPanel();
    if (!window->gitPanel()->isVisible() || (shown.count() == 0 && !shown.wait(30'000)))
        return nullptr;
    return window;
}

void GitPanelTests::opensFromEveryTabWithTheBranch()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    auto window = std::make_unique<MainWindow>(m_settings.get());
    window->show();
    QSignalSpy finished(window.get(), &MainWindow::openFinished);
    window->openFolder(demo.root());
    QVERIFY(finished.wait(30'000));
    GitPanel* panel = window->gitPanel();
    QVERIFY(!panel->isVisible());

    // The branch in the status bar, at all times; a click opens the panel.
    auto* branch = window->findChild<QLabel*>(QStringLiteral("branch"));
    QTRY_COMPARE(branch->text(), QStringLiteral("main"));
    QSignalSpy shown(panel, &GitPanel::stateShown);
    QTest::mouseClick(branch, Qt::LeftButton);
    QVERIFY(panel->isVisible());
    QVERIFY(shown.wait(30'000));
    QCOMPARE(panel->header()->text(), QStringLiteral("main"));
    QCOMPARE(panel->files()->topLevelItemCount(), 0);
    // No remote: Fetch, Pull and Push are hidden.
    QVERIFY(!panel->button(Button::Fetch)->isVisible());
    QVERIFY(!panel->button(Button::Pull)->isVisible());
    QVERIFY(!panel->button(Button::Push)->isVisible());
    QVERIFY(panel->button(Button::Commit)->isVisible());
    QVERIFY(panel->button(Button::RunChecks)->isVisible());

    // It stays open on every tab; Ctrl+Shift+G (the View menu's action) closes and opens it.
    window->showTab(loom::Tab::Settings);
    QVERIFY(panel->isVisible());
    window->showTab(loom::Tab::Review);
    QVERIFY(panel->isVisible());
    QAction* action = window->findChild<QAction*>(QStringLiteral("gitPanelAction"));
    QVERIFY(action);
    QCOMPARE(action->shortcut(), QKeySequence(QStringLiteral("Ctrl+Shift+G")));
    action->trigger();
    QVERIFY(!panel->isVisible());
    action->trigger();
    QVERIFY(panel->isVisible());

    // Reading the state is logged in Output's Git channel, command by command.
    QTRY_VERIFY(window->bottomPanel()->output(OutputChannel::Git)->toPlainText().contains(
        QStringLiteral("git status --porcelain=v2 -z --branch --untracked-files=all")));
    QVERIFY(window->bottomPanel()->output(OutputChannel::Git)->toPlainText().contains(QStringLiteral("Exit code 0")));

    // A detached HEAD says so, in the panel and the status bar.
    QCOMPARE(git(demo, {"switch", "-q", "--detach"}), QString());
    window->session()->refresh();
    QTRY_VERIFY(panel->header()->text().startsWith(QStringLiteral("Not on a branch (")));
    QTRY_COMPARE(branch->text(), QStringLiteral("Not on a branch"));
}

void GitPanelTests::commitsOnlyTheTickedFiles()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    QVERIFY(writeFile(demo.root() + "/Statements/A2DF.tex", readFile(demo.root() + "/Statements/A2DF.tex") + "A line added.\n"));
    QVERIFY(writeFile(demo.root() + "/Statements/B3GH.tex", readFile(demo.root() + "/Statements/B3GH.tex") + "Left for later.\n"));
    QVERIFY(writeFile(demo.root() + "/Notes/idea.md", "An idea.\n"));
    const std::unique_ptr<MainWindow> window = openDemo(demo.root());
    QVERIFY(window);
    GitPanel* panel = window->gitPanel();
    QTRY_COMPARE(listed(panel), (QStringList{"Notes/idea.md", "Statements/A2DF.tex", "Statements/B3GH.tex"}));
    // All ticked by default; the kinds as Review marks them.
    for (int i = 0; i < 3; ++i)
        QCOMPARE(panel->files()->topLevelItem(i)->checkState(0), Qt::Checked);
    QCOMPARE(panel->files()->topLevelItem(0)->text(1), QStringLiteral("new"));
    QCOMPARE(panel->files()->topLevelItem(1)->text(1), QStringLiteral("changed"));

    // A click shows the diff.
    panel->files()->setCurrentItem(panel->files()->topLevelItem(1));
    QTRY_VERIFY(panel->diff()->toPlainText().contains(QStringLiteral("+A line added.")));

    // Nothing runs git until Commit is clicked; an empty message is refused.
    const QString before = git(demo, {"rev-parse", "HEAD"});
    QVERIFY(click(panel, Button::Commit));
    QCOMPARE(panel->result()->text(), QStringLiteral("Write a commit message first."));
    QCOMPARE(git(demo, {"rev-parse", "HEAD"}), before);

    // A long first line warns, without stopping the commit.
    panel->message()->setPlainText(QString(80, u'x'));
    QVERIFY(panel->messageWarning()->isVisible());
    QVERIFY(panel->messageWarning()->text().contains(QStringLiteral("80 characters")));
    panel->message()->setPlainText(QStringLiteral("Add to A2DF and an idea"));
    QVERIFY(!panel->messageWarning()->isVisible());

    QVERIFY(panel->setTicked(QStringLiteral("Statements/B3GH.tex"), false));
    screenshot(window.get(), QStringLiteral("git-panel"));
    QVERIFY(click(panel, Button::Commit));
    QVERIFY2(panel->result()->text().startsWith(QStringLiteral("Committed 2 files to main as ")), qPrintable(panel->result()->text()));
    QCOMPARE(git(demo, {"log", "-1", "--format=%s"}), QStringLiteral("Add to A2DF and an idea"));
    QCOMPARE(git(demo, {"show", "--name-only", "--format=", "HEAD"}), QStringLiteral("Notes/idea.md\nStatements/A2DF.tex"));
    QVERIFY(panel->message()->toPlainText().isEmpty());
    QTRY_COMPARE(listed(panel), QStringList{"Statements/B3GH.tex"});
    // B3GH stays unticked until it's committed or ticked again.
    QCOMPARE(panel->files()->topLevelItem(0)->checkState(0), Qt::Unchecked);

    const QString output = window->bottomPanel()->output(OutputChannel::Git)->toPlainText();
    QVERIFY(output.contains(QStringLiteral("git add -- Notes/idea.md")));
    QVERIFY(output.contains(QStringLiteral("git commit -m \"Add to A2DF and an idea\" -- Statements/A2DF.tex Notes/idea.md"))
            || output.contains(QStringLiteral("git commit -m \"Add to A2DF and an idea\" -- Notes/idea.md Statements/A2DF.tex")));
}

void GitPanelTests::aRefusedCommitListsClickableProblems()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const QString file = demo.root() + "/Statements/A2DF.tex";
    QVERIFY(writeFile(file, readFile(file).replace("kind = \"definition\"", "kind = \"bogus\"")));
    const std::unique_ptr<MainWindow> window = openDemo(demo.root());
    QVERIFY(window);
    GitPanel* panel = window->gitPanel();
    QTRY_COMPARE(listed(panel), QStringList{"Statements/A2DF.tex"});
    const QString before = git(demo, {"rev-parse", "HEAD"});

    panel->message()->setPlainText(QStringLiteral("Break A2DF"));
    QVERIFY(click(panel, Button::Commit));
    QCOMPARE(git(demo, {"rev-parse", "HEAD"}), before);
    QVERIFY2(panel->result()->text().startsWith(QStringLiteral("Loom's commit guard refused the commit")), qPrintable(panel->result()->text()));
    // Git's output in the panel; the message kept.
    QVERIFY(panel->gitOutput()->isVisible());
    QVERIFY(panel->gitOutput()->toPlainText().contains(QStringLiteral("Statements/A2DF.tex:3: error:")));
    QCOMPARE(panel->message()->toPlainText(), QStringLiteral("Break A2DF"));

    // Each problem line in the Problems panel, which opens its file at its line.
    loom::BottomPanel* bottom = window->bottomPanel();
    QVERIFY(bottom->isVisible());
    // The re-read after the commit rebuilds the list; the row is found once it's done.
    QVERIFY(window->projectLane()->waitUntilIdle());
    QVERIFY(window->gitLane()->waitUntilIdle());
    QTreeWidgetItem* refused = nullptr;
    for (int i = 0; i < bottom->problems()->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = bottom->problems()->topLevelItem(i);
        if (item->text(loom::BottomPanel::MessageColumn).startsWith(QStringLiteral("Not committed: ")))
            refused = item;
    }
    QVERIFY(refused);
    QCOMPARE(refused->data(loom::BottomPanel::FileColumn, Qt::UserRole).toString(), QStringLiteral("Statements/A2DF.tex"));
    screenshot(window.get(), QStringLiteral("git-panel-refused"));
    window->showTab(loom::Tab::Review);
    bottom->problems()->scrollToItem(refused);
    QCoreApplication::processEvents();
    const QRect rect = bottom->problems()->visualItemRect(refused);
    QVERIFY(!rect.isEmpty());
    QTest::mouseClick(bottom->problems()->viewport(), Qt::LeftButton, {}, rect.center());
    QCOMPARE(window->currentTab(), loom::Tab::Statements);
    QCOMPARE(window->statementsPage()->openTag(), QStringLiteral("A2DF"));

    // Fixed, the commit goes in and the problems go.
    QVERIFY(writeFile(file, readFile(file).replace("kind = \"bogus\"", "kind = \"definition\"") + "Fixed.\n"));
    window->session()->refresh();
    QTRY_COMPARE(window->statementsPage()->snapshot()->errors, 0);
    QVERIFY(click(panel, Button::Commit));
    QVERIFY2(panel->result()->text().startsWith(QStringLiteral("Committed 1 file")), qPrintable(panel->result()->text()));
    for (int i = 0; i < bottom->problems()->topLevelItemCount(); ++i)
        QVERIFY(!bottom->problems()->topLevelItem(i)->text(loom::BottomPanel::MessageColumn).startsWith(QStringLiteral("Not committed: ")));
}

void GitPanelTests::tickedFilesWithUnsavedEditsAskFirst()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const QString file = demo.root() + "/Statements/A2DF.tex";
    QVERIFY(writeFile(file, readFile(file) + "Saved before.\n"));
    const std::unique_ptr<MainWindow> window = openDemo(demo.root());
    QVERIFY(window);
    GitPanel* panel = window->gitPanel();
    QTRY_COMPARE(listed(panel), QStringList{"Statements/A2DF.tex"});
    loom::StatementsPage* page = window->statementsPage();
    page->openStatement(QStringLiteral("A2DF"));
    loom::FilePage* shown = page->currentPage();
    QVERIFY(shown);
    shown->editor()->setText(shown->editor()->text() + QStringLiteral("Typed, not saved.\n"));
    QTRY_VERIFY(page->hasUnsavedEdits());
    panel->message()->setPlainText(QStringLiteral("A2DF"));
    const QString before = git(demo, {"rev-parse", "HEAD"});

    // Cancel commits nothing.
    QTest::mouseClick(panel->button(Button::Commit), Qt::LeftButton);
    auto* box = window->findChild<QMessageBox*>(QStringLiteral("unsavedCommitMessage"));
    QVERIFY(box);
    QVERIFY(box->text().contains(QStringLiteral("Statements/A2DF.tex")));
    QCOMPARE(box->defaultButton()->objectName(), QStringLiteral("cancel"));
    answer(*window, QStringLiteral("unsavedCommitMessage"), QStringLiteral("cancel"));
    QCOMPARE(git(demo, {"rev-parse", "HEAD"}), before);

    // Save and commit: the editor's text is saved, then committed.
    QSignalSpy finished(panel, &GitPanel::operationFinished);
    QTest::mouseClick(panel->button(Button::Commit), Qt::LeftButton);
    answer(*window, QStringLiteral("unsavedCommitMessage"), QStringLiteral("saveAndCommit"));
    QVERIFY(finished.wait(60'000));
    QVERIFY2(panel->result()->text().startsWith(QStringLiteral("Committed 1 file")), qPrintable(panel->result()->text()));
    QVERIFY(!page->hasUnsavedEdits());
    QVERIFY(git(demo, {"show", "HEAD:Statements/A2DF.tex"}).contains(QStringLiteral("Typed, not saved.")));

    // Unsaved edits refuse switching and pulling, before any git runs.
    shown->editor()->setText(shown->editor()->text() + QStringLiteral("More.\n"));
    QTRY_VERIFY(page->hasUnsavedEdits());
    QTest::mouseClick(panel->button(Button::SwitchBranch), Qt::LeftButton);
    QCOMPARE(panel->result()->text(), QStringLiteral("Save or discard your unsaved edits first."));
    QVERIFY(!dialog(*window, QStringLiteral("switchBranchDialog")));
    panel->result()->clear();
    QTest::mouseClick(panel->button(Button::Pull), Qt::LeftButton);
    QVERIFY(!panel->button(Button::Pull)->isVisible() || panel->result()->text() == u"Save or discard your unsaved edits first.");
}

void GitPanelTests::switchingIsRefusedWithChanges()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    QCOMPARE(git(demo, {"branch", "other"}), QString());
    const QString file = demo.root() + "/Statements/A2DF.tex";
    QVERIFY(writeFile(file, readFile(file) + "Uncommitted.\n"));
    const std::unique_ptr<MainWindow> window = openDemo(demo.root());
    QVERIFY(window);
    GitPanel* panel = window->gitPanel();
    QTRY_COMPARE(listed(panel), QStringList{"Statements/A2DF.tex"});

    QTest::mouseClick(panel->button(Button::SwitchBranch), Qt::LeftButton);
    QTRY_VERIFY(dialog(*window, QStringLiteral("switchBranchDialog")));
    auto* branches = dialog(*window, QStringLiteral("switchBranchDialog"))->findChild<QListWidget*>(QStringLiteral("branches"));
    QCOMPARE(branches->count(), 1);
    QCOMPARE(branches->item(0)->text(), QStringLiteral("other"));
    branches->setCurrentRow(0);
    QSignalSpy finished(panel, &GitPanel::operationFinished);
    answer(*window, QStringLiteral("switchBranchDialog"), QStringLiteral("switch"));
    QVERIFY(finished.wait(60'000));
    QCOMPARE(panel->result()->text(), QStringLiteral("Commit your changes first, or they would come along to other."));
    QCOMPARE(git(demo, {"branch", "--show-current"}), QStringLiteral("main"));

    // Committed, the switch goes ahead, and the status bar follows.
    panel->message()->setPlainText(QStringLiteral("A2DF"));
    QVERIFY(click(panel, Button::Commit));
    QTest::mouseClick(panel->button(Button::SwitchBranch), Qt::LeftButton);
    QTRY_VERIFY(dialog(*window, QStringLiteral("switchBranchDialog")));
    dialog(*window, QStringLiteral("switchBranchDialog"))->findChild<QListWidget*>(QStringLiteral("branches"))->setCurrentRow(0);
    answer(*window, QStringLiteral("switchBranchDialog"), QStringLiteral("switch"));
    QTRY_COMPARE(panel->result()->text(), QStringLiteral("Switched to other."));
    QCOMPARE(git(demo, {"branch", "--show-current"}), QStringLiteral("other"));
    QTRY_COMPARE(window->findChild<QLabel*>(QStringLiteral("branch"))->text(), QStringLiteral("other"));
    QTRY_COMPARE(panel->header()->text(), QStringLiteral("other"));

    // New branch…: a name git rejects is refused; a good one is made and checked out.
    QTest::mouseClick(panel->button(Button::NewBranch), Qt::LeftButton);
    QTRY_VERIFY(dialog(*window, QStringLiteral("newBranchDialog")));
    dialog(*window, QStringLiteral("newBranchDialog"))->findChild<QLineEdit*>(QStringLiteral("branchName"))->setText(QStringLiteral("a..b"));
    answer(*window, QStringLiteral("newBranchDialog"), QStringLiteral("make"));
    QTRY_COMPARE(panel->result()->text(), QStringLiteral("`a..b` can't be a branch name."));
    QTest::mouseClick(panel->button(Button::NewBranch), Qt::LeftButton);
    QTRY_VERIFY(dialog(*window, QStringLiteral("newBranchDialog")));
    dialog(*window, QStringLiteral("newBranchDialog"))->findChild<QLineEdit*>(QStringLiteral("branchName"))->setText(QStringLiteral("tails"));
    answer(*window, QStringLiteral("newBranchDialog"), QStringLiteral("make"));
    QTRY_COMPARE(panel->result()->text(), QStringLiteral("Made the branch tails and switched to it."));
    QCOMPARE(git(demo, {"branch", "--show-current"}), QStringLiteral("tails"));
}

void GitPanelTests::runChecksListsProblems()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = openDemo(demo.root());
    QVERIFY(window);
    GitPanel* panel = window->gitPanel();
    QVERIFY(click(panel, Button::RunChecks));
    QVERIFY2(panel->result()->text().startsWith(QStringLiteral("Checked: ")), qPrintable(panel->result()->text()));

    const QString file = demo.root() + "/Statements/A2DF.tex";
    QVERIFY(writeFile(file, readFile(file).replace("kind = \"definition\"", "kind = \"bogus\"")));
    QVERIFY(click(panel, Button::RunChecks));
    QVERIFY2(panel->result()->text().contains(QStringLiteral(" error")), qPrintable(panel->result()->text()));
    loom::BottomPanel* bottom = window->bottomPanel();
    QVERIFY(bottom->isVisible());
    bool found = false;
    for (int i = 0; i < bottom->problems()->topLevelItemCount(); ++i)
        found = found || bottom->problems()->topLevelItem(i)->data(loom::BottomPanel::FileColumn, Qt::UserRole).toString() == u"Statements/A2DF.tex";
    QVERIFY(found);
}

void GitPanelTests::pushListsTheCommitsFirst()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    QTemporaryDir remoteDir;
    QVERIFY(remoteDir.isValid());
    const QString remote = remoteDir.path() + "/remote.git";
    QVERIFY(QDir().mkpath(remote));
    QCOMPARE(git(demo, {"init", "-q", "--bare", "-b", "main"}, remote), QString());
    QCOMPARE(git(demo, {"remote", "add", "origin", remote}), QString());
    const std::unique_ptr<MainWindow> window = openDemo(demo.root());
    QVERIFY(window);
    GitPanel* panel = window->gitPanel();
    QVERIFY(panel->button(Button::Fetch)->isVisible());
    QVERIFY(panel->button(Button::Pull)->isVisible());
    QVERIFY(panel->button(Button::Push)->isVisible());

    // The commits are listed first; Cancel pushes nothing.
    QVERIFY(click(panel, Button::Push));
    QTRY_VERIFY(dialog(*window, QStringLiteral("pushDialog")));
    QDialog* push = dialog(*window, QStringLiteral("pushDialog"));
    auto* commits = push->findChild<QListWidget*>(QStringLiteral("commits"));
    QCOMPARE(commits->count(), 1);
    QCOMPARE(commits->item(0)->text(), git(demo, {"log", "--oneline", "-1"}));
    QCOMPARE(push->findChild<QPushButton*>(QStringLiteral("push"))->text(), QStringLiteral("Push 1 commit"));
    QVERIFY(push->findChild<QPushButton*>(QStringLiteral("cancel"))->hasFocus());
    screenshot(push, QStringLiteral("git-panel-push"));
    answer(*window, QStringLiteral("pushDialog"), QStringLiteral("cancel"));
    QVERIFY(git(demo, {"rev-parse", "--verify", "-q", "main"}, remote).startsWith(QStringLiteral("<failed")));

    QVERIFY(click(panel, Button::Push));
    QSignalSpy finished(panel, &GitPanel::operationFinished);
    answer(*window, QStringLiteral("pushDialog"), QStringLiteral("push"));
    QVERIFY(finished.wait(60'000));
    QCOMPARE(panel->result()->text(), QStringLiteral("Pushed 1 commit to origin/main."));
    QCOMPARE(git(demo, {"rev-parse", "main"}, remote), git(demo, {"rev-parse", "HEAD"}));
    QTRY_VERIFY(panel->header()->text().contains(QStringLiteral("Up to date with origin/main")));

    // Nothing more to push: said, with no dialog.
    QVERIFY(click(panel, Button::Push));
    QCOMPARE(panel->result()->text(), QStringLiteral("Nothing to push: origin/main has every commit of main."));
    QVERIFY(!dialog(*window, QStringLiteral("pushDialog")));

    // Fetch runs `git fetch --prune`.
    QVERIFY(click(panel, Button::Fetch));
    QCOMPARE(panel->result()->text(), QStringLiteral("Fetched."));
    QVERIFY(window->bottomPanel()->output(OutputChannel::Git)->toPlainText().contains(QStringLiteral("git fetch --prune")));
    QVERIFY(click(panel, Button::Pull));
    QCOMPARE(panel->result()->text(), QStringLiteral("Pulled origin/main into main."));
}

QTEST_MAIN(GitPanelTests)
#include "GitPanelTests.moc"
