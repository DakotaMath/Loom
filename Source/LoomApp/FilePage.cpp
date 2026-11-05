#include "LoomApp/FilePage.h"

#include "LoomApp/AppSettings.h"
#include "LoomApp/EditBuffer.h"
#include "LoomApp/FindBar.h"
#include "LoomApp/HeaderForms.h"
#include "LoomApp/LatexEditor.h"
#include "LoomApp/StatementsPage.h"
#include "LoomApp/StatusMarks.h"

#include "LoomCore/FrontMatter.h"

#include <QButtonGroup>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace loom {

namespace {

/// "2026-11-05 14:02", in local time.
QString shortTime(const QDateTime& when)
{
    return when.isValid() ? when.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("an earlier session");
}

/// The file line a body starts on in `text`, or 0 if it has no front matter.
int bodyLineOf(const QByteArray& text, const QString& path)
{
    const FrontMatterSplit split = splitFrontMatter(text, path);
    return split.found ? split.bodyLine : 0;
}

} // namespace

FilePage::FilePage(StatementsPage* owner, const QString& tag, const QString& proofId)
    : QWidget(owner)
    , m_owner(owner)
    , m_tag(tag)
    , m_proofId(proofId)
    , m_heading(new QLabel(this))
    , m_status(new QLabel(this))
    , m_raw(new QToolButton(this))
    , m_showProof(new QToolButton(this))
    , m_showVerbose(new QToolButton(this))
    , m_bars(new QWidget(this))
    , m_barsLayout(new QVBoxLayout(m_bars))
    , m_findBar(new FindBar(this))
    , m_editor(new LatexEditor(this))
    , m_writeVerbose(new QWidget(this))
{
    setObjectName(proofId.isEmpty() ? QStringLiteral("page:") + tag : QStringLiteral("page:") + proofId);
    m_heading->setObjectName(QStringLiteral("pageHeading"));
    QFont font = m_heading->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.15);
    m_heading->setFont(font);
    m_heading->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_status->setObjectName(QStringLiteral("statusLine"));
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_raw->setObjectName(QStringLiteral("raw"));
    m_raw->setText(QStringLiteral("Raw"));
    m_raw->setCheckable(true);
    m_raw->setToolTip(QStringLiteral("Show the whole file, front matter included, in the text editor"));
    m_showProof->setObjectName(QStringLiteral("showProof"));
    m_showProof->setText(QStringLiteral("Proof"));
    m_showProof->setCheckable(true);
    m_showVerbose->setObjectName(QStringLiteral("showVerbose"));
    m_showVerbose->setText(QStringLiteral("Verbose"));
    m_showVerbose->setCheckable(true);
    m_showVerbose->setToolTip(QStringLiteral("Edit the verbose version of this proof"));
    auto* toggle = new QButtonGroup(this);
    toggle->setExclusive(true);
    toggle->addButton(m_showProof);
    toggle->addButton(m_showVerbose);
    m_showProof->setChecked(true);
    const bool proofPage = !proofId.isEmpty();
    m_showProof->setVisible(proofPage);
    m_showVerbose->setVisible(proofPage);
    m_editor->setObjectName(QStringLiteral("editor"));
    m_findBar->setEditor(m_editor);
    m_barsLayout->setContentsMargins(0, 0, 0, 0);
    m_barsLayout->setSpacing(2);

    // In place of the editor when Verbose is on and the proof has no verbose version yet.
    m_writeVerbose->setObjectName(QStringLiteral("noVerbose"));
    auto* writeLabel = new QLabel(QStringLiteral("This proof has no verbose version: a longer write-up of it, with every step spelled out."),
                                  m_writeVerbose);
    writeLabel->setWordWrap(true);
    auto* writeButton = new QPushButton(QStringLiteral("Write a verbose version"), m_writeVerbose);
    writeButton->setObjectName(QStringLiteral("writeVerbose"));
    auto* writeLayout = new QVBoxLayout(m_writeVerbose);
    writeLayout->addWidget(writeLabel);
    writeLayout->addWidget(writeButton, 0, Qt::AlignLeft);
    writeLayout->addStretch(1);
    m_writeVerbose->hide();
    connect(writeButton, &QPushButton::clicked, this, [this] { m_owner->writeVerbose(m_proofId); });

    auto* headingRow = new QHBoxLayout;
    headingRow->addWidget(m_heading, 1);
    headingRow->addWidget(m_showProof);
    headingRow->addWidget(m_showVerbose);
    headingRow->addWidget(m_raw);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("pageHeader"));
    auto* headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(10, 8, 10, 6);
    headerLayout->setSpacing(4);
    headerLayout->addLayout(headingRow);
    headerLayout->addWidget(m_status);
    if (proofPage) {
        m_proofForm = new ProofForm(header);
        headerLayout->addWidget(m_proofForm);
        connect(m_proofForm, &ProofForm::edited, this, [this] { m_owner->bufferEdited(m_main); });
        connect(m_proofForm, &ProofForm::failedChosen, this, [this](const QString& previous) { m_owner->chooseFailed(m_main, previous); });
        connect(m_proofForm, &ProofForm::addPremiseRequested, this, [this] { m_owner->addReference(m_main, QStringLiteral("premises")); });
        connect(m_proofForm, &ProofForm::idClicked, m_owner, &StatementsPage::openId);
    } else {
        m_statementForm = new StatementForm(header);
        headerLayout->addWidget(m_statementForm);
        connect(m_statementForm, &StatementForm::edited, this, [this] { m_owner->bufferEdited(m_main); });
        connect(m_statementForm, &StatementForm::addStatedUsingRequested, this,
                [this] { m_owner->addReference(m_main, QStringLiteral("statedUsing")); });
        connect(m_statementForm, &StatementForm::chooseSupersededByRequested, this,
                [this] { m_owner->addReference(m_main, QStringLiteral("supersededBy")); });
        connect(m_statementForm, &StatementForm::idClicked, m_owner, &StatementsPage::openId);
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);
    layout->addWidget(m_bars);
    layout->addWidget(m_findBar);
    layout->addWidget(m_editor, 1);
    layout->addWidget(m_writeVerbose, 1);

    connect(m_raw, &QToolButton::clicked, this, [this](bool on) {
        if (EditBuffer* shown = shownBuffer())
            m_owner->setRaw(shown, on);
    });
    connect(m_showVerbose, &QToolButton::clicked, this, [this] { setShowingVerbose(true); });
    connect(m_showProof, &QToolButton::clicked, this, [this] { setShowingVerbose(false); });
    connect(m_editor, &LatexEditor::textChanged, this, [this] {
        if (m_updating || !m_editorBuffer || m_editorBuffer->readOnly())
            return;
        m_editorBuffer->setEditorText(m_editor->text());
        m_owner->bufferEdited(m_editorBuffer);
    });
    connect(m_editor, &LatexEditor::focusLost, m_owner, &StatementsPage::writeDrafts);
}

