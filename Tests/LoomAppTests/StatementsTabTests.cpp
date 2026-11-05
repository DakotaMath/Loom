// The Statements tab (Specs/m2-statements-review.md, "The Statements tab" and "Never losing text";
// Verification, "The Statements tab": "Open, edit, save, refusals keep text, outdate-checks dialog
// confirmed and cancelled, new statement and proof, verbose, back and forward, find", and "Never
// losing text": "A file changed on disk with and without edits; recovery after the app is killed").
// Each test drives the real widgets on a copy of the demo project (Tests/Support/DemoProject), with
// the app's settings in a temporary folder. The rules themselves (what a save refuses, what a
// recovery file holds) are LoomCore's and tested there; these check the wiring and behaviour.

#include "LoomApp/AppSettings.h"
#include "LoomApp/BottomPanel.h"
#include "LoomApp/EditBuffer.h"
#include "LoomApp/FilePage.h"
#include "LoomApp/FindBar.h"
#include "LoomApp/HeaderForms.h"
#include "LoomApp/Lane.h"
#include "LoomApp/LatexEditor.h"
#include "LoomApp/MainWindow.h"
#include "LoomApp/NewItemDialogs.h"
#include "LoomApp/ProjectSession.h"
#include "LoomApp/SideBar.h"
#include "LoomApp/StatementList.h"
#include "LoomApp/StatementsPage.h"
#include "LoomApp/StatusMarks.h"
#include "LoomApp/TextDiff.h"

#include "LoomCore/Drafts.h"
#include "LoomCore/EditorSaves.h"

#include "DemoProject.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>

#include <memory>

using loom::EditBuffer;
using loom::FilePage;
using loom::MainWindow;
using loom::StatementsPage;
using loom::Tab;

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

/// A message box of the window's, by object name, or null.
QMessageBox* box(const QWidget& window, const QString& name)
{
    return window.findChild<QMessageBox*>(name);
}

/// Clicks a button of a message box and waits for the box to go.
void answer(QWidget& window, const QString& boxName, const QString& button)
{
    QTRY_VERIFY(box(window, boxName));
    QMessageBox* message = box(window, boxName);
    QPushButton* clicked = message->findChild<QPushButton*>(button);
    QVERIFY2(clicked, qPrintable(button));
    QTest::mouseClick(clicked, Qt::LeftButton);
    QTRY_VERIFY(!box(window, boxName));
}

/// The row of an ID in the list, or null.
QTreeWidgetItem* row(StatementsPage* page, const QString& id)
{
    for (QTreeWidgetItemIterator it(page->list()->tree()); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == id)
            return *it;
    }
    return nullptr;
}

/// Clicks the row of an ID in the list.
void clickRow(StatementsPage* page, const QString& id)
{
    QTreeWidgetItem* item = row(page, id);
    QVERIFY2(item, qPrintable(id));
    QTreeWidget* tree = page->list()->tree();
    tree->scrollToItem(item);
    QTest::mouseClick(tree->viewport(), Qt::LeftButton, {}, tree->visualItemRect(item).center());
}

} // namespace

/// Qt Test cases for the Statements tab.
class StatementsTabTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void statusColoursHaveContrast();
    void editBufferKeepsFormAndRawInStep();
    void searchMatchesWordStarts();
    void diffMarksLines();

    void opensAStatementFromTheList();
    void listSearchesFiltersAndSorts();
    void editsAndSaves();
    void aRefusedSaveKeepsTheText();
    void outdatingChecksAsksFirst();
    void markingAProofFailedAsksFirst();
    void newStatementAndNewProof();
    void verboseVersions();
    void backAndForward();
    void findAndReplace();
    void editsSurviveLeavingAndSwitchingTabs();
    void aFileChangedOnDiskReloadsOrAsks();
    void recoveryAfterACrash();
    void closingWithUnsavedEditsAsks();
    void aBrokenFileOpensInRawMode();
    void aProblemClickOpensTheLine();

private:
    /// A new window on the test's settings, shown.
    std::unique_ptr<MainWindow> newWindow();
    /// Opens the demo in a window and waits until the tab has offered its recovery files.
    bool openDemo(MainWindow& window, const QString& root);

    std::unique_ptr<QTemporaryDir> m_temp;
    std::unique_ptr<loom::AppSettings> m_settings;
    QTemporaryDir m_gitHome;
};

void StatementsTabTests::initTestCase()
{
    QVERIFY(m_gitHome.isValid());
    const QString config = m_gitHome.filePath(QStringLiteral("gitconfig"));
    QVERIFY(writeFile(config, "[user]\n\tname = Loom Test\n\temail = test@example.com\n"));
    qputenv("GIT_CONFIG_GLOBAL", QDir::toNativeSeparators(config).toLocal8Bit());
    qputenv("GIT_CONFIG_NOSYSTEM", "1");
}

void StatementsTabTests::init()
{
    m_temp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_temp->isValid());
    m_settings = std::make_unique<loom::AppSettings>(pathsIn(*m_temp));
}

void StatementsTabTests::cleanup()
{
    m_settings.reset();
    m_temp.reset();
}

std::unique_ptr<MainWindow> StatementsTabTests::newWindow()
{
    auto window = std::make_unique<MainWindow>(m_settings.get());
    window->resize(1300, 800);
    window->show();
    return window;
}

