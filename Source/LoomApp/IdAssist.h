#pragma once

// IDs in a LaTeX editor (Specs/m2-statements-review.md, "IDs in the editor"): the picker when
// `\ref{`, `\paperref{` or `\cite{` is typed or Ctrl+Space is pressed inside one; each line's IDs
// with their titles in a grey annotation under it; IDs that name nothing underlined in red; the
// card on hover; Ctrl+click to open.

#include "LoomCore/IdSearch.h"

#include <QObject>
#include <QPoint>
#include <QTimer>

#include <vector>

namespace loom {

class IdTools;
class LatexEditor;

/// Attached to one LaTeX editor (its child; it goes with the editor).
class IdAssist : public QObject
{
    Q_OBJECT

public:
    /// The editor's indicators: IDs that name something (invisible until hovered, then
    /// underlined, so Ctrl+click can be seen to work), IDs that name nothing (red), and the
    /// brief flash of a Ctrl+click on one that names nothing.
    static constexpr int knownIndicator = 20;
    static constexpr int unknownIndicator = 21;
    static constexpr int flashIndicator = 22;
    /// How long after typing stops the IDs are read again.
    static constexpr int decorateDelayMs = 250;

    /// One ID in the text, with where it is (Scintilla's byte positions) and what it names.
    struct Span {
        int position = 0;
        int length = 0;
        IdMention mention;
        IdResolution resolution;
    };

    /// @param editor The editor; the assist becomes its child.
    /// @param tools  The tab's ID search, previews, picker and card.
    IdAssist(LatexEditor* editor, IdTools* tools);

    /// Reads the IDs of the text again now: annotations, underlines, spans.
    void decorate();
    /// The IDs found when the text was last read.
    const std::vector<Span>& spans() const { return m_spans; }
    /// Ctrl+Space: opens the picker for the braces the cursor is in.
    /// @returns Whether it is in the braces of `\ref`, `\paperref` or `\cite` (else nothing opens).
    bool openPickerAtCursor();
    /// The point in the editor's viewport at the start of a byte position, for tests and popups.
    QPoint pointOf(int position) const;
    /// Opens what a span names (Ctrl+click), or flashes its red underline if it names nothing.
    void activate(const Span& span);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// The span at a byte position, or null.
    const Span* spanAt(int position) const;

    LatexEditor* m_editor;
    IdTools* m_tools;
    QTimer m_timer;
    std::vector<Span> m_spans;
};

} // namespace loom
