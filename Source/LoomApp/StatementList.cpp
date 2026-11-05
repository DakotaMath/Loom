#include "LoomApp/StatementList.h"

#include "LoomApp/IdTools.h"
#include "LoomApp/LineIcons.h"
#include "LoomApp/StatusMarks.h"

#include <QAction>
#include <QComboBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QPixmap>
#include <QRegularExpression>
#include <QScrollBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace loom {

namespace {

/// What a row is, in its UserRole + 1.
const QString statementRow = QStringLiteral("statement");
const QString proofRow = QStringLiteral("proof");
const QString stubRow = QStringLiteral("stub");
const QString paperRow = QStringLiteral("paper");
/// The ID a hover over a row shows the card of (none for a paper's row).
constexpr int idRole = Qt::UserRole + 2;

/// The words of a text, folded.
QStringList wordsOf(const QString& text)
{
    return loom::searchWords(text);
}

/// The rank of a status in the spec's order (proved first), for sorting by status.
int statusRank(Status status)
{
    const std::vector<Status>& order = statusOrder();
    return static_cast<int>(std::find(order.begin(), order.end(), status) - order.begin());
}

} // namespace

bool matchesQuery(const QString& query, const QStringList& words)
{
    return matchesEveryWord(query, words);
}

QStringList searchWords(const ProjectSnapshot& snapshot, const QString& tag)
{
    QStringList words{foldForSearch(tag)};
    if (const StatementFile* file = snapshot.contents.statement(tag)) {
        const Statement& s = file->reading.document;
        for (const QString& alias : s.aliases)
            words << foldForSearch(alias) << wordsOf(alias);
        words << wordsOf(s.title);
        for (const QString& t : s.tags)
            words << foldForSearch(t) << wordsOf(t);
        words << wordsOf(QString::fromUtf8(s.body));
    } else if (const StubFile* stub = snapshot.contents.stub(tag)) {
        const Stub& s = stub->reading.document;
        for (const QString& alias : s.aliases)
            words << foldForSearch(alias) << wordsOf(alias);
        words << wordsOf(s.title) << wordsOf(s.gist) << wordsOf(stub->paperFolder);
    }
    return words;
}

StatementList::StatementList(QWidget* parent)
    : QWidget(parent)
    , m_search(new QLineEdit(this))
    , m_filters(new QToolButton(this))
    , m_filterMenu(new QMenu(this))
    , m_sort(new QComboBox(this))
    , m_new(new QToolButton(this))
    , m_tree(new QTreeWidget(this))
{
    setObjectName(QStringLiteral("statementListPane"));
    m_search->setObjectName(QStringLiteral("search"));
    m_search->setPlaceholderText(QStringLiteral("Search: ID, title, tag or text"));
    m_search->setClearButtonEnabled(true);
    m_filters->setObjectName(QStringLiteral("filters"));
    m_filters->setText(QStringLiteral("Filter"));
    m_filters->setPopupMode(QToolButton::InstantPopup);
    m_filters->setMenu(m_filterMenu);
    m_sort->setObjectName(QStringLiteral("sort"));
    m_sort->addItem(QStringLiteral("By ID"), static_cast<int>(Sort::Id));
    m_sort->addItem(QStringLiteral("By title"), static_cast<int>(Sort::Title));
    m_sort->addItem(QStringLiteral("By status"), static_cast<int>(Sort::Status));
    m_sort->addItem(QStringLiteral("Recently changed"), static_cast<int>(Sort::RecentlyChanged));
    m_new->setObjectName(QStringLiteral("newStatement"));
    m_new->setText(QStringLiteral("+"));
    m_new->setToolTip(QStringLiteral("New statement (Ctrl+N)"));
    m_tree->setObjectName(QStringLiteral("statementList"));
    m_tree->setProperty(idRoleProperty, idRole);
    m_tree->setColumnCount(2);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    m_paperResults = new QAction(QStringLiteral("Show paper results"), this);
    m_paperResults->setObjectName(QStringLiteral("paper-results"));
    m_paperResults->setCheckable(true);
    connect(m_paperResults, &QAction::toggled, this, &StatementList::rebuild);

    auto* top = new QHBoxLayout;
    top->setSpacing(4);
    top->addWidget(m_search, 1);
    top->addWidget(m_new);
    auto* second = new QHBoxLayout;
    second->setSpacing(4);
    second->addWidget(m_filters);
    second->addWidget(m_sort, 1);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 0, 6);
    layout->setSpacing(4);
    layout->addLayout(top);
    layout->addLayout(second);
    layout->addWidget(m_tree, 1);

    connect(m_search, &QLineEdit::textChanged, this, &StatementList::rebuild);
    connect(m_sort, &QComboBox::currentIndexChanged, this, &StatementList::rebuild);
    connect(m_new, &QToolButton::clicked, this, &StatementList::newStatementClicked);
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        const QString id = item->data(0, Qt::UserRole).toString();
        const QString what = item->data(0, Qt::UserRole + 1).toString();
        if (what == statementRow)
            emit statementActivated(id);
        else if (what == proofRow)
            emit proofActivated(id);
        else if (what == stubRow)
            emit stubActivated(id);
    });
}

