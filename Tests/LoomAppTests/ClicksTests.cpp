// Status, checks and Dakota's clicks in the Statements tab (Specs/m2-statements-review.md, "Status
// and checks in the tab" and "Dakota's clicks"; Verification, "Checks and clicks": "Each click
// confirmed and cancelled; status updates everywhere"). Each test drives the real widgets on a copy
// of the demo project (Tests/Support/DemoProject). What a check or refutation does to statuses is
// LoomCore's, tested there (StatusChangesTests); these check the wiring: the dialogs say what
// LoomCore says, cancelling writes nothing, confirming writes what `loom` would, and the new status
// shows in the list, the header and the check history.

#include "LoomApp/AppSettings.h"
#include "LoomApp/ChecksPanel.h"
#include "LoomApp/EditBuffer.h"
#include "LoomApp/FilePage.h"
#include "LoomApp/HeaderForms.h"
#include "LoomApp/IdPicker.h"
#include "LoomApp/IdTools.h"
#include "LoomApp/LatexEditor.h"
#include "LoomApp/MainWindow.h"
#include "LoomApp/ProjectSession.h"
#include "LoomApp/StatementList.h"
#include "LoomApp/StatementsPage.h"

#include "LoomCore/Checks.h"

#include "DemoProject.h"

#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocumentFragment>
#include <QTimeZone>
#include <QToolButton>
#include <QTreeWidget>

#include <memory>

using loom::ChecksPanel;
using loom::FilePage;
using loom::MainWindow;
using loom::StatementsPage;

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

/// Writes a file. @returns Whether it worked.
bool writeFile(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

/// A dialog of the window's, by object name, or null.
template <typename T = QDialog>
T* dialog(const QWidget& window, const QString& name)
{
    return window.findChild<T*>(name);
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

/// The plain text of a rich-text label.
QString plain(const QLabel* label)
{
    return QTextDocumentFragment::fromHtml(label->text()).toPlainText();
}

/// The list row of an ID's tooltip, which names its status, or empty.
QString rowTip(StatementsPage* page, const QString& id)
{
    for (QTreeWidgetItemIterator it(page->list()->tree()); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == id)
            return (*it)->toolTip(0);
    }
    return {};
}

/// The status of a statement in the latest snapshot.
loom::Status statusOf(MainWindow& window, const QString& tag)
{
    const loom::StatementNode* node = window.session()->snapshot()->graph.statement(tag);
    return node ? node->status : loom::Status::None;
}

} // namespace

/// Qt Test cases for the checks and Dakota's clicks.
class ClicksTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void sentencesForWhatAProofGives();
    void statusLineAndCheckHistory();
    void dakotasPassAndNeedsChanges();
    void failThenMarkFailed();
    void clicksWaitForUnsavedEdits();
    void aProofChangedUnderTheDialog();
    void refutingAsksFirst();

private:
    /// A new window on the test's settings, shown, with the demo open.
    std::unique_ptr<MainWindow> openDemo(const QString& root);

    std::unique_ptr<QTemporaryDir> m_temp;
    std::unique_ptr<loom::AppSettings> m_settings;
    QTemporaryDir m_gitHome;
};

void ClicksTests::initTestCase()
{
    QVERIFY(m_gitHome.isValid());
    const QString config = m_gitHome.filePath(QStringLiteral("gitconfig"));
    QVERIFY(writeFile(config, "[user]\n\tname = Loom Test\n\temail = test@example.com\n"));
    qputenv("GIT_CONFIG_GLOBAL", QDir::toNativeSeparators(config).toLocal8Bit());
    qputenv("GIT_CONFIG_NOSYSTEM", "1");
}

void ClicksTests::init()
{
    m_temp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_temp->isValid());
    m_settings = std::make_unique<loom::AppSettings>(pathsIn(*m_temp));
    // Compiling isn't what these tests are about, and needs TeX.
    m_settings->setCompileOnSave(false);
}

