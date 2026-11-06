// The Review tab (Specs/m2-statements-review.md, "The Review tab"; Verification, "Review": "The
// `review` overlay listed and grouped; before and after; plain words; status changes; raw diff;
// nothing written"). Each test drives the real widgets on a copy of the demo project with the
// `review` overlay applied, uncommitted (Tests/Support/DemoProject). What the lines say is
// LoomCore's, tested there (ReviewChangesTests); these check the wiring. The cases that build need
// TeX: they skip without latexmk and fail instead under LOOM_REQUIRE_TEX=1, as on Linux CI.

#include "LoomApp/AppSettings.h"
#include "LoomApp/FilePage.h"
#include "LoomApp/Lane.h"
#include "LoomApp/MainWindow.h"
#include "LoomApp/PdfPane.h"
#include "LoomApp/ProjectSession.h"
#include "LoomApp/ReviewPage.h"
#include "LoomApp/SideBar.h"
#include "LoomApp/StatementsPage.h"

#include "DemoProject.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QLabel>
#include <QPdfDocument>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSplitter>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <memory>

using loom::BuildMode;
using loom::MainWindow;
using loom::ReviewPage;
using loom::ReviewSide;
using loom::Tab;

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

/// Saves a picture of a widget for people to look at (never compared pixel by pixel), after a
/// moment for the PDF views to render their pages, which they do in the background.
void screenshot(QWidget* widget, const QString& name)
{
    QTest::qWait(800);
    QDir().mkpath(QStringLiteral(LOOM_SCREENSHOTS));
    widget->grab().save(QStringLiteral(LOOM_SCREENSHOTS "/") + name + QStringLiteral(".png"));
}

/// Writes a file, creating its folder. @returns Whether it worked.
bool writeFile(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

/// Runs git in the demo. @returns Its stdout, or `<failed>`.
QString git(const loomtest::DemoProject& demo, const QStringList& arguments)
{
    const loom::GitRun run = loom::runGit(demo.git(), arguments, demo.root());
    return run.ok() ? run.output : QStringLiteral("<failed: %1>").arg(run.errors.trimmed());
}

/// Every file of the project outside `.git/` and `.loom/` with the hash of its bytes, and what git
/// says of `HEAD`, the index and the working tree: what "Review never changes a file or git" keeps.
QStringList fingerprintOf(const loomtest::DemoProject& demo)
{
    QStringList out;
    QDirIterator it(demo.root(), QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = QDir(demo.root()).relativeFilePath(it.next());
        if (path.startsWith(u".git/") || path.startsWith(u".loom/"))
            continue;
        QFile file(it.filePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        out << path + u' ' + QString::fromLatin1(QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256).toHex());
    }
    out.sort();
    out << git(demo, {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
    out << git(demo, {QStringLiteral("diff"), QStringLiteral("--cached")});
    out << git(demo, {QStringLiteral("status"), QStringLiteral("--porcelain=v1"), QStringLiteral("--untracked-files=all")});
    out << git(demo, {QStringLiteral("branch"), QStringLiteral("--list")});
    return out;
}

/// The items of the list, as `text` or `text | mark | summary`, in order, groups included.
QStringList listed(const ReviewPage* page)
{
    QStringList out;
    for (QTreeWidgetItemIterator it(page->entries()); *it; ++it) {
        QTreeWidgetItem* item = *it;
        out << (item->text(1).isEmpty() ? item->text(0) : QStringLiteral("%1 | %2 | %3").arg(item->text(0), item->text(1), item->text(2)));
    }
    return out;
}

/// The lines of the changes shown, headings included.
QStringList changesShown(const ReviewPage* page)
{
    QStringList out;
    for (QTreeWidgetItemIterator it(page->changes()); *it; ++it)
        out << ((*it)->parent() ? QStringLiteral("  ") : QString()) + (*it)->text(0);
    return out;
}

/// How clickItem() finds an item.
enum class Find {
    Entry, ///< An entry of Review's list (it has a mark), which becomes the current item.
    Any,   ///< Any item; a status change's line makes its statement's entry current instead.
};

/// Clicks the item of a tree whose first column starts with `text`.
/// @returns Whether there is one (for Find::Entry, and it became the current item).
bool clickItem(QTreeWidget* tree, const QString& text, Find find = Find::Entry)
{
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if (find == Find::Entry && (*it)->text(1).isEmpty())
            continue;
        if ((*it)->text(0).startsWith(text)) {
            tree->scrollToItem(*it);
            QTest::mouseClick(tree->viewport(), Qt::LeftButton, {}, tree->visualItemRect(*it).center());
            return find == Find::Any || tree->currentItem() == *it;
        }
    }
    return false;
}

/// The builds a page reports, collected as they finish.
struct Builds {
    explicit Builds(ReviewPage* page)
    {
        QObject::connect(page, &ReviewPage::buildFinished, page, [this](const loom::BuildResult& result) { results.push_back(result); });
    }
    std::vector<loom::BuildResult> results;
};

/// Waits until the page has read its list and nothing is queued on the Git lane.
bool waitForList(MainWindow& window)
{
    ReviewPage* page = window.reviewPage();
    return QTest::qWaitFor([&] { return !page->reading() && !window.gitLane()->busy() && !window.projectLane()->busy(); }, 30'000);
}

/// Waits until both sides' builds are done.
bool waitForBuilds(ReviewPage* page)
{
    return QTest::qWaitFor([page] { return page->building() == 0; }, 2 * buildWaitMs);
}

} // namespace

/// Qt Test cases for the Review tab.
class ReviewTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void listsTheOverlayGrouped();
    void showsChangesInPlainWords();
    void rawDiffForEveryEntry();
    void buildsBeforeAndAfter();
    void aSideThatDoesntCompileShowsItsErrors();
    void openInStatementsOpensTheFile();
    void updatesWhenFilesChange();
    void neverChangesAFileOrGit();

private:
    /// A new window on the test's settings, shown, with the demo open on the Review tab and its list read.
    std::unique_ptr<MainWindow> openReview(const QString& root);

    std::unique_ptr<QTemporaryDir> m_temp;
    std::unique_ptr<loom::AppSettings> m_settings;
    QTemporaryDir m_gitHome;
};

void ReviewTests::initTestCase()
{
    QVERIFY(m_gitHome.isValid());
    const QString config = m_gitHome.filePath(QStringLiteral("gitconfig"));
    QVERIFY(writeFile(config, "[user]\n\tname = Loom Test\n\temail = test@example.com\n"));
    qputenv("GIT_CONFIG_GLOBAL", QDir::toNativeSeparators(config).toLocal8Bit());
    qputenv("GIT_CONFIG_NOSYSTEM", "1");
}

void ReviewTests::init()
{
    m_temp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_temp->isValid());
    m_settings = std::make_unique<loom::AppSettings>(pathsIn(*m_temp));
    m_settings->setCompileOnSave(false);
}

