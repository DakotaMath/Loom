#include "LoomApp/GitPanel.h"

#include "LoomApp/AppSettings.h"
#include "LoomApp/EditBuffer.h"
#include "LoomApp/Lane.h"
#include "LoomApp/ProjectSession.h"
#include "LoomApp/StatementsPage.h"

#include "LoomCore/Validation.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QSyntaxHighlighter>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace loom {

namespace {

/// The project-state key of the unfinished commit message (Specs/m2-statements-review.md,
/// "Settings": "the git panel's unfinished commit message").
const QString messageKey = QStringLiteral("git/message");

/// Colours a unified diff: added lines green, removed lines red, hunk headers blue, file headers bold.
class DiffHighlighter : public QSyntaxHighlighter
{
public:
    using QSyntaxHighlighter::QSyntaxHighlighter;

protected:
    void highlightBlock(const QString& text) override
    {
        QTextCharFormat format;
        if (text.startsWith(QStringLiteral("diff --git")) || text.startsWith(QStringLiteral("+++")) || text.startsWith(QStringLiteral("---"))) {
            format.setFontWeight(QFont::Bold);
        } else if (text.startsWith(u'+')) {
            format.setForeground(QColor(0x1a, 0x7f, 0x37));
        } else if (text.startsWith(u'-')) {
            format.setForeground(QColor(0xcf, 0x22, 0x2e));
        } else if (text.startsWith(QStringLiteral("@@"))) {
            format.setForeground(QColor(0x09, 0x69, 0xda));
        } else {
            return;
        }
        setFormat(0, static_cast<int>(text.size()), format);
    }
};

/// "1 error", "2 warnings".
QString counted(qsizetype n, const char* one, const char* many)
{
    return QStringLiteral("%1 %2").arg(n).arg(QString::fromLatin1(n == 1 ? one : many));
}

/// How a changed file is marked, as in Review.
QString changeName(FileChange change)
{
    switch (change) {
    case FileChange::New: return QStringLiteral("new");
    case FileChange::Changed: return QStringLiteral("changed");
    case FileChange::Deleted: return QStringLiteral("deleted");
    }
    return {};
}

/// A dialog with a **Cancel** button focused and the action's button beside it, so Enter alone
/// never acts (as every dialog of Dakota's clicks, Specs/m2-statements-review.md, "Dakota's clicks").
/// @param action The action button's text; @param name its object name.
/// @returns The dialog, deleted on close. Side effects: none until opened.
QDialog* actionDialog(QWidget* parent, const QString& objectName, const QString& title, const QString& action, const QString& name,
                      QPushButton*& button, QVBoxLayout*& layout)
{
    auto* dialog = new QDialog(parent);
    dialog->setObjectName(objectName);
    dialog->setWindowTitle(title);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    layout = new QVBoxLayout(dialog);
    auto* buttons = new QDialogButtonBox(dialog);
    button = buttons->addButton(action, QDialogButtonBox::AcceptRole);
    button->setObjectName(name);
    button->setAutoDefault(false);
    QPushButton* cancel = buttons->addButton(QDialogButtonBox::Cancel);
    cancel->setObjectName(QStringLiteral("cancel"));
    cancel->setDefault(true);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    // Laid out by openActionDialog(), under whatever the caller adds.
    dialog->setProperty("buttons", QVariant::fromValue<QObject*>(buttons));
    return dialog;
}

/// Puts the dialog's buttons at the bottom, opens it, and focuses **Cancel**.
void openActionDialog(QDialog* dialog, QVBoxLayout* layout)
{
    auto* buttons = qobject_cast<QDialogButtonBox*>(dialog->property("buttons").value<QObject*>());
    layout->addWidget(buttons);
    dialog->open();
    if (QPushButton* cancel = buttons->button(QDialogButtonBox::Cancel))
        cancel->setFocus();
}

} // namespace

GitPanel::GitPanel(AppSettings* settings, Lane* gitLane, Lane* projectLane, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_gitLane(gitLane)
    , m_projectLane(projectLane)
{
    setObjectName(QStringLiteral("gitPanel"));
    // A line between the panel and the tab it sits beside.
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QStringLiteral("#gitPanel { border-left: 1px solid palette(mid); }"));
    createWidgets();
    showState();
}

