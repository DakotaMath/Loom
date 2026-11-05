// Compiling, the PDF pane and the Problems panel (Specs/m2-statements-review.md, "The PDF pane" and
// "The Problems panel and the Output view"; Verification, "Compile and PDF": "Ctrl+S builds; the
// last good PDF dimmed on an error; a problem click lands on the line"). Each test drives the real
// widgets on a copy of the demo project. The cases that build need TeX: they skip without latexmk
// (QSKIP, "latexmk not found") and fail instead under LOOM_REQUIRE_TEX=1, as on Linux CI; the
// others run anywhere. What a build does is LoomCore's and tested there; these check the wiring.

#include "LoomApp/AppSettings.h"
#include "LoomApp/BottomPanel.h"
#include "LoomApp/EditBuffer.h"
#include "LoomApp/FilePage.h"
#include "LoomApp/LatexEditor.h"
#include "LoomApp/MainWindow.h"
#include "LoomApp/PdfPane.h"
#include "LoomApp/ProjectSession.h"
#include "LoomApp/StatementsPage.h"

#include "LoomCore/SyncTex.h"
#include "LoomCore/BuildDocument.h"

#include "DemoProject.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPdfDocument>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>

#include <memory>

using loom::BottomPanel;
using loom::BuildMode;
using loom::BuildOutcome;
using loom::MainWindow;
using loom::OutputChannel;
using loom::PdfPane;
using loom::StatementsPage;

namespace {

/// How long a test waits for one build at most.
constexpr int buildWaitMs = 120'000;

/// Skips a test that builds when latexmk isn't found, unless LOOM_REQUIRE_TEX=1 asks for TeX.
#define REQUIRE_TEX()                                                                                                        \
    do {                                                                                                                     \
        if (QStandardPaths::findExecutable(QStringLiteral("latexmk")).isEmpty()) {                                         \
            if (qEnvironmentVariableIntValue("LOOM_REQUIRE_TEX") == 1)                                                       \
                QFAIL("latexmk not found, and LOOM_REQUIRE_TEX=1");                                                          \
            QSKIP("latexmk not found");                                                                                      \
        }                                                                                                                    \
    } while (false)

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

/// Presses a window shortcut. Shortcuts need the window active, and while tests run in parallel
/// another test's window can take that, so it is made active first.
/// @returns Whether the window could be made active.
bool press(QWidget* window, Qt::Key key, Qt::KeyboardModifiers modifiers = {})
{
    window->activateWindow();
    if (!QTest::qWaitForWindowActive(window))
        return false;
    QTest::keyClick(window, key, modifiers);
    return true;
}

/// Whether the pane's latest build is of this target and mode, and finished.
bool builtAs(const PdfPane* pane, const QString& target, BuildMode mode = BuildMode::Alone, bool verbose = false)
{
    return !pane->building() && pane->hasResult() && pane->result().target == target && pane->result().mode == mode
        && pane->result().verbose == verbose;
}

} // namespace

/// Qt Test cases for compiling and the PDF pane.
class CompileTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void ctrlSBuildsAndShowsThePdf();
    void anErrorKeepsTheLastGoodPdfDimmed();
    void modesAndVerboseAreRememberedPerStatement();
    void synctexBothWays();
    void aBrokenFrontMatterKeepsTheLastGoodPdf();
    void errorsOnlyWithoutAGoodBuildAndClearedOnLeaving();
    void aNewerBuildReplacesTheRunningOne();
    void opensAtTheStatementsPage();
    void zoomAndScrollAreKept();
    void findInThePdf();
    void compileOnSaveOffOnlySaves();
    void withoutLatexmkThePaneSaysSo();
    void aPaperResultIsntBuilt();
    void problemsNameFilesAndF8Steps();

private:
    /// A new window on the test's settings, shown.
    std::unique_ptr<MainWindow> newWindow();
    /// Opens the demo in a window and waits until the tab has offered its recovery files.
    bool openDemo(MainWindow& window, const QString& root);

    std::unique_ptr<QTemporaryDir> m_temp;
    std::unique_ptr<loom::AppSettings> m_settings;
    QTemporaryDir m_gitHome;
};

