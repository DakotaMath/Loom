#pragma once

// The forms in the page headers of the Statements tab (Specs/m2-statements-review.md, "The
// editor"): a statement's front matter (kind, title, tags, stated using, superseded by, and
// read-only lines) and a proof's (level, premises). Each edits the fields of an EditBuffer; the
// page saves them, through LoomCore's writer, with the body.

#include <QIcon>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>

class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;

namespace loom {

class EditBuffer;
struct ProjectSnapshot;

/// A row of chips, one per ID or tag, each with a × to remove it, and a + to add one.
class ChipList : public QWidget
{
    Q_OBJECT

public:
    /// What a chip shows for an ID: its text and icon.
    using Describe = std::function<std::pair<QString, QIcon>(const QString& id)>;

    explicit ChipList(const QString& objectName, QWidget* parent = nullptr);

    /// Shows these items, in order.
    void setItems(const QStringList& items, const Describe& describe);
    QStringList items() const { return m_items; }
    /// Whether + and × are offered.
    void setEditable(bool editable);
    bool isEditable() const { return m_editable; }
    /// Whether the chips are IDs: hovering one then shows its card (IdTools). Set before setItems().
    void setShowsIds(bool ids) { m_showsIds = ids; }
    /// The + button, for menus and tests.
    QToolButton* addButton() const { return m_add; }

signals:
    void addClicked();
    void removeClicked(const QString& item);
    /// A chip's text was clicked (an ID chip opens its statement).
    void chipClicked(const QString& item);

private:
    QHBoxLayout* m_layout;
    QToolButton* m_add;
    QStringList m_items;
    bool m_editable = true;
    bool m_showsIds = false;
};

/// The statement page's form.
class StatementForm : public QWidget
{
    Q_OBJECT

public:
    explicit StatementForm(QWidget* parent = nullptr);

    /// Shows a buffer's fields; edits go into it. Null shows nothing.
    void setBuffer(EditBuffer* buffer, const ProjectSnapshot* snapshot);
    /// Re-reads the buffer's fields into the widgets.
    void refresh();

    QComboBox* kind() const { return m_kind; }
    QLineEdit* title() const { return m_title; }
    ChipList* tags() const { return m_tags; }
    ChipList* statedUsing() const { return m_statedUsing; }
    /// Superseded by: at most one chip, chosen with the ID picker.
    ChipList* supersededBy() const { return m_supersededBy; }
    QPushButton* markVerified() const { return m_markVerified; }
    QLabel* info() const { return m_info; }

signals:
    /// A field changed in the buffer.
    void edited();
    /// + of `stated using` was clicked: the page opens the ID picker.
    void addStatedUsingRequested();
    /// + of `superseded by` was clicked: the page opens the ID picker.
    void chooseSupersededByRequested();
    /// An ID chip was clicked.
    void idClicked(const QString& id);

private:
    /// Copies the widgets into the buffer and emits edited().
    void store();

    EditBuffer* m_buffer = nullptr;
    const ProjectSnapshot* m_snapshot = nullptr;
    bool m_updating = false;
    QComboBox* m_kind;
    QLineEdit* m_title;
    ChipList* m_tags;
    ChipList* m_statedUsing;
    ChipList* m_supersededBy;
    QPushButton* m_markVerified;
    QLabel* m_info;
};

/// The proof page's form.
class ProofForm : public QWidget
{
    Q_OBJECT

public:
    explicit ProofForm(QWidget* parent = nullptr);

    /// Shows a buffer's fields; edits go into it. Null shows nothing.
    void setBuffer(EditBuffer* buffer, const ProjectSnapshot* snapshot);
    void refresh();

    QComboBox* level() const { return m_level; }
    ChipList* premises() const { return m_premises; }
    QPushButton* markVerified() const { return m_markVerified; }
    QLabel* info() const { return m_info; }

signals:
    void edited();
    /// Dakota chose `failed` in the level list: the page asks first, then saves
    /// (Specs/m2-statements-review.md, "Status and checks in the tab": "Marking a proof failed").
    /// The buffer isn't changed yet; `previous` is the level it had.
    void failedChosen(const QString& previous);
    void addPremiseRequested();
    void idClicked(const QString& id);

private:
    void store();

    EditBuffer* m_buffer = nullptr;
    const ProjectSnapshot* m_snapshot = nullptr;
    bool m_updating = false;
    QComboBox* m_level;
    ChipList* m_premises;
    QPushButton* m_markVerified;
    QLabel* m_info;
};

/// `ID · title` and the status mark for a chip or a line, from the snapshot.
std::pair<QString, QIcon> describeId(const ProjectSnapshot* snapshot, const QString& id);

} // namespace loom