void StatementList::setSnapshot(std::shared_ptr<const ProjectSnapshot> snapshot)
{
    m_snapshot = std::move(snapshot);
    rebuildFilters();
    rebuild();
}

void StatementList::setUnsaved(const QSet<QString>& paths)
{
    if (paths == m_unsaved)
        return;
    m_unsaved = paths;
    rebuild();
}

void StatementList::select(const QString& id)
{
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == id && (*it)->data(0, Qt::UserRole + 1).toString() != paperRow) {
            m_tree->setCurrentItem(*it);
            m_tree->scrollToItem(*it);
            return;
        }
    }
}

QAction* StatementList::filterAction(const QString& group, const QString& value) const
{
    if (group == u"paper-results")
        return m_paperResults;
    return m_filterActions.value(group).value(value);
}

QStringList StatementList::shownIds() const
{
    QStringList ids;
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole + 1).toString() != paperRow)
            ids << (*it)->data(0, Qt::UserRole).toString();
    }
    return ids;
}

void StatementList::rebuildFilters()
{
    m_filterMenu->clear();
    m_filterActions.clear();
    if (!m_snapshot)
        return;
    const auto addGroup = [this](const QString& group, const QString& title, const QList<QPair<QString, QString>>& values) {
        QMenu* menu = m_filterMenu->addMenu(title);
        for (const auto& [value, label] : values) {
            QAction* action = menu->addAction(label);
            action->setObjectName(group + u':' + value);
            action->setCheckable(true);
            action->setChecked(m_checked.value(group).contains(value));
            connect(action, &QAction::toggled, this, [this, group, value](bool on) {
                if (on)
                    m_checked[group].insert(value);
                else
                    m_checked[group].remove(value);
                rebuild();
            });
            m_filterActions[group].insert(value, action);
        }
    };
    QList<QPair<QString, QString>> kinds;
    for (const QString& kind : statementKinds)
        kinds.append({kind, kindTitle(kind)});
    addGroup(QStringLiteral("kind"), QStringLiteral("Kind"), kinds);
    QList<QPair<QString, QString>> statuses;
    for (const Status status : statusOrder()) {
        if (status != Status::Cited && status != Status::Extracted)
            statuses.append({statusWord(status), statusWord(status)});
    }
    addGroup(QStringLiteral("status"), QStringLiteral("Status"), statuses);
    QList<QPair<QString, QString>> tags;
    for (const ProjectTag& tag : m_snapshot->config.tags)
        tags.append({tag.id, QStringLiteral("%1 · %2").arg(tag.id, tag.title)});
    addGroup(QStringLiteral("tag"), QStringLiteral("Tag"), tags);
    m_filterMenu->addSeparator();
    m_filterMenu->addAction(m_paperResults);
    const int checked = m_checked.value(QStringLiteral("kind")).size() + m_checked.value(QStringLiteral("status")).size()
                        + m_checked.value(QStringLiteral("tag")).size();
    m_filters->setText(checked > 0 ? QStringLiteral("Filter (%1)").arg(checked) : QStringLiteral("Filter"));
}