void FilePage::bind(EditBuffer* main, EditBuffer* verbose)
{
    m_main = main;
    m_verbose = verbose;
    refresh();
}

EditBuffer* FilePage::shownBuffer() const
{
    if (m_showingVerbose)
        return m_verbose;
    return m_main;
}

void FilePage::setShowingVerbose(bool verbose)
{
    if (m_proofId.isEmpty())
        return;
    storeView();
    m_showingVerbose = verbose;
    m_showVerbose->setChecked(verbose);
    m_showProof->setChecked(!verbose);
    refresh();
}

QWidget* FilePage::bar(const QString& name) const
{
    return m_bars->findChild<QWidget*>(name, Qt::FindDirectChildrenOnly);
}

void FilePage::showNote(const QString& note)
{
    m_note = note;
    refreshBars();
}

void FilePage::refresh()
{
    const std::shared_ptr<const ProjectSnapshot> snapshot = m_owner->snapshot();
    const ProjectSnapshot* snap = snapshot.get();
    const StatementNode* node = snap ? snap->graph.statement(m_tag) : nullptr;
    EditBuffer* shown = shownBuffer();
    const bool dark = darkTheme();

    if (m_proofId.isEmpty()) {
        if (m_main && m_main->kind() == FileKind::Stub) {
            m_heading->setText(node ? QStringLiteral("%1 · %2 · %3").arg(m_tag, node->paper, node->title) : m_tag);
        } else {
            const QString kind = m_main && !m_main->raw() ? m_main->statement().kind : (node ? node->kind : QString());
            const QString title = m_main && !m_main->raw() ? m_main->statement().title : (node ? node->title : QString());
            m_heading->setText(QStringLiteral("%1 · %2 · %3").arg(m_tag, kindTitle(kind), title));
        }
    } else {
        m_heading->setText(QStringLiteral("%1 · proof of %2").arg(m_proofId, m_tag));
    }
    // The status mark and LoomCore's sentence, the same as `loom status <id>`, which spells the
    // status word out ("4KX7 conditional: …"), so the colour is never the only signal.
    if (node) {
        const QString mark = node->status == Status::None
                                 ? QString()
                                 : QStringLiteral("<span style='color:%1'>%2</span> ")
                                       .arg(statusColor(node->status, dark).name(), statusHollow(node->status) ? QStringLiteral("○") : QStringLiteral("●"));
        m_status->setText(mark + (node->explanation.isEmpty() ? QStringLiteral("<b>%1</b>").arg(statusWord(node->status).toHtmlEscaped())
                                                              : node->explanation.toHtmlEscaped()));
    } else {
        m_status->setText(QStringLiteral("Loom can't read this statement's front matter, so it has no status yet."));
    }

    const bool stub = m_main && m_main->kind() == FileKind::Stub;
    const bool raw = shown && shown->raw();
    m_raw->setVisible(!stub && !m_showingVerbose);
    m_raw->setChecked(raw);
    if (m_statementForm) {
        m_statementForm->setVisible(!raw && !stub && m_main);
        if (m_main && !raw && !stub)
            m_statementForm->setBuffer(m_main, snap);
    }
    if (m_proofForm) {
        m_proofForm->setVisible(!raw && !m_showingVerbose && m_main);
        if (m_main && !m_main->raw())
            m_proofForm->setBuffer(m_main, snap);
    }
    m_writeVerbose->setVisible(m_showingVerbose && !m_verbose);
    m_editor->setVisible(shown != nullptr);
    m_editor->setReadOnly(!shown || shown->readOnly());
    m_findBar->setEditor(m_editor);
    loadEditor();
    refreshBars();
    markProblems();
}