void CompileTests::initTestCase()
{
    QVERIFY(m_gitHome.isValid());
    const QString config = m_gitHome.filePath(QStringLiteral("gitconfig"));
    QVERIFY(writeFile(config, "[user]\n\tname = Loom Test\n\temail = test@example.com\n"));
    qputenv("GIT_CONFIG_GLOBAL", QDir::toNativeSeparators(config).toLocal8Bit());
    qputenv("GIT_CONFIG_NOSYSTEM", "1");
}

void CompileTests::init()
{
    m_temp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_temp->isValid());
    m_settings = std::make_unique<loom::AppSettings>(pathsIn(*m_temp));
}

void CompileTests::cleanup()
{
    m_settings.reset();
    m_temp.reset();
}

std::unique_ptr<MainWindow> CompileTests::newWindow()
{
    auto window = std::make_unique<MainWindow>(m_settings.get());
    window->resize(1500, 900);
    window->show();
    return window;
}

bool CompileTests::openDemo(MainWindow& window, const QString& root)
{
    QSignalSpy offered(window.statementsPage(), &StatementsPage::recoveryOffered);
    QSignalSpy finished(&window, &MainWindow::openFinished);
    window.openFolder(root);
    if (finished.count() == 0 && !finished.wait(30'000))
        return false;
    return offered.count() > 0 || offered.wait(30'000);
}

void CompileTests::ctrlSBuildsAndShowsThePdf()
{
    REQUIRE_TEX();
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    PdfPane* pdf = page->pdfPane();

    // Opening a statement shows its build. (K3AB has no proofs, so editing it outdates no checks.)
    page->openStatement(QStringLiteral("K3AB"));
    QCOMPARE(pdf->target(), QStringLiteral("K3AB"));
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("K3AB")), buildWaitMs);
    QCOMPARE(pdf->result().outcome, BuildOutcome::Succeeded);
    QTRY_COMPARE(pdf->document()->status(), QPdfDocument::Status::Ready);
    QVERIFY(pdf->document()->pageCount() >= 1);
    QVERIFY(!pdf->dimmed());
    QVERIFY(pdf->stateLabel()->text().startsWith(QStringLiteral("Built ")));
    auto* buildState = window->statusBar()->findChild<QLabel*>(QStringLiteral("buildState"));
    QVERIFY(buildState->isVisible());
    QVERIFY(buildState->text().startsWith(QStringLiteral("Built ")));

    // The PDF is read into memory, never held open: the next build can overwrite it.
    const QString mainPdf = demo.root() + u'/' + pdf->result().pdf;
    QVERIFY(QFileInfo::exists(mainPdf));
    {
        QFile file(mainPdf);
        QVERIFY(file.open(QIODevice::ReadWrite)); // Would fail on Windows if the view held it.
    }

    // Ctrl+S saves and compiles.
    page->currentPage()->editor()->setFocus();
    page->currentPage()->editor()->setCursorPosition(0, 0);
    QTest::keyClicks(page->currentPage()->editor(), QStringLiteral("Indeed: "));
    QSignalSpy saved(page, &StatementsPage::saved);
    QSignalSpy started(pdf, &PdfPane::buildStarted);
    QSignalSpy finished(pdf, &PdfPane::buildFinished);
    QVERIFY(press(window.get(), Qt::Key_S, Qt::ControlModifier));
    QTRY_COMPARE(saved.count(), 1);
    QTRY_COMPARE(started.count(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 1, buildWaitMs);
    QCOMPARE(pdf->result().outcome, BuildOutcome::Succeeded);
    QVERIFY(!pdf->result().reused);
    QVERIFY(readFile(demo.root() + QStringLiteral("/") + pdf->result().folder + QStringLiteral("/src/Statements/K3AB.tex")).contains("Indeed: "));
    // Our own save isn't a change on disk: the snapshot it brings starts no second build.
    QTest::qWait(1000);
    QCOMPARE(started.count(), 1);

    // Output's Build channel has the build, its command and its log.
    const QString output = window->bottomPanel()->output(OutputChannel::Build)->toPlainText();
    QVERIFY(output.contains(QStringLiteral("Built K3AB (alone) in ")));
    QVERIFY(output.contains(QStringLiteral("latexmk -norc -pdf")));
    QVERIFY(output.contains(QStringLiteral("main.log:")));

    // Compile (Ctrl+B) builds the saved files again; unchanged, the build is reused.
    finished.clear();
    started.clear();
    QVERIFY(press(window.get(), Qt::Key_B, Qt::ControlModifier));
    QTRY_COMPARE(started.count(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 1, buildWaitMs);
    QVERIFY(pdf->result().reused);
    // A reused build keeps the time and length of the build it reuses.
    QVERIFY(!pdf->stateLabel()->text().endsWith(QStringLiteral(" in 0.0 s")));
    QTest::qWait(1000); // Qt PDF renders pages in the background.
    screenshot(window.get(), QStringLiteral("compile-pdf"));
}

void CompileTests::anErrorKeepsTheLastGoodPdfDimmed()
{
    REQUIRE_TEX();
    loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    PdfPane* pdf = page->pdfPane();
    page->openStatement(QStringLiteral("K3AB"));
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("K3AB")), buildWaitMs);
    QCOMPARE(pdf->result().outcome, BuildOutcome::Succeeded);
    const QByteArray good = readFile(demo.root() + QStringLiteral("/Statements/K3AB.tex"));

    // A file changed on disk that's in the shown build compiles it again: here, an unclosed brace.
    QSignalSpy finished(pdf, &PdfPane::buildFinished);
    QVERIFY(demo.applyOverlay(QStringLiteral("latex-error")).isEmpty());
    window->session()->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 1, buildWaitMs);
    QCOMPARE(pdf->result().outcome, BuildOutcome::Failed);

    // The last good PDF stays, dimmed, under the bar.
    QVERIFY(pdf->dimmed());
    QCOMPARE(pdf->document()->status(), QPdfDocument::Status::Ready);
    QVERIFY(pdf->lastGoodBar()->isVisible());
    const QString bar = pdf->lastGoodBar()->findChild<QLabel*>(QStringLiteral("lastGoodText"))->text();
    QVERIFY2(bar.startsWith(QStringLiteral("Showing the last good build (")), qPrintable(bar));
    QVERIFY2(bar.endsWith(QStringLiteral("). 1 error: fix them to update it.")), qPrintable(bar));
    QCOMPARE(window->statusBar()->findChild<QLabel*>(QStringLiteral("buildState"))->text(), QStringLiteral("1 error"));

    // The error is in the Problems panel, named by ID and title, at its line.
    QVERIFY(window->bottomPanel()->isVisible());
    QTreeWidget* problems = window->bottomPanel()->problems();
    QTreeWidgetItem* error = nullptr;
    for (int i = 0; i < problems->topLevelItemCount(); ++i) {
        if (problems->topLevelItem(i)->data(BottomPanel::SeverityColumn, Qt::UserRole) == QStringLiteral("tex-error"))
            error = problems->topLevelItem(i);
    }
    QVERIFY(error);
    QCOMPARE(error->text(BottomPanel::SeverityColumn), QStringLiteral("error"));
    QCOMPARE(error->text(BottomPanel::FileColumn), QStringLiteral("K3AB · A tail bound for sums"));
    QCOMPARE(error->text(BottomPanel::LineColumn), QStringLiteral("13"));
    QCOMPARE(error->text(BottomPanel::MessageColumn), QStringLiteral("Missing } inserted."));
    QTest::qWait(1000);
    screenshot(window.get(), QStringLiteral("compile-error-dimmed"));

    // Clicking it lands on the line: file line 13 is the body's fifth line, in the form. (The
    // list is rebuilt with each snapshot, so the row is found again first.)
    page->currentPage()->editor()->setCursorPosition(0, 0);
    error = nullptr;
    for (int i = 0; i < problems->topLevelItemCount(); ++i) {
        if (problems->topLevelItem(i)->data(BottomPanel::SeverityColumn, Qt::UserRole) == QStringLiteral("tex-error"))
            error = problems->topLevelItem(i);
    }
    QVERIFY(error);
    problems->scrollToItem(error);
    QTest::mouseClick(problems->viewport(), Qt::LeftButton, {}, problems->visualItemRect(error).center());
    QCOMPARE(page->openTag(), QStringLiteral("K3AB"));
    QVERIFY(!page->currentPage()->rawToggle()->isChecked());
    QCOMPARE(page->currentPage()->editor()->cursorLine(), 5);
    // The editor marks the line too.
    QTRY_VERIFY(page->currentPage()->editor()->problemLines().contains(5));

    // Fixed on disk: the PDF updates and isn't dimmed.
    finished.clear();
    QVERIFY(writeFile(demo.root() + QStringLiteral("/Statements/K3AB.tex"), good));
    window->session()->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 1 && pdf->result().outcome == BuildOutcome::Succeeded, buildWaitMs);
    QVERIFY(!pdf->dimmed());
    QVERIFY(pdf->lastGoodBar()->isHidden());
}

