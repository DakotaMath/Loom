#include "LoomApp/BottomPanel.h"

#include "LoomApp/LineIcons.h"

#include <QHeaderView>
#include <QPlainTextEdit>
#include <QTreeWidget>

namespace loom {

BottomPanel::BottomPanel(QWidget* parent)
    : QTabWidget(parent)
    , m_problems(new QTreeWidget(this))
    , m_output(new QPlainTextEdit(this))
{
    setObjectName(QStringLiteral("bottomPanel"));
    setDocumentMode(true);
    m_problems->setObjectName(QStringLiteral("problems"));
    m_problems->setColumnCount(3);
    m_problems->setHeaderLabels({QStringLiteral("File"), QStringLiteral("Line"), QStringLiteral("Problem")});
    m_problems->setRootIsDecorated(false);
    m_problems->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_problems->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_output->setObjectName(QStringLiteral("output"));
    m_output->setReadOnly(true);
    addTab(m_problems, QStringLiteral("Problems"));
    addTab(m_output, QStringLiteral("Output"));
}

void BottomPanel::setProblems(const std::vector<Problem>& problems)
{
    m_problems->clear();
    for (const Problem& problem : problems) {
        auto* item = new QTreeWidgetItem(m_problems, {problem.file, QString::number(problem.line), problem.message});
        const bool error = problem.severity == Severity::Error;
        item->setIcon(0, lineIcon(error ? LineIcon::Cross : LineIcon::Warning));
        item->setData(0, Qt::UserRole, problem.code);
        item->setToolTip(2, problem.message);
    }
    setTabText(indexOf(m_problems), problems.empty() ? QStringLiteral("Problems") : QStringLiteral("Problems (%1)").arg(problems.size()));
}

void BottomPanel::showProblems()
{
    setCurrentWidget(m_problems);
}

} // namespace loom