void FilePage::loadEditor()
{
    EditBuffer* shown = shownBuffer();
    if (!shown) {
        m_editorBuffer = nullptr;
        return;
    }
    const QString text = shown->editorText();
    if (m_editorBuffer == shown && m_editor->text() == text)
        return;
    const bool same = m_editorBuffer == shown;
    int line = shown->cursorLine;
    int index = shown->cursorIndex;
    int scroll = shown->scroll;
    if (same) {
        // The file reloaded under the editor: keep the cursor and scroll where they were.
        m_editor->getCursorPosition(&line, &index);
        scroll = m_editor->verticalScrollBar()->value();
    }
    m_updating = true;
    m_editor->setText(text);
    m_editorBuffer = shown;
    const int lastLine = std::max(0, m_editor->lines() - 1);
    line = std::clamp(line, 0, lastLine);
    m_editor->setCursorPosition(line, std::clamp(index, 0, static_cast<int>(m_editor->text(line).size())));
    m_editor->verticalScrollBar()->setValue(scroll);
    if (!same)
        m_editor->setModified(false);
    m_updating = false;
}

void FilePage::storeView()
{
    if (!m_editorBuffer)
        return;
    m_editor->getCursorPosition(&m_editorBuffer->cursorLine, &m_editorBuffer->cursorIndex);
    m_editorBuffer->scroll = m_editor->verticalScrollBar()->value();
}