void CompileTests::modesAndVerboseAreRememberedPerStatement()
{
    REQUIRE_TEX();
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    PdfPane* pdf = page->pdfPane();
    page->openStatement(QStringLiteral("E6PQ"));
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("E6PQ")), buildWaitMs);
    QVERIFY(pdf->modeButton(BuildMode::Alone)->isChecked());

    QTest::mouseClick(pdf->modeButton(BuildMode::Premises), Qt::LeftButton);
    QVERIFY(pdf->modeButton(BuildMode::Premises)->isChecked());
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("E6PQ"), BuildMode::Premises), buildWaitMs);
    QCOMPARE(pdf->result().folder, QStringLiteral(".loom/build/E6PQ/premises"));
    QCOMPARE(pdf->result().outcome, BuildOutcome::Succeeded);

    QTest::mouseClick(pdf->modeButton(BuildMode::Chain), Qt::LeftButton);
    QTest::mouseClick(pdf->verboseButton(), Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("E6PQ"), BuildMode::Chain, true), buildWaitMs);
    QCOMPARE(pdf->result().folder, QStringLiteral(".loom/build/E6PQ/chain-verbose"));
    QCOMPARE(pdf->result().outcome, BuildOutcome::Succeeded);

    // A proof's page builds the proof, in its statement's mode.
    page->openProof(QStringLiteral("E6PQ.a"));
    QCOMPARE(pdf->target(), QStringLiteral("E6PQ.a"));
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("E6PQ.a"), BuildMode::Chain, true), buildWaitMs);

    // Another statement opens in the default mode; coming back restores E6PQ's.
    page->openStatement(QStringLiteral("C4JK"));
    QVERIFY(pdf->modeButton(BuildMode::Alone)->isChecked());
    QVERIFY(!pdf->verboseButton()->isChecked());
    page->openStatement(QStringLiteral("E6PQ"));
    QVERIFY(pdf->modeButton(BuildMode::Chain)->isChecked());
    QVERIFY(pdf->verboseButton()->isChecked());
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("E6PQ"), BuildMode::Chain, true), buildWaitMs);
}