bool StatementList::passesFilters(const StatementNode& node) const
{
    const QSet<QString> kinds = m_checked.value(QStringLiteral("kind"));
    if (!kinds.isEmpty() && !kinds.contains(node.kind))
        return false;
    const QSet<QString> statuses = m_checked.value(QStringLiteral("status"));
    if (!statuses.isEmpty() && !statuses.contains(statusWord(node.status)))
        return false;
    const QSet<QString> tags = m_checked.value(QStringLiteral("tag"));
    if (!tags.isEmpty() && std::none_of(node.tags.begin(), node.tags.end(), [&](const QString& t) { return tags.contains(t); }))
        return false;
    return true;
}

QIcon StatementList::problemIcon(const QStringList& paths) const
{
    bool error = false;
    bool warning = false;
    for (const Problem& problem : m_snapshot->problems) {
        if (paths.contains(problem.file)) {
            error |= problem.severity == Severity::Error;
            warning |= problem.severity == Severity::Warning;
        }
    }
    return error ? lineIcon(LineIcon::Cross) : warning ? lineIcon(LineIcon::Warning) : QIcon();
}

QString StatementList::rowText(const QString& text, bool unsaved) const
{
    // First, so a long title elided at the end never hides it.
    return unsaved ? QStringLiteral("● ") + text : text;
}

