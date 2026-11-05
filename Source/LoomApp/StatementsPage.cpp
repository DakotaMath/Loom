#include "LoomApp/StatementsPage.h"

#include "LoomApp/AppSettings.h"
#include "LoomApp/ChecksPanel.h"
#include "LoomApp/FilePage.h"
#include "LoomApp/FindBar.h"
#include "LoomApp/HeaderForms.h"
#include "LoomApp/IdAssist.h"
#include "LoomApp/IdPicker.h"
#include "LoomApp/IdPreviews.h"
#include "LoomApp/IdTools.h"
#include "LoomApp/Lane.h"
#include "LoomApp/LatexEditor.h"
#include "LoomApp/NewItemDialogs.h"
#include "LoomApp/PdfPane.h"
#include "LoomApp/ProjectSession.h"
#include "LoomApp/StatementList.h"
#include "LoomApp/TextDiff.h"

#include "LoomCore/Ids.h"
#include "LoomCore/ProjectFiles.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSplitter>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace loom {

namespace {

/// Asks Dakota to confirm one of his clicks (Specs/m2-statements-review.md, "Dakota's clicks":
/// "a dialog … has buttons named for the action (never "OK"), and starts with **Cancel** focused,
/// so Enter alone never confirms. Cancelling writes nothing").
/// @param answer Called once, with whether he confirmed. Side effects: opens a window-modal box.
void confirm(QWidget* parent, const QString& objectName, const QString& title, const QString& text, const QString& action,
             std::function<void(bool)> answer)
{
    auto* box = new QMessageBox(QMessageBox::Question, title, text, QMessageBox::NoButton, parent);
    box->setObjectName(objectName);
    box->setTextFormat(Qt::PlainText);
    box->setAttribute(Qt::WA_DeleteOnClose);
    QPushButton* yes = box->addButton(action, QMessageBox::AcceptRole);
    yes->setObjectName(QStringLiteral("confirm"));
    QPushButton* cancel = box->addButton(QMessageBox::Cancel);
    cancel->setObjectName(QStringLiteral("cancel"));
    box->setDefaultButton(cancel);
    box->setEscapeButton(cancel);
    QObject::connect(box, &QMessageBox::finished, box, [box, yes, answer] { answer(box->clickedButton() == yes); });
    box->open();
    cancel->setFocus();
}

/// "A, B and C".
QString listed(const QStringList& items)
{
    if (items.size() < 2)
        return items.join(QString());
    return items.first(items.size() - 1).join(QStringLiteral(", ")) + QStringLiteral(" and ") + items.last();
}

/// The per-project state's key for a file's cursor: the path, percent-encoded, since QSettings
/// reads `/` as a group.
QString cursorKey(const QString& path)
{
    return QStringLiteral("cursors/") + QString::fromLatin1(QUrl::toPercentEncoding(path));
}

/// Asks Dakota for a `needs-changes` and what needs to change (Specs/m2-statements-review.md,
/// "Dakota's clicks": "For needs-changes, the text box for changes (required)"). Its confirming
/// button stays disabled while the box is empty. Cancel is the default button, so Enter alone
/// never confirms; the box has the focus, since the text is required, and Enter there is a new line.
/// @param answer Called once: the text, or nothing if he cancelled. Side effects: opens a window-modal dialog.
void askChanges(QWidget* parent, const QString& text, std::function<void(std::optional<QString>)> answer)
{
    auto* dialog = new QDialog(parent);
    dialog->setObjectName(QStringLiteral("needsChangesDialog"));
    dialog->setWindowTitle(QStringLiteral("Needs changes"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    auto* label = new QLabel(text, dialog);
    label->setObjectName(QStringLiteral("text"));
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    auto* prompt = new QLabel(QStringLiteral("What needs to change? (required)"), dialog);
    auto* changes = new QPlainTextEdit(dialog);
    changes->setObjectName(QStringLiteral("changes"));
    changes->setTabChangesFocus(true);
    auto* buttons = new QDialogButtonBox(dialog);
    QPushButton* yes = buttons->addButton(QStringLiteral("Record needs-changes"), QDialogButtonBox::AcceptRole);
    yes->setObjectName(QStringLiteral("confirm"));
    yes->setAutoDefault(false);
    yes->setEnabled(false);
    QPushButton* cancel = buttons->addButton(QDialogButtonBox::Cancel);
    cancel->setObjectName(QStringLiteral("cancel"));
    cancel->setDefault(true);
    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(label);
    layout->addWidget(prompt);
    layout->addWidget(changes, 1);
    layout->addWidget(buttons);
    dialog->resize(520, 320);
    QObject::connect(changes, &QPlainTextEdit::textChanged, yes, [changes, yes] { yes->setEnabled(!changes->toPlainText().trimmed().isEmpty()); });
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    QObject::connect(dialog, &QDialog::finished, dialog, [changes, answer](int result) {
        if (result == QDialog::Accepted && !changes->toPlainText().trimmed().isEmpty())
            answer(changes->toPlainText());
        else
            answer(std::nullopt);
    });
    dialog->open();
    changes->setFocus();
}

/// What a check's dialog asks (Specs/m2-statements-review.md, "Dakota's clicks": *"Record your
/// pass of 4KX7.a as it is now? With the agent's pass of 2026-11-04 this makes 4KX7.a checked:
/// 4KX7 becomes proved; P3QA becomes proved."*). What the check does comes from LoomCore: the
/// proof's standing before (the snapshot) and after (`asked`, its refusal for want of the click),
/// and the status changes it carries.
QString checkQuestion(const QString& proofId, const QString& verdict, const ProofNode& before, const CheckAdded& asked)
{
    const CheckStanding& was = before.checking.standing;
    const CheckStanding& will = asked.standing;
    QString effect;
    if (will.checked && !was.checked) {
        // The counting agent check is one already in the history: the new entry is Dakota's.
        const std::vector<Check>& checks = before.checking.checks;
        const qsizetype agent = will.countingAgent.value_or(-1);
        effect = agent >= 0 && agent < static_cast<qsizetype>(checks.size())
                     ? QStringLiteral("With the agent's pass of %1 this makes %2 checked")
                           .arg(checks[static_cast<size_t>(agent)].date.date().toString(Qt::ISODate), proofId)
                     : QStringLiteral("This makes %1 checked").arg(proofId);
    } else if (was.checked && !will.checked) {
        effect = QStringLiteral("This makes %1 no longer checked").arg(proofId);
    } else if (was.supports && !will.supports) {
        effect = QStringLiteral("This makes %1 stop supporting %2").arg(proofId, before.proves);
    }
    const QString statuses = statusBecomes(asked.change.statusChanges);
    QString text = QStringLiteral("Record your %1 of %2 as it is now?").arg(verdict, proofId);
    if (effect.isEmpty())
        text += statuses.isEmpty() ? QStringLiteral(" No status changes.") : QStringLiteral(" %1.").arg(statuses);
    else
        text += statuses.isEmpty() ? QStringLiteral(" %1; no status changes.").arg(effect) : QStringLiteral(" %1: %2.").arg(effect, statuses);
    return text;
}

} // namespace

QString Place::toString() const
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6").arg(tag, page).arg(verbose ? 1 : 0).arg(line).arg(index).arg(scroll);
}

Place Place::fromString(const QString& text)
{
    const QStringList parts = text.split(u'|');
    Place place;
    if (parts.size() != 6)
        return place;
    place.tag = parts[0];
    place.page = parts[1];
    place.verbose = parts[2] == u"1";
    place.line = parts[3].toInt();
    place.index = parts[4].toInt();
    place.scroll = parts[5].toInt();
    return place;
}

bool fileRole(const QString& path, FileRole& role)
{
    static const QRegularExpression statement(QStringLiteral("^Statements/([^/]+)\\.tex$"));
    static const QRegularExpression verbose(QStringLiteral("^Proofs/([^/]+)/([^/]+)\\.verbose\\.tex$"));
    static const QRegularExpression proof(QStringLiteral("^Proofs/([^/]+)/([^/]+)\\.tex$"));
    static const QRegularExpression stub(QStringLiteral("^Papers/[^/]+/Statements/([^/]+)\\.toml$"));
    if (const QRegularExpressionMatch m = statement.match(path); m.hasMatch()) {
        role = {FileKind::Statement, m.captured(1), m.captured(1)};
        return true;
    }
    if (const QRegularExpressionMatch m = verbose.match(path); m.hasMatch()) {
        role = {FileKind::Verbose, m.captured(2), m.captured(1)};
        return true;
    }
    if (const QRegularExpressionMatch m = proof.match(path); m.hasMatch()) {
        role = {FileKind::Proof, m.captured(2), m.captured(1)};
        return true;
    }
    if (const QRegularExpressionMatch m = stub.match(path); m.hasMatch()) {
        role = {FileKind::Stub, m.captured(1), m.captured(1)};
        return true;
    }
    return false;
}

StatementsPage::StatementsPage(AppSettings* settings, Lane* lane, Lane* buildLane, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_lane(lane)
    , m_list(new StatementList(this))
    , m_splitter(new QSplitter(Qt::Horizontal, this))
    , m_editorArea(new QWidget(this))
    , m_formDraftsBar(new QWidget(this))
    , m_empty(new QLabel(this))
    , m_tabs(new QTabWidget(this))
    , m_newProofButton(new QToolButton(this))
    , m_pdf(new PdfPane(settings, buildLane, this))
    , m_previews(new IdPreviews(settings, buildLane, this))
    , m_ids(new IdTools(settings, m_previews, this))
{
    setObjectName(QStringLiteral("statementsPage"));
    m_tabs->setObjectName(QStringLiteral("pages"));
    m_tabs->setDocumentMode(true);
    m_newProofButton->setObjectName(QStringLiteral("newProof"));
    m_newProofButton->setText(QStringLiteral("+ Proof"));
    m_newProofButton->setToolTip(QStringLiteral("New proof of this statement (Ctrl+Shift+N)"));
    m_newProofButton->setAutoRaise(true);
    m_tabs->setCornerWidget(m_newProofButton, Qt::TopRightCorner);
    m_empty->setObjectName(QStringLiteral("noStatement"));
    m_empty->setText(QStringLiteral("Choose a statement on the left, or make a new one with Ctrl+N."));
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setForegroundRole(QPalette::PlaceholderText);
    m_formDraftsBar->setObjectName(QStringLiteral("formDraftsBar"));
    m_formDraftsLayout = new QVBoxLayout(m_formDraftsBar);
    m_formDraftsLayout->setContentsMargins(0, 0, 0, 0);
    m_formDraftsBar->hide();

    auto* editorLayout = new QVBoxLayout(m_editorArea);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    editorLayout->addWidget(m_formDraftsBar);
    editorLayout->addWidget(m_empty, 1);
    editorLayout->addWidget(m_tabs, 1);
    m_tabs->hide();

    m_splitter->addWidget(m_list);
    m_splitter->addWidget(m_editorArea);
    m_splitter->addWidget(m_pdf);
    m_splitter->setStretchFactor(0, 2);
    m_splitter->setStretchFactor(1, 5);
    m_splitter->setStretchFactor(2, 3);
    m_splitter->setChildrenCollapsible(false);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_splitter);

    m_draftTimer.setSingleShot(true);
    m_draftTimer.setInterval(draftDelayMs);
    connect(&m_draftTimer, &QTimer::timeout, this, &StatementsPage::writeDrafts);
    m_formDraftTimer.setSingleShot(true);
    m_formDraftTimer.setInterval(draftDelayMs);
    connect(&m_formDraftTimer, &QTimer::timeout, this, &StatementsPage::writeFormDrafts);

    connect(m_list, &StatementList::statementActivated, this, [this](const QString& tag) { openStatement(tag); });
    connect(m_list, &StatementList::proofActivated, this, &StatementsPage::openProof);
    connect(m_list, &StatementList::stubActivated, this, &StatementsPage::openStub);
    connect(m_list, &StatementList::newStatementClicked, this, &StatementsPage::newStatement);
    connect(m_newProofButton, &QToolButton::clicked, this, &StatementsPage::newProof);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this] {
        if (FilePage* page = currentPage())
            page->refresh();
        updatePdfTarget();
    });
    // Double-clicking the PDF opens the source at that line (inverse SyncTeX).
    connect(m_pdf, &PdfPane::placeActivated, this, &StatementsPage::showFileLine);
    connect(m_pdf, &PdfPane::outputLine, this, &StatementsPage::outputLine);
    connect(m_settings, &AppSettings::changed, this, &StatementsPage::applyEditorFont);
    qApp->installEventFilter(this);

    // IDs (Specs/m2-statements-review.md, "IDs in the editor"): Ctrl+click and the card open them;
    // ties in the picker go to what was opened most recently.
    connect(m_ids, &IdTools::openRequested, this, &StatementsPage::openId);
    m_ids->setRecent([this] {
        QStringList recent;
        const auto add = [&](const QString& id) {
            if (!id.isEmpty() && !recent.contains(id))
                recent << id;
        };
        add(m_tag);
        for (auto it = m_back.rbegin(); it != m_back.rend(); ++it) {
            add(it->page);
            add(it->tag);
        }
        return recent;
    });
}