void ClicksTests::cleanup()
{
    m_settings.reset();
    m_temp.reset();
}

std::unique_ptr<MainWindow> ClicksTests::openDemo(const QString& root)
{
    auto window = std::make_unique<MainWindow>(m_settings.get());
    window->resize(1300, 900);
    window->show();
    QSignalSpy offered(window->statementsPage(), &StatementsPage::recoveryOffered);
    QSignalSpy finished(window.get(), &MainWindow::openFinished);
    window->openFolder(root);
    if ((finished.count() == 0 && !finished.wait(30'000)) || (offered.count() == 0 && !offered.wait(30'000)))
        return nullptr;
    return window;
}

void ClicksTests::sentencesForWhatAProofGives()
{
    // The line puts LoomCore's standing into words; the standing itself is set here by hand.
    loom::ProofNode proof;
    proof.id = QStringLiteral("4KX7.a");
    proof.proves = QStringLiteral("4KX7");
    proof.level = QStringLiteral("complete");
    proof.checking.fingerprint = QStringLiteral("5f0e5152b635");
    const auto check = [](const char* by, const char* verdict, int day) {
        loom::Check c;
        c.by = QString::fromLatin1(by);
        c.verdict = QString::fromLatin1(verdict);
        c.fingerprint = QStringLiteral("5f0e5152b635");
        c.date = QDateTime(QDate(2026, 11, day), QTime(10, 0), QTimeZone::fromSecondsAheadOfUtc(-6 * 3600));
        return c;
    };

    QCOMPARE(loom::givesSentence(proof), QStringLiteral("No checks yet: waiting for an agent's check."));

    proof.checking.checks = {check("agent:claude-opus-5-5", "pass", 4)};
    proof.checking.standing.countingAgent = 0;
    QCOMPARE(loom::givesSentence(proof), QStringLiteral("Waiting for your pass."));

    proof.checking.checks.push_back(check("dakota", "pass", 4));
    proof.checking.standing.countingDakota = 1;
    proof.checking.standing.checked = true;
    QCOMPARE(loom::givesSentence(proof), QStringLiteral("Checked: the agent's pass of 2026-11-04 and your pass of 2026-11-04."));

    proof.checking.checks.push_back(check("agent:claude-opus-5-5", "fail", 20));
    proof.checking.standing.countingAgent = 2;
    proof.checking.standing.checked = false;
    proof.checking.standing.supports = false;
    QCOMPARE(loom::givesSentence(proof), QStringLiteral("Doesn't support 4KX7: the agent says it's wrong (fail, 2026-11-20)."));

    proof.level = QStringLiteral("failed");
    QCOMPARE(loom::givesSentence(proof), QStringLiteral("Marked failed: it doesn't support 4KX7."));

    // The dialogs' words for LoomCore's status changes.
    const std::vector<loom::StatusChange> changes = {
        {QStringLiteral("4KX7"), QStringLiteral("T"), loom::Status::Unchecked, loom::Status::Proved, loom::StatusDirection::Raise},
        {QStringLiteral("P3QA"), QStringLiteral("T"), loom::Status::Proved, loom::Status::Conditional, loom::StatusDirection::Drop}};
    QCOMPARE(loom::statusBecomes(changes), QStringLiteral("4KX7 becomes proved; P3QA becomes conditional"));
    QCOMPARE(loom::statusArrows(changes), QStringLiteral("4KX7 unchecked → proved, P3QA proved → conditional"));
}

