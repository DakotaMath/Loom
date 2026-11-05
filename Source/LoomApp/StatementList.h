#pragma once

// The list on the left of the Statements tab (Specs/m2-statements-review.md, "The list"): every
// statement with its proofs, as status mark, `ID · title` and kind; search, filters and sort.

#include "LoomApp/ProjectSnapshot.h"

#include <QHash>
#include <QIcon>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <memory>

class QAction;
class QComboBox;
class QLineEdit;
class QMenu;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace loom {

/// Whether a statement or stub matches a search: every word of the query is the start of a word
/// of its tag, aliases, title, tags or body, ignoring case and accents (Specs/m2-statements-review.md,
/// "The list", "Search"). An empty query matches everything. LoomCore's matchesEveryWord(); the list
/// itself filters with the ID picker's search (IdIndex::search(), IdField::StatedUsing).
/// @param query The search box's text.
/// @param words The statement's words, from searchWords().
/// No side effects.
bool matchesQuery(const QString& query, const QStringList& words);

/// The words a statement or stub is found by: its tag, aliases, title, tags and body text, folded
/// to lower case without accents. No side effects.
QStringList searchWords(const ProjectSnapshot& snapshot, const QString& tag);

// foldForSearch() is LoomCore's (LoomCore/IdSearch.h, included through ProjectSnapshot.h).

/// The list widget.
class StatementList : public QWidget
{
    Q_OBJECT

public:
    /// The sort orders (Specs/m2-statements-review.md, "Sort").
    enum class Sort { Id, Title, Status, RecentlyChanged };

    explicit StatementList(QWidget* parent = nullptr);

    /// Shows the project as the snapshot has it, keeping the selection, what is expanded and the
    /// scroll position.
    void setSnapshot(std::shared_ptr<const ProjectSnapshot> snapshot);
    /// The files with unsaved edits, which get a dot.
    void setUnsaved(const QSet<QString>& paths);
    /// Selects the row of a statement or proof, without opening it.
    void select(const QString& id);

    QLineEdit* search() const { return m_search; }
    QComboBox* sort() const { return m_sort; }
    QToolButton* filters() const { return m_filters; }
    QTreeWidget* tree() const { return m_tree; }
    /// The filter menu's action for a kind, status word, tag, or `paper-results`, for tests.
    QAction* filterAction(const QString& group, const QString& value) const;

    /// The IDs shown, top to bottom, statements and proofs (and stubs when shown), for tests.
    QStringList shownIds() const;

signals:
    /// A statement row was clicked.
    void statementActivated(const QString& tag);
    /// A proof row was clicked.
    void proofActivated(const QString& proofId);
    /// A paper stub row was clicked.
    void stubActivated(const QString& tag);
    /// The + at the top was clicked.
    void newStatementClicked();

private:
    /// Fills the tree from the snapshot, the search, the filters and the sort.
    void rebuild();
    /// Fills the filter menu's kind, status and tag entries from the snapshot.
    void rebuildFilters();
    /// Whether a statement passes the filter menu.
    bool passesFilters(const StatementNode& node) const;
    /// The mark for a file's problems: red for errors, yellow for warnings.
    QIcon problemIcon(const QStringList& paths) const;
    /// A row's text, after a dot when it has unsaved edits.
    QString rowText(const QString& text, bool unsaved) const;

    std::shared_ptr<const ProjectSnapshot> m_snapshot;
    QSet<QString> m_unsaved;
    QLineEdit* m_search;
    QToolButton* m_filters;
    QMenu* m_filterMenu;
    QComboBox* m_sort;
    QToolButton* m_new;
    QTreeWidget* m_tree;
    QAction* m_paperResults;
    /// Checked filter values by group (`kind`, `status`, `tag`), kept across rebuilds of the menu.
    QHash<QString, QSet<QString>> m_checked;
    QHash<QString, QHash<QString, QAction*>> m_filterActions;
};

} // namespace loom