StatementsPage::~StatementsPage()
{
    qApp->removeEventFilter(this);
    storeViews();
    saveState();
}

void StatementsPage::setSession(ProjectSession* session)
{
    if (m_session) {
        storeViews();
        saveState();
        // Text typed in the last second isn't in a recovery file yet: write it before the buffers go.
        writeDrafts();
        disconnect(m_session, nullptr, this, nullptr);
    }
    // Buffers go with their project; their recovery files stay, for next time.
    m_draftTimer.stop();
    m_formDraftTimer.stop();
    delete m_newStatement;
    delete m_newProof;
    while (m_tabs->count() > 0) {
        QWidget* old = m_tabs->widget(0);
        m_tabs->removeTab(0);
        delete old;
    }
    m_buffers.clear();
    m_clicks.clear();
    m_back.clear();
    m_forward.clear();
    m_formDrafts.clear();
    m_tag.clear();
    m_stub = false;
    m_tabs->hide();
    m_empty->show();
    showFormDrafts();
    m_pdf->setSession(nullptr, nullptr);
    m_buildProblems.clear();
    m_state.reset();
    m_session = session;
    m_snapshot = session ? session->snapshot() : nullptr;
    m_ids->setSnapshot(m_snapshot);
    m_list->setSnapshot(m_snapshot);
    updateUnsaved();
    if (!session)
        return;

    m_state = std::make_unique<QSettings>(projectStateFile(m_settings->paths(), session->root()), QSettings::IniFormat);
    m_pdf->setSession(session, m_state.get());
    for (const QString& text : m_state->value(QStringLiteral("history/back")).toStringList()) {
        if (const Place place = Place::fromString(text); !place.tag.isEmpty())
            m_back.push_back(place);
    }
    for (const QString& text : m_state->value(QStringLiteral("history/forward")).toStringList()) {
        if (const Place place = Place::fromString(text); !place.tag.isEmpty())
            m_forward.push_back(place);
    }
    connect(session, &ProjectSession::snapshotChanged, this, &StatementsPage::snapshotChanged);
    // Dakota's check buttons wait while the snapshot is older than the disk.
    connect(session, &ProjectSession::becameStale, this, &StatementsPage::refreshClicks);

    const QString root = session->root();
    m_lane->run(this, [root] { return listDrafts(root); }, [this, session](std::vector<Draft> drafts) {
        if (m_session != session)
            return;
        offerDrafts(drafts);
    });
}

void StatementsPage::offerDrafts(const std::vector<Draft>& drafts)
{
    QString first;
    QString firstPage;
    bool firstVerbose = false;
    for (const Draft& draft : drafts) {
        if (draft.path.startsWith(QStringLiteral("new/"))) {
            m_formDrafts.push_back(draft);
            continue;
        }
        FileRole role;
        if (!fileRole(draft.path, role) || role.kind == FileKind::Stub)
            continue;
        EditBuffer* buffer = ensureBuffer(draft.path, role.kind, role.id);
        const bool exists = m_snapshot->files.contains(draft.path);
        const QByteArray now = m_snapshot->files.value(draft.path);
        // The table of *On opening a project*: the file is still `base`, changed since, or gone.
        if (exists && fileBase(now) == draft.base) {
            buffer->setText(draft.text);
            if (!buffer->dirty()) {
                // Nothing in it that the file doesn't say already.
                dropDraft(buffer);
                continue;
            }
            buffer->setRecovery(Recovery::Same, draft.written);
        } else if (exists) {
            buffer->setText(draft.text);
            buffer->setRecovery(Recovery::Changed, draft.written);
        } else {
            buffer->rebase({}, false);
            buffer->setText(draft.text);
            buffer->setRecovery(Recovery::Gone, draft.written);
        }
        buffer->setDrafted(draft.text);
        if (first.isEmpty()) {
            first = role.tag;
            firstPage = role.kind == FileKind::Statement ? QString() : role.id;
            firstVerbose = role.kind == FileKind::Verbose;
        }
    }
    showFormDrafts();
    updateUnsaved();
    if (!first.isEmpty()) {
        openStatement(first, firstPage, false);
        if (firstVerbose)
            if (FilePage* page = this->page(firstPage))
                page->setShowingVerbose(true);
    } else if (m_state) {
        // Otherwise the statement open last time, where it was.
        const QString tag = m_state->value(QStringLiteral("statements/open")).toString();
        if (!tag.isEmpty() && (m_snapshot->contents.statement(tag) || m_snapshot->contents.stub(tag))) {
            Place place = Place::fromString(m_state->value(QStringLiteral("statements/place")).toString());
            if (place.tag != tag) {
                place = Place();
                place.tag = tag;
            }
            goTo(place);
        }
    }
    emit recoveryOffered();
}

void StatementsPage::showFormDrafts()
{
    while (QLayoutItem* item = m_formDraftsLayout->takeAt(0)) {
        if (QWidget* old = item->widget()) {
            old->setObjectName(QString());
            old->hide();
            old->deleteLater();
        }
        delete item;
    }
    for (const Draft& draft : m_formDrafts) {
        const bool statement = draft.path == NewStatementDialog::draftName();
        const QString what = statement ? QStringLiteral("a new statement")
                                       : QStringLiteral("a new proof of %1").arg(draft.path.mid(10).chopped(4));
        auto* bar = new QWidget(m_formDraftsBar);
        bar->setObjectName(QStringLiteral("formDraft:") + draft.path);
        auto* label = new QLabel(QStringLiteral("Recovered %1 you hadn't created yet, from %2.")
                                     .arg(what, draft.written.isValid() ? draft.written.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                                                                        : QStringLiteral("an earlier session")),
                                 bar);
        label->setWordWrap(true);
        auto* open = new QPushButton(QStringLiteral("Open it"), bar);
        open->setObjectName(QStringLiteral("open"));
        auto* discard = new QPushButton(QStringLiteral("Discard"), bar);
        discard->setObjectName(QStringLiteral("discard"));
        auto* layout = new QHBoxLayout(bar);
        layout->setContentsMargins(10, 5, 10, 5);
        layout->addWidget(label, 1);
        layout->addWidget(open);
        layout->addWidget(discard);
        m_formDraftsLayout->addWidget(bar);
        bar->show();
        connect(open, &QPushButton::clicked, this, [this, draft] { openFormDraft(draft); });
        connect(discard, &QPushButton::clicked, this, [this, path = draft.path] {
            std::erase_if(m_formDrafts, [&](const Draft& d) { return d.path == path; });
            if (m_session) {
                const QString root = m_session->root();
                m_lane->run(this, [root, path] { return removeDraft(root, path); }, [](const QString&) {});
            }
            showFormDrafts();
        });
    }
    m_formDraftsBar->setVisible(!m_formDrafts.empty());
}