GitPanel::~GitPanel() = default;

void GitPanel::createWidgets()
{
    m_header = new QLabel(this);
    m_header->setObjectName(QStringLiteral("gitHeader"));
    m_header->setWordWrap(true);
    m_header->setTextFormat(Qt::PlainText);
    QFont bold = m_header->font();
    bold.setBold(true);
    m_header->setFont(bold);

    const struct {
        Button button;
        const char* text;
        const char* name;
        void (GitPanel::*run)();
    } buttons[] = {
        {Button::Commit, "Commit", "commit", &GitPanel::commit},
        {Button::RunChecks, "Run checks", "runChecks", &GitPanel::runChecks},
        {Button::NewBranch, "New branch…", "newBranch", &GitPanel::newBranch},
        {Button::SwitchBranch, "Switch branch…", "switchBranch", &GitPanel::switchBranch},
        {Button::Fetch, "Fetch", "fetch", &GitPanel::fetch},
        {Button::Pull, "Pull", "pull", &GitPanel::pull},
        {Button::Push, "Push…", "push", &GitPanel::push},
    };
    for (const auto& b : buttons) {
        auto* button = new QPushButton(QString::fromUtf8(b.text), this);
        button->setObjectName(QString::fromLatin1(b.name));
        connect(button, &QPushButton::clicked, this, b.run);
        m_buttons[static_cast<int>(b.button)] = button;
    }

    m_operations = new QWidget(this);
    auto* remoteRow = new QHBoxLayout;
    remoteRow->addWidget(button(Button::Fetch));
    remoteRow->addWidget(button(Button::Pull));
    remoteRow->addWidget(button(Button::Push));
    remoteRow->addStretch(1);
    auto* branchRow = new QHBoxLayout;
    branchRow->addWidget(button(Button::NewBranch));
    branchRow->addWidget(button(Button::SwitchBranch));
    branchRow->addWidget(button(Button::RunChecks));
    branchRow->addStretch(1);
    auto* operations = new QVBoxLayout(m_operations);
    operations->setContentsMargins(0, 0, 0, 0);
    operations->addLayout(remoteRow);
    operations->addLayout(branchRow);

    m_busy = new QProgressBar(this);
    m_busy->setObjectName(QStringLiteral("gitBusy"));
    m_busy->setRange(0, 0);
    m_busy->setTextVisible(false);
    m_busy->setMaximumHeight(8);
    m_busy->hide();

    m_result = new QLabel(this);
    m_result->setObjectName(QStringLiteral("gitResult"));
    m_result->setWordWrap(true);
    m_result->setTextFormat(Qt::PlainText);
    m_result->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_result->hide();
    const QFont fixed = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_gitOutput = new QPlainTextEdit(this);
    m_gitOutput->setObjectName(QStringLiteral("gitOutput"));
    m_gitOutput->setReadOnly(true);
    m_gitOutput->setFont(fixed);
    m_gitOutput->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_gitOutput->setMaximumHeight(140);
    m_gitOutput->hide();

    m_filesLabel = new QLabel(QStringLiteral("Changed files"), this);
    m_files = new QTreeWidget(this);
    m_files->setObjectName(QStringLiteral("changedFiles"));
    m_files->setColumnCount(2);
    m_files->setHeaderHidden(true);
    m_files->setRootIsDecorated(false);
    m_files->setUniformRowHeights(true);
    m_files->header()->setStretchLastSection(false);
    m_files->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_files->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    connect(m_files, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        if (column != 0)
            return;
        const QString path = item->data(0, Qt::UserRole).toString();
        if (item->checkState(0) == Qt::Checked)
            m_unticked.erase(path);
        else
            m_unticked.insert(path);
    });
    connect(m_files, &QTreeWidget::currentItemChanged, this, &GitPanel::showDiff);

    m_diff = new QPlainTextEdit(this);
    m_diff->setObjectName(QStringLiteral("gitDiff"));
    m_diff->setReadOnly(true);
    m_diff->setFont(fixed);
    m_diff->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_diff->setPlaceholderText(QStringLiteral("Click a file to see its changes."));
    new DiffHighlighter(m_diff->document());

    auto* splitter = new QSplitter(Qt::Vertical, this);
    auto* filesBox = new QWidget(splitter);
    auto* filesLayout = new QVBoxLayout(filesBox);
    filesLayout->setContentsMargins(0, 0, 0, 0);
    filesLayout->addWidget(m_filesLabel);
    filesLayout->addWidget(m_files, 1);
    splitter->addWidget(filesBox);
    splitter->addWidget(m_diff);
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    m_message = new QPlainTextEdit(this);
    m_message->setObjectName(QStringLiteral("commitMessage"));
    m_message->setPlaceholderText(QStringLiteral("Commit message"));
    m_message->setTabChangesFocus(true);
    m_message->setMaximumHeight(90);
    connect(m_message, &QPlainTextEdit::textChanged, this, &GitPanel::messageChanged);
    m_messageWarning = new QLabel(this);
    m_messageWarning->setObjectName(QStringLiteral("messageWarning"));
    m_messageWarning->setWordWrap(true);
    m_messageWarning->setForegroundRole(QPalette::PlaceholderText);
    m_messageWarning->hide();
    auto* commitRow = new QHBoxLayout;
    commitRow->addStretch(1);
    commitRow->addWidget(button(Button::Commit));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->addWidget(m_header);
    layout->addWidget(m_operations);
    layout->addWidget(m_busy);
    layout->addWidget(m_result);
    layout->addWidget(m_gitOutput);
    layout->addWidget(splitter, 1);
    layout->addWidget(m_message);
    layout->addWidget(m_messageWarning);
    layout->addLayout(commitRow);
    setMinimumWidth(320);
}

