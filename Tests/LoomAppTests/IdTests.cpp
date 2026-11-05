// IDs in the editor (Specs/m2-statements-review.md, "IDs in the editor"; Verification, "IDs": "The
// picker from `\ref{`, a field and Ctrl+P; insertion; Ctrl+click"): each test drives the real
// widgets on a copy of the demo project (Tests/Support/DemoProject). The search's rules are
// LoomCore's and tested there (IdSearchTests); these check the wiring and behaviour. The preview
// test builds with TeX: it skips without latexmk, and fails instead under LOOM_REQUIRE_TEX=1.

#include "LoomApp/AppSettings.h"
#include "LoomApp/EditBuffer.h"
#include "LoomApp/FilePage.h"
#include "LoomApp/HeaderForms.h"
#include "LoomApp/IdAssist.h"
#include "LoomApp/IdPicker.h"
#include "LoomApp/IdPreviews.h"
#include "LoomApp/IdTools.h"
#include "LoomApp/LatexEditor.h"
#include "LoomApp/MainWindow.h"
#include "LoomApp/NewItemDialogs.h"
#include "LoomApp/StatementList.h"
#include "LoomApp/StatementsPage.h"

#include "DemoProject.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHelpEvent>
#include <QLabel>
#include <QLineEdit>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTreeWidget>

#include <memory>

using loom::FilePage;
using loom::IdAssist;
using loom::IdPicker;
using loom::IdTools;
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

/// Writes a file. @returns Whether it worked.
bool writeFile(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

/// Presses a window shortcut, making the window active first: parallel tests can take activation
/// (Wiki/gotchas/qt/shortcuts-need-an-active-window.md). @returns Whether it could be made active.
bool press(QWidget* window, Qt::Key key, Qt::KeyboardModifiers modifiers = {})
{
    window->activateWindow();
    if (!QTest::qWaitForWindowActive(window))
        return false;
    QTest::keyClick(window, key, modifiers);
    return true;
}

/// The editor's ID assist, or null.
IdAssist* assistOf(FilePage* page)
{
    return page ? page->editor()->findChild<IdAssist*>(QStringLiteral("idAssist")) : nullptr;
}

/// The span of a name in an assist, or null.
const IdAssist::Span* spanOf(IdAssist* assist, const QString& name)
{
    for (const IdAssist::Span& span : assist->spans()) {
        if (span.mention.name == name)
            return &span;
    }
    return nullptr;
}

/// Whether an indicator is on at a byte position of an editor.
bool indicatorAt(loom::LatexEditor* editor, int indicator, int position)
{
    return editor->SendScintilla(QsciScintillaBase::SCI_INDICATORVALUEAT, indicator, position) != 0;
}

/// The annotation under an editor line (from 0). QsciScintilla::annotation() can't be used: in
/// QScintilla 2.14.1 it returns a buffer it never filled
/// (Wiki/gotchas/qt/qscintilla-annotation-returns-garbage.md).
QString annotationOf(loom::LatexEditor* editor, int line)
{
    const auto size = editor->SendScintilla(QsciScintillaBase::SCI_ANNOTATIONGETTEXT, static_cast<unsigned long>(line), static_cast<void*>(nullptr));
    QByteArray bytes(static_cast<qsizetype>(size), char(0));
    editor->SendScintilla(QsciScintillaBase::SCI_ANNOTATIONGETTEXT, static_cast<unsigned long>(line), static_cast<void*>(bytes.data()));
    return QString::fromUtf8(bytes);
}

/// The middle of the first character of a span, in the editor's viewport.
QPoint pointIn(IdAssist* assist, loom::LatexEditor* editor, const IdAssist::Span& span)
{
    const int height = static_cast<int>(editor->SendScintilla(QsciScintillaBase::SCI_TEXTHEIGHT, 0));
    return assist->pointOf(span.position + 1) + QPoint(1, height / 2);
}

/// Puts the cursor at the end of an editor's text, on a new line.
void endOnNewLine(loom::LatexEditor* editor)
{
    editor->setFocus();
    const int last = editor->lines() - 1;
    editor->setCursorPosition(last, static_cast<int>(editor->text(last).size()));
    if (!editor->text(last).isEmpty())
        QTest::keyClick(editor, Qt::Key_Return);
}

} // namespace

