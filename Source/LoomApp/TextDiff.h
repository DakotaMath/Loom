#pragma once

// **Compare** in the recovery and changed-on-disk bars (Specs/m2-statements-review.md, "Never
// losing text": "a diff of the recovered text against the file"): a line diff of two texts.

#include <QString>
#include <QStringList>

#include <vector>

class QWidget;

namespace loom {

/// One line of a diff.
struct DiffLine {
    enum class Kind { Same, Removed, Added };
    Kind kind = Kind::Same;
    QString text;
};

/// The lines of `before` and `after`, each marked same, removed (only in `before`) or added (only
/// in `after`), by the longest common subsequence of lines; removals before additions where they
/// meet. No side effects.
std::vector<DiffLine> diffLines(const QString& before, const QString& after);

/// Opens a window showing the diff from the file (`before`) to Dakota's text (`after`): removed
/// lines red with `−`, added lines green with `+`.
/// @param parent The window to centre on; the dialog deletes itself when closed.
/// @param title  Its title, e.g. "4KX7.tex: your text against the file".
/// Side effects: shows a non-modal dialog.
void showCompare(QWidget* parent, const QString& title, const QString& before, const QString& after);

} // namespace loom