void ClicksTests::statusLineAndCheckHistory()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = openDemo(demo.root());
    QVERIFY(window);
    StatementsPage* page = window->statementsPage();

    // The status line is LoomCore's sentence, as `loom status D5MN` gives it, its IDs links.
    page->openStatement(QStringLiteral("D5MN"));
    FilePage* statement = page->currentPage();
    const loom::StatementNode* node = window->session()->snapshot()->graph.statement(QStringLiteral("D5MN"));
    QVERIFY(!node->explanation.isEmpty());
    QVERIFY(plain(statement->statusLine()).contains(node->explanation));
    QVERIFY(statement->statusLine()->text().contains(QStringLiteral("<a href=\"D5MN.a\">D5MN.a</a>")));
    emit statement->statusLine()->linkActivated(QStringLiteral("D5MN.a"));
    QTRY_COMPARE(page->currentPage()->proofId(), QStringLiteral("D5MN.a"));

    // C4JK.a: an agent's pass and Dakota's, both of this version, both counting.
    page->openProof(QStringLiteral("C4JK.a"));
    ChecksPanel* checks = page->currentPage()->checksPanel();
    QVERIFY(checks && checks->isVisible());
    QCOMPARE(checks->gives()->text(), QStringLiteral("Checked: the agent's pass of 2026-11-05 and your pass of 2026-11-05."));
    QTreeWidget* history = checks->history();
    QCOMPARE(history->topLevelItemCount(), 2);
    QCOMPARE(history->topLevelItem(0)->text(1), QStringLiteral("agent claude-opus-5-5"));
    QCOMPARE(history->topLevelItem(0)->text(0), QStringLiteral("2026-11-05 10:00"));
    QCOMPARE(history->topLevelItem(1)->text(1), QStringLiteral("Dakota"));
    for (int i = 0; i < 2; ++i) {
        QCOMPARE(history->topLevelItem(i)->text(2), QStringLiteral("pass"));
        QCOMPARE(history->topLevelItem(i)->text(3), QStringLiteral("this version"));
        QCOMPARE(history->topLevelItem(i)->text(4), QStringLiteral("counts"));
    }
    screenshot(page->currentPage(), QStringLiteral("checks-history"));

    // The record opens read-only as plain text, in a window of its own.
    auto* record = history->findChild<QLabel*>(QStringLiteral("files0"));
    QVERIFY(record);
    emit record->linkActivated(QStringLiteral("Records/2026-11/demo-0001.md"));
    QTRY_VERIFY(dialog(*window, QStringLiteral("projectText")) || page->findChild<QDialog*>(QStringLiteral("projectText")));
    auto* text = page->findChild<QDialog*>(QStringLiteral("projectText"))->findChild<QPlainTextEdit*>(QStringLiteral("text"));
    QVERIFY(text->isReadOnly());
    QCOMPARE(text->toPlainText(), QString::fromUtf8(readFile(demo.root() + QStringLiteral("/Records/2026-11/demo-0001.md"))));
    page->findChild<QDialog*>(QStringLiteral("projectText"))->close();
    QTRY_VERIFY(!page->findChild<QDialog*>(QStringLiteral("projectText")));
    // A missing one says so.
    page->openProjectText(QStringLiteral("Records/2026-11/gone.md"));
    QTRY_VERIFY(page->findChild<QDialog*>(QStringLiteral("projectText")));
    QVERIFY(page->findChild<QDialog*>(QStringLiteral("projectText"))->findChild<QPlainTextEdit*>(QStringLiteral("text"))->toPlainText()
                .contains(QStringLiteral("is missing")));
    page->findChild<QDialog*>(QStringLiteral("projectText"))->close();

    // G8TV.a: an agent's pass only.
    page->openProof(QStringLiteral("G8TV.a"));
    QCOMPARE(page->currentPage()->checksPanel()->gives()->text(), QStringLiteral("Waiting for your pass."));
    QVERIFY2(page->currentPage()->checksPanel()->passButton()->isEnabled(),
             qPrintable(page->currentPage()->checksPanel()->passButton()->toolTip()));
    QVERIFY(!page->currentPage()->checksPanel()->markFailedButton()->isVisible());
}