void CompileTests::synctexBothWays()
{
    REQUIRE_TEX();
    if (QStandardPaths::findExecutable(QStringLiteral("synctex")).isEmpty()) {
        if (qEnvironmentVariableIntValue("LOOM_REQUIRE_TEX") == 1)
            QFAIL("synctex not found, and LOOM_REQUIRE_TEX=1");
        QSKIP("synctex not found");
    }
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    PdfPane* pdf = page->pdfPane();
    page->openProof(QStringLiteral("D5MN.a"));
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("D5MN.a")), buildWaitMs);
    QTRY_COMPARE(pdf->document()->status(), QPdfDocument::Status::Ready);

    // Ctrl+J shows the cursor's place in the PDF and marks it.
    page->currentPage()->editor()->setCursorPosition(0, 2);
    page->showCursorInPdf();
    QTRY_VERIFY(pdf->view()->marked());

    // Double-clicking D5MN's text opens its statement page at that line.
    loom::SyncTexBox box;
    QCOMPARE(loom::synctexForward(loom::SyncTexTool(), demo.root() + u'/' + pdf->result().folder, QStringLiteral("Statements/D5MN.tex"), 9, 0, box),
             loom::SyncTexOutcome::Found);
    QTest::qWait(100);
    const QRectF rect = pdf->view()->pageRect(box.page - 1);
    QVERIFY(!rect.isEmpty());
    const qreal scale = rect.width() / pdf->document()->pagePointSize(box.page - 1).width();
    const QPoint at = (rect.topLeft() + QPointF(box.x + box.width / 2, box.y + box.height / 2) * scale).toPoint();
    QVERIFY2(pdf->view()->viewport()->rect().contains(at), "the box is out of view");
    int clickedPage = -1;
    QPointF point;
    QVERIFY(pdf->view()->pointAt(at, clickedPage, point));
    QCOMPARE(clickedPage, box.page - 1);
    QTest::mouseDClick(pdf->view()->viewport(), Qt::LeftButton, {}, at);
    QTRY_COMPARE(page->openTag(), QStringLiteral("D5MN"));
    QTRY_VERIFY(page->currentPage()->proofId().isEmpty());
    QCOMPARE(page->currentPage()->editor()->cursorLine(), 1);
}