QPushButton* GitPanel::button(Button which) const
{
    return m_buttons[static_cast<int>(which)];
}

void GitPanel::setSession(ProjectSession* session, StatementsPage* page)
{
    if (m_session)
        disconnect(m_session, nullptr, this, nullptr);
    ++m_generation;
    m_session = session;
    m_page = page;
    m_state = GitState();
    m_unticked.clear();
    m_reading = false;
    m_readAgain = false;
    m_result->hide();
    m_gitOutput->hide();
    m_diff->clear();
    m_projectState.reset();
    {
        const QSignalBlocker blocker(m_message);
        m_message->clear();
    }
    if (session) {
        m_projectState = std::make_unique<QSettings>(projectStateFile(m_settings->paths(), session->root()), QSettings::IniFormat);
        const QSignalBlocker blocker(m_message);
        m_message->setPlainText(m_projectState->value(messageKey).toString());
        connect(session, &ProjectSession::snapshotChanged, this, &GitPanel::refresh);
    }
    messageChanged();
    showState();
    refresh();
}

QString GitPanel::root() const
{
    return m_session ? m_session->root() : QString();
}

GitTool GitPanel::git() const
{
    return m_settings->gitTool();
}

void GitPanel::setRunning(int delta)
{
    m_running += delta;
    m_busy->setVisible(m_running > 0);
    updateButtons();
}

void GitPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refresh();
}

void GitPanel::refresh()
{
    if (!m_session || !isVisible())
        return;
    if (m_reading) {
        m_readAgain = true;
        return;
    }
    m_reading = true;
    const int generation = m_generation;
    m_gitLane->run(this, [git = git(), root = root()] { return readGitState(git, root); },
        [this, generation](GitState state) {
            if (generation != m_generation)
                return;
            m_reading = false;
            emit gitSteps(state.steps);
            m_state = std::move(state);
            showState();
            emit stateShown();
            if (m_readAgain) {
                m_readAgain = false;
                refresh();
            }
        });
}

