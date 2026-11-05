#include "LoomApp/ChecksPanel.h"

#include "LoomApp/ProjectSnapshot.h"
#include "LoomApp/StatusMarks.h"

#include "LoomCore/Status.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace loom {

namespace {

/// The history's columns.
enum Column { When, Who, Verdict, Version, Counts, Files, ColumnCount };

/// The check at `index` of the history, or null.
const Check* checkAt(const std::vector<Check>& checks, const std::optional<qsizetype>& index)
{
    return index && *index >= 0 && *index < static_cast<qsizetype>(checks.size()) ? &checks[static_cast<size_t>(*index)] : nullptr;
}

/// A check's day, as the sentences give it: `2026-11-04`.
QString dayOf(const Check& check)
{
    return check.date.date().toString(Qt::ISODate);
}

/// The status word a change goes to or from; `no status` for none.
QString wordOf(Status status)
{
    return status == Status::None ? QStringLiteral("no status") : statusName(status);
}

} // namespace

QString checkAuthor(const Check& check)
{
    if (check.byDakota())
        return QStringLiteral("Dakota");
    if (check.byAgent())
        return QStringLiteral("agent ") + check.by.mid(6);
    return check.by;
}

QString givesSentence(const ProofNode& proof)
{
    const ProofChecking& checking = proof.checking;
    const CheckStanding& standing = checking.standing;
    const Check* agent = checkAt(checking.checks, standing.countingAgent);
    const Check* dakota = checkAt(checking.checks, standing.countingDakota);
    const QString& tag = proof.proves;
    // Checks make only a complete proof count as complete-checked (Specs/m1-core.md, "What a proof
    // gives"); a sketch or heuristic gives its level whatever its checks say.
    const QString levelNote = proof.level == u"complete" || proof.level.isEmpty()
                                  ? QString()
                                  : QStringLiteral(" As a %1, it can make %2 no more than %1.").arg(proof.level, tag);

    if (proof.level == u"failed")
        return QStringLiteral("Marked failed: it doesn't support %1.").arg(tag);
    if (!standing.supports) {
        // Rule 7: a counting check is `fail`.
        const bool agentFails = agent && agent->verdict == u"fail";
        const bool dakotaFails = dakota && dakota->verdict == u"fail";
        if (agentFails && dakotaFails)
            return QStringLiteral("Doesn't support %1: the agent and you say it's wrong (fail, %2 and %3).").arg(tag, dayOf(*agent), dayOf(*dakota));
        if (dakotaFails)
            return QStringLiteral("Doesn't support %1: you say it's wrong (fail, %2).").arg(tag, dayOf(*dakota));
        if (agentFails)
            return QStringLiteral("Doesn't support %1: the agent says it's wrong (fail, %2).").arg(tag, dayOf(*agent));
        return QStringLiteral("Doesn't support %1.").arg(tag);
    }
    if (checking.fingerprint.isEmpty())
        return QStringLiteral("No check counts: Loom can't compute this proof's fingerprint. %1").arg(checking.fingerprintProblem).trimmed();
    if (standing.checked && agent && dakota)
        return QStringLiteral("Checked: the agent's pass of %1 and your pass of %2.").arg(dayOf(*agent), dayOf(*dakota)) + levelNote;

    QString sentence;
    if (checking.checks.empty())
        sentence = QStringLiteral("No checks yet: waiting for an agent's check.");
    else if (!agent && !dakota)
        sentence = QStringLiteral("Not checked: every check is of an older version.");
    else if (!agent)
        sentence = QStringLiteral("Waiting for an agent's check of this version.");
    else if (agent->verdict == u"needs-changes")
        sentence = QStringLiteral("Not checked: the agent asks for changes (needs-changes, %1).").arg(dayOf(*agent));
    else if (!dakota)
        sentence = QStringLiteral("Waiting for your pass.");
    else if (dakota->verdict == u"needs-changes")
        sentence = QStringLiteral("Not checked: you asked for changes (needs-changes, %1).").arg(dayOf(*dakota));
    else
        // Rule 6 wants Dakota's pass newer than the agent's.
        sentence = QStringLiteral("Waiting for your pass: the agent's pass of %1 is newer than yours of %2.").arg(dayOf(*agent), dayOf(*dakota));
    return sentence + levelNote;
}

QString statusBecomes(const std::vector<StatusChange>& changes)
{
    QStringList parts;
    for (const StatusChange& change : changes)
        parts << (change.after == Status::None ? QStringLiteral("%1 loses its status").arg(change.id)
                                               : QStringLiteral("%1 becomes %2").arg(change.id, statusName(change.after)));
    return parts.join(QStringLiteral("; "));
}

QString statusArrows(const std::vector<StatusChange>& changes)
{
    QStringList parts;
    for (const StatusChange& change : changes)
        parts << QStringLiteral("%1 %2 → %3").arg(change.id, wordOf(change.before), wordOf(change.after));
    return parts.join(QStringLiteral(", "));
}