/// Qt Test cases for IDs in the editor.
class IdTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void typingRefOpensThePickerAndInserts();
    void ctrlSpaceInsideBracesOpensIt();
    void fieldsOfferWhatTheyTake();
    void newFormsPickIdsToo();
    void goToStatement();
    void titlesBesideIdsAndRedUnderlines();
    void ctrlClickOpens();
    void hoverShowsTheCard();
    void listSearchUsesTheIdSearch();
    void previewsAreBuiltAndCut();

private:
    std::unique_ptr<MainWindow> newWindow();
    bool openDemo(MainWindow& window, const QString& root);

    std::unique_ptr<QTemporaryDir> m_temp;
    std::unique_ptr<loom::AppSettings> m_settings;
    QTemporaryDir m_gitHome;
};

void IdTests::initTestCase()
{
    QVERIFY(m_gitHome.isValid());
    const QString config = m_gitHome.filePath(QStringLiteral("gitconfig"));
    QVERIFY(writeFile(config, "[user]\n\tname = Loom Test\n\temail = test@example.com\n"));
    qputenv("GIT_CONFIG_GLOBAL", QDir::toNativeSeparators(config).toLocal8Bit());
    qputenv("GIT_CONFIG_NOSYSTEM", "1");
}

void IdTests::init()
{
    m_temp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_temp->isValid());
    m_settings = std::make_unique<loom::AppSettings>(pathsIn(*m_temp));
    // No TeX runs unless a test asks: previews would otherwise start on every hover.
    m_settings->setLatexmkPath(QStringLiteral("loom-no-latexmk-here"));
}

void IdTests::cleanup()
{
    m_settings.reset();
    m_temp.reset();
}

std::unique_ptr<MainWindow> IdTests::newWindow()
{
    auto window = std::make_unique<MainWindow>(m_settings.get());
    window->resize(1300, 800);
    window->show();
    return window;
}