void GitPanel::showState()
{
    const GitState& state = m_state;
    QString header;
    if (!m_session) {
        header = QStringLiteral("No project is open.");
    } else if (!state.ok) {
        if (state.code.isEmpty())
            header = QStringLiteral("Reading git…");
        else if (state.code == u"git-missing")
            header = QStringLiteral("Loom can't find git; Check my setup says what to install.");
        else
            header = state.message;
    } else {
        if (state.detached())
            header = QStringLiteral("Not on a branch (%1)").arg(state.head);
        else if (!state.hasCommits)
            header = QStringLiteral("%1 · no commits yet").arg(state.branch);
        else
            header = state.branch;
        if (!state.upstream.isEmpty()) {
            if (state.ahead == 0 && state.behind == 0)
                header += QStringLiteral("\nUp to date with %1 as of the last fetch").arg(state.upstream);
            else
                header += QStringLiteral("\n%1 ahead, %2 behind %3 as of the last fetch").arg(state.ahead).arg(state.behind).arg(state.upstream);
        }
        if (!state.inProgress.isEmpty())
            header += QStringLiteral("\nA %1 is in progress. Finish it in a terminal.").arg(state.inProgress);
    }
    m_header->setText(header);

    const QString current = m_files->currentItem() ? m_files->currentItem()->data(0, Qt::UserRole).toString() : QString();
    {
        const QSignalBlocker blocker(m_files);
        m_files->clear();
        for (const ChangedFile& file : state.files) {
            auto* item = new QTreeWidgetItem(m_files, {file.path, changeName(file.change)});
            item->setData(0, Qt::UserRole, file.path);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, m_unticked.count(file.path) ? Qt::Unchecked : Qt::Checked);
            item->setToolTip(0, file.path);
            item->setForeground(1, palette().brush(QPalette::PlaceholderText));
            if (file.path == current)
                m_files->setCurrentItem(item);
        }
    }
    // Unticks of files that are no longer changed are forgotten.
    for (auto it = m_unticked.begin(); it != m_unticked.end();) {
        const bool listed = std::any_of(state.files.cbegin(), state.files.cend(), [&](const ChangedFile& f) { return f.path == *it; });
        it = listed ? std::next(it) : m_unticked.erase(it);
    }
    m_filesLabel->setText(state.files.empty() ? QStringLiteral("No changes since the last commit")
                                              : QStringLiteral("Changed files (%1)").arg(state.files.size()));
    if (!m_files->currentItem())
        m_diff->clear();
    else
        showDiff();
    updateButtons();
}

void GitPanel::updateButtons()
{
    const GitState& state = m_state;
    const bool ok = m_session && state.ok;
    const bool free = state.inProgress.isEmpty();
    const bool remote = !state.remotes.isEmpty();
    // While a merge, rebase, cherry-pick or revert is in progress, only Run checks and Fetch.
    button(Button::Commit)->setVisible(ok && free);
    button(Button::NewBranch)->setVisible(ok && free);
    button(Button::SwitchBranch)->setVisible(ok && free);
    button(Button::Fetch)->setVisible(ok && remote);
    button(Button::Pull)->setVisible(ok && remote && free);
    // "A repository with no commits: … Push is hidden."
    button(Button::Push)->setVisible(ok && remote && free && state.hasCommits);
    button(Button::RunChecks)->setVisible(m_session != nullptr);
    for (QPushButton* each : m_buttons)
        each->setEnabled(m_running == 0);
    const bool committing = ok && free;
    m_filesLabel->setVisible(ok);
    m_files->setVisible(ok);
    m_diff->setVisible(ok);
    m_message->setVisible(committing);
    m_messageWarning->setVisible(committing && !m_messageWarning->text().isEmpty());
}

void GitPanel::showMessage(const QString& message)
{
    m_result->setText(message);
    m_result->setVisible(!message.isEmpty());
    m_gitOutput->clear();
    m_gitOutput->hide();
}

void GitPanel::showResult(const GitResult& result)
{
    showMessage(result.message);
    // Git's output is shown for a refused or failed command (a refused commit: the hook's lines).
    if (!result.done() && !result.output.isEmpty()) {
        m_gitOutput->setPlainText(result.output);
        m_gitOutput->show();
    }
}

void GitPanel::runOperation(std::function<GitResult()> work, std::function<void(const GitResult&)> after)
{
    setRunning(1);
    const int generation = m_generation;
    m_gitLane->run(this, std::move(work), [this, generation, after](GitResult result) {
        setRunning(-1);
        if (generation != m_generation)
            return;
        emit gitSteps(result.steps);
        showResult(result);
        if (after)
            after(result);
        emit operationFinished();
        emit repositoryChanged();
        refresh();
    });
}

