#include "LoomApp/BottomPanel.h"

#include "LoomApp/LineIcons.h"

#include <QDateTime>
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
    connect(m_problems, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item) {
        emit problemActivated(item->data(0, Qt::UserRole + 1).toString(), item->data(1, Qt::UserRole).toInt());
    });
}

void BottomPanel::setProblems(const std::vector<Problem>& problems)
{
    m_validation = problems;
    rebuild();
}

void BottomPanel::setSaveProblems(const std::vector<Problem>& problems)
{
    m_save = problems;
    rebuild();
}

void BottomPanel::rebuild()
{
    m_problems->clear();
    const auto add = [this](const Problem& problem, bool refused) {
        auto* item = new QTreeWidgetItem(m_problems, {problem.file, QString::number(problem.line),
                                                      refused ? QStringLiteral("Not saved: ") + problem.message : problem.message});
        const bool error = problem.severity == Severity::Error;
        item->setIcon(0, lineIcon(error ? LineIcon::Cross : LineIcon::Warning));
        item->setData(0, Qt::UserRole, problem.code);
        item->setData(0, Qt::UserRole + 1, problem.file);
        item->setData(1, Qt::UserRole, problem.line);
        item->setToolTip(2, problem.message);
    };
    for (const Problem& problem : m_save)
        add(problem, true);
    for (const Problem& problem : m_validation)
        add(problem, false);
    const std::size_t count = m_save.size() + m_validation.size();
    setTabText(indexOf(m_problems), count == 0 ? QStringLiteral("Problems") : QStringLiteral("Problems (%1)").arg(count));
}

void BottomPanel::appendOutput(const QString& line)
{
    m_output->appendPlainText(QStringLiteral("%1  %2").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), line));
}

void BottomPanel::showProblems()
{
    setCurrentWidget(m_problems);
}

} // namespace loom