void StatementsPage::openFormDraft(const Draft& draft)
{
    std::erase_if(m_formDrafts, [&](const Draft& d) { return d.path == draft.path; });
    showFormDrafts();
    if (draft.path == NewStatementDialog::draftName()) {
        newStatement();
        if (m_newStatement)
            m_newStatement->fill(draft.text);
        return;
    }
    const QString tag = draft.path.mid(10).chopped(4); // new/proof-<tag>.txt
    openStatement(tag);
    newProof();
    if (m_newProof)
        m_newProof->fill(draft.text);
}

void StatementsPage::snapshotChanged()
{
    if (!m_session)
        return;
    m_snapshot = m_session->snapshot();
    m_ids->setSnapshot(m_snapshot);
    m_list->setSnapshot(m_snapshot);
    checkOutside();
    if (!m_pendingOpen.isEmpty()) {
        const QString id = m_pendingOpen;
        if (const std::optional<ProofIdParts> parts = splitProofId(id); parts && m_snapshot->contents.proof(id)) {
            m_pendingOpen.clear();
            openStatement(parts->tag, id);
            emit created(id);
        } else if (m_snapshot->contents.statement(id)) {
            m_pendingOpen.clear();
            openStatement(id);
            emit created(id);
        }
    }
    if (!m_tag.isEmpty() && !m_stub) {
        QStringList shown;
        for (int i = 1; i < m_tabs->count(); ++i)
            shown << static_cast<FilePage*>(m_tabs->widget(i))->proofId();
        if (shown != pageIds()) {
            const FilePage* current = currentPage();
            rebuildPages(current ? current->proofId() : QString());
        }
    }
    refreshPages();
    updateUnsaved();
    m_pdf->snapshotChanged(m_snapshot);
}

EditBuffer* StatementsPage::ensureBuffer(const QString& path, FileKind kind, const QString& id)
{
    if (const auto it = m_buffers.find(path); it != m_buffers.end())
        return it->second.get();
    auto buffer = std::make_unique<EditBuffer>(path, kind, id);
    if (m_snapshot)
        buffer->load(m_snapshot->files.value(path), m_snapshot->files.contains(path));
    if (m_state) {
        // Where the cursor was the last time this file was open (Specs/m2-statements-review.md,
        // "What the app remembers").
        const QStringList cursor = m_state->value(cursorKey(path)).toString().split(u',');
        if (cursor.size() == 3) {
            buffer->cursorLine = cursor[0].toInt();
            buffer->cursorIndex = cursor[1].toInt();
            buffer->scroll = cursor[2].toInt();
        }
    }
    EditBuffer* raw = buffer.get();
    m_buffers.emplace(path, std::move(buffer));
    return raw;
}

EditBuffer* StatementsPage::buffer(const QString& path) const
{
    const auto it = m_buffers.find(path);
    return it == m_buffers.end() ? nullptr : it->second.get();
}

QStringList StatementsPage::pageIds() const
{
    QStringList ids;
    if (!m_snapshot)
        return ids;
    for (const ProofFile& proof : m_snapshot->contents.proofs) {
        if (proof.folder == m_tag)
            ids << proof.id;
    }
    for (const auto& [path, buffer] : m_buffers) {
        if (buffer->kind() == FileKind::Proof && splitProofId(buffer->id()) && splitProofId(buffer->id())->tag == m_tag
            && !ids.contains(buffer->id()) && (buffer->dirty() || buffer->recovery() != Recovery::None))
            ids << buffer->id();
    }
    std::sort(ids.begin(), ids.end(), [](const QString& a, const QString& b) {
        const std::optional<ProofIdParts> pa = splitProofId(a);
        const std::optional<ProofIdParts> pb = splitProofId(b);
        return pa && pb ? proofNumber(pa->letters) < proofNumber(pb->letters) : a < b;
    });
    return ids;
}

FilePage* StatementsPage::currentPage() const
{
    return qobject_cast<FilePage*>(m_tabs->currentWidget());
}

FilePage* StatementsPage::page(const QString& proofId) const
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* page = static_cast<FilePage*>(m_tabs->widget(i));
        if (page->proofId() == proofId)
            return page;
    }
    return nullptr;
}

void StatementsPage::rebuildPages(const QString& select)
{
    storeViews();
    while (m_tabs->count() > 0) {
        QWidget* old = m_tabs->widget(0);
        m_tabs->removeTab(0);
        // Later, not now: a click on one of its bars may be what led here.
        old->hide();
        old->setObjectName(QString());
        old->deleteLater();
    }
    if (m_tag.isEmpty() || !m_snapshot) {
        m_tabs->hide();
        m_empty->show();
        updatePdfTarget();
        return;
    }
    m_empty->hide();
    m_tabs->show();
    m_newProofButton->setVisible(!m_stub);
    if (m_stub) {
        const StubFile* stub = m_snapshot->contents.stub(m_tag);
        auto* page = new FilePage(this, m_tag, QString());
        if (stub)
            page->bind(ensureBuffer(stub->path, FileKind::Stub, m_tag), nullptr);
        m_tabs->addTab(page, QStringLiteral("Paper result"));
        new IdAssist(page->editor(), m_ids);
        applyEditorFont();
        updatePdfTarget();
        return;
    }
    const QString statementPath = QStringLiteral("Statements/%1.tex").arg(m_tag);
    auto* statementPage = new FilePage(this, m_tag, QString());
    statementPage->bind(ensureBuffer(statementPath, FileKind::Statement, m_tag), nullptr);
    m_tabs->addTab(statementPage, QStringLiteral("Statement"));
    new IdAssist(statementPage->editor(), m_ids);
    int selected = 0;
    for (const QString& id : pageIds()) {
        const QString path = QStringLiteral("Proofs/%1/%2.tex").arg(m_tag, id);
        const QString verbosePath = QStringLiteral("Proofs/%1/%2.verbose.tex").arg(m_tag, id);
        EditBuffer* verbose = buffer(verbosePath);
        if (!verbose && m_snapshot->files.contains(verbosePath))
            verbose = ensureBuffer(verbosePath, FileKind::Verbose, id);
        auto* page = new FilePage(this, m_tag, id);
        page->bind(ensureBuffer(path, FileKind::Proof, id), verbose);
        new IdAssist(page->editor(), m_ids);
        const int index = m_tabs->addTab(page, splitProofId(id) ? splitProofId(id)->letters : id);
        if (id == select)
            selected = index;
    }
    m_tabs->setCurrentIndex(selected);
    applyEditorFont();
    updateUnsaved();
    updatePdfTarget();
}

void StatementsPage::updatePdfTarget()
{
    if (m_tag.isEmpty() || !m_session) {
        m_pdf->setTarget({}, {});
        return;
    }
    if (m_stub) {
        m_pdf->setTarget(m_tag, m_tag, true);
        return;
    }
    const FilePage* page = currentPage();
    m_pdf->setTarget(page && !page->proofId().isEmpty() ? page->proofId() : m_tag, m_tag);
}

void StatementsPage::openStatement(const QString& tag, const QString& page, bool record)
{
    if (!m_snapshot)
        return;
    // A statement that is gone (a place in the history, say) opens only if its text is still here.
    if (!m_snapshot->contents.statement(tag) && !buffer(QStringLiteral("Statements/%1.tex").arg(tag)))
        return;
    if (record && !m_tag.isEmpty()) {
        const Place here = currentPlace();
        if (here.tag != tag || here.page != page) {
            m_back.push_back(here);
            if (static_cast<int>(m_back.size()) > historyLimit)
                m_back.erase(m_back.begin());
            m_forward.clear();
        }
    }
    if (FilePage* existing = m_tag == tag && !m_stub ? this->page(page) : nullptr) {
        m_tabs->setCurrentWidget(existing);
    } else {
        m_tag = tag;
        m_stub = false;
        rebuildPages(page);
    }
    m_list->select(page.isEmpty() ? tag : page);
    saveState();
}

void StatementsPage::openProof(const QString& proofId)
{
    if (const std::optional<ProofIdParts> parts = splitProofId(proofId))
        openStatement(parts->tag, proofId);
}

void StatementsPage::openStub(const QString& tag)
{
    if (!m_snapshot || !m_snapshot->contents.stub(tag))
        return;
    if (!m_tag.isEmpty()) {
        m_back.push_back(currentPlace());
        m_forward.clear();
    }
    m_tag = tag;
    m_stub = true;
    rebuildPages({});
    m_list->select(tag);
}

void StatementsPage::openId(const QString& id)
{
    if (!m_snapshot)
        return;
    if (const std::optional<QString> proof = resolveProof(m_snapshot->contents, id)) {
        openProof(*proof);
        return;
    }
    if (const std::optional<QString> tag = resolveStatement(m_snapshot->contents, id)) {
        if (m_snapshot->contents.statement(*tag))
            openStatement(*tag);
        else
            openStub(*tag);
    }
}

void StatementsPage::showFileLine(const QString& path, int line)
{
    FileRole role;
    if (!fileRole(path, role))
        return;
    if (role.kind == FileKind::Stub) {
        openStub(role.tag);
    } else {
        openStatement(role.tag, role.kind == FileKind::Statement ? QString() : role.id);
    }
    FilePage* page = currentPage();
    if (!page)
        return;
    if (role.kind == FileKind::Verbose)
        page->setShowingVerbose(true);
    else if (role.kind == FileKind::Proof)
        page->setShowingVerbose(false);
    EditBuffer* shown = page->shownBuffer();
    if (!shown)
        return;
    int editorLine = line;
    if (shown->kind() == FileKind::Statement || shown->kind() == FileKind::Proof) {
        const int bodyLine = shown->loadedBodyLine();
        // A front-matter line is shown in raw mode (the spec: "in raw mode for a front-matter line").
        if (!shown->raw() && (bodyLine <= 0 || line < bodyLine))
            setRaw(shown, true);
        if (!shown->raw())
            editorLine = line - bodyLine + 1;
    } else if (shown->kind() == FileKind::Verbose) {
        editorLine = std::max(1, line - std::max(1, shown->loadedBodyLine()) + 1);
    }
    page->refresh();
    page->goToEditorLine(editorLine);
}

