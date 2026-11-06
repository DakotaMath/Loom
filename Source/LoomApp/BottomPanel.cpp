#include "LoomApp/BottomPanel.h"

#include "LoomApp/LineIcons.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextCursor>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace loom {

BottomPanel::BottomPanel(QWidget* parent)
    : QTabWidget(parent)
    , m_problems(new QTreeWidget(this))
    , m_outputTabs(new QTabWidget(this))
{
    setObjectName(QStringLiteral("bottomPanel"));
    setDocumentMode(true);
    m_problems->setObjectName(QStringLiteral("problems"));
    m_problems->setColumnCount(4);
    m_problems->setHeaderLabels({QStringLiteral("Severity"), QStringLiteral("File"), QStringLiteral("Line"), QStringLiteral("Problem")});
    m_problems->setRootIsDecorated(false);
    for (const int column : {SeverityColumn, FileColumn, LineColumn})
        m_problems->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    addTab(m_problems, QStringLiteral("Problems"));

    // Output: one channel each, with Copy and Clear.
    m_outputTabs->setObjectName(QStringLiteral("output"));
    m_outputTabs->setDocumentMode(true);
    m_outputTabs->setTabPosition(QTabWidget::West);
    const struct {
        OutputChannel channel;
        const char* name;
        const char* label;
    } channels[] = {{OutputChannel::Build, "outputBuild", "Build"}, {OutputChannel::Git, "outputGit", "Git"}, {OutputChannel::Loom, "outputLoom", "Loom"}};
    for (const auto& channel : channels) {
        auto* page = new QWidget(m_outputTabs);
        auto* text = new QPlainTextEdit(page);
        text->setObjectName(QString::fromLatin1(channel.name));
        text->setReadOnly(true);
        text->setLineWrapMode(QPlainTextEdit::NoWrap);
        text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        auto* copy = new QToolButton(page);
        copy->setObjectName(QStringLiteral("copy"));
        copy->setText(QStringLiteral("Copy"));
        copy->setAutoRaise(true);
        auto* clear = new QToolButton(page);
        clear->setObjectName(QStringLiteral("clear"));
        clear->setText(QStringLiteral("Clear"));
        clear->setAutoRaise(true);
        connect(copy, &QToolButton::clicked, text, [text] { QApplication::clipboard()->setText(text->toPlainText()); });
        connect(clear, &QToolButton::clicked, text, &QPlainTextEdit::clear);
        auto* buttons = new QVBoxLayout;
        buttons->addWidget(copy);
        buttons->addWidget(clear);
        buttons->addStretch(1);
        auto* layout = new QHBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(text, 1);
        layout->addLayout(buttons);
        m_outputTabs->addTab(page, QString::fromLatin1(channel.label));
        m_channels[static_cast<int>(channel.channel)] = text;
    }
    addTab(m_outputTabs, QStringLiteral("Output"));

    const auto activate = [this](QTreeWidgetItem* item) {
        const QString file = item->data(FileColumn, Qt::UserRole).toString();
        if (!file.isEmpty())
            emit problemActivated(file, item->data(LineColumn, Qt::UserRole).toInt());
    };
    connect(m_problems, &QTreeWidget::itemClicked, this, activate);
    connect(m_problems, &QTreeWidget::itemActivated, this, activate);
}

QPlainTextEdit* BottomPanel::output(OutputChannel channel) const
{
    return m_channels[static_cast<int>(channel)];
}

void BottomPanel::setSnapshot(std::shared_ptr<const ProjectSnapshot> snapshot)
{
    m_snapshot = std::move(snapshot);
    rebuild();
}

void BottomPanel::setProblems(const std::vector<Problem>& problems)
{
    m_validation = problems;
    rebuild();
}

void BottomPanel::setBuildProblems(const std::vector<Problem>& problems)
{
    m_build = problems;
    rebuild();
}

void BottomPanel::setSaveProblems(const std::vector<Problem>& problems)
{
    m_save = problems;
    rebuild();
}

