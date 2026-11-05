#pragma once

// The text editor of the Statements tab (Specs/m2-statements-review.md, "The editor": "QScintilla
// with LaTeX highlighting, line numbers, matching braces, and a margin mark on lines with
// problems"), and its find and replace ("Find and replace").

#include "LoomCore/Problem.h"

#include <Qsci/qsciscintilla.h>

#include <QFont>
#include <QList>
#include <QPair>

class QsciLexerTeX;

namespace loom {

/// A QScintilla editor set up for LaTeX: UTF-8, LF line endings, the TeX lexer, line numbers,
/// brace matching, and a margin of problem marks.
class LatexEditor : public QsciScintilla
{
    Q_OBJECT

public:
    /// How a find matches.
    struct FindOptions {
        bool matchCase = false;
        bool wholeWord = false;
        bool regularExpression = false;
    };

    explicit LatexEditor(QWidget* parent = nullptr);

    /// Uses this font for every style, and the theme's colours.
    void setEditorFont(const QFont& font);
    /// Re-reads the theme's colours (light or dark) from the application's palette.
    void applyTheme();

    /// Marks lines with problems in the margin: a red mark for an error, yellow for a warning.
    /// @param lines Editor lines, from 1, with the worst severity on each.
    void setProblemLines(const QList<QPair<int, Severity>>& lines);
    /// The lines marked, from 1, for tests.
    QList<int> problemLines() const;

    /// Finds the next (or previous) match from the cursor, wrapping around, and selects it.
    /// @returns Whether there was one. Side effects: moves the selection.
    bool find(const QString& text, const FindOptions& options, bool forward = true);
    /// Replaces the selected match, if the selection is one, then finds the next.
    /// @returns Whether a replacement was made. Side effects: changes the text (one undo step).
    bool replaceOne(const QString& text, const QString& replacement, const FindOptions& options);
    /// Replaces every match in the text.
    /// @returns How many were replaced. Side effects: changes the text (one undo step).
    int replaceAll(const QString& text, const QString& replacement, const FindOptions& options);

    /// The 1-based line of the cursor.
    int cursorLine() const;
    /// Puts the cursor at the start of a 1-based line and shows it.
    void goToLine(int line);

signals:
    /// The editor lost the keyboard focus (recovery files are written then).
    void focusLost();

protected:
    void focusOutEvent(QFocusEvent* event) override;

private:
    QsciLexerTeX* m_lexer;
    QList<int> m_marked;
};

} // namespace loom