void StatementsPage::checkOutside()
{
    // A snapshot that couldn't read the project says nothing about its files.
    if (!m_snapshot || !m_snapshot->unreadable.isEmpty())
        return;
    for (auto& [path, buffer] : m_buffers) {
        if (buffer->recovery() != Recovery::None)
            continue; // Its bar already asks Dakota what to do.
        const bool exists = m_snapshot->files.contains(path);
        const QByteArray now = m_snapshot->files.value(path);
        if (exists && buffer->exists() && now == buffer->loaded()) {
            if (buffer->outside() != Outside::None)
                buffer->setOutside(Outside::None);
            continue;
        }
        if (!exists && !buffer->exists())
            continue;
        if (!exists) {
            if (buffer->outside() != Outside::Deleted) {
                buffer->setOutside(Outside::Deleted);
                emit outputLine(QStringLiteral("%1 was deleted on disk.").arg(path));
            }
        } else if (!buffer->dirty()) {
            // No unsaved edits: it reloads silently, keeping the cursor and scroll.
            buffer->load(now, true);
            emit outputLine(QStringLiteral("Reloaded %1: it changed on disk.").arg(path));
        } else if (buffer->outside() != Outside::Changed || buffer->outsideBytes() != now) {
            buffer->setOutside(Outside::Changed, now);
            emit outputLine(QStringLiteral("%1 changed on disk while it had unsaved edits.").arg(path));
        }
    }
}

void StatementsPage::refreshPages()
{
    for (int i = 0; i < m_tabs->count(); ++i)
        static_cast<FilePage*>(m_tabs->widget(i))->refresh();
}

QStringList StatementsPage::unsavedPaths() const
{
    QStringList paths;
    for (const auto& [path, buffer] : m_buffers) {
        if (buffer->dirty())
            paths << path;
    }
    return paths;
}

bool StatementsPage::hasUnsavedEdits() const
{
    return std::any_of(m_buffers.begin(), m_buffers.end(), [](const auto& entry) { return entry.second->dirty(); });
}

void StatementsPage::updateUnsaved()
{
    const QStringList paths = unsavedPaths();
    m_list->setUnsaved(QSet<QString>(paths.begin(), paths.end()));
    for (int i = 0; i < m_tabs->count(); ++i) {
        const auto* page = static_cast<FilePage*>(m_tabs->widget(i));
        bool dirty = page->mainBuffer() && page->mainBuffer()->dirty();
        dirty |= page->verboseBuffer() && page->verboseBuffer()->dirty();
        QString label = page->proofId().isEmpty() ? (m_stub ? QStringLiteral("Paper result") : QStringLiteral("Statement"))
                                                  : (splitProofId(page->proofId()) ? splitProofId(page->proofId())->letters : page->proofId());
        m_tabs->setTabText(i, dirty ? label + QStringLiteral(" ●") : label);
    }
    const bool any = !paths.isEmpty();
    if (any != m_unsaved) {
        m_unsaved = any;
        emit unsavedChanged(any);
    }
}

void StatementsPage::bufferEdited(EditBuffer* buffer)
{
    Q_UNUSED(buffer);
    m_draftTimer.start();
    updateUnsaved();
    // Unsaved edits disable Dakota's clicks on the file ("Save first").
    refreshClicks();
}

void StatementsPage::writeDrafts()
{
    if (!m_session)
        return;
    const QString root = m_session->root();
    const QDateTime now = QDateTime::currentDateTime();
    std::vector<Draft> writes;
    QStringList removals;
    for (auto& [path, buffer] : m_buffers) {
        if (buffer->dirty()) {
            const QByteArray text = buffer->text();
            if (text != buffer->drafted()) {
                writes.push_back({path, text, buffer->base(), now});
                buffer->setDrafted(text);
            }
        } else if (!buffer->drafted().isEmpty() && buffer->recovery() == Recovery::None) {
            removals << path;
            buffer->setDrafted({});
        }
    }
    if (writes.empty() && removals.isEmpty())
        return;
    m_lane->run(this, [root, writes, removals] {
        QStringList errors;
        for (const Draft& draft : writes) {
            if (const QString error = writeDraft(root, draft); !error.isEmpty())
                errors << error;
        }
        for (const QString& path : removals)
            removeDraft(root, path);
        return errors;
    }, [this](const QStringList& errors) {
        for (const QString& error : errors)
            emit outputLine(QStringLiteral("Loom couldn't write a recovery file: %1").arg(error));
    });
}

void StatementsPage::writeFormDrafts()
{
    if (!m_session)
        return;
    std::vector<Draft> drafts;
    const QDateTime now = QDateTime::currentDateTime();
    if (m_newStatement)
        drafts.push_back({NewStatementDialog::draftName(), m_newStatement->draftText(), QString(), now});
    if (m_newProof)
        drafts.push_back({NewProofDialog::draftName(m_newProof->tag()), m_newProof->draftText(), QString(), now});
    const QString root = m_session->root();
    m_lane->run(this, [root, drafts] {
        for (const Draft& draft : drafts)
            writeDraft(root, draft);
        return 0;
    }, [](int) {});
}

void StatementsPage::writeAllDrafts(std::function<void()> done)
{
    m_draftTimer.stop();
    writeDrafts();
    m_formDraftTimer.stop();
    writeFormDrafts();
    // The lane runs in order, so this runs once every write queued above is done.
    m_lane->run(this, [] { return 0; }, [done](int) {
        if (done)
            done();
    });
}

void StatementsPage::dropDraft(EditBuffer* buffer)
{
    buffer->setDrafted({});
    if (!m_session)
        return;
    const QString root = m_session->root();
    const QString path = buffer->path();
    m_lane->run(this, [root, path] { return removeDraft(root, path); }, [](const QString&) {});
}

void StatementsPage::save()
{
    FilePage* page = currentPage();
    if (!page || !page->shownBuffer())
        return;
    // "Ctrl+S saves and compiles"; with Compile on save off, it only saves.
    saveBuffer(page->shownBuffer(), {}, [this](bool ok) {
        if (ok && m_settings->compileOnSave())
            m_pdf->compile();
    });
}

void StatementsPage::compile()
{
    m_pdf->compileClicked();
}

void StatementsPage::showCursorInPdf()
{
    FilePage* page = currentPage();
    EditBuffer* shown = page ? page->shownBuffer() : nullptr;
    if (!shown)
        return;
    int line = 0;
    int index = 0;
    page->editor()->getCursorPosition(&line, &index);
    ++line; // The editor counts from 0.
    // The form shows the body only: its line n is the file's line n + the front matter's lines.
    if (!shown->raw() && shown->kind() != FileKind::Stub)
        line += std::max(1, shown->loadedBodyLine()) - 1;
    m_pdf->showPlace(shown->path(), line, index);
}

void StatementsPage::setBuildProblems(const std::vector<Problem>& problems)
{
    m_buildProblems = problems;
    refreshPages();
}

void StatementsPage::saveBuffer(EditBuffer* buffer, const SaveConfirmations& confirmations, std::function<void(bool)> done)
{
    const auto finish = [done](bool ok) {
        if (done)
            done(ok);
    };
    if (!m_session || !buffer || buffer->readOnly()) {
        finish(false);
        return;
    }
    const QString name = QFileInfo(buffer->path()).fileName();
    // The choices a bar asks for come first; until then LoomCore would refuse with `file-changed`
    // anyway, and the bar says why.
    QString blocked;
    if (buffer->recovery() == Recovery::Changed)
        blocked = QStringLiteral("%1 changed after your text was recovered. Choose Keep mine or Discard mine first.").arg(name);
    else if (buffer->outside() == Outside::Changed)
        blocked = QStringLiteral("%1 changed on disk while you were editing it. Choose Compare, Keep mine or Take the file's first.").arg(name);
    else if (buffer->outside() == Outside::Deleted)
        blocked = QStringLiteral("%1 was deleted on disk. Choose Restore it or Close first.").arg(name);
    if (!blocked.isEmpty()) {
        buffer->setRefusal(blocked, {});
        refreshPages();
        emit saveFailed(buffer->path());
        finish(false);
        return;
    }
    if (!buffer->dirty() && buffer->exists()) {
        finish(true);
        return;
    }
    // One save at a time per file (Specs/m2-statements-review.md, "Busy controls"): a second one
    // would carry a `base` the first is about to change.
    if (m_saving.contains(buffer->path())) {
        finish(false);
        return;
    }
    if (buffer->recovery() == Recovery::Same)
        buffer->setRecovery(Recovery::None);
    if (buffer->recovery() == Recovery::Gone)
        buffer->setRecovery(Recovery::None);

    const QString root = m_session->root();
    const QString path = buffer->path();
    const QString id = buffer->id();
    const FileKind kind = buffer->kind();
    const QByteArray text = buffer->text();
    const QString base = buffer->base();
    if (FilePage* page = currentPage())
        page->storeView();
    m_saving.insert(path);
    m_lane->run(this,
        [root, path, id, kind, text, base, confirmations] {
            switch (kind) {
            case FileKind::Statement: return saveStatementText(root, path, text, base, confirmations);
            case FileKind::Proof: return saveProofText(root, path, text, base, confirmations);
            case FileKind::Verbose: return saveVerboseBody(root, id, text, base);
            case FileKind::Stub: break;
            }
            return ChangeResult{};
        },
        [this, path, text, confirmations, done](ChangeResult result) {
            m_saving.remove(path);
            // The buffer may have gone (another project opened meanwhile).
            if (EditBuffer* buffer = this->buffer(path))
                finishSave(buffer, text, result, confirmations, done);
            else if (done)
                done(false);
        });
}

