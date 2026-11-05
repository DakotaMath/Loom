#include "LoomApp/IdPicker.h"

#include "LoomApp/IdPreviews.h"
#include "LoomApp/IdTools.h"
#include "LoomApp/ProjectSnapshot.h"
#include "LoomApp/StatusMarks.h"

#include <QEvent>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QScreen>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace loom {

namespace {

/// How many lines and characters of source a card shows before it stops.
constexpr int sourceLines = 14;
constexpr int sourceCharacters = 900;
/// How many results the picker lists: enough to scroll, few enough to fill at once.
constexpr int resultLimit = 200;

/// The first lines of a text, with `…` if there is more.
QString firstLines(const QString& text)
{
    QStringList lines = text.trimmed().split(u'\n');
    bool cut = false;
    if (lines.size() > sourceLines) {
        lines = lines.mid(0, sourceLines);
        cut = true;
    }
    QString shown = lines.join(u'\n');
    if (shown.size() > sourceCharacters) {
        shown = shown.left(sourceCharacters);
        cut = true;
    }
    return cut ? shown + QStringLiteral("\n…") : shown;
}

/// A stub's preview: its gist and locators, as text (Specs/m2-statements-review.md, "Previews").
QString stubText(const StubFile& stub)
{
    const Stub& s = stub.reading.document;
    QStringList lines{s.gist, QString()};
    for (const Locator& locator : s.locators) {
        lines << QStringLiteral("%1: %2 (%3)")
                     .arg(locator.version, locator.at,
                          locator.checkedAgainstPdf.isValid()
                              ? QStringLiteral("checked against the PDF %1").arg(locator.checkedAgainstPdf.toString(Qt::ISODate))
                              : QStringLiteral("not checked against the PDF"));
    }
    return lines.join(u'\n').trimmed();
}

/// Moves a popup's top-left corner so the whole popup is on the screen at `global`.
QPoint onScreen(const QPoint& global, const QSize& size)
{
    QScreen* screen = QGuiApplication::screenAt(global);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return global;
    const QRect area = screen->availableGeometry();
    QPoint at = global;
    at.setX(std::clamp(at.x(), area.left(), std::max(area.left(), area.right() - size.width())));
    at.setY(std::clamp(at.y(), area.top(), std::max(area.top(), area.bottom() - size.height())));
    return at;
}

} // namespace

QString pickerPrompt(IdField field)
{
    switch (field) {
    case IdField::Ref: return QStringLiteral("A statement, proof or paper result: ID, title, tag or text");
    case IdField::PaperRef: return QStringLiteral("A paper result: its tag, paper, locator or gist");
    case IdField::Premises: return QStringLiteral("A premise: a statement or paper result by ID, title, tag or text");
    case IdField::StatedUsing: return QStringLiteral("Stated using: a statement or paper result by ID, title, tag or text");
    case IdField::SupersededBy: return QStringLiteral("Superseded by: a statement by ID, title, tag or text");
    case IdField::Cite: return QStringLiteral("A paper: its key, authors or title");
    }
    return {};
}

IdCard::IdCard(IdTools* tools, QWidget* parent)
    : QFrame(parent)
    , m_tools(tools)
    , m_mark(new QLabel(this))
    , m_heading(new QLabel(this))
    , m_kind(new QLabel(this))
    , m_sentence(new QLabel(this))
    , m_picture(new QLabel(this))
    , m_source(new QLabel(this))
{
    setObjectName(QStringLiteral("idCard"));
    m_heading->setObjectName(QStringLiteral("heading"));
    m_heading->setWordWrap(true);
    QFont bold = m_heading->font();
    bold.setBold(true);
    m_heading->setFont(bold);
    m_kind->setObjectName(QStringLiteral("kind"));
    m_kind->setForegroundRole(QPalette::PlaceholderText);
    m_sentence->setObjectName(QStringLiteral("sentence"));
    m_sentence->setWordWrap(true);
    m_picture->setObjectName(QStringLiteral("picture"));
    m_picture->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_picture->setAutoFillBackground(true);
    QPalette paper = m_picture->palette();
    paper.setColor(QPalette::Window, Qt::white); // The PDF's paper.
    m_picture->setPalette(paper);
    m_source->setObjectName(QStringLiteral("source"));
    m_source->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_source->setWordWrap(true);
    m_source->setTextFormat(Qt::PlainText);
    m_source->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_source->setMinimumWidth(320);

    auto* top = new QHBoxLayout;
    top->setSpacing(6);
    top->addWidget(m_mark, 0, Qt::AlignTop);
    top->addWidget(m_heading, 1);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);
    layout->addLayout(top);
    layout->addWidget(m_kind);
    layout->addWidget(m_sentence);
    layout->addWidget(m_picture);
    layout->addWidget(m_source);
    layout->addStretch(1);

    if (m_tools && m_tools->previews()) {
        connect(m_tools->previews(), &IdPreviews::ready, this, [this](const QString& tag) {
            if (tag == m_id)
                refresh();
        });
    }
    setId({});
}