void ClicksTests::dakotasPassAndNeedsChanges()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = openDemo(demo.root());
    QVERIFY(window);
    StatementsPage* page = window->statementsPage();
    page->openProof(QStringLiteral("G8TV.a"));
    ChecksPanel* checks = page->currentPage()->checksPanel();
    const QString history = demo.root() + QStringLiteral("/Proofs/G8TV/G8TV.a.checks.toml");
    const QByteArray before = readFile(history);
    QCOMPARE(statusOf(*window, QStringLiteral("G8TV")), loom::Status::Unchecked);
    QSignalSpy finished(page, &StatementsPage::clickFinished);

    // Pass, cancelled: the dialog says what LoomCore says it does; nothing is written.
    QTest::mouseClick(checks->passButton(), Qt::LeftButton);
    QTRY_VERIFY(dialog<QMessageBox>(*window, QStringLiteral("checkDialog")));
    QMessageBox* box = dialog<QMessageBox>(*window, QStringLiteral("checkDialog"));
    QCOMPARE(box->text(), QStringLiteral("Record your pass of G8TV.a as it is now? With the agent's pass of 2026-11-05 this makes "
                                         "G8TV.a checked: G8TV becomes proved."));
    QCOMPARE(box->findChild<QPushButton*>(QStringLiteral("confirm"))->text(), QStringLiteral("Record pass"));
    QCOMPARE(box->defaultButton(), box->findChild<QPushButton*>(QStringLiteral("cancel")));
    // While the dialog is open the buttons wait for it.
    QVERIFY(!checks->passButton()->isEnabled());
    screenshot(box, QStringLiteral("checks-pass-dialog"));
    answer(*window, QStringLiteral("checkDialog"), QStringLiteral("cancel"));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(1).toBool(), false);
    QCOMPARE(readFile(history), before);

    // Pass, confirmed: recorded as `loom add-check --by dakota` would, and the status shows everywhere.
    QTest::mouseClick(checks->passButton(), Qt::LeftButton);
    answer(*window, QStringLiteral("checkDialog"), QStringLiteral("confirm"));
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(finished.at(1).at(1).toBool(), true);
    const loom::ChecksReading reading = loom::readChecks(readFile(history), QStringLiteral("Proofs/G8TV/G8TV.a.checks.toml"));
    QCOMPARE(static_cast<int>(reading.checks.size()), 2);
    QVERIFY(reading.problems.empty());
    QCOMPARE(reading.checks[1].by, QStringLiteral("dakota"));
    QCOMPARE(reading.checks[1].verdict, QStringLiteral("pass"));
    QVERIFY(readFile(history).startsWith(before));
    QTRY_COMPARE(statusOf(*window, QStringLiteral("G8TV")), loom::Status::Proved);
    QTRY_VERIFY(rowTip(page, QStringLiteral("G8TV")).endsWith(QStringLiteral("\nproved")));
    checks = page->currentPage()->checksPanel();
    QTRY_VERIFY(checks->gives()->text().startsWith(QStringLiteral("Checked: the agent's pass of 2026-11-05 and your pass of ")));
    QTRY_VERIFY(plain(page->currentPage()->statusLine()).contains(QStringLiteral("G8TV proved")));
    QCOMPARE(checks->history()->topLevelItemCount(), 2);
    QTRY_VERIFY(checks->passButton()->isEnabled());

    // Needs changes: the text is required; cancelled writes nothing.
    const QByteArray passed = readFile(history);
    QTest::mouseClick(checks->needsChangesButton(), Qt::LeftButton);
    QTRY_VERIFY(dialog(*window, QStringLiteral("needsChangesDialog")));
    QDialog* ask = dialog(*window, QStringLiteral("needsChangesDialog"));
    QCOMPARE(ask->findChild<QLabel*>(QStringLiteral("text"))->text(),
             QStringLiteral("Record your needs-changes of G8TV.a as it is now? This makes G8TV.a no longer checked: G8TV becomes unchecked."));
    QPushButton* confirm = ask->findChild<QPushButton*>(QStringLiteral("confirm"));
    QVERIFY(!confirm->isEnabled());
    answer(*window, QStringLiteral("needsChangesDialog"), QStringLiteral("cancel"));
    QTRY_COMPARE(finished.count(), 3);
    QCOMPARE(readFile(history), passed);

    // Confirmed with its text: the proof drops back, and the line says why.
    QTest::mouseClick(checks->needsChangesButton(), Qt::LeftButton);
    QTRY_VERIFY(dialog(*window, QStringLiteral("needsChangesDialog")));
    ask = dialog(*window, QStringLiteral("needsChangesDialog"));
    QTest::keyClicks(ask->findChild<QPlainTextEdit*>(QStringLiteral("changes")), QStringLiteral("Say which limit theorem step 2 uses."));
    QVERIFY(ask->findChild<QPushButton*>(QStringLiteral("confirm"))->isEnabled());
    screenshot(ask, QStringLiteral("checks-needs-changes-dialog"));
    answer(*window, QStringLiteral("needsChangesDialog"), QStringLiteral("confirm"));
    QTRY_COMPARE(finished.count(), 4);
    QVERIFY(finished.at(3).at(1).toBool());
    QVERIFY(readFile(history).contains("changes = \"Say which limit theorem step 2 uses.\""));
    QTRY_COMPARE(statusOf(*window, QStringLiteral("G8TV")), loom::Status::Unchecked);
    QTRY_VERIFY(rowTip(page, QStringLiteral("G8TV")).endsWith(QStringLiteral("\nunchecked")));
    checks = page->currentPage()->checksPanel();
    QTRY_VERIFY(checks->gives()->text().startsWith(QStringLiteral("Not checked: you asked for changes (needs-changes, ")));
    QTRY_VERIFY(plain(page->currentPage()->statusLine()).contains(QStringLiteral("G8TV unchecked")));
    // The changes show in full under their entry.
    QTreeWidgetItem* last = checks->history()->topLevelItem(2);
    QCOMPARE(last->childCount(), 1);
    QCOMPARE(last->child(0)->text(0), QStringLiteral("Say which limit theorem step 2 uses."));
}