bool StatementsTabTests::openDemo(MainWindow& window, const QString& root)
{
    QSignalSpy offered(window.statementsPage(), &StatementsPage::recoveryOffered);
    QSignalSpy finished(&window, &MainWindow::openFinished);
    window.openFolder(root);
    if (finished.count() == 0 && !finished.wait(30'000))
        return false;
    return offered.count() > 0 || offered.wait(30'000);
}

void StatementsTabTests::statusColoursHaveContrast()
{
    // "Each colour has a light-theme and a dark-theme value with at least 3:1 contrast against the
    // list's background" (Specs/m2-statements-review.md, "The list").
    for (const loom::Status status : loom::statusOrder()) {
        if (status == loom::Status::None) {
            QVERIFY(!loom::statusColor(status, false).isValid());
            QVERIFY(loom::statusIcon(status, false).isNull());
            continue;
        }
        QVERIFY2(loom::contrastRatio(loom::statusColor(status, false), QColor(Qt::white)) >= 3.0, qPrintable(loom::statusName(status)));
        QVERIFY2(loom::contrastRatio(loom::statusColor(status, false), QColor(0xf3f3f3)) >= 3.0, qPrintable(loom::statusName(status)));
        QVERIFY2(loom::contrastRatio(loom::statusColor(status, true), QColor(0x1e1e1e)) >= 3.0, qPrintable(loom::statusName(status)));
        QVERIFY2(loom::contrastRatio(loom::statusColor(status, true), QColor(0x2d2d2d)) >= 3.0, qPrintable(loom::statusName(status)));
        QVERIFY(!loom::statusIcon(status, false).isNull());
    }
    QVERIFY(loom::statusHollow(loom::Status::Open));
    QVERIFY(loom::statusHollow(loom::Status::Extracted));
    QVERIFY(!loom::statusHollow(loom::Status::Proved));
    QCOMPARE(loom::kindShort(QStringLiteral("theorem")), QStringLiteral("thm"));
    QCOMPARE(loom::kindShort(QStringLiteral("question")), QStringLiteral("q"));
    QCOMPARE(loom::kindShort(QStringLiteral("example")), QStringLiteral("ex"));
}

void StatementsTabTests::editBufferKeepsFormAndRawInStep()
{
    const QByteArray file = "+++\nid = \"4KX7\"\nkind = \"theorem\"\ntitle = \"T\" # a comment\ncreated = 2026-11-03\n+++\nBody.\n";
    EditBuffer buffer(QStringLiteral("Statements/4KX7.tex"), loom::FileKind::Statement, QStringLiteral("4KX7"));
    buffer.load(file, true);
    QVERIFY(!buffer.raw());
    QVERIFY(!buffer.dirty());
    QCOMPARE(buffer.editorText(), QStringLiteral("Body.\n"));
    QCOMPARE(buffer.base(), loom::fileBase(file));

    // Unedited, raw shows the file exactly, its comment included.
    QVERIFY(buffer.setRaw(true));
    QCOMPARE(buffer.editorText(), QString::fromUtf8(file));
    QVERIFY(!buffer.dirty());
    // An edit in raw text carries back to the form.
    buffer.setEditorText(QString::fromUtf8(file).replace(QStringLiteral("title = \"T\""), QStringLiteral("title = \"Energy\"")));
    QVERIFY(buffer.dirty());
    QVERIFY(buffer.setRaw(false));
    QCOMPARE(buffer.statement().title, QStringLiteral("Energy"));
    QVERIFY(buffer.dirty());
    // Front matter that doesn't parse can't go to the form.
    QVERIFY(buffer.setRaw(true));
    buffer.setEditorText(QStringLiteral("+++\nid = \"4KX7\nkind = \"theorem\"\n+++\nBody.\n"));
    QString why;
    QVERIFY(!buffer.setRaw(false, &why));
    QVERIFY(why.contains(QStringLiteral("can't read")));
    QVERIFY(buffer.raw());

    // A file that doesn't parse loads raw.
    EditBuffer broken(QStringLiteral("Statements/5HN8.tex"), loom::FileKind::Statement, QStringLiteral("5HN8"));
    broken.load("+++\nid = \"5HN8\nkind = \"lemma\"\n+++\nBody.\n", true);
    QVERIFY(broken.raw());
    QVERIFY(!broken.dirty());

    // Keep mine: the edits stay, the base moves.
    buffer.load(file, true);
    buffer.setEditorText(QStringLiteral("Mine.\n"));
    const QByteArray theirs = QByteArray(file).replace("Body.", "Theirs.");
    buffer.rebase(theirs, true);
    QCOMPARE(buffer.base(), loom::fileBase(theirs));
    QCOMPARE(buffer.editorText(), QStringLiteral("Mine.\n"));
    QVERIFY(buffer.dirty());
}

void StatementsTabTests::searchMatchesWordStarts()
{
    const QStringList words = {QStringLiteral("4kx7"), QStringLiteral("energie"), QStringLiteral("decay"), QStringLiteral("model")};
    QVERIFY(loom::matchesQuery(QString(), words));
    QVERIFY(loom::matchesQuery(QStringLiteral("4K"), words));
    QVERIFY(loom::matchesQuery(QStringLiteral("dec mod"), words));
    QVERIFY(loom::matchesQuery(QStringLiteral("Énergie"), words)); // Accents and case ignored.
    QVERIFY(!loom::matchesQuery(QStringLiteral("ecay"), words));   // Starts of words only.
    QVERIFY(!loom::matchesQuery(QStringLiteral("decay tail"), words)); // Every word must match.
    QCOMPARE(loom::foldForSearch(QStringLiteral("Hölder")), QStringLiteral("holder"));
}

void StatementsTabTests::diffMarksLines()
{
    const std::vector<loom::DiffLine> lines = loom::diffLines(QStringLiteral("a\nb\nc"), QStringLiteral("a\nB\nc\nd"));
    QCOMPARE(lines.size(), std::size_t(5));
    QCOMPARE(lines[0].kind, loom::DiffLine::Kind::Same);
    QCOMPARE(lines[1].kind, loom::DiffLine::Kind::Removed);
    QCOMPARE(lines[1].text, QStringLiteral("b"));
    QCOMPARE(lines[2].kind, loom::DiffLine::Kind::Added);
    QCOMPARE(lines[2].text, QStringLiteral("B"));
    QCOMPARE(lines[4].kind, loom::DiffLine::Kind::Added);
}

void StatementsTabTests::opensAStatementFromTheList()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    QCOMPARE(window->page(Tab::Statements), page);

    // The list: every statement, its proofs as child rows, the kind in small letters.
    QTreeWidgetItem* c4jk = row(page, QStringLiteral("C4JK"));
    QVERIFY(c4jk);
    QCOMPARE(c4jk->text(0), QStringLiteral("C4JK · Tails of tame sequences vanish"));
    QCOMPARE(c4jk->text(1), QStringLiteral("lem"));
    QVERIFY(c4jk->toolTip(0).contains(QStringLiteral("proved")));
    QVERIFY(!c4jk->icon(0).isNull());
    QVERIFY(row(page, QStringLiteral("C4JK.a")));
    QCOMPARE(row(page, QStringLiteral("C4JK.a"))->text(0), QStringLiteral("C4JK.a · complete"));
    QCOMPARE(row(page, QStringLiteral("A2DF"))->text(1), QStringLiteral("def"));
    QVERIFY(row(page, QStringLiteral("A2DF"))->toolTip(0).endsWith(QStringLiteral("no status"))); // No disc.
    QVERIFY(!row(page, QStringLiteral("7RT2"))); // Paper results are off by default.

    clickRow(page, QStringLiteral("C4JK"));
    QCOMPARE(page->openTag(), QStringLiteral("C4JK"));
    auto* tabs = page->findChild<QTabWidget*>(QStringLiteral("pages"));
    QCOMPARE(tabs->count(), 2);
    QCOMPARE(tabs->tabText(0), QStringLiteral("Statement"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("a"));
    FilePage* statement = page->currentPage();
    QVERIFY(statement);
    QCOMPARE(statement->findChild<QLabel*>(QStringLiteral("pageHeading"))->text(),
             QStringLiteral("C4JK · Lemma · Tails of tame sequences vanish"));
    QVERIFY(statement->findChild<QLabel*>(QStringLiteral("statusLine"))->text().contains(QStringLiteral("proved")));
    QVERIFY(statement->editor()->text().startsWith(QStringLiteral("If $a$ is tame")));
    QCOMPARE(statement->statementForm()->title()->text(), QStringLiteral("Tails of tame sequences vanish"));
    QCOMPARE(statement->statementForm()->kind()->currentData().toString(), QStringLiteral("lemma"));
    QCOMPARE(statement->statementForm()->tags()->items(), (QStringList{QStringLiteral("tame-sequences"), QStringLiteral("tail-bounds")}));
    QCOMPARE(statement->statementForm()->statedUsing()->items(), (QStringList{QStringLiteral("A2DF"), QStringLiteral("B3GH")}));
    screenshot(window.get(), QStringLiteral("statements-tab"));

    // A proof row opens its page.
    clickRow(page, QStringLiteral("D5MN.a"));
    QCOMPARE(page->openTag(), QStringLiteral("D5MN"));
    FilePage* proof = page->currentPage();
    QCOMPARE(proof->proofId(), QStringLiteral("D5MN.a"));
    QCOMPARE(proof->findChild<QLabel*>(QStringLiteral("pageHeading"))->text(), QStringLiteral("D5MN.a · proof of D5MN"));
    QCOMPARE(proof->proofForm()->level()->currentData().toString(), QStringLiteral("complete"));
    QCOMPARE(proof->proofForm()->premises()->items(), QStringList{QStringLiteral("C4JK")});
    QVERIFY(proof->editor()->text().startsWith(QStringLiteral("By \\ref{C4JK}")));
    screenshot(window.get(), QStringLiteral("statements-proof-page"));

    // Raw shows the whole file.
    QTest::mouseClick(proof->rawToggle(), Qt::LeftButton);
    QVERIFY(proof->editor()->text().startsWith(QStringLiteral("+++\nid = \"D5MN.a\"")));
    QVERIFY(proof->proofForm()->isHidden());
    QTest::mouseClick(proof->rawToggle(), Qt::LeftButton);
    QVERIFY(proof->editor()->text().startsWith(QStringLiteral("By \\ref{C4JK}")));
}

void StatementsTabTests::listSearchesFiltersAndSorts()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    loom::StatementList* list = window->statementsPage()->list();

    // By words of the title, ID, tags and body text.
    QTest::keyClicks(list->search(), QStringLiteral("rearr"));
    QCOMPARE(list->shownIds(), (QStringList{QStringLiteral("F7RS"), QStringLiteral("F7RS.a")}));
    list->search()->setText(QStringLiteral("k3"));
    QCOMPARE(list->shownIds(), QStringList{QStringLiteral("K3AB")});
    list->search()->setText(QStringLiteral("tail-bounds"));
    QVERIFY(list->shownIds().contains(QStringLiteral("K3AB")));
    QVERIFY(!list->shownIds().contains(QStringLiteral("A2DF")));
    list->search()->clear();

    // Filters: a kind, then a status.
    list->filterAction(QStringLiteral("kind"), QStringLiteral("definition"))->setChecked(true);
    QCOMPARE(list->shownIds(), (QStringList{QStringLiteral("A2DF"), QStringLiteral("B3GH")}));
    list->filterAction(QStringLiteral("kind"), QStringLiteral("definition"))->setChecked(false);
    list->filterAction(QStringLiteral("status"), QStringLiteral("open"))->setChecked(true);
    QCOMPARE(list->shownIds(), QStringList{QStringLiteral("K3AB")});
    list->filterAction(QStringLiteral("status"), QStringLiteral("open"))->setChecked(false);

    // Paper results, grouped under their paper, read-only.
    list->filterAction(QStringLiteral("paper-results"), {})->setChecked(true);
    QVERIFY(list->shownIds().contains(QStringLiteral("7RT2")));
    QVERIFY(list->shownIds().contains(QStringLiteral("3WQ6")));
    clickRow(window->statementsPage(), QStringLiteral("7RT2"));
    FilePage* stub = window->statementsPage()->currentPage();
    QVERIFY(stub);
    QVERIFY(stub->editor()->isReadOnly());
    QVERIFY(stub->bar(QStringLiteral("stubBar")));
    list->filterAction(QStringLiteral("paper-results"), {})->setChecked(false);

    // Sort by title.
    list->sort()->setCurrentIndex(list->sort()->findData(static_cast<int>(loom::StatementList::Sort::Title)));
    const QStringList byTitle = list->shownIds();
    QCOMPARE(byTitle.first(), QStringLiteral("H9WX")); // "A sharp bound for the tail"
    QVERIFY(byTitle.indexOf(QStringLiteral("K3AB")) > byTitle.indexOf(QStringLiteral("H9WX"))); // "A tail bound for sums"
    QVERIFY(byTitle.indexOf(QStringLiteral("K3AB")) < byTitle.indexOf(QStringLiteral("C4JK")));
    list->sort()->setCurrentIndex(list->sort()->findData(static_cast<int>(loom::StatementList::Sort::Status)));
    QCOMPARE(list->shownIds().first(), QStringLiteral("C4JK")); // Proved first, ties by ID.
}

void StatementsTabTests::editsAndSaves()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    page->openStatement(QStringLiteral("K3AB"));
    FilePage* file = page->currentPage();
    const QString path = demo.root() + QStringLiteral("/Statements/K3AB.tex");
    const QByteArray before = readFile(path);

    // Typing in the editor and the form: one set of unsaved edits.
    file->editor()->setFocus();
    file->editor()->setCursorPosition(0, 0);
    QTest::keyClicks(file->editor(), QStringLiteral("Indeed, "));
    file->statementForm()->title()->selectAll();
    QTest::keyClicks(file->statementForm()->title(), QStringLiteral("A sharper tail bound for sums"));
    QVERIFY(page->hasUnsavedEdits());
    QCOMPARE(page->unsavedPaths(), QStringList{QStringLiteral("Statements/K3AB.tex")});
    QVERIFY(row(page, QStringLiteral("K3AB"))->text(0).startsWith(QStringLiteral("● ")));
    QVERIFY(window->sideBar()->button(Tab::Statements)->badgeDot());
    QCOMPARE(readFile(path), before);

    // Ctrl+S saves through LoomCore.
    QSignalSpy saved(page, &StatementsPage::saved);
    QTest::keyClick(window.get(), Qt::Key_S, Qt::ControlModifier);
    QTRY_COMPARE(saved.count(), 1);
    const QByteArray after = readFile(path);
    QVERIFY(after.contains("title = \"A sharper tail bound for sums\""));
    QVERIFY(after.contains("+++\nIndeed, Let $a$ and $b$ be tame"));
    QVERIFY(!page->hasUnsavedEdits());
    QVERIFY(!window->sideBar()->button(Tab::Statements)->badgeDot());
    QTRY_VERIFY(!row(page, QStringLiteral("K3AB"))->text(0).startsWith(QStringLiteral("● ")));
    QTRY_VERIFY(window->session()->snapshot()->graph.statement(QStringLiteral("K3AB"))->title == QStringLiteral("A sharper tail bound for sums"));
    QVERIFY(!QFileInfo::exists(demo.root() + QStringLiteral("/.loom/drafts/Statements/K3AB.tex")));
}

void StatementsTabTests::aRefusedSaveKeepsTheText()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    page->openStatement(QStringLiteral("K3AB"));
    FilePage* file = page->currentPage();
    const QString path = demo.root() + QStringLiteral("/Statements/K3AB.tex");
    const QByteArray before = readFile(path);

    // A title is required: the save adds an error, so it is refused (E6).
    file->statementForm()->title()->selectAll();
    QTest::keyClick(file->statementForm()->title(), Qt::Key_Delete);
    file->editor()->setText(QStringLiteral("My careful new text.\n"));
    QSignalSpy failed(page, &StatementsPage::saveFailed);
    QSignalSpy problems(page, &StatementsPage::refusalProblems);
    page->save();
    QTRY_COMPARE(failed.count(), 1);
    QCOMPARE(readFile(path), before);
    QCOMPARE(file->editor()->text(), QStringLiteral("My careful new text.\n"));
    QVERIFY(page->hasUnsavedEdits());
    QVERIFY(file->bar(QStringLiteral("refusalBar")));
    QVERIFY(problems.count() >= 1);
    QVERIFY(window->bottomPanel()->isVisible());
    QVERIFY(window->bottomPanel()->problems()->topLevelItemCount() >= 1);
    QVERIFY(window->bottomPanel()->problems()->topLevelItem(0)->text(2).startsWith(QStringLiteral("Not saved: ")));
    screenshot(window.get(), QStringLiteral("statements-refused-save"));

    // In raw mode, a changed tag is refused too, and the text stays.
    QTest::mouseClick(file->rawToggle(), Qt::LeftButton);
    QString raw = file->editor()->text();
    raw.replace(QStringLiteral("id = \"K3AB\""), QStringLiteral("id = \"K3AC\""));
    raw.replace(QStringLiteral("title = \"\""), QStringLiteral("title = \"Fine\""));
    file->editor()->setText(raw);
    page->save();
    QTRY_COMPARE(failed.count(), 2);
    QVERIFY(file->bar(QStringLiteral("refusalBar"))->findChild<QLabel*>(QStringLiteral("text"))->text().contains(
        QStringLiteral("Loom can't change K3AB to K3AC")));
    QCOMPARE(file->editor()->text(), raw);
    QCOMPARE(readFile(path), before);
}