void StatementsPage::finishSave(EditBuffer* buffer, const QByteArray& sent, const ChangeResult& result,
                                const SaveConfirmations& confirmations, std::function<void(bool)> done)
{
    const auto finish = [done](bool ok) {
        if (done)
            done(ok);
    };
    const QString name = QFileInfo(buffer->path()).fileName();
    if (result.outcome == ChangeOutcome::Done) {
        // The file is now what the save wrote (or, when nothing needed writing, what was loaded).
        const QByteArray now = result.written.isEmpty() ? buffer->loaded() : result.written;
        // A save is Loom's own write, not a change on disk for the PDF pane to compile again for.
        m_pdf->noteWritten(buffer->path(), now);
        if (buffer->text() == sent) {
            buffer->load(now, true);
            dropDraft(buffer);
        } else {
            // Typed while the save ran: those edits stay, now on top of the file just written,
            // and their recovery file is written again with the new base.
            buffer->rebase(now, true);
            buffer->setDrafted({});
            m_draftTimer.start();
        }
        buffer->setRefusal({}, {});
        emit refusalProblems({});
        emit outputLine(result.changed.isEmpty() ? QStringLiteral("Saved %1: nothing had changed.").arg(buffer->path())
                                                 : QStringLiteral("Saved %1 (%2).").arg(buffer->path(), result.changed.join(QStringLiteral(", "))));
        if (m_session)
            m_session->refresh();
        refreshPages();
        updateUnsaved();
        emit saved(buffer->path());
        finish(true);
        return;
    }

    if (result.outcome == ChangeOutcome::Refused && result.code == u"outdates-checks" && result.outdatedChecks.isEmpty()
        && confirmations.outdateChecks) {
        // Dakota confirmed outdating checks that the save no longer outdates (the project changed
        // meanwhile): it needs no confirmation now, so it goes again without one, and asks anew
        // if that changes once more.
        SaveConfirmations again = confirmations;
        again.outdateChecks.reset();
        saveBuffer(buffer, again, done);
        return;
    }

    if (result.outcome == ChangeOutcome::Refused && result.code == u"outdates-checks" && !result.outdatedChecks.isEmpty()) {
        // Dakota's click (Specs/m2-statements-review.md, "Dakota's clicks": "A save that outdates
        // checks"); LoomCore gets exactly the proofs he was shown, and refuses if they changed.
        const QStringList proofs = result.outdatedChecks;
        const bool one = proofs.size() == 1;
        // "Their checks stop counting: 4KX7 proved → unchecked, P3QA proved → conditional", the
        // status changes as LoomCore computed them for the refused save.
        const QString statuses = statusArrows(result.statusChanges);
        const QString text = QStringLiteral("Saving %1 changes text that the checks of %2 read. %3 stop counting%4")
                                 .arg(name, listed(proofs), one ? QStringLiteral("Its checks") : QStringLiteral("Their checks"),
                                      statuses.isEmpty() ? QStringLiteral("; no status changes.") : QStringLiteral(": %1.").arg(statuses));
        const QString path = buffer->path();
        confirm(this, QStringLiteral("outdateChecksDialog"), QStringLiteral("Save and outdate checks"), text,
                QStringLiteral("Save and outdate checks"), [this, path, proofs, confirmations, done](bool yes) {
                    EditBuffer* buffer = this->buffer(path);
                    if (!yes || !buffer) {
                        if (buffer)
                            emit saveFailed(path);
                        if (done)
                            done(false);
                        return;
                    }
                    SaveConfirmations again = confirmations;
                    again.outdateChecks = proofs;
                    saveBuffer(buffer, again, done);
                });
        return;
    }

    if (result.outcome == ChangeOutcome::Refused && result.code == u"dakota-only") {
        const QString path = buffer->path();
        const QStringList proofs = result.outdatedChecks;
        QString text;
        QString action;
        QString objectName;
        SaveConfirmations again = confirmations;
        if (buffer->kind() == FileKind::Proof) {
            // "Mark 4KX7.b failed? It stops supporting 4KX7; …" with the outdated checks in the
            // same dialog.
            objectName = QStringLiteral("markFailedDialog");
            action = QStringLiteral("Mark failed");
            text = QStringLiteral("Mark %1 failed? It stops supporting %2.").arg(buffer->id(), splitProofId(buffer->id()) ? splitProofId(buffer->id())->tag : QString());
            if (!proofs.isEmpty())
                text += QStringLiteral(" Saving it also changes what the checks of %1 read, so those checks stop counting.").arg(listed(proofs));
            // The dialog names everything it writes: the file's other unsaved edits go with it.
            EditBuffer levelOnly = *buffer;
            Proof p = levelOnly.proof();
            p.level = levelOnly.loadedProof().level;
            levelOnly.setProof(p);
            if (!levelOnly.raw() && levelOnly.dirty())
                text += QStringLiteral(" Your other unsaved edits to %1 are saved with it.").arg(name);
            again.failedConfirmed = true;
        } else {
            objectName = QStringLiteral("refuteDialog");
            action = QStringLiteral("Mark false");
            const Statement s = readStatement(buffer->text(), buffer->path()).document;
            text = QStringLiteral("Mark %1 false, refuted by %2 (accepted %3)?")
                       .arg(buffer->id(), s.refutedBy.isEmpty() ? QStringLiteral("(no proof)") : s.refutedBy,
                            s.refutedOn.isValid() ? s.refutedOn.toString(Qt::ISODate) : QStringLiteral("no date"));
            again.refutationConfirmed = true;
        }
        // The status changes LoomCore computed for the refused save (Specs/m2-statements-review.md,
        // "Dakota's clicks": "The status changes in each dialog come from LoomCore").
        if (const QString statuses = statusArrows(result.statusChanges); !statuses.isEmpty())
            text += QStringLiteral(" Status changes: %1.").arg(statuses);
        if (!proofs.isEmpty())
            again.outdateChecks = proofs;
        confirm(this, objectName, action, text, action, [this, path, again, done](bool yes) {
            EditBuffer* buffer = this->buffer(path);
            if (!yes || !buffer) {
                if (buffer)
                    emit saveFailed(path);
                if (done)
                    done(false);
                return;
            }
            saveBuffer(buffer, again, done);
        });
        return;
    }

    // Any other refusal, or a failure: nothing was written, and the text stays (E6).
    buffer->setRefusal(result.message.isEmpty() ? QStringLiteral("Loom didn't save %1.").arg(name) : result.message, result.problems);
    emit outputLine(QStringLiteral("Didn't save %1: %2").arg(buffer->path(), result.message));
    emit refusalProblems(result.problems);
    refreshPages();
    emit saveFailed(buffer->path());
    finish(false);
}

void StatementsPage::saveAll(std::function<void(bool)> done)
{
    QStringList paths = unsavedPaths();
    auto next = std::make_shared<std::function<void()>>();
    *next = [this, paths, next, done]() mutable {
        if (paths.isEmpty()) {
            if (done)
                done(true);
            return;
        }
        const QString path = paths.takeFirst();
        EditBuffer* buffer = this->buffer(path);
        if (!buffer) {
            (*next)();
            return;
        }
        saveBuffer(buffer, {}, [this, path, next, done](bool ok) {
            if (ok) {
                (*next)();
                return;
            }
            // Show the file that didn't save, with why.
            FileRole role;
            if (fileRole(path, role))
                showFileLine(path, 1);
            if (done)
                done(false);
        });
    };
    (*next)();
}

void StatementsPage::keepRecovered(EditBuffer* buffer)
{
    buffer->setRecovery(Recovery::None);
    refreshPages();
}

void StatementsPage::discardRecovered(EditBuffer* buffer)
{
    const QString path = buffer->path();
    const bool exists = m_snapshot && m_snapshot->files.contains(path);
    dropDraft(buffer);
    if (!exists) {
        // Nothing left to show: the file is gone and so is the text.
        closeDeleted(buffer);
        return;
    }
    buffer->load(m_snapshot->files.value(path), true);
    refreshPages();
    updateUnsaved();
}

void StatementsPage::compare(EditBuffer* buffer)
{
    const QByteArray file = buffer->outside() == Outside::Changed ? buffer->outsideBytes() : buffer->loaded();
    showCompare(this, QStringLiteral("%1: the file on disk against your text").arg(QFileInfo(buffer->path()).fileName()),
                QString::fromUtf8(file), QString::fromUtf8(buffer->text()));
}

void StatementsPage::keepMine(EditBuffer* buffer)
{
    // The text stays as unsaved edits, and `base` becomes the file now.
    if (buffer->outside() == Outside::Changed)
        buffer->rebase(buffer->outsideBytes(), true);
    buffer->setRecovery(Recovery::None);
    buffer->setRefusal({}, {});
    m_draftTimer.start();
    refreshPages();
    updateUnsaved();
}

void StatementsPage::takeTheirs(EditBuffer* buffer)
{
    const QString path = buffer->path();
    confirm(this, QStringLiteral("takeTheirsDialog"), QStringLiteral("Take the file's"),
            QStringLiteral("Discard your unsaved edits to %1 and take the file as it is on disk now?").arg(QFileInfo(path).fileName()),
            QStringLiteral("Discard my edits"), [this, path](bool yes) {
                EditBuffer* buffer = this->buffer(path);
                if (!yes || !buffer)
                    return;
                buffer->load(buffer->outsideBytes(), true);
                dropDraft(buffer);
                refreshPages();
                updateUnsaved();
            });
}

void StatementsPage::restore(EditBuffer* buffer)
{
    // Written through the usual save rules, as a file that doesn't exist yet.
    buffer->rebase({}, false);
    buffer->setRecovery(Recovery::None);
    refreshPages();
    saveBuffer(buffer);
}