bool IdTests::openDemo(MainWindow& window, const QString& root)
{
    QSignalSpy offered(window.statementsPage(), &StatementsPage::recoveryOffered);
    QSignalSpy finished(&window, &MainWindow::openFinished);
    window.openFolder(root);
    if (finished.count() == 0 && !finished.wait(30'000))
        return false;
    return offered.count() > 0 || offered.wait(30'000);
}

void IdTests::typingRefOpensThePickerAndInserts()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    IdTools* tools = page->idTools();
    page->openStatement(QStringLiteral("K3AB"));
    loom::LatexEditor* editor = page->currentPage()->editor();
    endOnNewLine(editor);

    // Typing `\ref{` opens the picker at the cursor, offering everything `\ref` can name.
    QTest::keyClicks(editor, QStringLiteral("See \\ref{"));
    QTRY_VERIFY(tools->openPicker());
    IdPicker* picker = tools->openPicker();
    QVERIFY(picker->shownIds().contains(QStringLiteral("C4JK.a"))); // Proofs too.
    QVERIFY(picker->shownIds().contains(QStringLiteral("7RT2")));   // And paper results.

    // A word of a title finds the statement; its card is beside the list.
    QTest::keyClicks(picker->search(), QStringLiteral("bounded"));
    QCOMPARE(picker->shownIds().value(0), QStringLiteral("D5MN"));
    QCOMPARE(picker->card()->id(), QStringLiteral("D5MN"));
    QCOMPARE(picker->card()->heading()->text(), QStringLiteral("D5MN · Tame sequences are bounded"));
    QVERIFY(!picker->card()->source()->isHidden()); // Its source until a preview is built (E2).
    screenshot(picker, QStringLiteral("id-picker"));

    // Enter inserts the tag and closes the brace.
    QTest::keyClick(picker->search(), Qt::Key_Return);
    QTRY_VERIFY(!tools->openPicker());
    const int last = editor->lines() - 1;
    QCOMPARE(editor->text(last), QStringLiteral("See \\ref{D5MN}"));
    int line = 0;
    int index = 0;
    editor->getCursorPosition(&line, &index);
    QCOMPARE(index, static_cast<int>(editor->text(last).size()));
    QVERIFY(page->buffer(QStringLiteral("Statements/K3AB.tex"))->dirty());

    // Something that names nothing can't be inserted: Enter does nothing, Escape closes.
    QTest::keyClicks(editor, QStringLiteral(" and \\paperref{"));
    QTRY_VERIFY(tools->openPicker());
    picker = tools->openPicker();
    QCOMPARE(picker->shownIds(), (QStringList{QStringLiteral("3WQ6"), QStringLiteral("7RT2")})); // Stubs only.
    picker->search()->setText(QStringLiteral("zzzz"));
    QVERIFY(picker->shownIds().isEmpty());
    QTest::keyClick(picker->search(), Qt::Key_Return);
    QVERIFY(tools->openPicker());
    QVERIFY(!picker->findChild<QLabel*>(QStringLiteral("idPickerNothing"))->isHidden());
    QTest::keyClick(picker->search(), Qt::Key_Escape);
    QTRY_VERIFY(!tools->openPicker());
    QCOMPARE(editor->text(last), QStringLiteral("See \\ref{D5MN} and \\paperref{"));

    // `\cite{` offers paper keys.
    editor->setFocus();
    QTest::keyClicks(editor, QStringLiteral("} \\cite{"));
    QTRY_VERIFY(tools->openPicker());
    QCOMPARE(tools->openPicker()->shownIds(), QStringList{QStringLiteral("SmithJones24")});
    QTest::keyClick(tools->openPicker()->search(), Qt::Key_Return);
    QTRY_VERIFY(!tools->openPicker());
    QVERIFY(editor->text(last).endsWith(QStringLiteral("\\cite{SmithJones24}")));
}

void IdTests::ctrlSpaceInsideBracesOpensIt()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    IdTools* tools = page->idTools();
    page->openStatement(QStringLiteral("K3AB"));
    loom::LatexEditor* editor = page->currentPage()->editor();
    endOnNewLine(editor);
    const int last = editor->lines() - 1;
    editor->insert(QStringLiteral("By \\ref{tai} here."));

    // Ctrl+Space outside braces does nothing.
    editor->setCursorPosition(last, 1);
    QTest::keyClick(editor, Qt::Key_Space, Qt::ControlModifier);
    QVERIFY(!tools->openPicker());

    // Inside, it starts from what is typed there, and the choice replaces the whole name.
    editor->setCursorPosition(last, 10); // After `\ref{ta`.
    QTest::keyClick(editor, Qt::Key_Space, Qt::ControlModifier);
    QTRY_VERIFY(tools->openPicker());
    IdPicker* picker = tools->openPicker();
    QCOMPARE(picker->search()->text(), QStringLiteral("ta"));
    picker->search()->setText(QStringLiteral("tails vanish"));
    QCOMPARE(picker->shownIds().value(0), QStringLiteral("C4JK"));
    QVERIFY(picker->choose());
    QCOMPARE(editor->text(last), QStringLiteral("By \\ref{C4JK} here."));
}

void IdTests::fieldsOfferWhatTheyTake()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    IdTools* tools = page->idTools();

    // Premises: provable statements and stubs, never the statement the proof proves.
    page->openProof(QStringLiteral("D5MN.a"));
    FilePage* proof = page->currentPage();
    QTest::mouseClick(proof->proofForm()->premises()->addButton(), Qt::LeftButton);
    QTRY_VERIFY(tools->openPicker());
    IdPicker* picker = tools->openPicker();
    QVERIFY(!picker->shownIds().contains(QStringLiteral("D5MN")));
    QVERIFY(!picker->shownIds().contains(QStringLiteral("A2DF"))); // A definition has no status.
    QVERIFY(!picker->shownIds().contains(QStringLiteral("D5MN.a"))); // No proofs.
    QVERIFY(picker->shownIds().contains(QStringLiteral("7RT2")));
    QTest::keyClicks(picker->search(), QStringLiteral("rearr"));
    QCOMPARE(picker->shownIds(), QStringList{QStringLiteral("F7RS")});
    QTest::keyClick(picker->search(), Qt::Key_Return);
    QTRY_VERIFY(!tools->openPicker());
    proof = page->currentPage();
    QCOMPARE(proof->proofForm()->premises()->items(), (QStringList{QStringLiteral("C4JK"), QStringLiteral("F7RS")}));
    QVERIFY(page->buffer(QStringLiteral("Proofs/D5MN/D5MN.a.tex"))->dirty());

    // Stated using: statements and stubs, not itself.
    page->openStatement(QStringLiteral("E6PQ"));
    FilePage* statement = page->currentPage();
    QTest::mouseClick(statement->statementForm()->statedUsing()->addButton(), Qt::LeftButton);
    QTRY_VERIFY(tools->openPicker());
    picker = tools->openPicker();
    QVERIFY(!picker->shownIds().contains(QStringLiteral("E6PQ")));
    QVERIFY(picker->shownIds().contains(QStringLiteral("3WQ6")));
    QTest::keyClicks(picker->search(), QStringLiteral("tail of"));
    QCOMPARE(picker->shownIds().value(0), QStringLiteral("B3GH"));
    QTest::keyClick(picker->search(), Qt::Key_Return);
    QTRY_VERIFY(!tools->openPicker());
    statement = page->currentPage();
    QCOMPARE(statement->statementForm()->statedUsing()->items(), (QStringList{QStringLiteral("A2DF"), QStringLiteral("B3GH")}));

    // Superseded by: statements only, one at most; × clears it.
    loom::ChipList* superseded = statement->statementForm()->supersededBy();
    QVERIFY(superseded->items().isEmpty());
    QTest::mouseClick(superseded->addButton(), Qt::LeftButton);
    QTRY_VERIFY(tools->openPicker());
    picker = tools->openPicker();
    QVERIFY(!picker->shownIds().contains(QStringLiteral("7RT2")));
    QVERIFY(!picker->shownIds().contains(QStringLiteral("E6PQ")));
    QTest::keyClicks(picker->search(), QStringLiteral("g8tv"));
    QTest::keyClick(picker->search(), Qt::Key_Return);
    QTRY_VERIFY(!tools->openPicker());
    statement = page->currentPage();
    superseded = statement->statementForm()->supersededBy();
    QCOMPARE(superseded->items(), QStringList{QStringLiteral("G8TV")});
    QVERIFY(superseded->addButton()->isHidden());
    QCOMPARE(page->buffer(QStringLiteral("Statements/E6PQ.tex"))->statement().supersededBy, QStringLiteral("G8TV"));
    auto* remove = superseded->findChild<QToolButton*>(QStringLiteral("remove"));
    QVERIFY(remove);
    QTest::mouseClick(remove, Qt::LeftButton);
    QTRY_VERIFY(page->buffer(QStringLiteral("Statements/E6PQ.tex"))->statement().supersededBy.isEmpty());
    QTRY_VERIFY(page->currentPage()->statementForm()->supersededBy()->items().isEmpty());
}