void StatementsTabTests::outdatingChecksAsksFirst()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    page->openStatement(QStringLiteral("C4JK"));
    FilePage* file = page->currentPage();
    const QString path = demo.root() + QStringLiteral("/Statements/C4JK.tex");
    const QByteArray before = readFile(path);
    file->editor()->setText(QStringLiteral("If $a$ is tame, its tails tend to zero quickly.\n"));

    // C4JK.a proves it and D5MN.a uses it; both are checked.
    QSignalSpy failed(page, &StatementsPage::saveFailed);
    page->save();
    QTRY_VERIFY(box(*window, QStringLiteral("outdateChecksDialog")));
    QMessageBox* dialog = box(*window, QStringLiteral("outdateChecksDialog"));
    QVERIFY(dialog->text().contains(QStringLiteral("C4JK.a and D5MN.a")));
    QCOMPARE(dialog->findChild<QPushButton*>(QStringLiteral("confirm"))->text(), QStringLiteral("Save and outdate checks"));
    // Cancel is focused, so Enter alone never confirms.
    QVERIFY(dialog->findChild<QPushButton*>(QStringLiteral("cancel"))->hasFocus() || dialog->defaultButton()
            == dialog->findChild<QPushButton*>(QStringLiteral("cancel")));
    screenshot(dialog, QStringLiteral("statements-outdate-checks"));
    answer(*window, QStringLiteral("outdateChecksDialog"), QStringLiteral("cancel"));
    QTRY_COMPARE(failed.count(), 1);
    QCOMPARE(readFile(path), before);
    QCOMPARE(file->editor()->text(), QStringLiteral("If $a$ is tame, its tails tend to zero quickly.\n"));

    // Confirmed: saved, and the checks stop counting.
    QSignalSpy saved(page, &StatementsPage::saved);
    page->save();
    answer(*window, QStringLiteral("outdateChecksDialog"), QStringLiteral("confirm"));
    QTRY_COMPARE(saved.count(), 1);
    QVERIFY(readFile(path).endsWith("+++\nIf $a$ is tame, its tails tend to zero quickly.\n"));
    QTRY_VERIFY(window->session()->snapshot()->graph.statement(QStringLiteral("C4JK"))->status != loom::Status::Proved);
}