void StatementsPage::closeDeleted(EditBuffer* buffer)
{
    const QString path = buffer->path();
    const FileKind kind = buffer->kind();
    const QString id = buffer->id();
    dropDraft(buffer);
    // Pages show buffers they don't own: take them off first.
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto* page = static_cast<FilePage*>(m_tabs->widget(i));
        if (page->mainBuffer() == buffer)
            page->bind(nullptr, nullptr);
        else if (page->verboseBuffer() == buffer)
            page->bind(page->mainBuffer(), nullptr);
    }
    m_buffers.erase(path);
    if (kind == FileKind::Statement && id == m_tag) {
        m_tag.clear();
        rebuildPages({});
    } else if (kind == FileKind::Proof) {
        rebuildPages({});
    } else {
        refreshPages();
    }
    updateUnsaved();
}

void StatementsPage::markVerboseUpToDate(EditBuffer* buffer)
{
    if (!m_session)
        return;
    const QString root = m_session->root();
    const QString id = buffer->id();
    const QString base = buffer->base();
    const QString path = buffer->path();
    m_lane->run(this, [root, id, base] { return loom::markVerboseUpToDate(root, id, base); }, [this, path](ChangeResult result) {
        EditBuffer* buffer = this->buffer(path);
        if (!buffer)
            return;
        if (result.outcome == ChangeOutcome::Done) {
            // Only the fingerprint changed on disk; unsaved body edits stay.
            if (!result.written.isEmpty())
                buffer->rebase(result.written, true);
            emit outputLine(QStringLiteral("Marked the verbose version of %1 up to date.").arg(buffer->id()));
            if (m_session)
                m_session->refresh();
        } else {
            buffer->setRefusal(result.message, result.problems);
        }
        refreshPages();
        updateUnsaved();
    });
}

void StatementsPage::writeVerbose(const QString& proofId)
{
    const std::optional<ProofIdParts> parts = splitProofId(proofId);
    if (!parts)
        return;
    const QString path = QStringLiteral("Proofs/%1/%2.verbose.tex").arg(parts->tag, proofId);
    EditBuffer* verbose = ensureBuffer(path, FileKind::Verbose, proofId);
    if (FilePage* page = this->page(proofId)) {
        page->bind(page->mainBuffer(), verbose);
        page->setShowingVerbose(true);
        page->editor()->setFocus();
    }
}

void StatementsPage::chooseFailed(EditBuffer* buffer, const QString& previous)
{
    // "Marking a proof failed: choosing failed in the level list asks first (Dakota's clicks),
    // then saves." The save asks: LoomCore refuses it with `dakota-only` until he confirms.
    Proof p = buffer->proof();
    p.level = QStringLiteral("failed");
    buffer->setProof(p);
    const QString path = buffer->path();
    saveBuffer(buffer, {}, [this, path, previous](bool ok) {
        EditBuffer* buffer = this->buffer(path);
        if (ok || !buffer || buffer->raw())
            return;
        Proof back = buffer->proof();
        back.level = previous;
        buffer->setProof(back);
        refreshPages();
        updateUnsaved();
    });
}

void StatementsPage::addReference(EditBuffer* buffer, const QString& field)
{
    if (!m_snapshot || !buffer)
        return;
    // The picker opens under the field's + (Specs/m2-statements-review.md, "IDs in the editor": "in
    // the form: under the field"), offering what the field takes; only what it lists can be added.
    FilePage* page = currentPage();
    QWidget* plus = nullptr;
    IdField offer = IdField::StatedUsing;
    if (field == u"premises") {
        offer = IdField::Premises;
        if (page && page->proofForm())
            plus = page->proofForm()->premises()->addButton();
    } else if (field == u"supersededBy") {
        offer = IdField::SupersededBy;
        if (page && page->statementForm())
            plus = page->statementForm()->supersededBy()->addButton();
    } else if (page && page->statementForm()) {
        plus = page->statementForm()->statedUsing()->addButton();
    }
    const QPoint at = plus ? plus->mapToGlobal(QPoint(0, plus->height())) : mapToGlobal(QPoint(width() / 4, 40));
    // A proof's premises never offer the statement it proves; a statement's fields never offer itself.
    const QString self = buffer->kind() == FileKind::Proof ? m_tag : buffer->id();
    const QString path = buffer->path();
    m_ids->pick(offer, self, {}, at, [this, path, field](const QString& id) {
        EditBuffer* buffer = this->buffer(path);
        if (!buffer || !m_snapshot)
            return;
        if (field == u"premises") {
            Proof p = buffer->proof();
            if (!p.premises.contains(id))
                p.premises << id;
            buffer->setProof(p);
        } else if (field == u"supersededBy") {
            Statement s = buffer->statement();
            s.supersededBy = id;
            buffer->setStatement(s);
        } else {
            Statement s = buffer->statement();
            if (!s.statedUsing.contains(id))
                s.statedUsing << id;
            buffer->setStatement(s);
        }
        refreshPages();
        bufferEdited(buffer);
    });
}

void StatementsPage::goToStatement()
{
    if (!m_snapshot)
        return;
    // Centred near the top of the tab, as a command palette.
    const QPoint at = mapToGlobal(QPoint(std::max(0, (width() - 820) / 2), 48));
    m_ids->pick(IdField::Ref, {}, {}, at, [this](const QString& id) { openId(id); });
}

void StatementsPage::showRefusalProblems(EditBuffer* buffer)
{
    emit refusalProblems(buffer->refusalProblems());
}

void StatementsPage::setRaw(EditBuffer* buffer, bool raw)
{
    QString why;
    FilePage* page = currentPage();
    if (page)
        page->storeView();
    if (!buffer->setRaw(raw, &why)) {
        if (page)
            page->showNote(why);
    }
    refreshPages();
}

void StatementsPage::newStatement()
{
    if (!m_session || !m_snapshot)
        return;
    if (m_newStatement) {
        m_newStatement->raise();
        m_newStatement->activateWindow();
        return;
    }
    m_dialogSnapshot = m_snapshot;
    m_newStatement = new NewStatementDialog(m_dialogSnapshot.get(), this);
    m_newStatement->setAttribute(Qt::WA_DeleteOnClose);
    m_newStatement->body()->setEditorFont(QFont(m_settings->editorFontFamily(), m_settings->editorFontSize()));
    new IdAssist(m_newStatement->body(), m_ids);
    connect(m_newStatement, &NewStatementDialog::pickStatedUsing, this, [this] {
        QToolButton* plus = m_newStatement->statedUsingPicker();
        m_ids->pick(IdField::StatedUsing, {}, {}, plus->mapToGlobal(QPoint(0, plus->height())), [this](const QString& id) {
            if (m_newStatement)
                m_newStatement->addStatedUsing(id);
        }, m_newStatement);
    });
    connect(m_newStatement, &NewStatementDialog::changed, this, [this] { m_formDraftTimer.start(); });
    connect(m_newStatement, &NewStatementDialog::createClicked, this, [this] { createFromDialog(true); });
    connect(m_newStatement, &QDialog::rejected, this, [this] {
        if (m_session) {
            const QString root = m_session->root();
            m_lane->run(this, [root] { return removeDraft(root, NewStatementDialog::draftName()); }, [](const QString&) {});
        }
    });
    m_newStatement->open();
    m_newStatement->title()->setFocus();
}

void StatementsPage::newProof()
{
    if (!m_session || !m_snapshot || m_tag.isEmpty() || m_stub)
        return;
    if (m_newProof) {
        m_newProof->raise();
        m_newProof->activateWindow();
        return;
    }
    m_newProof = new NewProofDialog(m_tag, this);
    m_newProof->setAttribute(Qt::WA_DeleteOnClose);
    m_newProof->body()->setEditorFont(QFont(m_settings->editorFontFamily(), m_settings->editorFontSize()));
    new IdAssist(m_newProof->body(), m_ids);
    connect(m_newProof, &NewProofDialog::pickPremises, this, [this] {
        QToolButton* plus = m_newProof->premisesPicker();
        m_ids->pick(IdField::Premises, m_newProof->tag(), {}, plus->mapToGlobal(QPoint(0, plus->height())), [this](const QString& id) {
            if (m_newProof)
                m_newProof->addPremise(id);
        }, m_newProof);
    });
    connect(m_newProof, &NewProofDialog::changed, this, [this] { m_formDraftTimer.start(); });
    connect(m_newProof, &NewProofDialog::createClicked, this, [this] { createFromDialog(false); });
    const QString draft = NewProofDialog::draftName(m_tag);
    connect(m_newProof, &QDialog::rejected, this, [this, draft] {
        if (m_session) {
            const QString root = m_session->root();
            m_lane->run(this, [root, draft] { return removeDraft(root, draft); }, [](const QString&) {});
        }
    });
    m_newProof->open();
    m_newProof->body()->setFocus();
}

void StatementsPage::createFromDialog(bool statement)
{
    if (!m_session)
        return;
    const QString root = m_session->root();
    if (statement && m_newStatement) {
        const NewStatement request = m_newStatement->request();
        m_newStatement->setBusy(true);
        m_lane->run(this, [root, request] {
            ChangeResult result = createStatement(root, request);
            if (result.outcome == ChangeOutcome::Done)
                removeDraft(root, NewStatementDialog::draftName());
            return result;
        }, [this](ChangeResult result) {
            if (!m_newStatement)
                return;
            m_newStatement->setBusy(false);
            if (result.outcome != ChangeOutcome::Done) {
                m_newStatement->showRefusal(result.message);
                return;
            }
            m_formDraftTimer.stop();
            // A recovery file written while Create ran (the timer fired first) is queued after the
            // create's removal; remove it again, after it.
            if (m_session) {
                const QString root = m_session->root();
                m_lane->run(this, [root] { return removeDraft(root, NewStatementDialog::draftName()); }, [](const QString&) {});
            }
            emit outputLine(QStringLiteral("Created %1 (%2).").arg(result.id, result.path));
            m_pendingOpen = result.id;
            disconnect(m_newStatement, &QDialog::rejected, this, nullptr);
            m_newStatement->accept();
            if (m_session)
                m_session->refresh();
        });
        return;
    }
    if (!statement && m_newProof) {
        const NewProof request = m_newProof->request();
        const QString draft = NewProofDialog::draftName(m_newProof->tag());
        m_newProof->setBusy(true);
        m_lane->run(this, [root, request, draft] {
            ChangeResult result = createProof(root, request);
            if (result.outcome == ChangeOutcome::Done)
                removeDraft(root, draft);
            return result;
        }, [this, draft](ChangeResult result) {
            if (!m_newProof)
                return;
            m_newProof->setBusy(false);
            if (result.outcome != ChangeOutcome::Done) {
                m_newProof->showRefusal(result.message);
                return;
            }
            m_formDraftTimer.stop();
            if (m_session) {
                const QString root = m_session->root();
                m_lane->run(this, [root, draft] { return removeDraft(root, draft); }, [](const QString&) {});
            }
            emit outputLine(QStringLiteral("Created %1 (%2).").arg(result.id, result.path));
            m_pendingOpen = result.id;
            disconnect(m_newProof, &QDialog::rejected, this, nullptr);
            m_newProof->accept();
            if (m_session)
                m_session->refresh();
        });
    }
}