QString BottomPanel::fileLabel(const QString& path) const
{
    if (!m_snapshot)
        return path;
    // "Columns: severity, file (as `ID · title` for statements and proofs), line, message."
    static const QRegularExpression statement(QStringLiteral("^Statements/([^/]+)\\.tex$"));
    static const QRegularExpression proof(QStringLiteral("^Proofs/([^/]+)/([^/]+?)(\\.verbose)?\\.tex$"));
    if (const QRegularExpressionMatch m = statement.match(path); m.hasMatch()) {
        if (const StatementFile* file = m_snapshot->contents.statement(m.captured(1)))
            return QStringLiteral("%1 · %2").arg(file->tag, file->reading.document.title);
    } else if (const QRegularExpressionMatch p = proof.match(path); p.hasMatch()) {
        if (const StatementFile* file = m_snapshot->contents.statement(p.captured(1))) {
            const QString id = p.captured(3).isEmpty() ? p.captured(2) : p.captured(2) + QStringLiteral(" (verbose)");
            return QStringLiteral("%1 · %2").arg(id, file->reading.document.title);
        }
    }
    return path;
}

void BottomPanel::rebuild()
{
    // The selected problem stays selected (F8 goes on from it) if it's still listed.
    QStringList selected;
    if (const QTreeWidgetItem* current = m_problems->currentItem()) {
        for (const int column : {SeverityColumn, FileColumn, LineColumn, MessageColumn})
            selected << current->text(column);
    }
    m_problems->clear();
    const auto add = [this](const Problem& problem, const QString& prefix) {
        const bool error = problem.severity == Severity::Error;
        auto* item = new QTreeWidgetItem(m_problems, {error ? QStringLiteral("error") : QStringLiteral("warning"), fileLabel(problem.file),
                                                      problem.line > 0 ? QString::number(problem.line) : QString(),
                                                      prefix + problem.message});
        item->setIcon(SeverityColumn, lineIcon(error ? LineIcon::Cross : LineIcon::Warning));
        item->setData(SeverityColumn, Qt::UserRole, problem.code);
        item->setData(FileColumn, Qt::UserRole, problem.file);
        item->setData(LineColumn, Qt::UserRole, problem.line);
        item->setToolTip(FileColumn, problem.file);
        item->setToolTip(MessageColumn, problem.message);
    };
    for (const Problem& problem : m_save)
        add(problem, QStringLiteral("Not saved: "));
    for (const Problem& problem : m_commit)
        add(problem, QStringLiteral("Not committed: "));
    for (const Problem& problem : m_build)
        add(problem, QString());
    for (const Problem& problem : m_validation)
        add(problem, QString());
    for (int i = 0; !selected.isEmpty() && i < m_problems->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_problems->topLevelItem(i);
        if (item->text(SeverityColumn) == selected[0] && item->text(FileColumn) == selected[1] && item->text(LineColumn) == selected[2]
            && item->text(MessageColumn) == selected[3]) {
            m_problems->setCurrentItem(item);
            break;
        }
    }
    const std::size_t count = m_save.size() + m_commit.size() + m_build.size() + m_validation.size();
    setTabText(indexOf(m_problems), count == 0 ? QStringLiteral("Problems") : QStringLiteral("Problems (%1)").arg(count));
}

bool BottomPanel::stepProblem(bool forward)
{
    const int count = m_problems->topLevelItemCount();
    if (count == 0)
        return false;
    const int current = m_problems->currentItem() ? m_problems->indexOfTopLevelItem(m_problems->currentItem()) : -1;
    const int next = current < 0 ? (forward ? 0 : count - 1) : (current + (forward ? 1 : count - 1)) % count;
    QTreeWidgetItem* item = m_problems->topLevelItem(next);
    m_problems->setCurrentItem(item);
    m_problems->scrollToItem(item);
    const QString file = item->data(FileColumn, Qt::UserRole).toString();
    if (!file.isEmpty())
        emit problemActivated(file, item->data(LineColumn, Qt::UserRole).toInt());
    return true;
}

void BottomPanel::setCommitProblems(const std::vector<Problem>& problems)
{
    m_commit = problems;
    rebuild();
}

void BottomPanel::appendOutput(OutputChannel channel, const QString& text, const QDateTime& at)
{
    QPlainTextEdit* edit = output(channel);
    const QDateTime when = at.isValid() ? at : QDateTime::currentDateTime();
    edit->appendPlainText(QStringLiteral("%1  %2").arg(when.toString(QStringLiteral("HH:mm:ss")), text));
    // Keep the last channelLimit characters, dropping whole lines from the start.
    QTextDocument* document = edit->document();
    const int excess = document->characterCount() - channelLimit;
    if (excess > 0) {
        QTextCursor cursor(document);
        cursor.movePosition(QTextCursor::Start);
        cursor.setPosition(excess, QTextCursor::KeepAnchor);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    }
}

void BottomPanel::showProblems()
{
    setCurrentWidget(m_problems);
}

} // namespace loom