void ClicksTests::failThenMarkFailed()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = openDemo(demo.root());
    QVERIFY(window);
    StatementsPage* page = window->statementsPage();
    page->openProof(QStringLiteral("G8TV.a"));
    ChecksPanel* checks = page->currentPage()->checksPanel();
    QSignalSpy finished(page, &StatementsPage::clickFinished);

    // Fail: the proof stops supporting G8TV, which has no other proof.
    QTest::mouseClick(checks->failButton(), Qt::LeftButton);
    QTRY_VERIFY(dialog<QMessageBox>(*window, QStringLiteral("checkDialog")));
    QCOMPARE(dialog<QMessageBox>(*window, QStringLiteral("checkDialog"))->text(),
             QStringLiteral("Record your fail of G8TV.a as it is now? This makes G8TV.a stop supporting G8TV: G8TV becomes open."));
    answer(*window, QStringLiteral("checkDialog"), QStringLiteral("confirm"));
    QTRY_COMPARE(finished.count(), 1);
    QTRY_COMPARE(statusOf(*window, QStringLiteral("G8TV")), loom::Status::Open);

    // A fail doesn't mark the proof failed; the page then offers it, as a click of its own.
    const QString path = demo.root() + QStringLiteral("/Proofs/G8TV/G8TV.a.tex");
    QVERIFY(readFile(path).contains("level = \"complete\""));
    checks = page->currentPage()->checksPanel();
    QTRY_VERIFY(checks->gives()->text().startsWith(QStringLiteral("Doesn't support G8TV: you say it's wrong (fail, ")));
    QTRY_VERIFY(checks->markFailedButton()->isVisible());
    QTRY_VERIFY(checks->markFailedButton()->isEnabled());
    const QByteArray before = readFile(path);
    QTest::mouseClick(checks->markFailedButton(), Qt::LeftButton);
    QTRY_VERIFY(dialog<QMessageBox>(*window, QStringLiteral("markFailedDialog")));
    QVERIFY(dialog<QMessageBox>(*window, QStringLiteral("markFailedDialog"))->text().startsWith(QStringLiteral("Mark G8TV.a failed?")));
    answer(*window, QStringLiteral("markFailedDialog"), QStringLiteral("cancel"));
    QTRY_COMPARE(page->currentPage()->proofForm()->level()->currentData().toString(), QStringLiteral("complete"));
    QCOMPARE(readFile(path), before);
    QVERIFY(!page->hasUnsavedEdits());

    QSignalSpy saved(page, &StatementsPage::saved);
    QTest::mouseClick(page->currentPage()->checksPanel()->markFailedButton(), Qt::LeftButton);
    answer(*window, QStringLiteral("markFailedDialog"), QStringLiteral("confirm"));
    QTRY_COMPARE(saved.count(), 1);
    QVERIFY(readFile(path).contains("level = \"failed\""));
    QTRY_VERIFY(page->currentPage()->checksPanel()->gives()->text() == QStringLiteral("Marked failed: it doesn't support G8TV."));
    QTRY_VERIFY(!page->currentPage()->checksPanel()->markFailedButton()->isVisible());
}

