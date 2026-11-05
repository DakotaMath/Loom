#include "LoomApp/HeaderForms.h"

#include "LoomApp/EditBuffer.h"
#include "LoomApp/ProjectSnapshot.h"
#include "LoomApp/StatusMarks.h"

#include <QComboBox>
#include <QDate>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>

namespace loom {

std::pair<QString, QIcon> describeId(const ProjectSnapshot* snapshot, const QString& id)
{
    if (snapshot) {
        if (const StatementNode* node = snapshot->graph.statement(id))
            return {QStringLiteral("%1 · %2").arg(id, node->title), statusIcon(node->status, darkTheme())};
    }
    return {QStringLiteral("%1 (names nothing)").arg(id), QIcon()};
}

ChipList::ChipList(const QString& objectName, QWidget* parent)
    : QWidget(parent)
    , m_layout(new QHBoxLayout(this))
    , m_add(new QToolButton(this))
{
    setObjectName(objectName);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(4);
    m_add->setObjectName(QStringLiteral("add"));
    m_add->setText(QStringLiteral("+"));
    m_add->setAutoRaise(true);
    m_layout->addWidget(m_add);
    m_layout->addStretch(1);
    connect(m_add, &QToolButton::clicked, this, &ChipList::addClicked);
}

void ChipList::setItems(const QStringList& items, const Describe& describe)
{
    m_items = items;
    // Everything before the + is a chip.
    // Later, not now: a chip's × may be what led here.
    while (m_layout->count() > 2) {
        QLayoutItem* item = m_layout->takeAt(0);
        if (QWidget* old = item->widget()) {
            old->setObjectName(QString());
            old->hide();
            old->deleteLater();
        }
        delete item;
    }
    int at = 0;
    for (const QString& id : items) {
        auto* chip = new QFrame(this);
        chip->setObjectName(QStringLiteral("chip:") + id);
        chip->setFrameShape(QFrame::StyledPanel);
        auto* chipLayout = new QHBoxLayout(chip);
        chipLayout->setContentsMargins(6, 0, 2, 0);
        chipLayout->setSpacing(0);
        auto* label = new QToolButton(chip);
        label->setObjectName(QStringLiteral("label"));
        label->setAutoRaise(true);
        label->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        const auto [text, icon] = describe(id);
        label->setText(text);
        label->setIcon(icon);
        connect(label, &QToolButton::clicked, this, [this, id] { emit chipClicked(id); });
        auto* remove = new QToolButton(chip);
        remove->setObjectName(QStringLiteral("remove"));
        remove->setText(QStringLiteral("×"));
        remove->setAutoRaise(true);
        remove->setToolTip(QStringLiteral("Remove %1").arg(id));
        remove->setVisible(m_editable);
        connect(remove, &QToolButton::clicked, this, [this, id] { emit removeClicked(id); });
        chipLayout->addWidget(label);
        chipLayout->addWidget(remove);
        m_layout->insertWidget(at++, chip);
        // Shown now: a widget added to a layout that is already on screen is otherwise shown only
        // when the event loop next runs.
        chip->show();
    }
}

void ChipList::setEditable(bool editable)
{
    m_editable = editable;
    m_add->setVisible(editable);
    for (QToolButton* remove : findChildren<QToolButton*>(QStringLiteral("remove")))
        remove->setVisible(editable);
}

StatementForm::StatementForm(QWidget* parent)
    : QWidget(parent)
    , m_kind(new QComboBox(this))
    , m_title(new QLineEdit(this))
    , m_tags(new ChipList(QStringLiteral("tags"), this))
    , m_statedUsing(new ChipList(QStringLiteral("statedUsing"), this))
    , m_supersededBy(new QLineEdit(this))
    , m_markVerified(new QPushButton(QStringLiteral("Mark verified"), this))
    , m_info(new QLabel(this))
{
    setObjectName(QStringLiteral("statementForm"));
    m_kind->setObjectName(QStringLiteral("kind"));
    for (const QString& kind : statementKinds)
        m_kind->addItem(kindTitle(kind), kind);
    m_title->setObjectName(QStringLiteral("title"));
    m_supersededBy->setObjectName(QStringLiteral("supersededBy"));
    m_supersededBy->setPlaceholderText(QStringLiteral("The tag of the statement that replaces this one"));
    m_markVerified->setObjectName(QStringLiteral("markVerified"));
    m_markVerified->setToolTip(QStringLiteral("Sets `verified` to today: you checked this front matter"));
    m_info->setObjectName(QStringLiteral("statementInfo"));
    m_info->setForegroundRole(QPalette::PlaceholderText);
    m_info->setWordWrap(true);

    auto* infoRow = new QHBoxLayout;
    infoRow->addWidget(m_info, 1);
    infoRow->addWidget(m_markVerified);
    auto* layout = new QFormLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addRow(QStringLiteral("Kind"), m_kind);
    layout->addRow(QStringLiteral("Title"), m_title);
    layout->addRow(QStringLiteral("Tags"), m_tags);
    layout->addRow(QStringLiteral("Stated using"), m_statedUsing);
    layout->addRow(QStringLiteral("Superseded by"), m_supersededBy);
    layout->addRow(infoRow);

    connect(m_kind, &QComboBox::currentIndexChanged, this, [this] { store(); });
    connect(m_title, &QLineEdit::textEdited, this, [this] { store(); });
    connect(m_supersededBy, &QLineEdit::textEdited, this, [this] { store(); });
    connect(m_markVerified, &QPushButton::clicked, this, [this] {
        if (!m_buffer)
            return;
        Statement s = m_buffer->statement();
        s.verified = QDate::currentDate();
        m_buffer->setStatement(s);
        refresh();
        emit edited();
    });
    connect(m_tags, &ChipList::addClicked, this, [this] {
        if (!m_buffer || !m_snapshot)
            return;
        QMenu menu;
        for (const ProjectTag& tag : m_snapshot->config.tags) {
            if (m_buffer->statement().tags.contains(tag.id))
                continue;
            QAction* action = menu.addAction(QStringLiteral("%1 · %2").arg(tag.id, tag.title));
            action->setData(tag.id);
        }
        if (menu.isEmpty())
            menu.addAction(QStringLiteral("Every tag of loom.toml is here already"))->setEnabled(false);
        if (QAction* chosen = menu.exec(m_tags->addButton()->mapToGlobal(QPoint(0, m_tags->addButton()->height())));
            chosen && chosen->data().isValid()) {
            Statement s = m_buffer->statement();
            s.tags << chosen->data().toString();
            m_buffer->setStatement(s);
            refresh();
            emit edited();
        }
    });
    connect(m_tags, &ChipList::removeClicked, this, [this](const QString& tag) {
        Statement s = m_buffer->statement();
        s.tags.removeAll(tag);
        m_buffer->setStatement(s);
        refresh();
        emit edited();
    });
    connect(m_statedUsing, &ChipList::addClicked, this, &StatementForm::addStatedUsingRequested);
    connect(m_statedUsing, &ChipList::removeClicked, this, [this](const QString& id) {
        Statement s = m_buffer->statement();
        s.statedUsing.removeAll(id);
        m_buffer->setStatement(s);
        refresh();
        emit edited();
    });
    connect(m_statedUsing, &ChipList::chipClicked, this, &StatementForm::idClicked);
}

void StatementForm::setBuffer(EditBuffer* buffer, const ProjectSnapshot* snapshot)
{
    m_buffer = buffer;
    m_snapshot = snapshot;
    refresh();
}

void StatementForm::refresh()
{
    if (!m_buffer)
        return;
    m_updating = true;
    const Statement& s = m_buffer->statement();
    int index = m_kind->findData(s.kind);
    if (index < 0) {
        m_kind->addItem(s.kind, s.kind);
        index = m_kind->count() - 1;
    }
    m_kind->setCurrentIndex(index);
    if (m_title->text() != s.title)
        m_title->setText(s.title);
    m_tags->setItems(s.tags, [this](const QString& tag) -> std::pair<QString, QIcon> {
        if (m_snapshot) {
            for (const ProjectTag& t : m_snapshot->config.tags)
                if (t.id == tag)
                    return {tag, QIcon()};
        }
        return {tag + QStringLiteral(" (not in loom.toml)"), QIcon()};
    });
    m_statedUsing->setItems(s.statedUsing, [this](const QString& id) { return describeId(m_snapshot, id); });
    if (m_supersededBy->text() != s.supersededBy)
        m_supersededBy->setText(s.supersededBy);

    QStringList info;
    if (!s.aliases.isEmpty())
        info << QStringLiteral("Aliases: %1").arg(s.aliases.join(QStringLiteral(", ")));
    info << QStringLiteral("Origin: %1").arg(s.origin.isEmpty() ? QStringLiteral("new") : s.origin);
    if (s.created.isValid())
        info << QStringLiteral("Created %1").arg(s.created.toString(Qt::ISODate));
    info << (s.verified.isValid() ? QStringLiteral("Verified %1").arg(s.verified.toString(Qt::ISODate)) : QStringLiteral("Not verified"));
    if (!s.refutedBy.isEmpty() || s.refutedOn.isValid())
        info << QStringLiteral("Refuted by %1 on %2").arg(s.refutedBy, s.refutedOn.toString(Qt::ISODate));
    m_info->setText(info.join(QStringLiteral("  ·  ")));
    m_updating = false;
}

void StatementForm::store()
{
    if (m_updating || !m_buffer)
        return;
    Statement s = m_buffer->statement();
    s.kind = m_kind->currentData().toString();
    s.title = m_title->text();
    s.supersededBy = m_supersededBy->text().trimmed();
    m_buffer->setStatement(s);
    emit edited();
}

ProofForm::ProofForm(QWidget* parent)
    : QWidget(parent)
    , m_level(new QComboBox(this))
    , m_premises(new ChipList(QStringLiteral("premises"), this))
    , m_markVerified(new QPushButton(QStringLiteral("Mark verified"), this))
    , m_info(new QLabel(this))
{
    setObjectName(QStringLiteral("proofForm"));
    m_level->setObjectName(QStringLiteral("level"));
    for (const QString& level : proofLevels)
        m_level->addItem(level, level);
    m_markVerified->setObjectName(QStringLiteral("markVerified"));
    m_markVerified->setToolTip(QStringLiteral("Sets `verified` to today: you checked this front matter"));
    m_info->setObjectName(QStringLiteral("proofInfo"));
    m_info->setForegroundRole(QPalette::PlaceholderText);

    auto* infoRow = new QHBoxLayout;
    infoRow->addWidget(m_info, 1);
    infoRow->addWidget(m_markVerified);
    auto* layout = new QFormLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addRow(QStringLiteral("Level"), m_level);
    layout->addRow(QStringLiteral("Premises"), m_premises);
    layout->addRow(infoRow);

    connect(m_level, &QComboBox::activated, this, [this] {
        if (m_updating || !m_buffer)
            return;
        const QString previous = m_buffer->proof().level;
        const QString chosen = m_level->currentData().toString();
        if (chosen == u"failed" && previous != u"failed") {
            // Not stored: the page asks first (Dakota's click), then stores and saves.
            m_updating = true;
            m_level->setCurrentIndex(m_level->findData(previous));
            m_updating = false;
            emit failedChosen(previous);
            return;
        }
        store();
    });
    connect(m_markVerified, &QPushButton::clicked, this, [this] {
        if (!m_buffer)
            return;
        Proof p = m_buffer->proof();
        p.verified = QDate::currentDate();
        m_buffer->setProof(p);
        refresh();
        emit edited();
    });
    connect(m_premises, &ChipList::addClicked, this, &ProofForm::addPremiseRequested);
    connect(m_premises, &ChipList::removeClicked, this, [this](const QString& id) {
        Proof p = m_buffer->proof();
        p.premises.removeAll(id);
        m_buffer->setProof(p);
        refresh();
        emit edited();
    });
    connect(m_premises, &ChipList::chipClicked, this, &ProofForm::idClicked);
}

void ProofForm::setBuffer(EditBuffer* buffer, const ProjectSnapshot* snapshot)
{
    m_buffer = buffer;
    m_snapshot = snapshot;
    refresh();
}

void ProofForm::refresh()
{
    if (!m_buffer)
        return;
    m_updating = true;
    const Proof& p = m_buffer->proof();
    int index = m_level->findData(p.level);
    if (index < 0) {
        m_level->addItem(p.level, p.level);
        index = m_level->count() - 1;
    }
    m_level->setCurrentIndex(index);
    m_premises->setItems(p.premises, [this](const QString& id) { return describeId(m_snapshot, id); });
    QStringList info;
    if (p.created.isValid())
        info << QStringLiteral("Created %1").arg(p.created.toString(Qt::ISODate));
    info << (p.verified.isValid() ? QStringLiteral("Verified %1").arg(p.verified.toString(Qt::ISODate)) : QStringLiteral("Not verified"));
    m_info->setText(info.join(QStringLiteral("  ·  ")));
    m_updating = false;
}

void ProofForm::store()
{
    if (m_updating || !m_buffer)
        return;
    Proof p = m_buffer->proof();
    p.level = m_level->currentData().toString();
    m_buffer->setProof(p);
    emit edited();
}

} // namespace loom