void IdTests::newFormsPickIdsToo()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    IdTools* tools = page->idTools();
    page->openStatement(QStringLiteral("H9WX"));
    page->newProof();
    loom::NewProofDialog* dialog = page->newProofDialog();
    QVERIFY(dialog);
    QTRY_VERIFY(dialog->isVisible());

    // The +, over the modal form.
    QTest::mouseClick(dialog->premisesPicker(), Qt::LeftButton);
    QTRY_VERIFY(tools->openPicker());
    QCOMPARE(tools->openPicker()->window(), tools->openPicker()); // A popup of its own…
    QCOMPARE(tools->openPicker()->parentWidget(), dialog);         // …over the form.
    QVERIFY(!tools->openPicker()->shownIds().contains(QStringLiteral("H9WX")));
    QTest::keyClicks(tools->openPicker()->search(), QStringLiteral("c4jk"));
    QTest::keyClick(tools->openPicker()->search(), Qt::Key_Return);
    QTRY_VERIFY(!tools->openPicker());
    QCOMPARE(dialog->premises()->text(), QStringLiteral("C4JK"));

    // The body's editor has the picker as well.
    dialog->body()->setFocus();
    QTest::keyClicks(dialog->body(), QStringLiteral("By \\ref{"));
    QTRY_VERIFY(tools->openPicker());
    QTest::keyClicks(tools->openPicker()->search(), QStringLiteral("c4jk"));
    QTest::keyClick(tools->openPicker()->search(), Qt::Key_Return);
    QTRY_VERIFY(!tools->openPicker());
    QCOMPARE(dialog->body()->text(), QStringLiteral("By \\ref{C4JK}"));
    dialog->body()->clear();
    dialog->premises()->clear();
    dialog->reject();
}