void ClicksTests::clicksWaitForUnsavedEdits()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = openDemo(demo.root());
    QVERIFY(window);
    StatementsPage* page = window->statementsPage();

    // "Disabled, with the reason on hover, while the proof's page has unsaved edits."
    page->openProof(QStringLiteral("G8TV.a"));
    FilePage* proof = page->currentPage();
    ChecksPanel* checks = proof->checksPanel();
    QTRY_VERIFY(checks->passButton()->isEnabled());
    proof->editor()->setText(proof->editor()->text() + QStringLiteral("\nOne more line.\n"));
    QTRY_VERIFY(!checks->passButton()->isEnabled());
    QCOMPARE(checks->passButton()->toolTip(), QStringLiteral("Save first: a check records the version on disk."));
    QVERIFY(!checks->failButton()->isEnabled());
    QVERIFY(!checks->needsChangesButton()->isEnabled());

    // Refute… waits for the statement's unsaved edits too.
    page->openStatement(QStringLiteral("K3AB"));
    FilePage* statement = page->currentPage();
    QVERIFY(statement->statementMenu()->isVisible());
    QVERIFY(statement->refuteAction()->isEnabled());
    statement->editor()->setText(QStringLiteral("A different body.\n"));
    QTRY_VERIFY(!statement->refuteAction()->isEnabled());
    QCOMPARE(statement->refuteAction()->toolTip(), QStringLiteral("Save first: refuting rewrites this statement's file."));
}

void ClicksTests::aProofChangedUnderTheDialog()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = openDemo(demo.root());
    QVERIFY(window);
    StatementsPage* page = window->statementsPage();
    page->openProof(QStringLiteral("G8TV.a"));
    ChecksPanel* checks = page->currentPage()->checksPanel();
    const QString history = demo.root() + QStringLiteral("/Proofs/G8TV/G8TV.a.checks.toml");
    const QByteArray before = readFile(history);
    QSignalSpy finished(page, &StatementsPage::clickFinished);

    QTest::mouseClick(checks->passButton(), Qt::LeftButton);
    QTRY_VERIFY(dialog<QMessageBox>(*window, QStringLiteral("checkDialog")));
    // Another editor changes the proof while Dakota reads the dialog.
    const QString proofPath = demo.root() + QStringLiteral("/Proofs/G8TV/G8TV.a.tex");
    QVERIFY(writeFile(proofPath, readFile(proofPath) + "A remark added elsewhere.\n"));
    answer(*window, QStringLiteral("checkDialog"), QStringLiteral("confirm"));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(finished.at(0).at(1).toBool(), false);
    QCOMPARE(readFile(history), before);

    // It says so, and offers the new version.
    QTRY_VERIFY(dialog<QMessageBox>(*window, QStringLiteral("proofChanged")));
    QVERIFY(dialog<QMessageBox>(*window, QStringLiteral("proofChanged"))->text().startsWith(QStringLiteral("G8TV.a changed since you opened it")));
    answer(*window, QStringLiteral("proofChanged"), QStringLiteral("showNew"));
    QTRY_COMPARE(page->currentPage()->proofId(), QStringLiteral("G8TV.a"));
    QTRY_VERIFY(page->currentPage()->editor()->text().contains(QStringLiteral("A remark added elsewhere.")));
}