void ReviewTests::cleanup()
{
    m_settings.reset();
    m_temp.reset();
}

std::unique_ptr<MainWindow> ReviewTests::openReview(const QString& root)
{
    auto window = std::make_unique<MainWindow>(m_settings.get());
    window->resize(1400, 900);
    window->show();
    QSignalSpy finished(window.get(), &MainWindow::openFinished);
    window->openFolder(root);
    if (finished.count() == 0 && !finished.wait(30'000))
        return nullptr;
    window->showTab(Tab::Review);
    if (!waitForList(*window))
        return nullptr;
    return window;
}

void ReviewTests::listsTheOverlayGrouped()
{
    const loomtest::DemoProject demo(QStringLiteral("review"));
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const auto window = openReview(demo.root());
    QVERIFY(window);
    ReviewPage* page = window->reviewPage();

    // Status changes at the top, drops marked; then the groups with their entries, each marked
    // new, changed or deleted, with what changed in a few words.
    QCOMPARE(listed(page),
             (QStringList{QStringLiteral("Status changes"),
                          QStringLiteral(u"C4JK · Tails of tame sequences vanish: proved → unchecked ▼"),
                          QStringLiteral(u"D5MN · Tame sequences are bounded: proved → unchecked ▼"),
                          QStringLiteral(u"E6PQ · Products with bounded sequences stay tame: proved → conditional ▼"),
                          QStringLiteral(u"M7QR · Tails of products: none → open"),
                          QStringLiteral("Statements and proofs"),
                          QStringLiteral(u"C4JK · Tails of tame sequences vanish | changed | text"),
                          QStringLiteral(u"G8TV · Tame sums commute with limits | changed | G8TV.a premises, check"),
                          QStringLiteral(u"M7QR · Tails of products | new | new"),
                          QStringLiteral("Markdown"),
                          QStringLiteral("Papers/SmithJones24/notes.md | changed | ")}));
    // The badge counts every entry but the status changes.
    QCOMPARE(window->sideBar()->button(Tab::Review)->badgeCount(), 4);
    screenshot(window.get(), QStringLiteral("review-list"));
}

void ReviewTests::showsChangesInPlainWords()
{
    const loomtest::DemoProject demo(QStringLiteral("review"));
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    m_settings->setLatexmkPath(m_temp->filePath(QStringLiteral("no-such-latexmk"))); // No builds needed here.
    const auto window = openReview(demo.root());
    QVERIFY(window);
    ReviewPage* page = window->reviewPage();

    QVERIFY(clickItem(page->entries(), QStringLiteral("G8TV")));
    QCOMPARE(page->shownEntry(), QStringLiteral("G8TV"));
    QCOMPARE(page->heading()->text(), QStringLiteral(u"G8TV · Tame sums commute with limits — changed"));
    QCOMPARE(changesShown(page), (QStringList{QStringLiteral("Proof G8TV.a"),
                                              QStringLiteral(u"  premises: +D5MN · Tame sequences are bounded"),
                                              QStringLiteral("Checks of G8TV.a"), QStringLiteral("  check: Dakota pass (this version)")}));

    QVERIFY(clickItem(page->entries(), QStringLiteral("C4JK")));
    QCOMPARE(changesShown(page), (QStringList{QStringLiteral("Statement"), QStringLiteral("  text changed; outdates the checks of C4JK.a, D5MN.a")}));

    // A status change's line shows its statement's entry.
    QVERIFY(clickItem(page->entries(), QStringLiteral(u"M7QR · Tails of products: none"), Find::Any));
    QCOMPARE(page->shownEntry(), QStringLiteral("M7QR"));
    QCOMPARE(changesShown(page), (QStringList{QStringLiteral("Statement"), QStringLiteral(u"  new lemma M7QR · Tails of products")}));
}

void ReviewTests::rawDiffForEveryEntry()
{
    const loomtest::DemoProject demo(QStringLiteral("review"));
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    m_settings->setLatexmkPath(m_temp->filePath(QStringLiteral("no-such-latexmk")));
    const auto window = openReview(demo.root());
    QVERIFY(window);
    ReviewPage* page = window->reviewPage();

    // A statement: Raw diff replaces the PDFs with git's diff, coloured, and back.
    QVERIFY(clickItem(page->entries(), QStringLiteral("C4JK")));
    QVERIFY(page->rawDiffButton()->isVisible());
    QVERIFY(page->sides()->isVisible());
    QSignalSpy diffShown(page, &ReviewPage::diffShown);
    QTest::mouseClick(page->rawDiffButton(), Qt::LeftButton);
    QVERIFY(diffShown.count() > 0 || diffShown.wait(30'000));
    QVERIFY(page->rawDiff()->isVisible());
    QVERIFY(!page->sides()->isVisible());
    QVERIFY(page->rawDiff()->toPlainText().contains(QStringLiteral("+If $a$ is tame")));
    QVERIFY(page->rawDiff()->toPlainText().contains(QStringLiteral("and they never increase.")));
    screenshot(window.get(), QStringLiteral("review-raw-diff"));
    QTest::mouseClick(page->rawDiffButton(), Qt::LeftButton);
    QVERIFY(page->sides()->isVisible());

    // A new statement's raw diff is the file itself.
    QVERIFY(clickItem(page->entries(), QStringLiteral("M7QR")));
    QTest::mouseClick(page->rawDiffButton(), Qt::LeftButton);
    QTRY_VERIFY(page->rawDiff()->toPlainText().contains(QStringLiteral("title = \"Tails of products\"")));
    QTest::mouseClick(page->rawDiffButton(), Qt::LeftButton);

    // Markdown shows only the raw diff: no plain words, no PDFs, no toggle, no Open.
    diffShown.clear();
    QVERIFY(clickItem(page->entries(), QStringLiteral("Papers/SmithJones24/notes.md")));
    QVERIFY(diffShown.count() > 0 || diffShown.wait(30'000));
    QVERIFY(page->rawDiff()->isVisible());
    QVERIFY(!page->changes()->isVisible());
    QVERIFY(!page->rawDiffButton()->isVisible());
    QVERIFY(!page->openButton()->isVisible());
    QVERIFY(page->rawDiff()->toPlainText().contains(QStringLiteral("diff --git a/Papers/SmithJones24/notes.md")));
}

void ReviewTests::buildsBeforeAndAfter()
{
    REQUIRE_TEX();
    const loomtest::DemoProject demo(QStringLiteral("review"));
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const auto window = openReview(demo.root());
    QVERIFY(window);
    ReviewPage* page = window->reviewPage();
    Builds built(page);

    // A changed statement: built at HEAD and now, side by side.
    QVERIFY(clickItem(page->entries(), QStringLiteral("C4JK")));
    QVERIFY(waitForBuilds(page));
    QCOMPARE(page->beforeSide()->shows(), ReviewSide::Shows::Pdf);
    QCOMPARE(page->afterSide()->shows(), ReviewSide::Shows::Pdf);
    QTRY_COMPARE(page->beforeSide()->document()->status(), QPdfDocument::Status::Ready);
    QTRY_COMPARE(page->afterSide()->document()->status(), QPdfDocument::Status::Ready);
    QCOMPARE(built.results.size(), std::size_t(2));
    QStringList revs;
    for (const loom::BuildResult& result : built.results)
        revs << (result.rev.isEmpty() ? QStringLiteral("now") : QStringLiteral("HEAD"));
    revs.sort();
    QCOMPARE(revs, (QStringList{QStringLiteral("HEAD"), QStringLiteral("now")}));
    screenshot(window.get(), QStringLiteral("review-before-after"));

    // The mode switch rebuilds both.
    built.results.clear();
    QTest::mouseClick(page->modeButton(BuildMode::Premises), Qt::LeftButton);
    QCOMPARE(page->mode(), BuildMode::Premises);
    QVERIFY(waitForBuilds(page));
    QCOMPARE(built.results.size(), std::size_t(2));
    for (const loom::BuildResult& result : built.results)
        QCOMPARE(result.mode, BuildMode::Premises);
    QCOMPARE(page->beforeSide()->shows(), ReviewSide::Shows::Pdf);
    QCOMPARE(page->afterSide()->shows(), ReviewSide::Shows::Pdf);

    // A new statement: no side at HEAD.
    built.results.clear();
    QVERIFY(clickItem(page->entries(), QStringLiteral("M7QR")));
    QVERIFY(waitForBuilds(page));
    QCOMPARE(page->beforeSide()->shows(), ReviewSide::Shows::Message);
    QCOMPARE(page->beforeSide()->message()->text(), QStringLiteral("New: not in the last commit."));
    QCOMPARE(page->afterSide()->shows(), ReviewSide::Shows::Pdf);
    QCOMPARE(built.results.size(), std::size_t(1));
}

void ReviewTests::aSideThatDoesntCompileShowsItsErrors()
{
    REQUIRE_TEX();
    loomtest::DemoProject demo(QStringLiteral("review"));
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    QVERIFY(demo.applyOverlay(QStringLiteral("latex-error")).isEmpty());
    const auto window = openReview(demo.root());
    QVERIFY(window);
    ReviewPage* page = window->reviewPage();

    // K3AB's body has an unclosed brace now: the side of now shows the error, HEAD's still its PDF.
    QVERIFY(clickItem(page->entries(), QStringLiteral("K3AB")));
    QVERIFY(waitForBuilds(page));
    QCOMPARE(page->beforeSide()->shows(), ReviewSide::Shows::Pdf);
    QCOMPARE(page->afterSide()->shows(), ReviewSide::Shows::Errors);
    QVERIFY(page->afterSide()->errors()->toPlainText().startsWith(QStringLiteral("It doesn't compile")));
    QVERIFY(page->afterSide()->errors()->toPlainText().contains(QStringLiteral("Statements/K3AB.tex:")));
    screenshot(window.get(), QStringLiteral("review-error-side"));
}

void ReviewTests::openInStatementsOpensTheFile()
{
    const loomtest::DemoProject demo(QStringLiteral("review"));
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    m_settings->setLatexmkPath(m_temp->filePath(QStringLiteral("no-such-latexmk")));
    const auto window = openReview(demo.root());
    QVERIFY(window);
    ReviewPage* page = window->reviewPage();

    // The statement.
    QVERIFY(clickItem(page->entries(), QStringLiteral("C4JK")));
    QTest::mouseClick(page->openButton(), Qt::LeftButton);
    QCOMPARE(window->currentTab(), Tab::Statements);
    QTRY_COMPARE(window->statementsPage()->openTag(), QStringLiteral("C4JK"));
    QVERIFY(window->statementsPage()->currentPage());
    QCOMPARE(window->statementsPage()->currentPage()->proofId(), QString());

    // The selected proof.
    window->showTab(Tab::Review);
    QVERIFY(clickItem(page->entries(), QStringLiteral("G8TV")));
    QVERIFY(clickItem(page->changes(), QStringLiteral("Proof G8TV.a"), Find::Any));
    QTest::mouseClick(page->openButton(), Qt::LeftButton);
    QCOMPARE(window->currentTab(), Tab::Statements);
    QTRY_COMPARE(window->statementsPage()->openTag(), QStringLiteral("G8TV"));
    QTRY_VERIFY(window->statementsPage()->currentPage());
    QCOMPARE(window->statementsPage()->currentPage()->proofId(), QStringLiteral("G8TV.a"));
}

void ReviewTests::updatesWhenFilesChange()
{
    const loomtest::DemoProject demo(QStringLiteral("review"));
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    m_settings->setLatexmkPath(m_temp->filePath(QStringLiteral("no-such-latexmk")));
    const auto window = openReview(demo.root());
    QVERIFY(window);
    ReviewPage* page = window->reviewPage();
    QCOMPARE(page->list().count(), 4);

    // A file written outside Loom: the watcher sees it, the snapshot is read, and so is the list.
    QSignalSpy shown(page, &ReviewPage::listShown);
    QVERIFY(writeFile(demo.root() + QStringLiteral("/Notes/plan.md"), "# A plan\n"));
    window->session()->refresh(); // The watcher doesn't watch Notes/; activating the window would.
    QVERIFY(shown.wait(30'000));
    QVERIFY(waitForList(*window));
    QCOMPARE(page->list().count(), 5);
    QCOMPARE(window->sideBar()->button(Tab::Review)->badgeCount(), 5);
    QVERIFY(listed(page).contains(QStringLiteral("Notes/plan.md | new | ")));

    // The entry shown stays shown, with its changes as they are now.
    QVERIFY(clickItem(page->entries(), QStringLiteral("C4JK")));
    QFile c4jk(demo.root() + QStringLiteral("/Statements/C4JK.tex"));
    QVERIFY(c4jk.open(QIODevice::ReadOnly));
    QByteArray bytes = c4jk.readAll();
    c4jk.close();
    bytes.replace("title = \"Tails of tame sequences vanish\"", "title = \"Tails vanish\"");
    QVERIFY(writeFile(c4jk.fileName(), bytes));
    shown.clear();
    window->session()->refresh();
    QVERIFY(shown.wait(30'000));
    QVERIFY(waitForList(*window));
    QCOMPARE(page->shownEntry(), QStringLiteral("C4JK"));
    QVERIFY(changesShown(page).contains(QStringLiteral(u"  title: \"Tails of tame sequences vanish\" → \"Tails vanish\"")));
}

void ReviewTests::neverChangesAFileOrGit()
{
    const loomtest::DemoProject demo(QStringLiteral("review"));
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const bool tex = !QStandardPaths::findExecutable(QStringLiteral("latexmk")).isEmpty();
    if (!tex)
        m_settings->setLatexmkPath(m_temp->filePath(QStringLiteral("no-such-latexmk")));
    const QStringList before = fingerprintOf(demo);
    {
        const auto window = openReview(demo.root());
        QVERIFY(window);
        ReviewPage* page = window->reviewPage();
        // Every entry, every mode and toggle, the raw diffs: Review only reads and builds into .loom/.
        for (const QString& entry : {QStringLiteral("C4JK"), QStringLiteral("G8TV"), QStringLiteral("M7QR"), QStringLiteral("Papers/")}) {
            QVERIFY(clickItem(page->entries(), entry));
            QVERIFY(waitForBuilds(page));
            if (page->rawDiffButton()->isVisible()) {
                QTest::mouseClick(page->verboseButton(), Qt::LeftButton);
                QVERIFY(waitForBuilds(page));
                QTest::mouseClick(page->modeButton(BuildMode::Chain), Qt::LeftButton);
                QVERIFY(waitForBuilds(page));
                QTest::mouseClick(page->rawDiffButton(), Qt::LeftButton);
                QTRY_VERIFY(!page->readingDiff());
                QTest::mouseClick(page->rawDiffButton(), Qt::LeftButton);
                QVERIFY(waitForBuilds(page));
            } else {
                QTRY_VERIFY(!page->readingDiff());
            }
        }
        QVERIFY(waitForList(*window));
    }
    QCOMPARE(fingerprintOf(demo), before);
}

QTEST_MAIN(ReviewTests)
#include "ReviewTests.moc"