void StatementList::rebuild()
{
    const QString current = m_tree->currentItem() ? m_tree->currentItem()->data(0, Qt::UserRole).toString() : QString();
    QSet<QString> expanded;
    for (QTreeWidgetItemIterator it(m_tree); *it; ++it) {
        if ((*it)->isExpanded())
            expanded.insert((*it)->data(0, Qt::UserRole).toString());
    }
    const int scroll = m_tree->verticalScrollBar()->value();
    const bool first = m_tree->topLevelItemCount() == 0;
    m_tree->clear();
    if (!m_snapshot)
        return;
    const ProjectSnapshot& snapshot = *m_snapshot;
    const bool dark = darkTheme();
    const QString query = m_search->text();
    // The ID picker's search (LoomCore's), over statements and stubs.
    QSet<QString> found;
    const bool searching = !query.trimmed().isEmpty() && snapshot.ids;
    if (searching) {
        for (const IdMatch& match : snapshot.ids->search(query, IdField::StatedUsing))
            found.insert(match.entry->id);
    }
    const auto matches = [&](const QString& id) { return !searching || found.contains(id); };
    QPixmap empty(12, 12);
    empty.fill(Qt::transparent);
    const QIcon blank(empty);

    std::vector<const StatementNode*> rows;
    for (const StatementNode& node : snapshot.graph.statements) {
        if (!node.paper.isEmpty())
            continue; // Stubs come under their papers, below.
        if (passesFilters(node) && matches(node.id))
            rows.push_back(&node);
    }
    // Statements whose front matter Loom couldn't read aren't in the graph; they still get a row,
    // so they can be opened (in raw mode) and fixed.
    std::vector<StatementNode> unread;
    for (const StatementFile& file : snapshot.contents.statements) {
        if (!snapshot.graph.statement(file.tag)) {
            StatementNode node;
            node.id = file.tag;
            node.path = file.path;
            node.title = QStringLiteral("(front matter Loom can't read)");
            unread.push_back(node);
        }
    }
    for (const StatementNode& node : unread) {
        if (matches(node.id))
            rows.push_back(&node);
    }

    const Sort sort = static_cast<Sort>(m_sort->currentData().toInt());
    std::stable_sort(rows.begin(), rows.end(), [&](const StatementNode* a, const StatementNode* b) {
        switch (sort) {
        case Sort::Title: {
            const int c = QString::localeAwareCompare(a->title, b->title);
            return c != 0 ? c < 0 : a->id < b->id;
        }
        case Sort::Status:
            return statusRank(a->status) != statusRank(b->status) ? statusRank(a->status) < statusRank(b->status) : a->id < b->id;
        case Sort::RecentlyChanged: {
            QDateTime ta = snapshot.modified.value(a->path);
            QDateTime tb = snapshot.modified.value(b->path);
            for (const QString& proof : a->proofs)
                if (const ProofNode* p = snapshot.graph.proof(proof))
                    ta = std::max(ta, snapshot.modified.value(p->path));
            for (const QString& proof : b->proofs)
                if (const ProofNode* p = snapshot.graph.proof(proof))
                    tb = std::max(tb, snapshot.modified.value(p->path));
            return ta != tb ? ta > tb : a->id < b->id;
        }
        case Sort::Id:
            break;
        }
        return a->id < b->id;
    });

    for (const StatementNode* node : rows) {
        auto* item = new QTreeWidgetItem(m_tree);
        const bool unsaved = m_unsaved.contains(node->path);
        item->setText(0, rowText(QStringLiteral("%1 · %2").arg(node->id, node->title), unsaved));
        item->setText(1, kindShort(node->kind));
        // A statement without a status gets an empty icon, so its text lines up with the others.
        item->setIcon(0, node->status == Status::None ? blank : statusIcon(node->status, dark));
        item->setToolTip(0, QStringLiteral("%1 · %2\n%3").arg(node->id, node->title, statusWord(node->status)));
        // In the tab, hovering shows its card instead, which spells out the status too.
        item->setData(0, idRole, node->id);
        item->setIcon(1, problemIcon({node->path}));
        item->setData(0, Qt::UserRole, node->id);
        item->setData(0, Qt::UserRole + 1, statementRow);
        if (node->status == Status::False) {
            QFont font = item->font(0);
            font.setStrikeOut(true);
            item->setFont(0, font);
        }
        QFont small = item->font(1);
        small.setPointSizeF(small.pointSizeF() * 0.85);
        item->setFont(1, small);
        item->setForeground(1, palette().brush(QPalette::PlaceholderText));

        for (const ProofFile& file : snapshot.contents.proofs) {
            if (file.folder != node->id)
                continue;
            const ProofNode* proof = snapshot.graph.proof(file.id);
            const QString level = proof ? proof->level : file.reading.document.level;
            const QString verbosePath = QStringLiteral("%1/%2.verbose.tex").arg(QFileInfo(file.path).path(), file.id);
            const QString checksPath = QStringLiteral("%1/%2.checks.toml").arg(QFileInfo(file.path).path(), file.id);
            auto* child = new QTreeWidgetItem(item);
            child->setText(0, rowText(QStringLiteral("%1 · %2").arg(file.id, level.isEmpty() ? QStringLiteral("?") : level),
                                      m_unsaved.contains(file.path) || m_unsaved.contains(verbosePath)));
            child->setData(0, Qt::UserRole, file.id);
            child->setData(0, Qt::UserRole + 1, proofRow);
            child->setData(0, idRole, file.id);
            child->setIcon(1, problemIcon({file.path, verbosePath, checksPath}));
            if (level == u"failed")
                child->setForeground(0, palette().brush(QPalette::Disabled, QPalette::Text));
        }
        if (expanded.contains(node->id) || first || unsaved)
            item->setExpanded(true);
    }

    if (m_paperResults->isChecked()) {
        for (const PaperFile& paper : snapshot.contents.papers) {
            auto* paperItem = new QTreeWidgetItem(m_tree);
            const QString key = paper.reading.document.key.isEmpty() ? paper.folder : paper.reading.document.key;
            paperItem->setText(0, key);
            paperItem->setData(0, Qt::UserRole, paper.folder);
            paperItem->setData(0, Qt::UserRole + 1, paperRow);
            paperItem->setForeground(0, palette().brush(QPalette::PlaceholderText));
            for (const StubFile& stub : snapshot.contents.stubs) {
                if (stub.paperFolder != paper.folder)
                    continue;
                const StatementNode* node = snapshot.graph.statement(stub.tag);
                if (node && (!passesFilters(*node) || !matches(stub.tag)))
                    continue;
                auto* child = new QTreeWidgetItem(paperItem);
                child->setText(0, QStringLiteral("%1 · %2").arg(stub.tag, node ? node->title : QString()));
                child->setText(1, node ? kindShort(node->kind) : QString());
                if (node)
                    child->setIcon(0, statusIcon(node->status, dark));
                child->setData(0, Qt::UserRole, stub.tag);
                child->setData(0, Qt::UserRole + 1, stubRow);
                child->setData(0, idRole, stub.tag);
            }
            paperItem->setExpanded(true);
            if (paperItem->childCount() == 0)
                delete paperItem;
        }
    }

    if (!current.isEmpty())
        select(current);
    m_tree->verticalScrollBar()->setValue(scroll);
}

} // namespace loom