void IdTests::goToStatement()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    IdTools* tools = page->idTools();

    QVERIFY(press(window.get(), Qt::Key_P, Qt::ControlModifier));
    QTRY_VERIFY(tools->openPicker());
    QTest::keyClicks(tools->openPicker()->search(), QStringLiteral("k3"));
    QCOMPARE(tools->openPicker()->shownIds().value(0), QStringLiteral("K3AB"));
    QTest::keyClick(tools->openPicker()->search(), Qt::Key_Return);
    QTRY_COMPARE(page->openTag(), QStringLiteral("K3AB"));

    // A proof opens on its page; a paper result read-only.
    page->goToStatement();
    QTRY_VERIFY(tools->openPicker());
    tools->openPicker()->search()->setText(QStringLiteral("e6pq.a"));
    QTest::keyClick(tools->openPicker()->search(), Qt::Key_Return);
    QTRY_COMPARE(page->currentPage()->proofId(), QStringLiteral("E6PQ.a"));
    page->goToStatement();
    QTRY_VERIFY(tools->openPicker());
    tools->openPicker()->search()->setText(QStringLiteral("7rt2"));
    QTest::keyClick(tools->openPicker()->search(), Qt::Key_Return);
    QTRY_COMPARE(page->openTag(), QStringLiteral("7RT2"));
    QVERIFY(page->currentPage()->bar(QStringLiteral("stubBar")));
}

void IdTests::titlesBesideIdsAndRedUnderlines()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    page->openStatement(QStringLiteral("C4JK"));
    loom::LatexEditor* editor = page->currentPage()->editor();
    IdAssist* assist = assistOf(page->currentPage());
    QVERIFY(assist);

    // The line with `\ref{A2DF}` and `\ref{B3GH}` has both titles under it.
    QTRY_VERIFY(!annotationOf(editor, 0).isEmpty());
    QVERIFY(annotationOf(editor, 0).contains(QStringLiteral("A2DF · Tame sequence")));
    QVERIFY(annotationOf(editor, 0).contains(QStringLiteral("B3GH · The tail of a sequence")));
    const IdAssist::Span* a2df = spanOf(assist, QStringLiteral("A2DF"));
    QVERIFY(a2df);
    QVERIFY(indicatorAt(editor, IdAssist::knownIndicator, a2df->position));
    QVERIFY(!indicatorAt(editor, IdAssist::unknownIndicator, a2df->position));
    screenshot(page, QStringLiteral("ids-with-titles"));

    // An ID that names nothing is underlined in red, and gets no title.
    endOnNewLine(editor);
    editor->insert(QStringLiteral("Also \\ref{ZZZZ}."));
    QTRY_VERIFY(spanOf(assist, QStringLiteral("ZZZZ")));
    const IdAssist::Span* zzzz = spanOf(assist, QStringLiteral("ZZZZ"));
    QVERIFY(!zzzz->resolution.found);
    QVERIFY(indicatorAt(editor, IdAssist::unknownIndicator, zzzz->position));
    QVERIFY(annotationOf(editor, editor->lines() - 1).isEmpty());

    // With *Show titles beside IDs* off, no titles.
    m_settings->setShowTitlesBesideIds(false);
    QTRY_VERIFY(annotationOf(editor, 0).isEmpty());
    m_settings->setShowTitlesBesideIds(true);
    QTRY_VERIFY(!annotationOf(editor, 0).isEmpty());
}