void CompileTests::compileOnSaveOffOnlySaves()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    m_settings->setCompileOnSave(false);
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    PdfPane* pdf = page->pdfPane();
    page->openStatement(QStringLiteral("J2YZ"));
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("J2YZ")), buildWaitMs);

    QSignalSpy started(pdf, &PdfPane::buildStarted);
    QSignalSpy saved(page, &StatementsPage::saved);
    page->currentPage()->editor()->setCursorPosition(0, 0);
    QTest::keyClicks(page->currentPage()->editor(), QStringLiteral("So: "));
    page->save();
    QTRY_COMPARE(saved.count(), 1);
    QTest::qWait(1500); // The save's snapshot arrives; the build would start after 300 ms.
    QCOMPARE(started.count(), 0);
}

void CompileTests::withoutLatexmkThePaneSaysSo()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    m_settings->setLatexmkPath(m_temp->filePath(QStringLiteral("no-such-latexmk")));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    PdfPane* pdf = page->pdfPane();
    page->openStatement(QStringLiteral("D5MN"));
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("D5MN")), buildWaitMs);
    QCOMPARE(pdf->result().outcome, BuildOutcome::CantWork);
    QCOMPARE(pdf->result().code, QStringLiteral("tex-missing"));
    QVERIFY(pdf->messageLabel()->isVisible());
    QVERIFY(pdf->messageLabel()->text().contains(QStringLiteral("Loom couldn't run `latexmk`")));
    QCOMPARE(pdf->stateLabel()->text(), QStringLiteral("Not built"));
    QVERIFY(window->bottomPanel()->output(OutputChannel::Build)->toPlainText().contains(QStringLiteral("Didn't build D5MN (alone): ")));
    // Compile is enabled again once the build is done.
    QVERIFY(pdf->compileButton()->isEnabled());
    screenshot(window.get(), QStringLiteral("compile-no-latexmk"));
}

void CompileTests::aPaperResultIsntBuilt()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    PdfPane* pdf = page->pdfPane();
    QSignalSpy started(pdf, &PdfPane::buildStarted);
    page->openStub(QStringLiteral("7RT2"));
    QVERIFY(pdf->messageLabel()->text().contains(QStringLiteral("it's built only as part of a statement's chain")));
    QVERIFY(!pdf->compileButton()->isEnabled());
    QVERIFY(!pdf->modeButton(BuildMode::Chain)->isEnabled());
    QCOMPARE(started.count(), 0);
}