std::vector<ChangedFile> GitPanel::tickedFiles() const
{
    std::vector<ChangedFile> ticked;
    for (const ChangedFile& file : m_state.files) {
        if (!m_unticked.count(file.path))
            ticked.push_back(file);
    }
    return ticked;
}

void GitPanel::commit()
{
    if (!m_session)
        return;
    QStringList unsaved;
    const QStringList edited = m_page ? m_page->unsavedPaths() : QStringList();
    for (const ChangedFile& file : tickedFiles()) {
        if (edited.contains(file.path))
            unsaved << file.path;
    }
    if (unsaved.isEmpty()) {
        commitNow();
        return;
    }
    // "A ticked file with unsaved edits asks first: Save and commit, Commit the saved version, Cancel."
    auto* box = new QMessageBox(QMessageBox::Question, QStringLiteral("Unsaved edits"),
                                QStringLiteral("%1 to commit %2 unsaved edits: %3.")
                                    .arg(unsaved.size() == 1 ? QStringLiteral("A file") : QStringLiteral("%1 files").arg(unsaved.size()),
                                         unsaved.size() == 1 ? QStringLiteral("has") : QStringLiteral("have"), unsaved.join(QStringLiteral(", "))),
                                QMessageBox::NoButton, this);
    box->setObjectName(QStringLiteral("unsavedCommitMessage"));
    box->setAttribute(Qt::WA_DeleteOnClose);
    QPushButton* save = box->addButton(QStringLiteral("Save and commit"), QMessageBox::AcceptRole);
    save->setObjectName(QStringLiteral("saveAndCommit"));
    QPushButton* saved = box->addButton(QStringLiteral("Commit the saved version"), QMessageBox::AcceptRole);
    saved->setObjectName(QStringLiteral("commitSaved"));
    QPushButton* cancel = box->addButton(QMessageBox::Cancel);
    cancel->setObjectName(QStringLiteral("cancel"));
    box->setDefaultButton(cancel);
    box->setEscapeButton(cancel);
    connect(box, &QMessageBox::finished, this, [this, box, save, saved, unsaved] {
        if (box->clickedButton() == saved) {
            commitNow();
        } else if (box->clickedButton() == save) {
            saveFiles(unsaved, [this](bool ok) {
                if (ok)
                    commitNow();
                else
                    showMessage(QStringLiteral("Not committed: a file didn't save. Its problems are listed in Problems."));
            });
        }
    });
    box->open();
    cancel->setFocus();
}

void GitPanel::saveFiles(QStringList paths, std::function<void(bool)> done)
{
    while (!paths.isEmpty() && !(m_page && m_page->buffer(paths.first())))
        paths.removeFirst();
    if (paths.isEmpty()) {
        done(true);
        return;
    }
    const QString path = paths.takeFirst();
    QPointer<GitPanel> self(this);
    m_page->saveBuffer(m_page->buffer(path), {}, [self, paths, done](bool ok) {
        if (!self)
            return;
        if (!ok)
            done(false);
        else
            self->saveFiles(paths, done);
    });
}

void GitPanel::commitNow()
{
    const std::vector<ChangedFile> ticked = tickedFiles();
    const QString message = m_message->toPlainText();
    runOperation([git = git(), root = root(), ticked, message] { return commitFiles(git, root, ticked, message); },
        [this](const GitResult& result) {
            if (result.done()) {
                // The message is kept until a commit goes in.
                m_message->clear();
                emit commitProblems({});
            } else if (result.code == u"hook-refused") {
                emit commitProblems(result.problems);
            }
        });
}