void IdTests::ctrlClickOpens()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    page->openProof(QStringLiteral("E6PQ.a"));
    FilePage* file = page->currentPage();
    loom::LatexEditor* editor = file->editor();
    IdAssist* assist = assistOf(file);
    QTRY_VERIFY(spanOf(assist, QStringLiteral("D5MN")));

    // A plain click only moves the cursor.
    const IdAssist::Span* d5mn = spanOf(assist, QStringLiteral("D5MN"));
    QTest::mouseClick(editor->viewport(), Qt::LeftButton, {}, pointIn(assist, editor, *d5mn));
    QTest::qWait(50);
    QCOMPARE(page->openTag(), QStringLiteral("E6PQ"));

    // Ctrl+click on a paper result opens it read-only; on a statement, on its page.
    const IdAssist::Span* rt2 = spanOf(assist, QStringLiteral("7RT2"));
    QVERIFY(rt2);
    QTest::mouseClick(editor->viewport(), Qt::LeftButton, Qt::ControlModifier, pointIn(assist, editor, *rt2));
    QTRY_COMPARE(page->openTag(), QStringLiteral("7RT2"));
    page->back();
    QTRY_COMPARE(page->currentPage()->proofId(), QStringLiteral("E6PQ.a"));
    file = page->currentPage();
    editor = file->editor();
    assist = assistOf(file);
    QTRY_VERIFY(spanOf(assist, QStringLiteral("D5MN")));
    QTest::mouseClick(editor->viewport(), Qt::LeftButton, Qt::ControlModifier, pointIn(assist, editor, *spanOf(assist, QStringLiteral("D5MN"))));
    QTRY_COMPARE(page->openTag(), QStringLiteral("D5MN"));
    QCOMPARE(page->currentPage()->proofId(), QString());

    // On an ID that names nothing, it only flashes the red underline.
    file = page->currentPage();
    editor = file->editor();
    assist = assistOf(file);
    endOnNewLine(editor);
    editor->insert(QStringLiteral("\\ref{ZZZZ}"));
    QTRY_VERIFY(spanOf(assist, QStringLiteral("ZZZZ")));
    const IdAssist::Span* zzzz = spanOf(assist, QStringLiteral("ZZZZ"));
    const int at = zzzz->position;
    QTest::mouseClick(editor->viewport(), Qt::LeftButton, Qt::ControlModifier, pointIn(assist, editor, *zzzz));
    QVERIFY(indicatorAt(editor, IdAssist::flashIndicator, at));
    QTRY_VERIFY(!indicatorAt(editor, IdAssist::flashIndicator, at));
    QCOMPARE(page->openTag(), QStringLiteral("D5MN"));
}

void IdTests::hoverShowsTheCard()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    IdTools* tools = page->idTools();
    page->openStatement(QStringLiteral("C4JK"));
    FilePage* file = page->currentPage();
    loom::LatexEditor* editor = file->editor();
    IdAssist* assist = assistOf(file);
    QTRY_VERIFY(spanOf(assist, QStringLiteral("B3GH")));

    // Resting the mouse on an ID in the text: its card, with the source until a preview is built.
    // Scintilla's dwell timer runs on real pointer moves, which a test can't make on every
    // platform; its notification is sent as Scintilla sends it (the Windows check hovers for real;
    // Wiki/gotchas/qt/qscintilla-dwell-needs-a-real-pointer.md).
    const IdAssist::Span* b3gh = spanOf(assist, QStringLiteral("B3GH"));
    const QPoint at = pointIn(assist, editor, *b3gh);
    emit editor->SCN_DWELLSTART(b3gh->position + 1, at.x(), at.y());
    QVERIFY(tools->hover() && tools->hover()->isVisible());
    loom::IdCard* card = tools->hover()->card();
    QCOMPARE(card->heading()->text(), QStringLiteral("B3GH · The tail of a sequence"));
    QVERIFY(card->kind()->text().startsWith(QStringLiteral("Definition")));
    QVERIFY(!card->source()->isHidden());
    QVERIFY(card->source()->text().contains(QStringLiteral("\\emph{tail}")));
    screenshot(tools->hover(), QStringLiteral("id-hover"));
    // Moving on hides it.
    emit editor->SCN_DWELLEND(b3gh->position + 1, at.x(), at.y());
    QVERIFY(!tools->hover()->isVisible());
    // Text that is no ID shows nothing.
    emit editor->SCN_DWELLSTART(0, 2, at.y());
    QVERIFY(!tools->hover()->isVisible());

    // A chip.
    QWidget* chip = file->statementForm()->statedUsing()->findChild<QWidget*>(QStringLiteral("chip:A2DF"));
    QVERIFY(chip);
    auto* label = chip->findChild<QToolButton*>(QStringLiteral("label"));
    QHelpEvent onChip(QEvent::ToolTip, QPoint(4, 4), label->mapToGlobal(QPoint(4, 4)));
    QApplication::sendEvent(label, &onChip);
    QVERIFY(tools->hover()->isVisible());
    QCOMPARE(tools->hover()->card()->heading()->text(), QStringLiteral("A2DF · Tame sequence"));
    tools->hideHover();

    // A list row: the status is spelled out.
    QTreeWidget* tree = page->list()->tree();
    QTreeWidgetItem* row = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == u"C4JK")
            row = *it;
    }
    QVERIFY(row);
    const QPoint centre = tree->visualItemRect(row).center();
    QHelpEvent onRow(QEvent::ToolTip, centre, tree->viewport()->mapToGlobal(centre));
    QApplication::sendEvent(tree->viewport(), &onRow);
    QVERIFY(tools->hover()->isVisible());
    QCOMPARE(tools->hover()->card()->heading()->text(), QStringLiteral("C4JK · Tails of tame sequences vanish"));
    QVERIFY(tools->hover()->card()->kind()->text().contains(QStringLiteral("proved")));
    QVERIFY(tools->hover()->card()->sentence()->text().contains(QStringLiteral("C4JK")));
    // Leaving the row hides it.
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(tree->viewport(), &leave);
    QVERIFY(!tools->hover()->isVisible());
}

