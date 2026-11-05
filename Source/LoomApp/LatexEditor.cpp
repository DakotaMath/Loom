#include "LoomApp/LatexEditor.h"

#include <Qsci/qscicommand.h>
#include <Qsci/qscicommandset.h>
#include <Qsci/qscilexertex.h>

#include <QApplication>
#include <QFocusEvent>
#include <QPalette>

#include <algorithm>

namespace loom {

namespace {

/// The margins: line numbers, then the problem marks.
constexpr int numberMargin = 0;
constexpr int markMargin = 1;
/// The markers for an error and a warning line.
constexpr int errorMarker = 0;
constexpr int warningMarker = 1;

} // namespace

LatexEditor::LatexEditor(QWidget* parent)
    : QsciScintilla(parent)
    , m_lexer(new QsciLexerTeX(this))
{
    setUtf8(true);
    // Loom writes LF (Specs/m1-core.md, "Front matter"); the editor makes the same.
    setEolMode(QsciScintilla::EolUnix);
    setLexer(m_lexer);
    setBraceMatching(QsciScintilla::SloppyBraceMatch);
    setWrapMode(QsciScintilla::WrapWord);
    setWrapVisualFlags(QsciScintilla::WrapFlagByBorder);
    setIndentationsUseTabs(false);
    setTabWidth(2);
    setAutoIndent(true);
    setCaretLineVisible(true);

    setMarginType(numberMargin, QsciScintilla::NumberMargin);
    setMarginLineNumbers(numberMargin, true);
    setMarginWidth(numberMargin, QStringLiteral("0000"));
    setMarginType(markMargin, QsciScintilla::SymbolMargin);
    setMarginWidth(markMargin, 12);
    setMarginSensitivity(markMargin, false);
    markerDefine(QsciScintilla::Circle, errorMarker);
    markerDefine(QsciScintilla::Circle, warningMarker);
    // Ctrl+- and Ctrl++ change the editor font size in Settings (the Statement menu), for every
    // editor, rather than zooming this one view.
    for (const int key : {Qt::Key_Minus | static_cast<int>(Qt::ControlModifier), Qt::Key_Plus | static_cast<int>(Qt::ControlModifier)}) {
        if (QsciCommand* command = standardCommands()->boundTo(key))
            command->setKey(0);
    }
    setMarginMarkerMask(markMargin, (1 << errorMarker) | (1 << warningMarker));
    applyTheme();
}

void LatexEditor::setEditorFont(const QFont& font)
{
    m_lexer->setFont(font, -1); // Every style.
    m_lexer->setDefaultFont(font);
    setMarginsFont(font);
    setMarginWidth(numberMargin, QStringLiteral("0000"));
}

void LatexEditor::applyTheme()
{
    const QPalette palette = QApplication::palette();
    const bool dark = palette.color(QPalette::Window).lightness() < 128;
    const QColor paper = palette.color(QPalette::Base);
    const QColor text = palette.color(QPalette::Text);
    m_lexer->setDefaultPaper(paper);
    m_lexer->setPaper(paper, -1);
    m_lexer->setDefaultColor(text);
    m_lexer->setColor(text, -1);
    // Commands stand out; groups and specials are quieter; math symbols in their own colour.
    m_lexer->setColor(dark ? QColor(0x7cb7ff) : QColor(0x1a4fa0), QsciLexerTeX::Command);
    m_lexer->setColor(dark ? QColor(0xd7a8ff) : QColor(0x7b2fa8), QsciLexerTeX::Symbol);
    m_lexer->setColor(dark ? QColor(0xe0b070) : QColor(0x8a5a00), QsciLexerTeX::Special);
    m_lexer->setColor(dark ? QColor(0x9ad0a0) : QColor(0x2e6b32), QsciLexerTeX::Group);
    setCaretForegroundColor(text);
    setCaretLineBackgroundColor(dark ? paper.lighter(125) : paper.darker(104));
    setMarginsBackgroundColor(palette.color(QPalette::Window));
    setMarginsForegroundColor(palette.color(QPalette::PlaceholderText));
    setMatchedBraceBackgroundColor(dark ? QColor(0x3d5a3d) : QColor(0xc8f0c8));
    setMatchedBraceForegroundColor(text);
    setUnmatchedBraceForegroundColor(QColor(0xd03030));
    setSelectionBackgroundColor(palette.color(QPalette::Highlight));
    setSelectionForegroundColor(palette.color(QPalette::HighlightedText));
    setMarkerBackgroundColor(QColor(0xd03030), errorMarker);
    setMarkerForegroundColor(QColor(0xd03030), errorMarker);
    setMarkerBackgroundColor(QColor(0xe0a800), warningMarker);
    setMarkerForegroundColor(QColor(0xe0a800), warningMarker);
}

void LatexEditor::setProblemLines(const QList<QPair<int, Severity>>& lines)
{
    markerDeleteAll(errorMarker);
    markerDeleteAll(warningMarker);
    m_marked.clear();
    for (const auto& [line, severity] : lines) {
        if (line < 1 || line > this->lines())
            continue;
        markerAdd(line - 1, severity == Severity::Error ? errorMarker : warningMarker);
        m_marked << line;
    }
}

QList<int> LatexEditor::problemLines() const
{
    return m_marked;
}

bool LatexEditor::find(const QString& text, const FindOptions& options, bool forward)
{
    if (text.isEmpty())
        return false;
    int line = -1;
    int index = -1;
    if (!forward && hasSelectedText()) {
        // Backwards from the start of the selection, so the match already selected isn't found again.
        int lineTo = 0;
        int indexTo = 0;
        getSelection(&line, &index, &lineTo, &indexTo);
    }
    return findFirst(text, options.regularExpression, options.matchCase, options.wholeWord, true, forward, line, index, true,
                     false, options.regularExpression);
}

bool LatexEditor::replaceOne(const QString& text, const QString& replacement, const FindOptions& options)
{
    if (text.isEmpty())
        return false;
    // Only a selection that is a match is replaced: find it from the selection's start.
    bool replaced = false;
    if (hasSelectedText()) {
        int lineFrom = 0;
        int indexFrom = 0;
        int lineTo = 0;
        int indexTo = 0;
        getSelection(&lineFrom, &indexFrom, &lineTo, &indexTo);
        if (findFirst(text, options.regularExpression, options.matchCase, options.wholeWord, false, true, lineFrom, indexFrom,
                      false, false, options.regularExpression)) {
            int foundLine = 0;
            int foundIndex = 0;
            int foundLineTo = 0;
            int foundIndexTo = 0;
            getSelection(&foundLine, &foundIndex, &foundLineTo, &foundIndexTo);
            if (foundLine == lineFrom && foundIndex == indexFrom && foundLineTo == lineTo && foundIndexTo == indexTo) {
                replace(replacement);
                replaced = true;
            } else {
                setSelection(lineFrom, indexFrom, lineTo, indexTo);
            }
        }
    }
    find(text, options, true);
    return replaced;
}

int LatexEditor::replaceAll(const QString& text, const QString& replacement, const FindOptions& options)
{
    if (text.isEmpty())
        return 0;
    int count = 0;
    beginUndoAction();
    bool found = findFirst(text, options.regularExpression, options.matchCase, options.wholeWord, false, true, 0, 0, false,
                           false, options.regularExpression);
    int lastLine = -1;
    int lastIndex = -1;
    while (found) {
        int line = 0;
        int index = 0;
        getCursorPosition(&line, &index);
        replace(replacement);
        ++count;
        getCursorPosition(&line, &index);
        // An empty match (a regular expression like `x*`) would match at the same place for ever.
        if (line == lastLine && index == lastIndex)
            break;
        lastLine = line;
        lastIndex = index;
        found = findNext();
    }
    endUndoAction();
    return count;
}

int LatexEditor::cursorLine() const
{
    int line = 0;
    int index = 0;
    getCursorPosition(&line, &index);
    return line + 1;
}

void LatexEditor::goToLine(int line)
{
    const int target = std::clamp(line, 1, std::max(1, lines())) - 1;
    setCursorPosition(target, 0);
    ensureLineVisible(target);
}

void LatexEditor::focusOutEvent(QFocusEvent* event)
{
    QsciScintilla::focusOutEvent(event);
    emit focusLost();
}

} // namespace loom