void StatementsTabTests::markingAProofFailedAsksFirst()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    page->openProof(QStringLiteral("G8TV.a"));
    FilePage* file = page->currentPage();
    const QString path = demo.root() + QStringLiteral("/Proofs/G8TV/G8TV.a.tex");
    const QByteArray before = readFile(path);
    QComboBox* level = file->proofForm()->level();

    // Choosing failed asks first; cancelling puts the level back and writes nothing.
    QSignalSpy failed(page, &StatementsPage::saveFailed);
    level->setCurrentIndex(level->findData(QStringLiteral("failed")));
    emit level->activated(level->currentIndex());
    QTRY_VERIFY(box(*window, QStringLiteral("markFailedDialog")));
    QVERIFY(box(*window, QStringLiteral("markFailedDialog"))->text().startsWith(QStringLiteral("Mark G8TV.a failed? It stops supporting G8TV.")));
    answer(*window, QStringLiteral("markFailedDialog"), QStringLiteral("cancel"));
    QTRY_COMPARE(failed.count(), 1);
    QCOMPARE(readFile(path), before);
    QTRY_COMPARE(file->proofForm()->level()->currentData().toString(), QStringLiteral("complete"));
    QVERIFY(!page->hasUnsavedEdits());

    // Confirmed: saved as failed.
    QSignalSpy saved(page, &StatementsPage::saved);
    level = file->proofForm()->level();
    level->setCurrentIndex(level->findData(QStringLiteral("failed")));
    emit level->activated(level->currentIndex());
    answer(*window, QStringLiteral("markFailedDialog"), QStringLiteral("confirm"));
    QTRY_COMPARE(saved.count(), 1);
    QVERIFY(readFile(path).contains("level = \"failed\""));
}