ChecksPanel::ChecksPanel(QWidget* parent)
    : QWidget(parent)
    , m_gives(new QLabel(this))
    , m_history(new QTreeWidget(this))
    , m_pass(new QPushButton(QStringLiteral("Pass"), this))
    , m_needsChanges(new QPushButton(QStringLiteral("Needs changes…"), this))
    , m_fail(new QPushButton(QStringLiteral("Fail"), this))
    , m_markFailed(new QPushButton(QStringLiteral("Mark this proof failed…"), this))
{
    setObjectName(QStringLiteral("checksPanel"));
    m_gives->setObjectName(QStringLiteral("givesLine"));
    m_gives->setWordWrap(true);
    m_gives->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_history->setObjectName(QStringLiteral("checkHistory"));
    m_history->setColumnCount(ColumnCount);
    m_history->setHeaderLabels({QStringLiteral("When"), QStringLiteral("Who"), QStringLiteral("Verdict"), QStringLiteral("Version"),
                                QString(), QStringLiteral("Files")});
    m_history->setRootIsDecorated(false);
    m_history->setUniformRowHeights(false);
    m_history->setWordWrap(true);
    m_history->setSelectionMode(QAbstractItemView::NoSelection);
    m_history->setFocusPolicy(Qt::NoFocus);
    m_history->header()->setStretchLastSection(true);
    m_history->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_history->setMaximumHeight(150);
    m_pass->setObjectName(QStringLiteral("passCheck"));
    m_needsChanges->setObjectName(QStringLiteral("needsChangesCheck"));
    m_fail->setObjectName(QStringLiteral("failCheck"));
    m_markFailed->setObjectName(QStringLiteral("markFailed"));
    m_markFailed->hide();

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(new QLabel(QStringLiteral("Your check:"), this));
    buttons->addWidget(m_pass);
    buttons->addWidget(m_needsChanges);
    buttons->addWidget(m_fail);
    buttons->addWidget(m_markFailed);
    buttons->addStretch(1);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(m_gives);
    layout->addWidget(m_history);
    layout->addLayout(buttons);

    connect(m_pass, &QPushButton::clicked, this, [this] { emit checkRequested(QStringLiteral("pass")); });
    connect(m_needsChanges, &QPushButton::clicked, this, [this] { emit checkRequested(QStringLiteral("needs-changes")); });
    connect(m_fail, &QPushButton::clicked, this, [this] { emit checkRequested(QStringLiteral("fail")); });
    connect(m_markFailed, &QPushButton::clicked, this, &ChecksPanel::markFailedRequested);
}

void ChecksPanel::setProof(const ProjectSnapshot* snapshot, const QString& proofId)
{
    m_history->clear();
    const ProofNode* proof = snapshot ? snapshot->graph.proof(proofId) : nullptr;
    if (!proof) {
        m_gives->setText(QStringLiteral("Loom can't read this proof's front matter, so its checks don't count yet."));
        m_history->hide();
        m_markFailed->hide();
        return;
    }
    const ProofChecking& checking = proof->checking;
    m_gives->setText(givesSentence(*proof));
    // Oldest first, as the file has them (Specs/m2-statements-review.md: "a table, oldest first,
    // with every entry"). Which two count is LoomCore's (rule 5), never worked out here.
    for (qsizetype i = 0; i < static_cast<qsizetype>(checking.checks.size()); ++i) {
        const Check& check = checking.checks[static_cast<size_t>(i)];
        auto* item = new QTreeWidgetItem(m_history);
        item->setText(When, check.date.isValid() ? check.date.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("(no date)"));
        item->setText(Who, checkAuthor(check));
        item->setText(Verdict, check.verdict);
        const bool current = !checking.fingerprint.isEmpty() && check.fingerprint == checking.fingerprint;
        item->setText(Version, current ? QStringLiteral("this version") : QStringLiteral("an older version"));
        const bool counts = checking.standing.countingAgent == i || checking.standing.countingDakota == i;
        if (counts) {
            item->setText(Counts, QStringLiteral("counts"));
            QFont bold = item->font(Counts);
            bold.setBold(true);
            item->setFont(Counts, bold);
            item->setToolTip(Counts, QStringLiteral("The newest check of this version by %1: it is one of the two that count.")
                                         .arg(check.byDakota() ? QStringLiteral("you") : QStringLiteral("an agent")));
        }
        QStringList links;
        if (!check.record.isEmpty())
            links << QStringLiteral("<a href=\"%1\">record</a>").arg(check.record.toHtmlEscaped());
        if (!check.note.isEmpty())
            links << QStringLiteral("<a href=\"%1\">note</a>").arg(check.note.toHtmlEscaped());
        if (!links.isEmpty()) {
            auto* label = new QLabel(links.join(QStringLiteral(" · ")), m_history);
            label->setObjectName(QStringLiteral("files%1").arg(i));
            label->setTextFormat(Qt::RichText);
            label->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
            QStringList paths = {check.record, check.note};
            paths.removeAll(QString());
            label->setToolTip(paths.join(u'\n'));
            connect(label, &QLabel::linkActivated, this, &ChecksPanel::fileRequested);
            m_history->setItemWidget(item, Files, label);
        }
        // "A `changes` text shows in full under its entry."
        if (!check.changes.isEmpty()) {
            auto* changes = new QTreeWidgetItem(item);
            changes->setText(0, check.changes.trimmed());
            changes->setFirstColumnSpanned(true);
            changes->setFlags(Qt::ItemIsEnabled);
            item->setExpanded(true);
        }
    }
    m_history->setVisible(!checking.checks.empty());
    // "After a fail is recorded, the page offers Mark this proof failed…": Dakota's counting check
    // says fail, and the proof isn't marked failed yet.
    const Check* dakota = checkAt(checking.checks, checking.standing.countingDakota);
    m_markFailed->setVisible(dakota && dakota->verdict == u"fail" && proof->level != u"failed");
}

void ChecksPanel::setBlocked(const QString& blocked, bool busy)
{
    for (QPushButton* button : {m_pass, m_needsChanges, m_fail, m_markFailed}) {
        button->setEnabled(blocked.isEmpty() && !busy);
        button->setToolTip(busy ? QStringLiteral("Recording your last click…") : blocked);
    }
}

} // namespace loom