Place StatementsPage::currentPlace() const
{
    Place place;
    place.tag = m_tag;
    if (FilePage* page = currentPage()) {
        place.page = page->proofId();
        place.verbose = page->showingVerbose();
        page->editor()->getCursorPosition(&place.line, &place.index);
        place.scroll = page->editor()->firstVisibleLine();
    }
    return place;
}

void StatementsPage::goTo(const Place& place)
{
    if (!m_snapshot)
        return;
    if (m_snapshot->contents.stub(place.tag) && !m_snapshot->contents.statement(place.tag)) {
        m_tag = place.tag;
        m_stub = true;
        rebuildPages({});
    } else {
        openStatement(place.tag, place.page, false);
    }
    FilePage* page = currentPage();
    if (!page)
        return;
    page->setShowingVerbose(place.verbose);
    page->editor()->setCursorPosition(place.line, place.index);
    page->editor()->setFirstVisibleLine(place.scroll);
    m_list->select(place.page.isEmpty() ? place.tag : place.page);
}

void StatementsPage::back()
{
    if (m_back.empty())
        return;
    if (!m_tag.isEmpty())
        m_forward.push_back(currentPlace());
    const Place place = m_back.back();
    m_back.pop_back();
    goTo(place);
    saveState();
}

void StatementsPage::forward()
{
    if (m_forward.empty())
        return;
    if (!m_tag.isEmpty())
        m_back.push_back(currentPlace());
    const Place place = m_forward.back();
    m_forward.pop_back();
    goTo(place);
    saveState();
}

void StatementsPage::find(bool replace)
{
    // "Find in the PDF (Ctrl+F while the PDF has focus) uses Qt PDF's search."
    if (!replace && QApplication::focusWidget() && m_pdf->isAncestorOf(QApplication::focusWidget())) {
        m_pdf->find();
        return;
    }
    if (FilePage* page = currentPage())
        page->findBar()->open(replace);
}

void StatementsPage::findNext(bool forward)
{
    if (FilePage* page = currentPage()) {
        if (page->findBar()->findBox()->text().isEmpty())
            page->findBar()->open(false);
        else
            page->findBar()->findNext(forward);
    }
}

void StatementsPage::findInProject()
{
    if (!m_snapshot)
        return;
    if (auto* open = findChild<QDialog*>(QStringLiteral("findInProject"))) {
        open->raise();
        return;
    }
    // Bodies only, and no replace: a project-wide replace could outdate many checks in one click.
    auto* dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("findInProject"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("Find in the project"));
    dialog->resize(640, 420);
    auto* query = new QLineEdit(dialog);
    query->setObjectName(QStringLiteral("query"));
    query->setPlaceholderText(QStringLiteral("Text to find in every statement and proof"));
    auto* results = new QTreeWidget(dialog);
    results->setObjectName(QStringLiteral("results"));
    results->setColumnCount(3);
    results->setHeaderLabels({QStringLiteral("Statement or proof"), QStringLiteral("Line"), QStringLiteral("Text")});
    results->setRootIsDecorated(false);
    results->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    results->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    auto* layout = new QVBoxLayout(dialog);
    layout->addWidget(query);
    layout->addWidget(results, 1);
    connect(query, &QLineEdit::returnPressed, dialog, [this, query, results] {
        const QString text = query->text();
        const std::shared_ptr<const ProjectSnapshot> snapshot = m_snapshot;
        if (text.isEmpty() || !snapshot)
            return;
        struct Hit {
            QString path;
            QString label;
            int line;
            QString text;
        };
        QPointer<QTreeWidget> guard(results);
        m_lane->run(this, [snapshot, text] {
            std::vector<Hit> hits;
            const auto search = [&](const QString& path, const QString& label, const QByteArray& body, int bodyLine) {
                const QStringList lines = QString::fromUtf8(body).split(u'\n');
                for (qsizetype i = 0; i < lines.size(); ++i) {
                    if (lines[i].contains(text, Qt::CaseInsensitive))
                        hits.push_back({path, label, bodyLine + static_cast<int>(i), lines[i].trimmed()});
                }
            };
            for (const StatementFile& file : snapshot->contents.statements)
                search(file.path, QStringLiteral("%1 · %2").arg(file.tag, file.reading.document.title), file.reading.document.body,
                       file.reading.bodyLine);
            for (const ProofFile& file : snapshot->contents.proofs) {
                const StatementFile* of = snapshot->contents.statement(file.folder);
                search(file.path, QStringLiteral("%1 · %2").arg(file.id, of ? of->reading.document.title : QString()),
                       file.reading.document.body, file.reading.bodyLine);
            }
            return hits;
        }, [guard](std::vector<Hit> hits) {
            if (!guard)
                return;
            guard->clear();
            for (const Hit& hit : hits) {
                auto* item = new QTreeWidgetItem(guard, {hit.label, QString::number(hit.line), hit.text});
                item->setData(0, Qt::UserRole, hit.path);
                item->setData(1, Qt::UserRole, hit.line);
            }
            if (hits.empty())
                new QTreeWidgetItem(guard, {QStringLiteral("No statement or proof has this text."), QString(), QString()});
        });
    });
    connect(results, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        const QString path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty())
            showFileLine(path, item->data(1, Qt::UserRole).toInt());
    });
    dialog->show();
    query->setFocus();
}

void StatementsPage::zoomEditor(int delta)
{
    m_settings->setEditorFontSize(std::clamp(m_settings->editorFontSize() + delta, 6, 48));
}

void StatementsPage::applyEditorFont()
{
    const QFont font(m_settings->editorFontFamily(), m_settings->editorFontSize());
    for (LatexEditor* editor : findChildren<LatexEditor*>())
        editor->setEditorFont(font);
}

void StatementsPage::storeViews()
{
    for (int i = 0; i < m_tabs->count(); ++i)
        static_cast<FilePage*>(m_tabs->widget(i))->storeView();
}

void StatementsPage::saveState()
{
    if (!m_state)
        return;
    m_state->setValue(QStringLiteral("statements/open"), m_tag);
    m_state->setValue(QStringLiteral("statements/place"), currentPlace().toString());
    QStringList back;
    for (const Place& place : m_back)
        back << place.toString();
    QStringList forward;
    for (const Place& place : m_forward)
        forward << place.toString();
    m_state->setValue(QStringLiteral("history/back"), back);
    m_state->setValue(QStringLiteral("history/forward"), forward);
    // The cursor and scroll of the last 100 files opened.
    QStringList order = m_state->value(QStringLiteral("cursorOrder")).toStringList();
    for (const auto& [path, buffer] : m_buffers) {
        m_state->setValue(cursorKey(path), QStringLiteral("%1,%2,%3").arg(buffer->cursorLine).arg(buffer->cursorIndex).arg(buffer->scroll));
        order.removeAll(path);
        order.prepend(path);
    }
    while (order.size() > historyLimit)
        m_state->remove(cursorKey(order.takeLast()));
    m_state->setValue(QStringLiteral("cursorOrder"), order);
    m_state->sync();
}