void IdCard::setId(const QString& id)
{
    m_id = id;
    refresh();
}

bool IdCard::showsPicture() const
{
    return !m_picture->isHidden();
}

void IdCard::refresh()
{
    const std::shared_ptr<const ProjectSnapshot> snapshot = m_tools ? m_tools->snapshot() : nullptr;
    const IdEntry* entry = snapshot && snapshot->ids ? snapshot->ids->entry(m_id) : nullptr;
    m_mark->clear();
    m_picture->hide();
    m_source->hide();
    m_kind->hide();
    m_sentence->hide();
    if (m_id.isEmpty()) {
        m_heading->clear();
        return;
    }
    if (!entry) {
        m_heading->setText(QStringLiteral("%1 names nothing in this project").arg(m_id));
        return;
    }
    m_heading->setText(entry->label());
    const StatementNode* node = snapshot->graph.statement(entry->kind == IdKind::Proof ? entry->statement : entry->id);
    if (node && entry->kind != IdKind::Proof && entry->kind != IdKind::Paper) {
        m_mark->setPixmap(statusIcon(node->status, darkTheme()).pixmap(14, 14));
        m_mark->setToolTip(statusWord(node->status));
    }

    QString kind;
    QString source;
    switch (entry->kind) {
    case IdKind::Statement:
        kind = kindTitle(entry->kindName);
        if (const StatementFile* file = snapshot->contents.statement(entry->id))
            source = QString::fromUtf8(file->reading.document.body);
        break;
    case IdKind::Proof:
        kind = QStringLiteral("Proof · %1").arg(entry->kindName);
        if (const ProofFile* file = snapshot->contents.proof(entry->id))
            source = QString::fromUtf8(file->reading.document.body);
        break;
    case IdKind::Stub:
        kind = QStringLiteral("Paper result · %1").arg(kindTitle(entry->kindName));
        if (const StubFile* file = snapshot->contents.stub(entry->id))
            source = stubText(*file);
        break;
    case IdKind::Paper:
        kind = QStringLiteral("Paper");
        if (const PaperFile* file = snapshot->contents.paperWithKey(entry->id); file && file->reading.parsed)
            source = file->reading.document.summary;
        break;
    case IdKind::Label:
        break;
    }
    if (node && entry->kind != IdKind::Proof && entry->kind != IdKind::Paper && node->status != Status::None)
        kind += QStringLiteral(" · ") + statusWord(node->status);
    m_kind->setText(kind);
    m_kind->show();
    if (node && !node->explanation.isEmpty() && entry->kind != IdKind::Paper) {
        m_sentence->setText(node->explanation);
        m_sentence->show();
    }

    // A statement's compiled preview once it's built; its source until then (E2).
    if (entry->kind == IdKind::Statement && m_tools && m_tools->previews()) {
        m_tools->previews()->request(entry->id);
        const QImage image = m_tools->previews()->image(entry->id);
        if (!image.isNull()) {
            m_picture->setPixmap(QPixmap::fromImage(image));
            m_picture->show();
            return;
        }
    }
    if (!source.trimmed().isEmpty()) {
        m_source->setText(firstLines(source));
        m_source->show();
    }
}

IdHover::IdHover(IdTools* tools, QWidget* parent)
    : QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint)
    , m_card(new IdCard(tools, this))
{
    setObjectName(QStringLiteral("idHover"));
    setFrameShape(QFrame::Box);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setBackgroundRole(QPalette::ToolTipBase);
    setAutoFillBackground(true);
    setMaximumWidth(720);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_card);
}

void IdHover::showAt(const QString& id, const QPoint& global)
{
    m_card->setId(id);
    adjustSize();
    move(onScreen(global, size()));
    show();
    raise();
}

