#pragma once

// The bottom panel, shared by every tab (Specs/m2-statements-review.md, "The window": "Problems and
// Output … Hidden until there is something to show, or toggled with Ctrl+`"; and "The Problems
// panel and the Output view"). Problems lists the validation problems, the latest build's problems
// for the open statement and a refused save's; clicking one opens its file at its line, and F8
// moves to the next. Output keeps one channel each for Build, Git and Loom.

#include "LoomApp/ProjectSnapshot.h"

#include "LoomCore/Problem.h"

#include <QTabWidget>

#include <memory>
#include <vector>

class QPlainTextEdit;
class QTreeWidget;

namespace loom {

/// One of Output's channels.
enum class OutputChannel {
    Build, ///< Each build's command, time and full `main.log`.
    Git,   ///< Each git command (the git panel, #39).
    Loom,  ///< Each LoomCore change: what was written, refused and why.
};

/// The panel: a Problems list and an Output view.
class BottomPanel : public QTabWidget
{
    Q_OBJECT

public:
    /// How much each Output channel keeps (the spec: "Output keeps the last 5 MB per channel").
    static constexpr int channelLimit = 5 * 1024 * 1024;

    /// The Problems list's columns.
    enum Column { SeverityColumn, FileColumn, LineColumn, MessageColumn };

    explicit BottomPanel(QWidget* parent = nullptr);

    /// Uses a snapshot to name files: a statement's or proof's as `ID · title`.
    void setSnapshot(std::shared_ptr<const ProjectSnapshot> snapshot);
    /// Shows these validation problems, in the order given (errors first, as `loom validate`
    /// sorts them).
    void setProblems(const std::vector<Problem>& problems);
    /// Shows the latest build's problems for the open statement; empty clears them.
    void setBuildProblems(const std::vector<Problem>& problems);
    /// Shows a refused save's problems above the others; empty clears them.
    void setSaveProblems(const std::vector<Problem>& problems);
    /// Adds a line to a channel of Output, with the time.
    void appendOutput(OutputChannel channel, const QString& text);
    /// Adds a line to Output's Loom channel.
    void appendOutput(const QString& line) { appendOutput(OutputChannel::Loom, line); }
    /// Brings the Problems list to the front.
    void showProblems();
    /// F8 and Shift+F8: selects the next (or previous) problem, round the list, and opens it.
    /// @returns Whether there was one.
    bool stepProblem(bool forward);

    /// The label of a project file: `ID · title` for a statement or proof the snapshot has, else
    /// the path. No side effects.
    QString fileLabel(const QString& path) const;

    QTreeWidget* problems() const { return m_problems; }
    /// A channel's text.
    QPlainTextEdit* output(OutputChannel channel) const;
    /// The Output page (the channels' tabs).
    QTabWidget* outputTabs() const { return m_outputTabs; }

signals:
    /// A problem was clicked: open its file at its line.
    void problemActivated(const QString& file, int line);

private:
    /// Fills the list from the save's, the build's and the validation problems.
    void rebuild();

    std::shared_ptr<const ProjectSnapshot> m_snapshot;
    std::vector<Problem> m_validation;
    std::vector<Problem> m_build;
    std::vector<Problem> m_save;
    QTreeWidget* m_problems;
    QTabWidget* m_outputTabs;
    QPlainTextEdit* m_channels[3];
};

} // namespace loom