bool StatementsPage::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress && isVisible()) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        auto* widget = qobject_cast<QWidget*>(watched);
        if ((mouse->button() == Qt::BackButton || mouse->button() == Qt::ForwardButton) && widget && isAncestorOf(widget)) {
            if (mouse->button() == Qt::BackButton)
                back();
            else
                forward();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QString StatementsPage::checkBlocker(const QString& proofId) const
{
    if (!m_session || !m_snapshot)
        return QStringLiteral("No project is open.");
    const ProofNode* proof = m_snapshot->graph.proof(proofId);
    if (!proof)
        return QStringLiteral("Loom can't read this proof's front matter, so it can't be checked.");
    if (const EditBuffer* open = buffer(proof->path)) {
        if (open->dirty())
            return QStringLiteral("Save first: a check records the version on disk.");
        if (!open->exists() || open->outside() == Outside::Deleted)
            return QStringLiteral("This proof's file is gone from disk.");
        // Saved, but the snapshot is from before the save.
        if (open->loaded() != m_snapshot->files.value(proof->path))
            return QStringLiteral("Loom is reading the latest version from disk; try again in a moment.");
    }
    if (m_session->stale())
        return QStringLiteral("Loom is reading the latest version from disk; try again in a moment.");
    return {};
}

QString StatementsPage::refuteBlocker(const QString& tag) const
{
    if (!m_session || !m_snapshot)
        return QStringLiteral("No project is open.");
    const StatementNode* node = m_snapshot->graph.statement(tag);
    if (!node || !m_snapshot->contents.statement(tag))
        return QStringLiteral("Loom can't read this statement's front matter.");
    if (const EditBuffer* open = buffer(node->path); open && open->dirty())
        return QStringLiteral("Save first: refuting rewrites this statement's file.");
    if (!node->refutedBy.isEmpty() || node->refutedOn.isValid())
        return QStringLiteral("Already refuted by %1.").arg(node->refutedBy.isEmpty() ? QStringLiteral("(no proof)") : node->refutedBy);
    return {};
}

void StatementsPage::refreshClicks()
{
    for (int i = 0; i < m_tabs->count(); ++i)
        static_cast<FilePage*>(m_tabs->widget(i))->refreshClicks();
}

void StatementsPage::endClick(const QString& id, bool written)
{
    m_clicks.remove(id);
    if (written && m_session)
        m_session->refresh();
    refreshClicks();
    emit clickFinished(id, written);
}

void StatementsPage::recordCheck(const QString& proofId, const QString& verdict)
{
    if (!m_session || !m_snapshot || clickRunning(proofId) || !checkBlocker(proofId).isEmpty())
        return;
    const ProofNode* proof = m_snapshot->graph.proof(proofId);
    NewCheck request;
    request.by = QStringLiteral("dakota");
    request.verdict = verdict;
    // The fingerprint of the version shown: LoomCore refuses with `proof-changed` if the file
    // changed since (Specs/m2-statements-review.md, "Dakota's clicks").
    request.fingerprint = proof->checking.fingerprint;
    // Asked first without his confirmation, for what the check would do. Its status changes don't
    // depend on what he will write for a needs-changes, but a needs-changes needs some text to pass
    // the rules, so the question carries a stand-in that is never written.
    NewCheck question = request;
    if (verdict == u"needs-changes")
        question.changes = QStringLiteral("(written in the dialog)");
    const QString root = m_session->root();
    const QPointer<ProjectSession> session = m_session;
    const std::shared_ptr<const ProjectSnapshot> snapshot = m_snapshot;
    m_clicks.insert(proofId);
    refreshClicks();
    m_lane->run(this, [root, proofId, question] { return addCheck(root, proofId, question, false); },
                [this, session, snapshot, proofId, request](CheckAdded asked) {
                    if (m_session != session)
                        return;
                    const ProofNode* before = snapshot->graph.proof(proofId);
                    if (asked.change.code != u"dakota-only" || !before) {
                        showClickRefusal(asked.change, proofId);
                        endClick(proofId, false);
                        return;
                    }
                    const QString text = checkQuestion(proofId, request.verdict, *before, asked);
                    const auto answer = [this, session, proofId, request](std::optional<QString> changes) {
                        if (m_session != session)
                            return;
                        if (!changes) {
                            endClick(proofId, false);
                            return;
                        }
                        NewCheck confirmed = request;
                        confirmed.changes = *changes;
                        addConfirmedCheck(proofId, confirmed);
                    };
                    if (request.verdict == u"needs-changes") {
                        askChanges(this, text, answer);
                        return;
                    }
                    const QString action = QStringLiteral("Record %1").arg(request.verdict);
                    confirm(this, QStringLiteral("checkDialog"), action, text, action,
                            [answer](bool yes) { answer(yes ? std::optional<QString>(QString()) : std::nullopt); });
                });
}

void StatementsPage::addConfirmedCheck(const QString& proofId, const NewCheck& request)
{
    const QString root = m_session->root();
    const QPointer<ProjectSession> session = m_session;
    m_lane->run(this, [root, proofId, request] { return addCheck(root, proofId, request, true); },
                [this, session, proofId, request](CheckAdded added) {
                    if (m_session != session)
                        return;
                    if (added.change.outcome != ChangeOutcome::Done) {
                        showClickRefusal(added.change, proofId);
                        endClick(proofId, false);
                        return;
                    }
                    const QString statuses = statusArrows(added.change.statusChanges);
                    emit outputLine(QStringLiteral("Recorded your %1 of %2 in %3%4.")
                                        .arg(request.verdict, proofId, added.change.path,
                                             statuses.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(statuses)));
                    endClick(proofId, true);
                });
}

void StatementsPage::markProofFailed(const QString& proofId)
{
    if (!m_snapshot || clickRunning(proofId) || !checkBlocker(proofId).isEmpty())
        return;
    const ProofNode* proof = m_snapshot->graph.proof(proofId);
    EditBuffer* open = ensureBuffer(proof->path, FileKind::Proof, proofId);
    if (open->raw()) {
        // The level is a field of the form; raw text without edits can always go back to it
        // unless it doesn't parse, which the page then says.
        QString why;
        if (!open->setRaw(false, &why)) {
            if (FilePage* shown = page(proofId))
                shown->showNote(why);
            return;
        }
        refreshPages();
    }
    // The same dialog and save as choosing failed in the level list. A copy of the level: the
    // buffer's own field becomes `failed` before chooseFailed() keeps `previous`.
    const QString previous = open->proof().level;
    chooseFailed(open, previous);
}

void StatementsPage::refute()
{
    if (m_tag.isEmpty() || m_stub || !m_snapshot || clickRunning(m_tag) || !refuteBlocker(m_tag).isEmpty())
        return;
    // "Refute… (on the statement page's menu) opens the picker for proofs of other statements,
    // then the dialog."
    FilePage* statementPage = page(QString());
    QWidget* anchor = statementPage ? statementPage->statementMenu() : nullptr;
    const QPoint at = anchor ? anchor->mapToGlobal(QPoint(0, anchor->height())) : mapToGlobal(QPoint(width() / 4, 40));
    const QString tag = m_tag;
    m_ids->pick(IdField::RefutedBy, tag, {}, at, [this, tag](const QString& proofId) { refuteWith(tag, proofId); });
}

void StatementsPage::refuteWith(const QString& tag, const QString& proofId)
{
    if (!m_session || clickRunning(tag))
        return;
    const QString root = m_session->root();
    const QPointer<ProjectSession> session = m_session;
    m_clicks.insert(tag);
    refreshClicks();
    // Asked first without his confirmation, for what it would do.
    m_lane->run(this, [root, tag, proofId] { return refuteStatement(root, tag, proofId, false); },
                [this, session, root, tag, proofId](ChangeResult asked) {
                    if (m_session != session)
                        return;
                    if (asked.code != u"dakota-only") {
                        showClickRefusal(asked, tag);
                        endClick(tag, false);
                        return;
                    }
                    const QString statuses = statusBecomes(asked.statusChanges);
                    const QString text = QStringLiteral("Mark %1 false, refuted by %2? Today's date is written as the date you accepted it.%3")
                                             .arg(tag, proofId, statuses.isEmpty() ? QString() : QStringLiteral(" %1.").arg(statuses));
                    confirm(this, QStringLiteral("refuteDialog"), QStringLiteral("Mark false"), text, QStringLiteral("Mark false"),
                            [this, session, root, tag, proofId](bool yes) {
                                if (m_session != session)
                                    return;
                                if (!yes) {
                                    endClick(tag, false);
                                    return;
                                }
                                m_lane->run(this, [root, tag, proofId] { return refuteStatement(root, tag, proofId, true); },
                                            [this, session, tag](ChangeResult done) {
                                                if (m_session != session)
                                                    return;
                                                if (done.outcome != ChangeOutcome::Done) {
                                                    showClickRefusal(done, tag);
                                                    endClick(tag, false);
                                                    return;
                                                }
                                                emit outputLine(QStringLiteral("Marked %1 false in %2 (%3).")
                                                                    .arg(tag, done.path, done.changed.join(QStringLiteral(", "))));
                                                endClick(tag, true);
                                            });
                            });
                });
}

void StatementsPage::showClickRefusal(const ChangeResult& result, const QString& id)
{
    const bool changed = result.code == u"proof-changed";
    const QString text = changed ? QStringLiteral("%1 changed since you opened it, so your check wasn't recorded: a check records the "
                                                  "version you read. Read the new version, then check it again.")
                                       .arg(id)
                                 : (result.message.isEmpty() ? QStringLiteral("Loom didn't write it.") : result.message);
    emit outputLine(QStringLiteral("Didn't record your click on %1: %2").arg(id, result.message));
    auto* box = new QMessageBox(QMessageBox::Warning, QStringLiteral("Not recorded"), text, QMessageBox::NoButton, this);
    box->setObjectName(changed ? QStringLiteral("proofChanged") : QStringLiteral("clickRefused"));
    box->setTextFormat(Qt::PlainText);
    box->setAttribute(Qt::WA_DeleteOnClose);
    if (changed) {
        // "A refusal because the proof changed under the dialog (proof-changed) says so and offers
        // to show the new version."
        QPushButton* show = box->addButton(QStringLiteral("Show the new version"), QMessageBox::AcceptRole);
        show->setObjectName(QStringLiteral("showNew"));
        connect(show, &QPushButton::clicked, this, [this, id] {
            if (m_session)
                m_session->refresh();
            openProof(id);
        });
    }
    QPushButton* close = box->addButton(QMessageBox::Close);
    close->setObjectName(QStringLiteral("close"));
    box->setDefaultButton(close);
    box->open();
}

void StatementsPage::openProjectText(const QString& path)
{
    if (!m_session)
        return;
    const QString root = m_session->root();
    m_lane->run(this, [root, path] { return readProjectFile(root, path); }, [this, path](std::optional<QByteArray> bytes) {
        // Plain text until rendered Markdown comes (M4).
        auto* window = new QDialog(this);
        window->setObjectName(QStringLiteral("projectText"));
        window->setWindowTitle(path);
        window->setAttribute(Qt::WA_DeleteOnClose);
        auto* text = new QPlainTextEdit(window);
        text->setObjectName(QStringLiteral("text"));
        text->setReadOnly(true);
        text->setPlainText(bytes ? QString::fromUtf8(*bytes)
                                 : QStringLiteral("%1 is missing: Loom can't find it in the project.").arg(path));
        auto* layout = new QVBoxLayout(window);
        layout->addWidget(text);
        window->resize(640, 480);
        window->show();
    });
}

} // namespace loom