void ClicksTests::refutingAsksFirst()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = openDemo(demo.root());
    QVERIFY(window);
    StatementsPage* page = window->statementsPage();
    page->openStatement(QStringLiteral("K3AB"));
    FilePage* statement = page->currentPage();
    const QString path = demo.root() + QStringLiteral("/Statements/K3AB.tex");
    const QByteArray before = readFile(path);
    QSignalSpy finished(page, &StatementsPage::clickFinished);

    // Refute… opens the picker for proofs of other statements.
    const auto pickProof = [&](const QString& id) {
        statement->refuteAction()->trigger();
        QTRY_VERIFY(page->idTools()->openPicker());
        loom::IdPicker* picker = page->idTools()->openPicker();
        const QStringList shown = picker->shownIds();
        QVERIFY(!shown.isEmpty());
        for (const QString& offered : shown)
            QVERIFY2(offered.contains(u'.') && !offered.startsWith(QStringLiteral("K3AB.")), qPrintable(offered));
        picker->search()->setText(id);
        QCOMPARE(picker->shownIds().value(0), id);
        QVERIFY(picker->choose());
    };

    // Cancelled: nothing written.
    pickProof(QStringLiteral("G8TV.a"));
    QTRY_VERIFY(dialog<QMessageBox>(*window, QStringLiteral("refuteDialog")));
    QMessageBox* box = dialog<QMessageBox>(*window, QStringLiteral("refuteDialog"));
    QCOMPARE(box->text(), QStringLiteral("Mark K3AB false, refuted by G8TV.a? Today's date is written as the date you accepted it. "
                                         "K3AB becomes false."));
    QCOMPARE(box->findChild<QPushButton*>(QStringLiteral("confirm"))->text(), QStringLiteral("Mark false"));
    QCOMPARE(box->defaultButton(), box->findChild<QPushButton*>(QStringLiteral("cancel")));
    screenshot(box, QStringLiteral("checks-refute-dialog"));
    answer(*window, QStringLiteral("refuteDialog"), QStringLiteral("cancel"));
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(readFile(path), before);
    QCOMPARE(statusOf(*window, QStringLiteral("K3AB")), loom::Status::Open);

    // Confirmed: written as `loom refute K3AB --by G8TV.a` would, and K3AB is false everywhere.
    pickProof(QStringLiteral("G8TV.a"));
    answer(*window, QStringLiteral("refuteDialog"), QStringLiteral("confirm"));
    QTRY_COMPARE(finished.count(), 2);
    QVERIFY(finished.at(1).at(1).toBool());
    const QByteArray after = readFile(path);
    QVERIFY(after.contains("refuted_by = \"G8TV.a\""));
    QVERIFY(after.contains("refuted_on = " + QDate::currentDate().toString(Qt::ISODate).toUtf8()));
    QTRY_COMPARE(statusOf(*window, QStringLiteral("K3AB")), loom::Status::False);
    QTRY_VERIFY(rowTip(page, QStringLiteral("K3AB")).endsWith(QStringLiteral("\nfalse")));
    QTRY_VERIFY(plain(page->currentPage()->statusLine()).contains(QStringLiteral("K3AB false")));
    // Already refuted: Refute… says so instead.
    QTRY_VERIFY(!page->currentPage()->refuteAction()->isEnabled());
    QVERIFY(page->currentPage()->refuteAction()->toolTip().startsWith(QStringLiteral("Already refuted by G8TV.a")));
}

QTEST_MAIN(ClicksTests)
#include "ClicksTests.moc"