void CompileTests::problemsNameFilesAndF8Steps()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    BottomPanel* panel = window->bottomPanel();
    QCOMPARE(panel->fileLabel(QStringLiteral("Statements/C4JK.tex")), QStringLiteral("C4JK · Tails of tame sequences vanish"));
    QCOMPARE(panel->fileLabel(QStringLiteral("Proofs/E6PQ/E6PQ.a.tex")), QStringLiteral("E6PQ.a · Products with bounded sequences stay tame"));
    QCOMPARE(panel->fileLabel(QStringLiteral("Proofs/E6PQ/E6PQ.a.verbose.tex")),
             QStringLiteral("E6PQ.a (verbose) · Products with bounded sequences stay tame"));
    QCOMPARE(panel->fileLabel(QStringLiteral("TeX/macros.tex")), QStringLiteral("TeX/macros.tex"));

    // A build's problems are listed; F8 and Shift+F8 go round them, opening each.
    loom::Problem first;
    first.severity = loom::Severity::Warning;
    first.code = QStringLiteral("tex-warning");
    first.file = QStringLiteral("Statements/C4JK.tex");
    first.line = 9;
    first.message = QStringLiteral("A made-up warning.");
    loom::Problem second = first;
    second.file = QStringLiteral("Proofs/D5MN/D5MN.a.tex");
    second.line = 10;
    // As validation problems: opening D5MN.a changes the PDF's target, which clears build problems.
    panel->setProblems({first, second});
    QCOMPARE(panel->problems()->topLevelItemCount(), 2);
    QSignalSpy activated(panel, &BottomPanel::problemActivated);
    QVERIFY(press(window.get(), Qt::Key_F8));
    QCOMPARE(activated.count(), 1);
    QCOMPARE(activated.at(0).at(0).toString(), QStringLiteral("Statements/C4JK.tex"));
    QCOMPARE(window->statementsPage()->openTag(), QStringLiteral("C4JK"));
    QVERIFY(press(window.get(), Qt::Key_F8));
    QCOMPARE(activated.count(), 2);
    QCOMPARE(activated.at(1).at(0).toString(), QStringLiteral("Proofs/D5MN/D5MN.a.tex"));
    QCOMPARE(window->statementsPage()->currentPage()->proofId(), QStringLiteral("D5MN.a"));
    // Rebuilding the list keeps the selected problem selected, so F8 goes on from it.
    panel->setSaveProblems({});
    QCOMPARE(panel->problems()->currentItem()->data(BottomPanel::FileColumn, Qt::UserRole).toString(), QStringLiteral("Proofs/D5MN/D5MN.a.tex"));
    QVERIFY(press(window.get(), Qt::Key_F8, Qt::ShiftModifier));
    QCOMPARE(activated.count(), 3);
    QCOMPARE(activated.at(2).at(0).toString(), QStringLiteral("Statements/C4JK.tex"));

    // Output: Build, Git and Loom, each with Copy and Clear.
    QCOMPARE(panel->outputTabs()->count(), 3);
    panel->appendOutput(OutputChannel::Git, QStringLiteral("git status"));
    QVERIFY(panel->output(OutputChannel::Git)->toPlainText().endsWith(QStringLiteral("git status")));
    QVERIFY(!panel->output(OutputChannel::Loom)->toPlainText().contains(QStringLiteral("git status")));
    auto* clear = panel->output(OutputChannel::Git)->parentWidget()->findChild<QToolButton*>(QStringLiteral("clear"));
    QVERIFY(clear);
    clear->click();
    QVERIFY(panel->output(OutputChannel::Git)->toPlainText().isEmpty());
}