IdPicker::IdPicker(IdTools* tools, QWidget* parent)
    : QFrame(parent, Qt::Popup)
    , m_tools(tools)
    , m_search(new QLineEdit(this))
    , m_results(new QTreeWidget(this))
    , m_nothing(new QLabel(this))
    , m_card(new IdCard(tools, this))
{
    setObjectName(QStringLiteral("idPicker"));
    setFrameShape(QFrame::StyledPanel);
    m_search->setObjectName(QStringLiteral("idPickerSearch"));
    m_search->setClearButtonEnabled(true);
    m_search->installEventFilter(this);
    m_results->setObjectName(QStringLiteral("idPickerResults"));
    m_results->setColumnCount(2);
    m_results->setHeaderHidden(true);
    m_results->setRootIsDecorated(false);
    m_results->setUniformRowHeights(true);
    m_results->header()->setStretchLastSection(false);
    m_results->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_results->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_results->setFocusPolicy(Qt::NoFocus);
    m_nothing->setObjectName(QStringLiteral("idPickerNothing"));
    m_nothing->setWordWrap(true);
    m_nothing->setForegroundRole(QPalette::PlaceholderText);
    m_card->setMinimumWidth(340);

    auto* left = new QVBoxLayout;
    left->setSpacing(4);
    left->addWidget(m_search);
    left->addWidget(m_results, 1);
    left->addWidget(m_nothing);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->addLayout(left, 3);
    layout->addWidget(m_card, 2);
    resize(820, 380);

    connect(m_search, &QLineEdit::textChanged, this, &IdPicker::update);
    connect(m_results, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item) {
        m_card->setId(item ? item->data(0, Qt::UserRole).toString() : QString());
    });
    connect(m_results, &QTreeWidget::itemDoubleClicked, this, [this] { choose(); });
}

void IdPicker::open(IdField field, const QString& self, const QString& query, const QPoint& global,
                    std::function<void(const QString&)> chosen)
{
    m_field = field;
    m_self = self;
    m_chosen = std::move(chosen);
    m_search->setPlaceholderText(pickerPrompt(field));
    m_search->blockSignals(true);
    m_search->setText(query);
    m_search->blockSignals(false);
    update();
    move(onScreen(global, size()));
    show();
    raise();
    activateWindow();
    m_search->setFocus();
    m_search->end(false);
}

QStringList IdPicker::shownIds() const
{
    QStringList ids;
    for (int i = 0; i < m_results->topLevelItemCount(); ++i)
        ids << m_results->topLevelItem(i)->data(0, Qt::UserRole).toString();
    return ids;
}

bool IdPicker::choose()
{
    QTreeWidgetItem* item = m_results->currentItem();
    if (!item)
        return false;
    const QString id = item->data(0, Qt::UserRole).toString();
    // Called after closing, so the field or editor has its focus back when it inserts.
    std::function<void(const QString&)> chosen = std::move(m_chosen);
    m_chosen = nullptr;
    close();
    if (chosen)
        chosen(id);
    return true;
}

bool IdPicker::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_search && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        switch (key->key()) {
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown: {
            // The list moves while the search box keeps the keyboard.
            QKeyEvent copy(key->type(), key->key(), key->modifiers());
            QCoreApplication::sendEvent(m_results, &copy);
            return true;
        }
        case Qt::Key_Return:
        case Qt::Key_Enter:
            // Only a result can be inserted: with none, Enter does nothing.
            choose();
            return true;
        case Qt::Key_Escape:
            close();
            return true;
        default:
            break;
        }
    }
    return QFrame::eventFilter(watched, event);
}

void IdPicker::hideEvent(QHideEvent* event)
{
    QFrame::hideEvent(event);
    m_chosen = nullptr;
    emit closed();
}

void IdPicker::update()
{
    m_results->clear();
    const std::shared_ptr<const ProjectSnapshot> snapshot = m_tools ? m_tools->snapshot() : nullptr;
    const IdIndex* index = snapshot ? snapshot->ids.get() : nullptr;
    if (!index) {
        m_nothing->setText(QStringLiteral("No project is open."));
        m_nothing->show();
        m_card->setId({});
        return;
    }
    const std::vector<IdMatch> matches = index->search(m_search->text(), m_field, m_self, m_tools->recent());
    const bool dark = darkTheme();
    int count = 0;
    for (const IdMatch& match : matches) {
        if (++count > resultLimit)
            break;
        const IdEntry& entry = *match.entry;
        auto* item = new QTreeWidgetItem(m_results);
        item->setText(0, entry.label());
        item->setData(0, Qt::UserRole, entry.id);
        item->setToolTip(0, entry.label());
        item->setText(1, entry.kind == IdKind::Proof   ? entry.kindName
                         : entry.kind == IdKind::Paper ? QStringLiteral("paper")
                                                       : kindShort(entry.kindName));
        item->setForeground(1, palette().brush(QPalette::PlaceholderText));
        if (entry.kind == IdKind::Statement || entry.kind == IdKind::Stub) {
            if (const StatementNode* node = snapshot->graph.statement(entry.id))
                item->setIcon(0, statusIcon(node->status, dark));
        }
    }
    if (m_results->topLevelItemCount() > 0) {
        m_results->setCurrentItem(m_results->topLevelItem(0));
        m_nothing->hide();
    } else {
        m_card->setId({});
        m_nothing->setText(m_search->text().trimmed().isEmpty()
                               ? QStringLiteral("There is nothing here to choose yet.")
                               : QStringLiteral("Nothing in this project matches “%1”: only what exists can be inserted.")
                                     .arg(m_search->text().trimmed()));
        m_nothing->show();
    }
}

} // namespace loom