void GitPanel::runChecks()
{
    if (!m_session)
        return;
    setRunning(1);
    const int generation = m_generation;
    m_projectLane->run(this,
        [git = git(), root = root()] {
            ValidationRequest request;
            request.root = root;
            request.git = git;
            return validateProject(request);
        },
        [this, generation](ValidationResult result) {
            setRunning(-1);
            if (generation != m_generation)
                return;
            if (result.outcome != ValidationOutcome::Checked) {
                showMessage(result.message);
            } else {
                emit checksFinished(result.problems);
                const int errors = result.errors();
                const int warnings = result.warnings();
                if (errors == 0 && warnings == 0)
                    showMessage(QStringLiteral("Checked: no problems."));
                else if (warnings == 0)
                    showMessage(QStringLiteral("Checked: %1, listed in Problems.").arg(counted(errors, "error", "errors")));
                else if (errors == 0)
                    showMessage(QStringLiteral("Checked: %1, listed in Problems.").arg(counted(warnings, "warning", "warnings")));
                else
                    showMessage(QStringLiteral("Checked: %1 and %2, listed in Problems.")
                                    .arg(counted(errors, "error", "errors"), counted(warnings, "warning", "warnings")));
            }
            emit operationFinished();
        });
}

void GitPanel::newBranch()
{
    if (!m_session)
        return;
    QPushButton* make = nullptr;
    QVBoxLayout* layout = nullptr;
    QDialog* dialog = actionDialog(this, QStringLiteral("newBranchDialog"), QStringLiteral("New branch"), QStringLiteral("Make branch"),
                                   QStringLiteral("make"), make, layout);
    layout->addWidget(new QLabel(QStringLiteral("A new branch from %1. Uncommitted changes come along.")
                                     .arg(m_state.branch.isEmpty() ? m_state.head : m_state.branch), dialog));
    auto* name = new QLineEdit(dialog);
    name->setObjectName(QStringLiteral("branchName"));
    name->setPlaceholderText(QStringLiteral("The branch's name"));
    layout->addWidget(name);
    make->setEnabled(false);
    connect(name, &QLineEdit::textChanged, make, [make](const QString& text) { make->setEnabled(!text.trimmed().isEmpty()); });
    connect(dialog, &QDialog::accepted, this, [this, name] {
        runOperation([git = git(), root = root(), branch = name->text().trimmed()] { return createBranch(git, root, branch); });
    });
    openActionDialog(dialog, layout);
    name->setFocus();
}

QString GitPanel::unsavedRefusal() const
{
    return m_page && m_page->hasUnsavedEdits() ? QStringLiteral("Save or discard your unsaved edits first.") : QString();
}

void GitPanel::switchBranch()
{
    if (!m_session)
        return;
    if (const QString refusal = unsavedRefusal(); !refusal.isEmpty()) {
        showMessage(refusal);
        return;
    }
    QStringList others = m_state.branches;
    others.removeAll(m_state.branch);
    if (others.isEmpty()) {
        showMessage(QStringLiteral("There is no other branch to switch to."));
        return;
    }
    QPushButton* go = nullptr;
    QVBoxLayout* layout = nullptr;
    QDialog* dialog = actionDialog(this, QStringLiteral("switchBranchDialog"), QStringLiteral("Switch branch"), QStringLiteral("Switch"),
                                   QStringLiteral("switch"), go, layout);
    layout->addWidget(new QLabel(QStringLiteral("Switch to:"), dialog));
    auto* list = new QListWidget(dialog);
    list->setObjectName(QStringLiteral("branches"));
    list->addItems(others);
    layout->addWidget(list);
    go->setEnabled(false);
    connect(list, &QListWidget::currentRowChanged, go, [go](int row) { go->setEnabled(row >= 0); });
    connect(dialog, &QDialog::accepted, this, [this, list] {
        if (!list->currentItem())
            return;
        runOperation([git = git(), root = root(), branch = list->currentItem()->text()] { return loom::switchBranch(git, root, branch); });
    });
    openActionDialog(dialog, layout);
}

void GitPanel::fetch()
{
    if (!m_session)
        return;
    runOperation([git = git(), root = root()] { return fetchRemotes(git, root); });
}

void GitPanel::pull()
{
    if (!m_session)
        return;
    if (const QString refusal = unsavedRefusal(); !refusal.isEmpty()) {
        showMessage(refusal);
        return;
    }
    runOperation([git = git(), root = root()] { return pullFastForward(git, root); });
}

