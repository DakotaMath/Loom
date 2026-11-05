#pragma once

// The bottom panel, shared by every tab (Specs/m2-statements-review.md, "The window": "Problems and
// Output … Hidden until there is something to show, or toggled with Ctrl+`"). It lists the
// validation problems and a refused save's, and clicking one opens its file at its line (#35);
// Output keeps what LoomCore wrote and refused. Its channels come with the build (#36).

#include "LoomCore/Problem.h"

#include <QTabWidget>

#include <vector>

class QPlainTextEdit;
class QTreeWidget;

namespace loom {

/// The panel: a Problems list and an Output view.
class BottomPanel : public QTabWidget
{
    Q_OBJECT

public:
    explicit BottomPanel(QWidget* parent = nullptr);

    /// Shows these problems, in the order given (errors first, as `loom validate` sorts them).
    void setProblems(const std::vector<Problem>& problems);
    /// Shows a refused save's problems above the validation problems; empty clears them.
    void setSaveProblems(const std::vector<Problem>& problems);
    /// Adds a line to Output, with the time.
    void appendOutput(const QString& line);
    /// Brings the Problems list to the front.
    void showProblems();

    QTreeWidget* problems() const { return m_problems; }
    QPlainTextEdit* output() const { return m_output; }

signals:
    /// A problem was clicked: open its file at its line.
    void problemActivated(const QString& file, int line);

private:
    /// Fills the list from the save's and the validation problems.
    void rebuild();

    std::vector<Problem> m_validation;
    std::vector<Problem> m_save;
    QTreeWidget* m_problems;
    QPlainTextEdit* m_output;
};

} // namespace loom