void StatementsTabTests::newStatementAndNewProof()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();

    // Ctrl+N: the form; an empty body is refused and the form stays.
    QTest::keyClick(window.get(), Qt::Key_N, Qt::ControlModifier);
    QTRY_VERIFY(page->newStatementDialog());
    loom::NewStatementDialog* form = page->newStatementDialog();
    form->kind()->setCurrentIndex(form->kind()->findData(QStringLiteral("proposition")));
    QTest::keyClicks(form->title(), QStringLiteral("Tails of sums of tame sequences"));
    QTest::mouseClick(form->createButton(), Qt::LeftButton);
    QTRY_VERIFY(form->refusal()->isVisible());
    QVERIFY(form->refusal()->text().contains(QStringLiteral("body")));
    QVERIFY(page->newStatementDialog());

    // Its text is kept in a recovery file while it's open.
    form->body()->setText(QStringLiteral("If $a$ and $b$ are tame, so is $a + b$.\n"));
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(demo.root() + QStringLiteral("/.loom/drafts/new/statement.txt")), 5000);

    QSignalSpy created(page, &StatementsPage::created);
    QTest::mouseClick(form->createButton(), Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(created.count(), 1, 15'000);
    const QString tag = created.first().first().toString();
    QTRY_VERIFY(!page->newStatementDialog());
    QCOMPARE(page->openTag(), tag);
    const QByteArray file = readFile(demo.root() + QStringLiteral("/Statements/%1.tex").arg(tag));
    QVERIFY(file.contains("kind = \"proposition\""));
    QVERIFY(file.contains("title = \"Tails of sums of tame sequences\""));
    QVERIFY(file.endsWith("+++\nIf $a$ and $b$ are tame, so is $a + b$.\n"));
    QVERIFY(!QFileInfo::exists(demo.root() + QStringLiteral("/.loom/drafts/new/statement.txt")));

    // + Proof (Ctrl+Shift+N): the new proof's page opens.
    QTest::keyClick(window.get(), Qt::Key_N, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY(page->newProofDialog());
    loom::NewProofDialog* proofForm = page->newProofDialog();
    QCOMPARE(proofForm->level()->findData(QStringLiteral("failed")), -1); // Not offered.
    proofForm->level()->setCurrentIndex(proofForm->level()->findData(QStringLiteral("sketch")));
    QTest::keyClicks(proofForm->premises(), QStringLiteral("C4JK"));
    proofForm->body()->setText(QStringLiteral("Add the tails, by \\ref{C4JK}.\n"));
    QTest::mouseClick(proofForm->createButton(), Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(created.count(), 2, 15'000);
    QCOMPARE(created.last().first().toString(), tag + QStringLiteral(".a"));
    QTRY_VERIFY(page->currentPage() && page->currentPage()->proofId() == tag + QStringLiteral(".a"));
    const QByteArray proof = readFile(demo.root() + QStringLiteral("/Proofs/%1/%1.a.tex").arg(tag));
    QVERIFY(proof.contains("premises = [\"C4JK\"]"));
    QVERIFY(proof.contains("level = \"sketch\""));
}

void StatementsTabTests::verboseVersions()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();

    // E6PQ.a has one: the toggle edits it in the same editor.
    page->openProof(QStringLiteral("E6PQ.a"));
    FilePage* file = page->currentPage();
    const QString verbosePath = demo.root() + QStringLiteral("/Proofs/E6PQ/E6PQ.a.verbose.tex");
    const QByteArray before = readFile(verbosePath);
    QTest::mouseClick(file->verboseToggle(), Qt::LeftButton);
    QVERIFY(file->showingVerbose());
    QVERIFY(file->editor()->text().startsWith(QStringLiteral("Write $M = \\sup")));
    QVERIFY(!file->bar(QStringLiteral("staleBar"))); // Written from this version.
    file->editor()->setText(file->editor()->text() + QStringLiteral("One more step.\n"));
    QSignalSpy saved(page, &StatementsPage::saved);
    page->save();
    QTRY_COMPARE(saved.count(), 1);
    const QByteArray after = readFile(verbosePath);
    QVERIFY(after.endsWith("One more step.\n"));
    // Saving its text doesn't change its fingerprint.
    QCOMPARE(after.left(after.indexOf("created")), before.left(before.indexOf("created")));
    QTest::mouseClick(file->proofToggle(), Qt::LeftButton);
    QVERIFY(file->editor()->text().startsWith(QStringLiteral("Write")) == false);

    // D5MN.a has none: Write a verbose version, then the first save creates it.
    page->openProof(QStringLiteral("D5MN.a"));
    file = page->currentPage();
    QTest::mouseClick(file->verboseToggle(), Qt::LeftButton);
    auto* write = file->findChild<QPushButton*>(QStringLiteral("writeVerbose"));
    QVERIFY(write && write->isVisible());
    QTest::mouseClick(write, Qt::LeftButton);
    file = page->currentPage();
    QVERIFY(file->editor()->isVisible());
    file->editor()->setText(QStringLiteral("Every step of D5MN.a.\n"));
    page->save();
    QTRY_COMPARE(saved.count(), 2);
    const QByteArray created = readFile(demo.root() + QStringLiteral("/Proofs/D5MN/D5MN.a.verbose.tex"));
    QVERIFY(created.startsWith("+++\nproof = \"D5MN.a\"\nfingerprint = \""));
    QVERIFY(created.endsWith("+++\nEvery step of D5MN.a.\n"));

    // When the proof changes, its verbose version is stale: Mark up to date.
    const QString proofPath = demo.root() + QStringLiteral("/Proofs/E6PQ/E6PQ.a.tex");
    QVERIFY(writeFile(proofPath, readFile(proofPath).replace("Let $|b_n|", "Suppose $|b_n|")));
    window->session()->refresh();
    page->openProof(QStringLiteral("E6PQ.a"));
    file = page->currentPage();
    QTest::mouseClick(file->verboseToggle(), Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(file->bar(QStringLiteral("staleBar")), 15'000);
    QTest::mouseClick(file->bar(QStringLiteral("staleBar"))->findChild<QPushButton*>(QStringLiteral("markUpToDate")), Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(!page->currentPage()->bar(QStringLiteral("staleBar")), 15'000);
    QVERIFY(readFile(verbosePath).left(60) != after.left(60));
}

void StatementsTabTests::backAndForward()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    clickRow(page, QStringLiteral("C4JK"));
    clickRow(page, QStringLiteral("D5MN"));
    clickRow(page, QStringLiteral("E6PQ.a"));
    QCOMPARE(page->openTag(), QStringLiteral("E6PQ"));
    QStringList back;
    for (const loom::Place& place : page->backHistory())
        back << place.tag + u'/' + place.page;
    QCOMPARE(back, (QStringList{QStringLiteral("C4JK/"), QStringLiteral("D5MN/")}));

    QTest::keyClick(window.get(), Qt::Key_Left, Qt::AltModifier);
    QCOMPARE(page->openTag(), QStringLiteral("D5MN"));
    QTest::keyClick(window.get(), Qt::Key_Left, Qt::AltModifier);
    QCOMPARE(page->openTag(), QStringLiteral("C4JK"));
    QTest::keyClick(window.get(), Qt::Key_Right, Qt::AltModifier);
    QCOMPARE(page->openTag(), QStringLiteral("D5MN"));
    QTest::keyClick(window.get(), Qt::Key_Right, Qt::AltModifier);
    QCOMPARE(page->openTag(), QStringLiteral("E6PQ"));
    QCOMPARE(page->currentPage()->proofId(), QStringLiteral("E6PQ.a"));

    // Kept per project between runs.
    page->back();
    window->close();
    QTRY_VERIFY(!window->isVisible());
    window.reset(); // The app quits with its last window, releasing the project.
    const std::unique_ptr<MainWindow> again = newWindow();
    QVERIFY(openDemo(*again, demo.root()));
    QCOMPARE(again->statementsPage()->openTag(), QStringLiteral("D5MN"));
    QCOMPARE(again->statementsPage()->backHistory().size(), std::size_t(1));
    QCOMPARE(again->statementsPage()->forwardHistory().size(), std::size_t(1));
    again->statementsPage()->forward();
    QCOMPARE(again->statementsPage()->openTag(), QStringLiteral("E6PQ"));
}

void StatementsTabTests::findAndReplace()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    page->openStatement(QStringLiteral("K3AB"));
    FilePage* file = page->currentPage();

    QTest::keyClick(window.get(), Qt::Key_F, Qt::ControlModifier);
    loom::FindBar* bar = file->findBar();
    QVERIFY(bar->isVisible());
    QVERIFY(!bar->replaceBox()->isVisible());
    QTest::keyClicks(bar->findBox(), QStringLiteral("T_N"));
    QTest::keyClick(bar->findBox(), Qt::Key_Return);
    QCOMPARE(file->editor()->selectedText(), QStringLiteral("T_N"));
    bar->matchCase()->setChecked(true);
    bar->findBox()->setText(QStringLiteral("t_n"));
    QVERIFY(!bar->findNext());
    QCOMPARE(bar->note()->text(), QStringLiteral("No matches"));
    bar->matchCase()->setChecked(false);

    // Ctrl+H adds replace; Replace all, in the open file only, as unsaved edits.
    QTest::keyClick(window.get(), Qt::Key_H, Qt::ControlModifier);
    QVERIFY(bar->replaceBox()->isVisible());
    bar->findBox()->setText(QStringLiteral("T_N"));
    bar->replaceBox()->setText(QStringLiteral("\\tau_N"));
    QTest::mouseClick(bar->replaceAllButton(), Qt::LeftButton);
    QCOMPARE(bar->note()->text(), QStringLiteral("3 replaced"));
    QVERIFY(file->editor()->text().contains(QStringLiteral("\\tau_N(a + b) \\le \\tau_N(a) + \\tau_N(b)")));
    QVERIFY(page->hasUnsavedEdits());

    // Ctrl+Shift+F: bodies of every statement and proof; a click opens the line.
    QTest::keyClick(window.get(), Qt::Key_F, Qt::ControlModifier | Qt::ShiftModifier);
    QTRY_VERIFY(window->findChild<QDialog*>(QStringLiteral("findInProject")));
    auto* dialog = window->findChild<QDialog*>(QStringLiteral("findInProject"));
    auto* query = dialog->findChild<QLineEdit*>(QStringLiteral("query"));
    QTest::keyClicks(query, QStringLiteral("there is $N$ with $T_N(a)"));
    QTest::keyClick(query, Qt::Key_Return);
    auto* results = dialog->findChild<QTreeWidget*>(QStringLiteral("results"));
    QTRY_COMPARE(results->topLevelItemCount(), 1);
    QCOMPARE(results->topLevelItem(0)->text(0), QStringLiteral("D5MN.a · Tame sequences are bounded"));
    QTest::mouseClick(results->viewport(), Qt::LeftButton, {}, results->visualItemRect(results->topLevelItem(0)).center());
    QCOMPARE(page->openTag(), QStringLiteral("D5MN"));
    QCOMPARE(page->currentPage()->proofId(), QStringLiteral("D5MN.a"));
    QCOMPARE(page->currentPage()->editor()->cursorLine(), 1);
    dialog->close();
}

void StatementsTabTests::editsSurviveLeavingAndSwitchingTabs()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    page->openStatement(QStringLiteral("K3AB"));
    page->currentPage()->editor()->setText(QStringLiteral("Half-typed \\frac{1}{"));
    page->openStatement(QStringLiteral("C4JK"));
    window->showTab(Tab::Review);
    window->showTab(Tab::Statements);
    QVERIFY(page->hasUnsavedEdits());
    QVERIFY(row(page, QStringLiteral("K3AB"))->text(0).startsWith(QStringLiteral("● ")));
    page->back();
    QCOMPARE(page->openTag(), QStringLiteral("K3AB"));
    QCOMPARE(page->currentPage()->editor()->text(), QStringLiteral("Half-typed \\frac{1}{"));
    QCOMPARE(page->currentPage()->editor()->text().size(), 20);
}

void StatementsTabTests::aFileChangedOnDiskReloadsOrAsks()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    page->openStatement(QStringLiteral("K3AB"));
    FilePage* file = page->currentPage();
    const QString path = demo.root() + QStringLiteral("/Statements/K3AB.tex");

    // No unsaved edits: it reloads silently.
    const QByteArray agents = readFile(path).replace("Let $a$", "Let $a$ (an agent's edit)");
    QVERIFY(writeFile(path, agents));
    window->session()->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(file->editor()->text().contains(QStringLiteral("an agent's edit")), 15'000);
    QVERIFY(!page->hasUnsavedEdits());
    QVERIFY(!file->bar(QStringLiteral("outsideBar")));

    // Unsaved edits: a bar asks, and saving is refused until Dakota chooses.
    file->editor()->setText(QStringLiteral("My own text.\n"));
    const QByteArray second = QByteArray(agents).replace("an agent's edit", "a second edit");
    QVERIFY(writeFile(path, second));
    window->session()->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(file->bar(QStringLiteral("outsideBar")), 15'000);
    QVERIFY(file->bar(QStringLiteral("outsideBar"))->findChild<QLabel*>(QStringLiteral("text"))->text()
            == QStringLiteral("K3AB.tex changed on disk while you were editing it."));
    QCOMPARE(file->editor()->text(), QStringLiteral("My own text.\n"));
    screenshot(window.get(), QStringLiteral("statements-changed-on-disk"));
    QSignalSpy failed(page, &StatementsPage::saveFailed);
    page->save();
    QTRY_COMPARE(failed.count(), 1);
    QCOMPARE(readFile(path), second);

    // Compare shows both; Take the file's asks, then discards mine.
    QTest::mouseClick(file->bar(QStringLiteral("outsideBar"))->findChild<QPushButton*>(QStringLiteral("compare")), Qt::LeftButton);
    QTRY_VERIFY(window->findChild<QDialog*>(QStringLiteral("compareDialog")));
    window->findChild<QDialog*>(QStringLiteral("compareDialog"))->close();
    QTest::mouseClick(file->bar(QStringLiteral("outsideBar"))->findChild<QPushButton*>(QStringLiteral("takeTheirs")), Qt::LeftButton);
    answer(*window, QStringLiteral("takeTheirsDialog"), QStringLiteral("confirm"));
    QTRY_VERIFY(file->editor()->text().contains(QStringLiteral("a second edit")));
    QVERIFY(!page->hasUnsavedEdits());

    // Keep mine: my text stays, and saving now goes over the file's.
    file->editor()->setText(QStringLiteral("Mine again.\n"));
    QVERIFY(writeFile(path, QByteArray(second).replace("a second edit", "a third edit")));
    window->session()->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(file->bar(QStringLiteral("outsideBar")), 15'000);
    QTest::mouseClick(file->bar(QStringLiteral("outsideBar"))->findChild<QPushButton*>(QStringLiteral("keepMine")), Qt::LeftButton);
    QTRY_VERIFY(!file->bar(QStringLiteral("outsideBar")));
    QSignalSpy saved(page, &StatementsPage::saved);
    page->save();
    QTRY_COMPARE(saved.count(), 1);
    QVERIFY(readFile(path).endsWith("+++\nMine again.\n"));

    // Deleted on disk: the text stays, with Restore it.
    QVERIFY(QFile::remove(path));
    window->session()->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(file->bar(QStringLiteral("outsideBar")), 15'000);
    QCOMPARE(file->bar(QStringLiteral("outsideBar"))->findChild<QLabel*>(QStringLiteral("text"))->text(),
             QStringLiteral("K3AB.tex was deleted on disk."));
    QTest::mouseClick(file->bar(QStringLiteral("outsideBar"))->findChild<QPushButton*>(QStringLiteral("restore")), Qt::LeftButton);
    QTRY_COMPARE(saved.count(), 2);
    QVERIFY(readFile(path).endsWith("+++\nMine again.\n"));
}

void StatementsTabTests::recoveryAfterACrash()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const QString path = demo.root() + QStringLiteral("/Statements/K3AB.tex");
    const QString c4jk = demo.root() + QStringLiteral("/Statements/C4JK.tex");
    const QString d5mn = demo.root() + QStringLiteral("/Statements/D5MN.tex");
    {
        auto window = newWindow();
        QVERIFY(openDemo(*window, demo.root()));
        StatementsPage* page = window->statementsPage();
        page->openStatement(QStringLiteral("K3AB"));
        page->currentPage()->editor()->setText(QStringLiteral("Unsaved, and the app is about to die.\n"));
        page->openStatement(QStringLiteral("C4JK"));
        page->currentPage()->editor()->setText(QStringLiteral("Also unsaved.\n"));
        page->openStatement(QStringLiteral("D5MN"));
        page->currentPage()->editor()->setText(QStringLiteral("Unsaved too.\n"));
        // A second after typing stops, the recovery files are written.
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(demo.root() + QStringLiteral("/.loom/drafts/Statements/D5MN.tex.toml")), 5000);
        QVERIFY(window->projectLane()->waitUntilIdle());
        // A crash: the window goes without closing, so nothing asks and nothing is cleaned up.
        window->hide();
        window.reset();
    }
    QCOMPARE(loom::listDrafts(demo.root()).size(), std::size_t(3));
    // Meanwhile C4JK changes on disk, and D5MN is deleted.
    QVERIFY(writeFile(c4jk, readFile(c4jk).replace("tend to zero", "tend to 0")));
    QVERIFY(QFile::remove(d5mn));

    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    QVERIFY(page->hasUnsavedEdits());

    // The file is still `base`: Keep or Discard.
    page->openStatement(QStringLiteral("K3AB"));
    FilePage* file = page->currentPage();
    QVERIFY(file->bar(QStringLiteral("recoveryBar")));
    QVERIFY(file->bar(QStringLiteral("recoveryBar"))->findChild<QLabel*>(QStringLiteral("text"))->text().startsWith(
        QStringLiteral("Recovered unsaved edits from ")));
    QCOMPARE(file->editor()->text(), QStringLiteral("Unsaved, and the app is about to die.\n"));
    screenshot(window.get(), QStringLiteral("statements-recovered"));
    QTest::mouseClick(file->bar(QStringLiteral("recoveryBar"))->findChild<QPushButton*>(QStringLiteral("keep")), Qt::LeftButton);
    QTRY_VERIFY(!file->bar(QStringLiteral("recoveryBar")));
    QVERIFY(page->unsavedPaths().contains(QStringLiteral("Statements/K3AB.tex")));

    // Changed since: saving waits for Keep mine or Discard mine.
    page->openStatement(QStringLiteral("C4JK"));
    file = page->currentPage();
    QVERIFY(file->bar(QStringLiteral("recoveryBar"))->findChild<QLabel*>(QStringLiteral("text"))->text().contains(
        QStringLiteral("but the file has changed since")));
    QCOMPARE(file->editor()->text(), QStringLiteral("Also unsaved.\n"));
    QTest::mouseClick(file->bar(QStringLiteral("recoveryBar"))->findChild<QPushButton*>(QStringLiteral("discardMine")), Qt::LeftButton);
    QTRY_VERIFY(file->editor()->text().contains(QStringLiteral("tend to 0")));
    QVERIFY(!page->unsavedPaths().contains(QStringLiteral("Statements/C4JK.tex")));

    // Gone: Restore it with my text.
    page->openStatement(QStringLiteral("D5MN"));
    file = page->currentPage();
    QVERIFY(file->bar(QStringLiteral("recoveryBar"))->findChild<QLabel*>(QStringLiteral("text"))->text().contains(
        QStringLiteral("but the file has been deleted")));
    QSignalSpy saved(page, &StatementsPage::saved);
    QTest::mouseClick(file->bar(QStringLiteral("recoveryBar"))->findChild<QPushButton*>(QStringLiteral("restore")), Qt::LeftButton);
    QTRY_COMPARE(saved.count(), 1);
    QVERIFY(readFile(d5mn).endsWith("+++\nUnsaved too.\n"));
    QTRY_VERIFY(!QFileInfo::exists(demo.root() + QStringLiteral("/.loom/drafts/Statements/D5MN.tex")));
    QTRY_VERIFY(!QFileInfo::exists(demo.root() + QStringLiteral("/.loom/drafts/Statements/C4JK.tex")));
    QVERIFY(QFileInfo::exists(demo.root() + QStringLiteral("/.loom/drafts/Statements/K3AB.tex")));
    QVERIFY(readFile(path).contains("Let $a$ and $b$ be tame")); // Kept as unsaved edits, not saved.
}

void StatementsTabTests::closingWithUnsavedEditsAsks()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    page->openStatement(QStringLiteral("K3AB"));
    page->currentPage()->editor()->setText(QStringLiteral("Not saved yet.\n"));

    // Cancel: the window stays, with the text.
    window->close();
    QTRY_VERIFY(box(*window, QStringLiteral("unsavedEditsMessage")));
    QVERIFY(box(*window, QStringLiteral("unsavedEditsMessage"))->defaultButton()->objectName() == QStringLiteral("cancel"));
    answer(*window, QStringLiteral("unsavedEditsMessage"), QStringLiteral("cancel"));
    QVERIFY(window->isVisible());
    QCOMPARE(page->currentPage()->editor()->text(), QStringLiteral("Not saved yet.\n"));

    // Opening another project asks the same.
    const loomtest::DemoProject other;
    QVERIFY2(other.ok(), qPrintable(other.error()));
    window->openFolder(other.root());
    answer(*window, QStringLiteral("unsavedEditsMessage"), QStringLiteral("cancel"));
    QCOMPARE(window->session()->root(), demo.root());

    // Keep them for next time: the recovery file is written, and the window closes.
    window->close();
    answer(*window, QStringLiteral("unsavedEditsMessage"), QStringLiteral("keep"));
    QTRY_VERIFY(!window->isVisible());
    window.reset(); // The app quits with its last window, releasing the project.
    const std::vector<loom::Draft> drafts = loom::listDrafts(demo.root());
    QCOMPARE(drafts.size(), std::size_t(1));
    QCOMPARE(drafts.front().path, QStringLiteral("Statements/K3AB.tex"));
    QVERIFY(drafts.front().text.endsWith("+++\nNot saved yet.\n"));

    // Save all, on the next window.
    const std::unique_ptr<MainWindow> again = newWindow();
    QVERIFY(openDemo(*again, demo.root()));
    QVERIFY(again->statementsPage()->hasUnsavedEdits());
    again->close();
    answer(*again, QStringLiteral("unsavedEditsMessage"), QStringLiteral("saveAll"));
    QTRY_VERIFY(!again->isVisible());
    QVERIFY(readFile(demo.root() + QStringLiteral("/Statements/K3AB.tex")).endsWith("+++\nNot saved yet.\n"));
    QVERIFY(loom::listDrafts(demo.root()).empty());
}

void StatementsTabTests::aBrokenFileOpensInRawMode()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const QString path = demo.root() + QStringLiteral("/Statements/J2YZ.tex");
    QVERIFY(writeFile(path, readFile(path).replace("title = \"Which tail bounds are sharp?\"", "title = \"Which tail bounds are sharp?")));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    QVERIFY(row(page, QStringLiteral("J2YZ")));
    page->openStatement(QStringLiteral("J2YZ"));
    FilePage* file = page->currentPage();
    QVERIFY(file->rawToggle()->isChecked());
    QVERIFY(file->editor()->text().startsWith(QStringLiteral("+++\nid = \"J2YZ\"")));
    QVERIFY(file->statementForm()->isHidden());
    // The form won't open on it, and says why.
    QTest::mouseClick(file->rawToggle(), Qt::LeftButton);
    QVERIFY(file->rawToggle()->isChecked());
    QVERIFY(file->bar(QStringLiteral("noteBar")));
    // Fixed in raw text and saved, it reads again.
    QString text = file->editor()->text();
    text.replace(QStringLiteral("are sharp?\n"), QStringLiteral("are sharp?\"\n"));
    file->editor()->setText(text);
    QSignalSpy saved(page, &StatementsPage::saved);
    page->save();
    QTRY_COMPARE(saved.count(), 1);
    QTRY_VERIFY(window->session()->snapshot()->graph.statement(QStringLiteral("J2YZ")));
}

void StatementsTabTests::aProblemClickOpensTheLine()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    // A body line opens in the form, at the body's line; a front-matter line in raw mode.
    emit window->bottomPanel()->problemActivated(QStringLiteral("Statements/K3AB.tex"), 10);
    QCOMPARE(page->openTag(), QStringLiteral("K3AB"));
    QVERIFY(!page->currentPage()->rawToggle()->isChecked());
    QCOMPARE(page->currentPage()->editor()->cursorLine(), 2);
    emit window->bottomPanel()->problemActivated(QStringLiteral("Proofs/D5MN/D5MN.a.tex"), 4);
    QCOMPARE(page->currentPage()->proofId(), QStringLiteral("D5MN.a"));
    QVERIFY(page->currentPage()->rawToggle()->isChecked());
    QCOMPARE(page->currentPage()->editor()->cursorLine(), 4);
}

QTEST_MAIN(StatementsTabTests)
#include "StatementsTabTests.moc"
