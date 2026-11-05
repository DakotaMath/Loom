#pragma once

// The ID picker and the card that describes an ID (Specs/m2-statements-review.md, "IDs in the
// editor"): "a popup at the cursor (in the form: under the field) with a search box and a list";
// each result "as the status mark, `ID · title` …, and the kind; beside the list, the selected
// result's preview"; and the hover: "the status mark, `ID · title`, the kind, the status sentence,
// and the compiled preview".

#include "LoomCore/IdSearch.h"

#include <QFrame>
#include <QString>
#include <QStringList>

#include <functional>

class QLabel;
class QLineEdit;
class QTreeWidget;

namespace loom {

class IdTools;

/// What one ID is: the status mark, `ID · title`, the kind, the status sentence, and the preview
/// (the compiled statement once it's built, its source until then; a stub's gist and locators).
class IdCard : public QFrame
{
    Q_OBJECT

public:
    explicit IdCard(IdTools* tools, QWidget* parent = nullptr);

    /// Shows an ID (a tag, proof ID or paper key), or nothing for an empty one. Asks for its
    /// preview, and shows it when it comes.
    void setId(const QString& id);
    const QString& id() const { return m_id; }

    QLabel* heading() const { return m_heading; }
    QLabel* kind() const { return m_kind; }
    QLabel* sentence() const { return m_sentence; }
    QLabel* picture() const { return m_picture; }
    QLabel* source() const { return m_source; }
    /// Whether the compiled preview is shown (not the source).
    bool showsPicture() const;

private:
    /// Re-reads the ID from the latest snapshot and preview.
    void refresh();

    IdTools* m_tools;
    QString m_id;
    QLabel* m_mark;
    QLabel* m_heading;
    QLabel* m_kind;
    QLabel* m_sentence;
    QLabel* m_picture;
    QLabel* m_source;
};

/// The popup a hover shows: an IdCard in a tool-tip window.
class IdHover : public QFrame
{
    Q_OBJECT

public:
    explicit IdHover(IdTools* tools, QWidget* parent = nullptr);
    /// Shows an ID's card with its top-left corner at `global` (moved to stay on the screen).
    void showAt(const QString& id, const QPoint& global);
    IdCard* card() const { return m_card; }

private:
    IdCard* m_card;
};

/// The picker: a search box over a list of results and the selected one's card. Enter (or a
/// double-click) chooses; only a result can be chosen, so an ID that names nothing never is.
class IdPicker : public QFrame
{
    Q_OBJECT

public:
    explicit IdPicker(IdTools* tools, QWidget* parent = nullptr);

    /// Opens the picker.
    /// @param field  What it offers.
    /// @param self   The statement being edited, which it never offers for a field; or empty.
    /// @param query  The text to start with (what is already typed in the braces).
    /// @param global Where its top-left corner goes, moved to stay on the screen.
    /// @param chosen Called with the ID chosen (a tag, proof ID or key, never an alias), once.
    void open(IdField field, const QString& self, const QString& query, const QPoint& global,
              std::function<void(const QString&)> chosen);

    QLineEdit* search() const { return m_search; }
    QTreeWidget* results() const { return m_results; }
    IdCard* card() const { return m_card; }
    /// The IDs listed, in order, for tests.
    QStringList shownIds() const;
    /// Chooses the selected result, if any. @returns Whether one was chosen (the picker then closes).
    bool choose();

signals:
    /// The picker closed, whether something was chosen or not.
    void closed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    /// Searches again for the box's text.
    void update();

    IdTools* m_tools;
    IdField m_field = IdField::Ref;
    QString m_self;
    std::function<void(const QString&)> m_chosen;
    QLineEdit* m_search;
    QTreeWidget* m_results;
    QLabel* m_nothing;
    IdCard* m_card;
};

/// The placeholder of the picker's search box for a field. No side effects.
QString pickerPrompt(IdField field);

} // namespace loom