void IdTests::listSearchUsesTheIdSearch()
{
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    loom::StatementList* list = window->statementsPage()->list();
    // Words of the body, in any order and case; a paper result by its paper's key when shown.
    list->search()->setText(QStringLiteral("BIJECTION"));
    QCOMPARE(list->shownIds(), (QStringList{QStringLiteral("F7RS"), QStringLiteral("F7RS.a")}));
    list->filterAction(QStringLiteral("paper-results"), {})->setChecked(true);
    list->search()->setText(QStringLiteral("smithjones lemma"));
    QVERIFY(list->shownIds().contains(QStringLiteral("3WQ6")));
    QVERIFY(!list->shownIds().contains(QStringLiteral("7RT2")));
}

void IdTests::previewsAreBuiltAndCut()
{
    // A preview made of a page with something drawn in the middle is cut to it, with a margin.
    QImage drawn(200, 300, QImage::Format_ARGB32);
    drawn.fill(Qt::white);
    for (int y = 100; y < 120; ++y)
        for (int x = 50; x < 90; ++x)
            drawn.setPixel(x, y, qRgb(0, 0, 0));
    QCOMPARE(loom::cropToContent(drawn, 6).size(), QSize(40 + 12, 20 + 12));
    QImage blank(10, 10, QImage::Format_ARGB32);
    blank.fill(Qt::white);
    QCOMPARE(loom::cropToContent(blank).size(), QSize(10, 10));

    if (QStandardPaths::findExecutable(QStringLiteral("latexmk")).isEmpty()) {
        if (qEnvironmentVariableIntValue("LOOM_REQUIRE_TEX") == 1)
            QFAIL("latexmk not found, and LOOM_REQUIRE_TEX=1");
        QSKIP("latexmk not found");
    }
    m_settings->setLatexmkPath(QString());
    const loomtest::DemoProject demo;
    QVERIFY2(demo.ok(), qPrintable(demo.error()));
    const std::unique_ptr<MainWindow> window = newWindow();
    QVERIFY(openDemo(*window, demo.root()));
    StatementsPage* page = window->statementsPage();
    IdTools* tools = page->idTools();

    // The card asks for the preview; once built (in the background), it shows the picture.
    tools->showHover(QStringLiteral("C4JK"), page, page->mapToGlobal(QPoint(100, 100)));
    loom::IdCard* card = tools->hover()->card();
    QVERIFY(!card->showsPicture());
    QTRY_VERIFY_WITH_TIMEOUT(card->showsPicture(), 120'000);
    const QImage image = tools->previews()->image(QStringLiteral("C4JK"));
    QVERIFY(!image.isNull());
    // Cut to the statement: much smaller than a page.
    QVERIFY(image.height() < image.width());
    QVERIFY(card->source()->isHidden());
    QDir previews(demo.root() + QStringLiteral("/.loom/build/preview"));
    QCOMPARE(previews.entryList({QStringLiteral("C4JK-*.pdf")}, QDir::Files).size(), 1);
    screenshot(tools->hover(), QStringLiteral("id-hover-preview"));
}

QTEST_MAIN(IdTests)
#include "IdTests.moc"
