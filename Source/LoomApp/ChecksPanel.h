#pragma once

// A proof page's checks (Specs/m2-statements-review.md, "Status and checks in the tab"): what the
// proof gives, its check history with the counting checks marked, and Dakota's buttons (Pass,
// Needs changes…, Fail, and Mark this proof failed… after a fail). It only shows what LoomCore
// computed in the snapshot and asks the Statements page to act; it writes nothing itself. Also
// the words Dakota's dialogs use for the status changes LoomCore gives them.

#include "LoomCore/Checks.h"
#include "LoomCore/StatusChanges.h"

#include <QString>
#include <QWidget>

#include <vector>

class QLabel;
class QPushButton;
class QTreeWidget;

namespace loom {

struct ProjectSnapshot;
struct ProofNode;

/// Who made a check, as the history shows it: `Dakota`, or `agent claude-opus-5-5`. No side effects.
QString checkAuthor(const Check& check);

/// The line above a proof's check history: what its checks make of it, from the snapshot
/// (Specs/m2-statements-review.md: *"Checked: the agent's pass of 2026-11-04 and your pass of
/// 2026-11-04."*, *"Waiting for your pass."*, *"Doesn't support 4KX7: the agent says it's wrong
/// (fail, 2026-11-20)."*). It puts LoomCore's standing (which checks count, checked, supports;
/// Specs/m1-core.md, "Checks", rules 4–7) into words and decides nothing itself.
/// @returns One or two sentences. No side effects.
QString givesSentence(const ProofNode& proof);

/// Status changes as a check's dialog says them: `4KX7 becomes proved; P3QA becomes proved`.
/// @returns Empty for no changes. No side effects.
QString statusBecomes(const std::vector<StatusChange>& changes);

/// Status changes as a save's dialog says them: `4KX7 proved → unchecked, P3QA proved → conditional`
/// (`no status` for a statement without one). @returns Empty for no changes. No side effects.
QString statusArrows(const std::vector<StatusChange>& changes);

/// The checks part of a proof page.
class ChecksPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ChecksPanel(QWidget* parent = nullptr);

    /// Shows a proof's checks as the snapshot has them; nothing if the snapshot lacks the proof.
    void setProof(const ProjectSnapshot* snapshot, const QString& proofId);
    /// Enables Dakota's buttons, or disables them saying why on hover.
    /// @param blocked Why they can't be used now (*"Save first: …"*), or empty.
    /// @param busy    A click of his on this proof is running: the buttons wait for it.
    void setBlocked(const QString& blocked, bool busy);

    QLabel* gives() const { return m_gives; }
    QTreeWidget* history() const { return m_history; }
    QPushButton* passButton() const { return m_pass; }
    QPushButton* needsChangesButton() const { return m_needsChanges; }
    QPushButton* failButton() const { return m_fail; }
    /// Shown after Dakota's counting check is a `fail` on a proof not yet marked failed.
    QPushButton* markFailedButton() const { return m_markFailed; }

signals:
    /// One of Dakota's verdict buttons: `pass`, `needs-changes` or `fail`.
    void checkRequested(const QString& verdict);
    /// **Mark this proof failed…**.
    void markFailedRequested();
    /// A `record` or `note` link: the project-relative path.
    void fileRequested(const QString& path);

private:
    QLabel* m_gives;
    QTreeWidget* m_history;
    QPushButton* m_pass;
    QPushButton* m_needsChanges;
    QPushButton* m_fail;
    QPushButton* m_markFailed;
};

} // namespace loom