void GitPanel::push()
{
    if (!m_session)
        return;
    setRunning(1);
    const int generation = m_generation;
    m_gitLane->run(this, [git = git(), root = root()] { return planPush(git, root); }, [this, generation](PushPlan plan) {
        setRunning(-1);
        if (generation != m_generation)
            return;
        emit gitSteps(plan.steps);
        if (!plan.ok) {
            showMessage(plan.message);
            emit operationFinished();
            return;
        }
        QPushButton* go = nullptr;
        QVBoxLayout* layout = nullptr;
        const QString action = plan.commits.size() == 1 ? QStringLiteral("Push 1 commit") : QStringLiteral("Push %1 commits").arg(plan.commits.size());
        QDialog* dialog = actionDialog(this, QStringLiteral("pushDialog"), QStringLiteral("Push"), action, QStringLiteral("push"), go, layout);
        const QString to = plan.upstream.isEmpty() ? QStringLiteral("a remote, as %1 (it has no upstream yet)").arg(plan.branch) : plan.upstream;
        auto* intro = new QLabel(QStringLiteral("%1 will go up to %2:").arg(plan.commits.size() == 1 ? QStringLiteral("This commit")
                                                                                                    : QStringLiteral("These %1 commits").arg(plan.commits.size()), to),
                                 dialog);
        intro->setObjectName(QStringLiteral("pushIntro"));
        intro->setWordWrap(true);
        layout->addWidget(intro);
        auto* commits = new QListWidget(dialog);
        commits->setObjectName(QStringLiteral("commits"));
        commits->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        commits->addItems(plan.commits);
        layout->addWidget(commits);
        QComboBox* remotes = nullptr;
        if (plan.upstream.isEmpty() && plan.remotes.size() > 1) {
            auto* row = new QHBoxLayout;
            row->addWidget(new QLabel(QStringLiteral("Remote:"), dialog));
            remotes = new QComboBox(dialog);
            remotes->setObjectName(QStringLiteral("remote"));
            remotes->addItems(plan.remotes);
            if (const int origin = static_cast<int>(plan.remotes.indexOf(QStringLiteral("origin"))); origin >= 0)
                remotes->setCurrentIndex(origin);
            row->addWidget(remotes, 1);
            layout->addLayout(row);
        }
        connect(dialog, &QDialog::accepted, this, [this, remotes] {
            const QString remote = remotes ? remotes->currentText() : QString();
            runOperation([git = git(), root = root(), remote] { return pushCommits(git, root, remote); });
        });
        openActionDialog(dialog, layout);
        emit operationFinished();
    });
}

void GitPanel::showDiff()
{
    QTreeWidgetItem* item = m_files->currentItem();
    if (!item || !m_session) {
        m_diff->clear();
        return;
    }
    const QString path = item->data(0, Qt::UserRole).toString();
    const auto file = std::find_if(m_state.files.cbegin(), m_state.files.cend(), [&](const ChangedFile& f) { return f.path == path; });
    if (file == m_state.files.cend())
        return;
    const int generation = m_generation;
    m_gitLane->run(this,
        [git = git(), root = root(), changed = *file] {
            std::vector<GitStep> steps;
            QString text = fileDiff(git, root, changed, steps);
            return std::make_pair(std::move(text), std::move(steps));
        },
        [this, generation, path](std::pair<QString, std::vector<GitStep>> shown) {
            if (generation != m_generation)
                return;
            emit gitSteps(shown.second);
            QTreeWidgetItem* current = m_files->currentItem();
            if (current && current->data(0, Qt::UserRole).toString() == path)
                m_diff->setPlainText(shown.first);
        });
}

bool GitPanel::setTicked(const QString& path, bool ticked)
{
    for (int i = 0; i < m_files->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_files->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == path) {
            item->setCheckState(0, ticked ? Qt::Checked : Qt::Unchecked);
            return true;
        }
    }
    return false;
}

void GitPanel::messageChanged()
{
    const QString text = m_message->toPlainText();
    const qsizetype first = text.section(u'\n', 0, 0).size();
    m_messageWarning->setText(first > firstLineLimit
        ? QStringLiteral("The first line is %1 characters; git's one-line views show 72 or fewer.").arg(first)
        : QString());
    m_messageWarning->setVisible(m_message->isVisible() && first > firstLineLimit);
    if (m_projectState) {
        if (text.isEmpty())
            m_projectState->remove(messageKey);
        else
            m_projectState->setValue(messageKey, text);
    }
}

} // namespace loom
