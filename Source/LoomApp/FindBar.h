#pragma once

// The find and replace bar over the editor (Specs/m2-statements-review.md, "Find and replace":
// "Ctrl+F finds in the open file (match case, whole word, regular expression); Ctrl+H adds replace
// and replace all, in the open file only").

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace loom {

class LatexEditor;

/// A bar with a find box and its options, and, when replacing, a replace box and its buttons.
/// It acts on one editor at a time.
class FindBar : public QWidget
{
    Q_OBJECT

public:
    explicit FindBar(QWidget* parent = nullptr);

    /// The editor finds act on; null for none.
    void setEditor(LatexEditor* editor);
    /// Shows the bar, with the replace row if `replace`, and puts the selected text (one line of
    /// it) in the find box, focused.
    void open(bool replace);
    /// Finds the next match (F3), or the previous (Shift+F3).
    /// @returns Whether there was one.
    bool findNext(bool forward = true);

    QLineEdit* findBox() const { return m_find; }
    QLineEdit* replaceBox() const { return m_replace; }
    QCheckBox* matchCase() const { return m_matchCase; }
    QCheckBox* wholeWord() const { return m_wholeWord; }
    QCheckBox* regularExpression() const { return m_regex; }
    QPushButton* replaceButton() const { return m_replaceOne; }
    QPushButton* replaceAllButton() const { return m_replaceAll; }
    /// "No matches", "3 replaced", or empty.
    QLabel* note() const { return m_note; }

protected:
    /// Escape closes the bar and gives the editor its focus back.
    void keyPressEvent(QKeyEvent* event) override;

private:
    LatexEditor* m_editor = nullptr;
    QLineEdit* m_find;
    QLineEdit* m_replace;
    QCheckBox* m_matchCase;
    QCheckBox* m_wholeWord;
    QCheckBox* m_regex;
    QPushButton* m_replaceOne;
    QPushButton* m_replaceAll;
    QWidget* m_replaceRow;
    QLabel* m_note;
};

} // namespace loom