void CompileTests::aBrokenFrontMatterKeepsTheLastGoodPdf()
{
    REQUIRE_TEX();
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    PdfPane* pdf = page->pdfPane();
    page->openStatement(QStringLiteral("K3AB"));
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("K3AB")), buildWaitMs);
    QCOMPARE(pdf->result().outcome, BuildOutcome::Succeeded);

    // An agent breaks the front matter: the build can't read it, and the last good PDF stays.
    const QString file = demo.root() + QStringLiteral("/Statements/K3AB.tex");
    const QByteArray good = readFile(file);
    QByteArray broken = good;
    broken.replace("kind = \"lemma\"", "kind = \"lemma");
    QVERIFY(broken != good);
    QSignalSpy finished(pdf, &PdfPane::buildFinished);
    QVERIFY(writeFile(file, broken));
    window->session()->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 1, buildWaitMs);
    QVERIFY2(pdf->result().outcome == BuildOutcome::Failed, qPrintable(pdf->result().code + u' ' + pdf->result().message));
    QVERIFY(pdf->dimmed());
    QCOMPARE(pdf->document()->status(), QPdfDocument::Status::Ready);
    QVERIFY(pdf->lastGoodBar()->isVisible());

    // Fixing it on disk compiles again.
    finished.clear();
    QVERIFY(writeFile(file, good));
    window->session()->refresh();
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 1 && pdf->result().outcome == BuildOutcome::Succeeded, buildWaitMs);
    QVERIFY(!pdf->dimmed());
}

void CompileTests::errorsOnlyWithoutAGoodBuildAndClearedOnLeaving()
{
    REQUIRE_TEX();
    const loomtest::DemoProject demo(QStringLiteral("latex-error"));
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    PdfPane* pdf = page->pdfPane();

    // K3AB has never built: "With no good build yet: the errors only."
    page->openStatement(QStringLiteral("K3AB"));
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("K3AB")), buildWaitMs);
    QCOMPARE(pdf->result().outcome, BuildOutcome::Failed);
    QVERIFY(!pdf->dimmed());
    QVERIFY(pdf->messageLabel()->isVisible());
    QVERIFY2(pdf->messageLabel()->text().startsWith(QStringLiteral("No PDF yet: 1 error.")), qPrintable(pdf->messageLabel()->text()));
    QVERIFY(pdf->messageLabel()->text().contains(QStringLiteral("Statements/K3AB.tex:13: Missing } inserted.")));
    const auto texErrors = [&] {
        int n = 0;
        QTreeWidget* problems = window->bottomPanel()->problems();
        for (int i = 0; i < problems->topLevelItemCount(); ++i)
            n += problems->topLevelItem(i)->data(BottomPanel::SeverityColumn, Qt::UserRole) == QStringLiteral("tex-error");
        return n;
    };
    QCOMPARE(texErrors(), 1);
    QVERIFY(!page->buildProblems().empty());
    auto* buildState = window->statusBar()->findChild<QLabel*>(QStringLiteral("buildState"));
    QCOMPARE(buildState->text(), QStringLiteral("1 error"));

    // Leaving for a paper's result: K3AB's problems aren't the open statement's any more.
    page->openStub(QStringLiteral("7RT2"));
    QCOMPARE(texErrors(), 0);
    QVERIFY(page->buildProblems().empty());
    QVERIFY(buildState->isHidden());
}

void CompileTests::aNewerBuildReplacesTheRunningOne()
{
    REQUIRE_TEX();
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    PdfPane* pdf = page->pdfPane();
    page->openStatement(QStringLiteral("D5MN"));
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("D5MN")), buildWaitMs);

    QSignalSpy started(pdf, &PdfPane::buildStarted);
    QSignalSpy finished(pdf, &PdfPane::buildFinished);
    pdf->compile();
    // Busy: Compile shows a spinner and doesn't start a second build.
    QVERIFY(!pdf->compileButton()->isEnabled());
    QVERIFY(pdf->findChild<QProgressBar*>(QStringLiteral("compileSpinner"))->isVisible());
    pdf->compileClicked();
    QCOMPARE(started.count(), 1);
    // Ctrl+S (or a mode click) does: the older build's result is dropped.
    pdf->compile();
    QCOMPARE(started.count(), 2);
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 1, buildWaitMs);
    QTest::qWait(1500);
    QCOMPARE(finished.count(), 1);
    QVERIFY(!pdf->building());
    QVERIFY(pdf->compileButton()->isEnabled());
    QVERIFY(pdf->findChild<QProgressBar*>(QStringLiteral("compileSpinner"))->isHidden());
}