void FilePage::goToEditorLine(int line)
{
    m_editor->goToLine(line);
    m_editor->setFocus();
}

void FilePage::markProblems()
{
    EditBuffer* shown = shownBuffer();
    QList<QPair<int, Severity>> lines;
    if (!shown) {
        m_editor->setProblemLines(lines);
        return;
    }
    // Problem lines are file lines; in the form the editor shows the body only, so they move up
    // by the lines of the front matter, and front-matter lines aren't marked.
    const auto add = [&](const Problem& problem, int bodyLine) {
        int line = problem.line;
        if (!shown->raw() && shown->kind() != FileKind::Stub) {
            if (bodyLine <= 0 || line < bodyLine)
                return;
            line = line - bodyLine + 1;
        }
        lines.append({line, problem.severity});
    };
    if (const std::shared_ptr<const ProjectSnapshot> snapshot = m_owner->snapshot(); snapshot && !shown->dirty()) {
        const int bodyLine = shown->loadedBodyLine();
        for (const Problem& problem : snapshot->problems) {
            if (problem.file == shown->path())
                add(problem, bodyLine);
        }
    }
    // The latest build's problems, while the text is what was built (unsaved edits move lines).
    if (!shown->dirty()) {
        const int bodyLine = shown->kind() == FileKind::Verbose ? std::max(1, shown->loadedBodyLine()) : shown->loadedBodyLine();
        for (const Problem& problem : m_owner->buildProblems()) {
            if (problem.file == shown->path())
                add(problem, bodyLine);
        }
    }
    const int refusedBodyLine = bodyLineOf(shown->text(), shown->path());
    for (const Problem& problem : shown->refusalProblems()) {
        if (problem.file == shown->path())
            add(problem, shown->kind() == FileKind::Verbose ? 1 : refusedBodyLine);
    }
    m_editor->setProblemLines(lines);
}

QWidget* FilePage::addBar(const QString& name, const QString& text,
                          const QList<std::tuple<QString, QString, std::function<void()>>>& buttons)
{
    auto* bar = new QWidget(m_bars);
    bar->setObjectName(name);
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setStyleSheet(QStringLiteral("#%1 { background: palette(alternate-base); border-bottom: 1px solid palette(mid); }").arg(name));
    auto* label = new QLabel(text, bar);
    label->setObjectName(QStringLiteral("text"));
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(10, 5, 10, 5);
    layout->addWidget(label, 1);
    for (const auto& [object, caption, action] : buttons) {
        auto* button = new QPushButton(caption, bar);
        button->setObjectName(object);
        connect(button, &QPushButton::clicked, this, action);
        layout->addWidget(button);
    }
    m_barsLayout->addWidget(bar);
    // Shown now, not when the event loop next runs (as a widget added to a layout on screen is).
    bar->show();
    return bar;
}

