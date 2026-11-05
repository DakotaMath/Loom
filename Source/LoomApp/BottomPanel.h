#pragma once

// The bottom panel, shared by every tab (Specs/m2-statements-review.md, "The window": "Problems and
// Output … Hidden until there is something to show, or toggled with Ctrl+`"). #34 lists the
// validation problems; jumping to a problem's line and the Output channels come with the editor
// and the build (#35, #36).

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
    /// Brings the Problems list to the front.
    void showProblems();

    QTreeWidget* problems() const { return m_problems; }
    QPlainTextEdit* output() const { return m_output; }

private:
    QTreeWidget* m_problems;
    QPlainTextEdit* m_output;
};

} // namespace loom