void CompileTests::opensAtTheStatementsPage()
{
    REQUIRE_TEX();
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    PdfPane* pdf = page->pdfPane();
    page->openStatement(QStringLiteral("E6PQ"));
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("E6PQ")), buildWaitMs);
    QTest::mouseClick(pdf->modeButton(BuildMode::Chain), Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("E6PQ"), BuildMode::Chain), buildWaitMs);
    QCOMPARE(pdf->result().outcome, BuildOutcome::Succeeded);
    // "A new document opens at the open statement's page" (from main.aux).
    const int page_ = loom::auxLabelPage(readFile(demo.root() + u'/' + pdf->result().folder + QStringLiteral("/main.aux")), QStringLiteral("E6PQ"));
    QVERIFY(page_ >= 1);
    QTRY_COMPARE(pdf->currentPage() + 1, page_);
}

void CompileTests::zoomAndScrollAreKept()
{
    REQUIRE_TEX();
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    PdfPane* pdf = window->statementsPage()->pdfPane();
    window->statementsPage()->openStatement(QStringLiteral("D5MN"));
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("D5MN")), buildWaitMs);
    QTRY_COMPARE(pdf->document()->status(), QPdfDocument::Status::Ready);
    auto* zoomIn = pdf->findChild<QToolButton*>(QStringLiteral("zoomIn"));
    for (int i = 0; i < 4; ++i)
        QTest::mouseClick(zoomIn, Qt::LeftButton);
    QCOMPARE(pdf->view()->zoomMode(), QPdfView::ZoomMode::Custom);
    const qreal zoom = pdf->view()->zoomFactor();

    // A rebuild of the same document keeps its scroll position.
    QScrollBar* bar = pdf->view()->verticalScrollBar();
    QTRY_VERIFY(bar->maximum() > 0);
    QTest::qWait(500); // The view lays the zoomed pages out again after a moment.
    bar->setValue(bar->maximum() / 2);
    const int scrolled = bar->value();
    QVERIFY(scrolled > 0);
    QTest::qWait(300);
    QCOMPARE(bar->value(), scrolled);
    QSignalSpy finished(pdf, &PdfPane::buildFinished);
    pdf->compile();
    QTRY_VERIFY_WITH_TIMEOUT(finished.count() >= 1, buildWaitMs);
    QTest::qWait(300);
    QCOMPARE(bar->value(), scrolled);

    // The zoom is kept per project.
    window.reset();
    window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    QCOMPARE(window->statementsPage()->pdfPane()->view()->zoomMode(), QPdfView::ZoomMode::Custom);
    QVERIFY(qAbs(window->statementsPage()->pdfPane()->view()->zoomFactor() - zoom) < 0.001);
}

void CompileTests::findInThePdf()
{
    REQUIRE_TEX();
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    PdfPane* pdf = window->statementsPage()->pdfPane();
    window->statementsPage()->openStatement(QStringLiteral("D5MN"));
    QTRY_VERIFY_WITH_TIMEOUT(builtAs(pdf, QStringLiteral("D5MN")), buildWaitMs);
    QTRY_COMPARE(pdf->document()->status(), QPdfDocument::Status::Ready);

    // Ctrl+F while the PDF has focus finds in the PDF.
    window->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(window.get()));
    pdf->view()->setFocus();
    QTRY_VERIFY(pdf->view()->hasFocus());
    QTest::keyClick(window.get(), Qt::Key_F, Qt::ControlModifier);
    QTRY_VERIFY(pdf->findBar()->isVisible());
    auto* text = pdf->findBar()->findChild<QLineEdit*>(QStringLiteral("pdfFindText"));
    QVERIFY(text->hasFocus());
    QTest::keyClicks(text, QStringLiteral("bounded"));
    QTest::keyClick(text, Qt::Key_Return);
    auto* count = pdf->findBar()->findChild<QLabel*>(QStringLiteral("pdfFindCount"));
    QTRY_VERIFY2(count->text().startsWith(QStringLiteral("1 of ")), qPrintable(count->text()));
}

QTEST_MAIN(CompileTests)
#include "CompileTests.moc"