void FilePage::refreshBars()
{
    // Later, not now: the click that led here may come from a button on one of these bars.
    while (QLayoutItem* item = m_barsLayout->takeAt(0)) {
        if (QWidget* old = item->widget()) {
            old->setObjectName(QString());
            old->hide();
            old->deleteLater();
        }
        delete item;
    }
    EditBuffer* shown = shownBuffer();
    if (!m_note.isEmpty())
        addBar(QStringLiteral("noteBar"), m_note, {{QStringLiteral("dismiss"), QStringLiteral("OK"), [this] { showNote({}); }}});
    if (!shown) {
        m_bars->setVisible(m_barsLayout->count() > 0);
        return;
    }
    const QString name = QFileInfo(shown->path()).fileName();
    const QString when = shortTime(shown->recoveredAt());
    switch (shown->recovery()) {
    case Recovery::Same:
        addBar(QStringLiteral("recoveryBar"), QStringLiteral("Recovered unsaved edits from %1: Loom closed without saving them.").arg(when),
               {{QStringLiteral("keep"), QStringLiteral("Keep"), [this, shown] { m_owner->keepRecovered(shown); }},
                {QStringLiteral("discard"), QStringLiteral("Discard"), [this, shown] { m_owner->discardRecovered(shown); }}});
        break;
    case Recovery::Changed:
        addBar(QStringLiteral("recoveryBar"), QStringLiteral("Recovered unsaved edits from %1, but the file has changed since.").arg(when),
               {{QStringLiteral("compare"), QStringLiteral("Compare"), [this, shown] { m_owner->compare(shown); }},
                {QStringLiteral("keepMine"), QStringLiteral("Keep mine"), [this, shown] { m_owner->keepMine(shown); }},
                {QStringLiteral("discardMine"), QStringLiteral("Discard mine"), [this, shown] { m_owner->discardRecovered(shown); }}});
        break;
    case Recovery::Gone:
        addBar(QStringLiteral("recoveryBar"), QStringLiteral("Recovered unsaved edits from %1, but the file has been deleted.").arg(when),
               {{QStringLiteral("restore"), QStringLiteral("Restore it with my text"), [this, shown] { m_owner->restore(shown); }},
                {QStringLiteral("discard"), QStringLiteral("Discard"), [this, shown] { m_owner->discardRecovered(shown); }}});
        break;
    case Recovery::None:
        break;
    }
    switch (shown->outside()) {
    case Outside::Changed:
        addBar(QStringLiteral("outsideBar"), QStringLiteral("%1 changed on disk while you were editing it.").arg(name),
               {{QStringLiteral("compare"), QStringLiteral("Compare"), [this, shown] { m_owner->compare(shown); }},
                {QStringLiteral("keepMine"), QStringLiteral("Keep mine"), [this, shown] { m_owner->keepMine(shown); }},
                {QStringLiteral("takeTheirs"), QStringLiteral("Take the file's"), [this, shown] { m_owner->takeTheirs(shown); }}});
        break;
    case Outside::Deleted:
        addBar(QStringLiteral("outsideBar"), QStringLiteral("%1 was deleted on disk.").arg(name),
               {{QStringLiteral("restore"), QStringLiteral("Restore it"), [this, shown] { m_owner->restore(shown); }},
                {QStringLiteral("close"), QStringLiteral("Close"), [this, shown] { m_owner->closeDeleted(shown); }}});
        break;
    case Outside::None:
        break;
    }
    if (!shown->refusalMessage().isEmpty()) {
        QList<std::tuple<QString, QString, std::function<void()>>> buttons;
        if (!shown->refusalProblems().empty())
            buttons.append({QStringLiteral("showProblems"), QStringLiteral("Show problems"), [this, shown] { m_owner->showRefusalProblems(shown); }});
        addBar(QStringLiteral("refusalBar"), shown->refusalMessage(), buttons);
    }
    if (shown->kind() == FileKind::Verbose && shown->exists()) {
        const std::shared_ptr<const ProjectSnapshot> snapshot = m_owner->snapshot();
        const ProofNode* proof = snapshot ? snapshot->graph.proof(m_proofId) : nullptr;
        if (proof && !proof->checking.fingerprint.isEmpty() && shown->loadedVerbose().fingerprint != proof->checking.fingerprint)
            addBar(QStringLiteral("staleBar"), QStringLiteral("Written from an older version of this proof."),
                   {{QStringLiteral("markUpToDate"), QStringLiteral("Mark up to date"), [this, shown] { m_owner->markVerboseUpToDate(shown); }}});
    }
    if (shown->kind() == FileKind::Stub) {
        const std::shared_ptr<const ProjectSnapshot> snapshot = m_owner->snapshot();
        const StatementNode* node = snapshot ? snapshot->graph.statement(m_tag) : nullptr;
        addBar(QStringLiteral("stubBar"),
               QStringLiteral("A result from %1, shown read-only: editing papers comes with the Library (M6).").arg(node ? node->paper : QString()), {});
    }
    m_bars->setVisible(m_barsLayout->count() > 0);
}

} // namespace loom
