#include "LoomCore/GitOperations.h"

#include "LoomCore/ProjectFiles.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QRegularExpression>

#include <algorithm>
#include <memory>
#include <optional>

namespace loom {

namespace {

/// How much of a step's output Output shows (the spec: "its output (cut at 1 MB)").
constexpr qsizetype outputLimit = 1024 * 1024;

/// How much of a new file a click shows before it stops.
constexpr qsizetype fileShowLimit = 1024 * 1024;

/// Runs git for one operation in one folder, keeping every command it ran. Git never waits for
/// a terminal (`GIT_TERMINAL_PROMPT=0`, the spec), so a remote that wants a password fails
/// rather than hangs; Git Credential Manager may still open its own window.
class Runner
{
public:
    Runner(const GitTool& git, const QString& root, std::vector<GitStep>& steps)
        : m_git(git), m_root(root), m_steps(steps)
    {
        m_git.environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    }

    /// Runs git with `arguments` in the folder. @returns What it did (a copy: the log may grow).
    /// Side effects: runs git; logs the step.
    GitRun run(const QStringList& arguments)
    {
        GitStep step;
        step.started = QDateTime::currentDateTime();
        step.folder = m_root;
        step.arguments = arguments;
        QElapsedTimer timer;
        timer.start();
        step.run = runGit(m_git, arguments, m_root);
        step.milliseconds = timer.elapsed();
        m_steps.push_back(std::move(step));
        return m_steps.back().run;
    }

    /// Runs git and gives its stdout, trimmed, or nothing if it didn't succeed.
    std::optional<QString> output(const QStringList& arguments)
    {
        const GitRun done = run(arguments);
        if (!done.ok())
            return std::nullopt;
        return done.output.trimmed();
    }

    /// The last step run.
    const GitStep& last() const { return m_steps.back(); }

private:
    GitTool m_git;
    QString m_root;
    std::vector<GitStep>& m_steps;
};

/// git's stdout and stderr together, for the panel.
QString combined(const GitRun& run)
{
    QString text = run.output.trimmed();
    const QString errors = run.errors.trimmed();
    if (!errors.isEmpty())
        text += (text.isEmpty() ? QString() : QStringLiteral("\n")) + errors;
    return text;
}

/// Fills a result for a git step that didn't run, didn't finish or failed: `git-missing` or
/// `git-timeout` with M1's messages (Specs/m1-core.md, A16), else `git-failed` with git's first
/// line of stderr.
void failed(GitResult& result, const GitStep& step)
{
    result.outcome = GitOutcome::CantWork;
    result.output = combined(step.run);
    if (!step.run.started) {
        result.code = gitDidntRunCode(step.run, QStringLiteral("git-missing"));
        result.message = gitDidntRunMessage(step.run, step.arguments);
        return;
    }
    result.code = QStringLiteral("git-failed");
    QString why = step.run.errors.trimmed().section(u'\n', 0, 0);
    if (why.isEmpty())
        why = QStringLiteral("it exited with code %1").arg(step.run.exitCode);
    result.message = QStringLiteral("`%1` failed: %2").arg(gitCommand(step.arguments), why);
}

/// A refusal.
GitResult refusal(const char* code, const QString& message, std::vector<GitStep> steps = {})
{
    GitResult result;
    result.outcome = GitOutcome::Refused;
    result.code = QString::fromLatin1(code);
    result.message = message;
    result.steps = std::move(steps);
    return result;
}

/// The operation git is in the middle of, from the files it leaves in its folder, or empty.
/// @param gitDir `git rev-parse --absolute-git-dir`.
QString operationInProgress(const QString& gitDir)
{
    const QDir dir(gitDir);
    if (dir.exists(QStringLiteral("rebase-merge")) || dir.exists(QStringLiteral("rebase-apply")))
        return QStringLiteral("rebase");
    if (dir.exists(QStringLiteral("MERGE_HEAD")))
        return QStringLiteral("merge");
    if (dir.exists(QStringLiteral("CHERRY_PICK_HEAD")))
        return QStringLiteral("cherry-pick");
    if (dir.exists(QStringLiteral("REVERT_HEAD")))
        return QStringLiteral("revert");
    return {};
}

/// The spec's words for an operation in progress: *"A {merge} is in progress. Finish it in a terminal."*
QString inProgressMessage(const QString& operation)
{
    return QStringLiteral("A %1 is in progress. Finish it in a terminal.").arg(operation);
}

/// What the operations need to know before they change anything.
struct Where {
    QString branch;     ///< Empty when `HEAD` is detached.
    bool hasCommits = false;
    QString inProgress;
};

/// Reads the branch, whether there's a commit, and any operation in progress.
/// @returns Nothing, with `result` filled, if git failed.
std::optional<Where> where(Runner& git, GitResult& result)
{
    Where here;
    const QStringList gitDir = {QStringLiteral("rev-parse"), QStringLiteral("--absolute-git-dir")};
    const std::optional<QString> dir = git.output(gitDir);
    if (!dir) {
        failed(result, git.last());
        return std::nullopt;
    }
    here.inProgress = operationInProgress(*dir);
    // `symbolic-ref -q` exits 1, quietly, for a detached HEAD.
    const GitRun& branch = git.run({QStringLiteral("symbolic-ref"), QStringLiteral("--short"), QStringLiteral("-q"), QStringLiteral("HEAD")});
    if (!branch.started) {
        failed(result, git.last());
        return std::nullopt;
    }
    if (branch.ok())
        here.branch = branch.output.trimmed();
    const GitRun& head = git.run({QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("-q"), QStringLiteral("HEAD")});
    if (!head.started) {
        failed(result, git.last());
        return std::nullopt;
    }
    here.hasCommits = head.ok();
    return here;
}

/// The remotes, one per line of `git remote`. @returns Nothing, with `result` filled, if git failed.
std::optional<QStringList> remotes(Runner& git, GitResult& result)
{
    const std::optional<QString> listed = git.output({QStringLiteral("remote")});
    if (!listed) {
        failed(result, git.last());
        return std::nullopt;
    }
    return listed->split(u'\n', Qt::SkipEmptyParts);
}

/// The branch's upstream (`origin/main`), or empty if it has none.
QString upstreamOf(Runner& git)
{
    return git.output({QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"), QStringLiteral("--symbolic-full-name"),
                       QStringLiteral("@{upstream}")})
        .value_or(QString());
}

/// The project's `.loom/write.lock`, taken (Specs/m2-statements-review.md, "Locks": git operations
/// that change the working tree take it too).
/// @returns The held lock, or null with `result` set to the refusal.
std::unique_ptr<QLockFile> takeWriteLock(const QString& root, int waitMs, GitResult& result)
{
    const QString relative = QStringLiteral(".loom/write.lock");
    const std::optional<QString> path = resolveProjectPath(root, relative);
    if (!path || !QDir().mkpath(QFileInfo(*path).absolutePath())) {
        result.outcome = GitOutcome::CantWork;
        result.code = QStringLiteral("write-failed");
        result.message = QStringLiteral("Loom couldn't create `%1`.").arg(QDir::toNativeSeparators(root + u'/' + relative));
        return nullptr;
    }
    auto lock = std::make_unique<QLockFile>(*path);
    if (!lock->tryLock(waitMs)) {
        // M1's message for a lock that isn't free in time.
        result.outcome = GitOutcome::Refused;
        result.code = QStringLiteral("locked");
        result.message = QStringLiteral("Another loom command is changing this project. Try again when it has finished.");
        return nullptr;
    }
    return lock;
}

/// "1 file", "3 commits".
QString counted(qsizetype n, const char* one, const char* many)
{
    return QStringLiteral("%1 %2").arg(n).arg(QString::fromLatin1(n == 1 ? one : many));
}

} // namespace

QString gitStepText(const GitStep& step)
{
    QString text = QStringLiteral("In %1: %2\n").arg(QDir::toNativeSeparators(step.folder), gitCommand(step.arguments));
    const QString seconds = QString::number(static_cast<double>(step.milliseconds) / 1000.0, 'f', 2);
    if (step.run.timedOut)
        text += QStringLiteral("Didn't finish in time; stopped after %1 s.").arg(seconds);
    else if (!step.run.started && step.run.exitCode < 0)
        text += QStringLiteral("Didn't run.");
    else
        text += QStringLiteral("Exit code %1 after %2 s.").arg(step.run.exitCode).arg(seconds);
    QString output = step.run.output;
    if (!step.run.errors.isEmpty())
        output += (output.isEmpty() || output.endsWith(u'\n') ? QString() : QStringLiteral("\n")) + step.run.errors;
    if (output.size() > outputLimit)
        output = output.first(outputLimit) + QStringLiteral("\n… (cut at 1 MB)");
    output = output.trimmed();
    if (!output.isEmpty())
        text += u'\n' + output;
    return text;
}

bool GitState::trackedChanges() const
{
    return std::any_of(files.cbegin(), files.cend(), [](const ChangedFile& file) { return !file.untracked; });
}

std::vector<ChangedFile> parseGitStatus(const QByteArray& output)
{
    std::vector<ChangedFile> files;
    const QList<QByteArray> records = output.split('\0');
    for (qsizetype i = 0; i < records.size(); ++i) {
        const QByteArray& record = records[i];
        if (record.isEmpty() || record.startsWith('#') || record.startsWith('!'))
            continue;
        const char type = record[0];
        if (type == '?') {
            files.push_back({QString::fromUtf8(record.mid(2)), FileChange::New, true});
            continue;
        }
        // `1 XY sub mH mI mW hH hI <path>`; `2 XY sub mH mI mW hH hI Xscore <path>`, then the
        // old path as the next record; `u XY sub m1 m2 m3 mW h1 h2 h3 <path>`.
        const int fields = type == '1' ? 9 : type == '2' ? 10 : type == 'u' ? 11 : 0;
        if (fields == 0)
            continue;
        const QList<QByteArray> parts = record.split(' ');
        if (parts.size() < fields || parts[1].size() != 2)
            continue;
        // The path is everything after the fixed fields, spaces and all.
        qsizetype start = 0;
        for (int f = 0; f < fields - 1; ++f)
            start = record.indexOf(' ', start) + 1;
        const QString path = QString::fromUtf8(record.mid(start));
        const char x = parts[1][0];
        const char y = parts[1][1];
        if (type == '2') {
            files.push_back({path, FileChange::New, false});
            if (i + 1 < records.size()) {
                ++i;
                if (x == 'R')
                    files.push_back({QString::fromUtf8(records[i]), FileChange::Deleted, false});
            }
            continue;
        }
        FileChange change = FileChange::Changed;
        if (type == '1') {
            if (x == 'D' || y == 'D')
                change = FileChange::Deleted;
            else if (x == 'A')
                change = FileChange::New;
        }
        files.push_back({path, change, false});
    }
    std::sort(files.begin(), files.end(), [](const ChangedFile& a, const ChangedFile& b) { return a.path < b.path; });
    return files;
}

GitState readGitState(const GitTool& git, const QString& root)
{
    GitState state;
    Runner runner(git, root, state.steps);
    GitResult failure;
    const auto fail = [&] {
        state.code = failure.code;
        state.message = failure.message;
        return state;
    };

    // In English, whatever the user's locale, so "not a git repository" can be recognised.
    GitTool english = git;
    english.environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    Runner status(english, root, state.steps);
    const QStringList statusArguments = {QStringLiteral("status"), QStringLiteral("--porcelain=v2"), QStringLiteral("-z"),
                                         QStringLiteral("--branch"), QStringLiteral("--untracked-files=all")};
    const GitRun& listed = status.run(statusArguments);
    if (!listed.ok()) {
        failed(failure, status.last());
        if (listed.started && listed.errors.contains(u"not a git repository")) {
            failure.code = QStringLiteral("not-a-repository");
            failure.message = QStringLiteral("This project isn't in a git repository");
        }
        return fail();
    }
    for (const QByteArray& record : listed.outputBytes.split('\0')) {
        if (!record.startsWith("# branch."))
            continue;
        const QString line = QString::fromUtf8(record.mid(9));
        const QString key = line.section(u' ', 0, 0);
        const QString value = line.section(u' ', 1);
        if (key == u"oid")
            state.hasCommits = value != u"(initial)";
        else if (key == u"head")
            state.branch = value == u"(detached)" ? QString() : value;
        else if (key == u"upstream")
            state.upstream = value;
        else if (key == u"ab") {
            state.ahead = value.section(u' ', 0, 0).mid(1).toInt();
            state.behind = value.section(u' ', 1, 1).mid(1).toInt();
        }
    }
    state.files = parseGitStatus(listed.outputBytes);

    if (state.hasCommits) {
        const std::optional<QString> head = runner.output({QStringLiteral("rev-parse"), QStringLiteral("--short"), QStringLiteral("HEAD")});
        if (!head) {
            failed(failure, runner.last());
            return fail();
        }
        state.head = *head;
    }
    const std::optional<QStringList> listedRemotes = remotes(runner, failure);
    if (!listedRemotes)
        return fail();
    state.remotes = *listedRemotes;
    const std::optional<QString> branches
        = runner.output({QStringLiteral("for-each-ref"), QStringLiteral("--format=%(refname:short)"), QStringLiteral("refs/heads/")});
    if (!branches) {
        failed(failure, runner.last());
        return fail();
    }
    state.branches = branches->split(u'\n', Qt::SkipEmptyParts);
    state.branches.sort();
    const std::optional<QString> gitDir = runner.output({QStringLiteral("rev-parse"), QStringLiteral("--absolute-git-dir")});
    if (!gitDir) {
        failed(failure, runner.last());
        return fail();
    }
    state.inProgress = operationInProgress(*gitDir);
    state.ok = true;
    return state;
}

QString fileDiff(const GitTool& git, const QString& root, const ChangedFile& file, std::vector<GitStep>& steps)
{
    Runner runner(git, root, steps);
    if (!file.untracked) {
        const QStringList arguments = {QStringLiteral("diff"), QStringLiteral("HEAD"), QStringLiteral("--"), file.path};
        const GitRun& diff = runner.run(arguments);
        if (diff.ok())
            return diff.output;
        if (!diff.started)
            return gitDidntRunMessage(diff, arguments);
        // With no commit yet there is no HEAD to compare with: a new file is shown as it is.
        if (file.change != FileChange::New)
            return combined(diff);
    }
    const std::optional<QString> path = resolveProjectPath(root, file.path);
    QFile contents(path.value_or(QString()));
    if (!path || !contents.open(QIODevice::ReadOnly))
        return QStringLiteral("Loom can't read %1.").arg(file.path);
    const QByteArray bytes = contents.read(fileShowLimit);
    const qint64 size = contents.size();
    // Git's own rule for a binary file: a NUL in its first 8000 bytes.
    if (bytes.first(std::min<qsizetype>(bytes.size(), 8000)).contains('\0'))
        return QStringLiteral("A new binary file, %1 bytes.").arg(size);
    QString text = QStringLiteral("A new file:\n\n") + QString::fromUtf8(bytes);
    if (size > fileShowLimit)
        text += QStringLiteral("\n… (the first 1 MB of %1 bytes)").arg(size);
    return text;
}

std::vector<Problem> parseProblemLines(const QString& output)
{
    static const QRegularExpression form(QRegularExpression::anchoredPattern(QStringLiteral(R"(([^:\s][^:]*):(\d+): (error|warning): (.+))")));
    std::vector<Problem> problems;
    for (QString line : output.split(u'\n')) {
        line = line.trimmed();
        const QRegularExpressionMatch match = form.match(line);
        if (!match.hasMatch())
            continue;
        Problem problem;
        problem.file = match.captured(1);
        problem.line = match.captured(2).toInt();
        problem.severity = match.captured(3) == u"error" ? Severity::Error : Severity::Warning;
        problem.message = match.captured(4);
        problems.push_back(std::move(problem));
    }
    return problems;
}

GitResult commitFiles(const GitTool& git, const QString& root, const std::vector<ChangedFile>& ticked, const QString& message)
{
    if (ticked.empty())
        return refusal("nothing-ticked", QStringLiteral("Tick the files to commit first."));
    if (message.trimmed().isEmpty())
        return refusal("empty-message", QStringLiteral("Write a commit message first."));

    GitResult result;
    Runner runner(git, root, result.steps);
    const std::optional<Where> here = where(runner, result);
    if (!here)
        return result;
    if (!here->inProgress.isEmpty())
        return refusal("in-progress", inProgressMessage(here->inProgress), std::move(result.steps));
    if (here->branch.isEmpty())
        return refusal("detached", QStringLiteral("Make a branch first: a commit here would be left on no branch."), std::move(result.steps));

    QStringList added;
    QStringList paths;
    for (const ChangedFile& file : ticked) {
        paths << file.path;
        if (file.untracked)
            added << file.path;
    }
    if (!added.isEmpty()) {
        const QStringList add = QStringList{QStringLiteral("add"), QStringLiteral("--")} + added;
        if (!runner.run(add).ok()) {
            failed(result, runner.last());
            return result;
        }
    }
    const QStringList commit = QStringList{QStringLiteral("commit"), QStringLiteral("-m"), message, QStringLiteral("--")} + paths;
    const GitRun& committed = runner.run(commit);
    if (!committed.ok()) {
        const GitStep step = runner.last();
        // A refused commit leaves the index as it was found: the files added for it come out again.
        if (!added.isEmpty()) {
            const QStringList undo = here->hasCommits ? QStringList{QStringLiteral("reset"), QStringLiteral("-q"), QStringLiteral("--")} + added
                                                      : QStringList{QStringLiteral("rm"), QStringLiteral("--cached"), QStringLiteral("-q"), QStringLiteral("--")} + added;
            runner.run(undo);
        }
        failed(result, step);
        result.problems = step.run.started ? parseProblemLines(step.run.output + u'\n' + step.run.errors) : std::vector<Problem>{};
        if (!result.problems.empty()) {
            const auto errors = std::count_if(result.problems.cbegin(), result.problems.cend(),
                                              [](const Problem& p) { return p.severity == Severity::Error; });
            result.outcome = GitOutcome::Refused;
            result.code = QStringLiteral("hook-refused");
            result.message = QStringLiteral("Loom's commit guard refused the commit: the files to commit have %1. They're listed in Problems.")
                                 .arg(counted(errors, "error", "errors"));
        }
        return result;
    }
    const QString hash = runner.output({QStringLiteral("rev-parse"), QStringLiteral("--short"), QStringLiteral("HEAD")}).value_or(QString());
    result.outcome = GitOutcome::Done;
    result.message = QStringLiteral("Committed %1 to %2 as %3.").arg(counted(paths.size(), "file", "files"), here->branch, hash);
    return result;
}

GitResult createBranch(const GitTool& git, const QString& root, const QString& name)
{
    GitResult result;
    Runner runner(git, root, result.steps);
    // `--branch` also expands `@{-1}` and the like; a name it changes isn't a plain branch name.
    const GitRun& format = runner.run({QStringLiteral("check-ref-format"), QStringLiteral("--branch"), name});
    if (!format.started) {
        failed(result, runner.last());
        return result;
    }
    if (name.isEmpty() || !format.ok() || format.output.trimmed() != name)
        return refusal("bad-branch-name", QStringLiteral("`%1` can't be a branch name.").arg(name), std::move(result.steps));
    const std::optional<Where> here = where(runner, result);
    if (!here)
        return result;
    if (!here->inProgress.isEmpty())
        return refusal("in-progress", inProgressMessage(here->inProgress), std::move(result.steps));
    const GitRun& exists = runner.run({QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("-q"), QStringLiteral("refs/heads/") + name});
    if (!exists.started) {
        failed(result, runner.last());
        return result;
    }
    if (exists.ok())
        return refusal("branch-exists", QStringLiteral("There is a branch called %1 already.").arg(name), std::move(result.steps));
    if (!runner.run({QStringLiteral("switch"), QStringLiteral("-c"), name}).ok()) {
        failed(result, runner.last());
        return result;
    }
    result.outcome = GitOutcome::Done;
    result.message = QStringLiteral("Made the branch %1 and switched to it.").arg(name);
    return result;
}

GitResult switchBranch(const GitTool& git, const QString& root, const QString& name, int lockWaitMs)
{
    GitResult result;
    Runner runner(git, root, result.steps);
    const std::optional<Where> here = where(runner, result);
    if (!here)
        return result;
    if (!here->inProgress.isEmpty())
        return refusal("in-progress", inProgressMessage(here->inProgress), std::move(result.steps));
    const GitRun& exists = runner.run({QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("-q"), QStringLiteral("refs/heads/") + name});
    if (!exists.started) {
        failed(result, runner.last());
        return result;
    }
    if (name.isEmpty() || !exists.ok())
        return refusal("no-such-branch", QStringLiteral("There is no branch called %1.").arg(name), std::move(result.steps));

    const std::unique_ptr<QLockFile> lock = takeWriteLock(root, lockWaitMs, result);
    if (!lock)
        return result;
    // Checked under the lock, so a save can't slip in between the check and the switch.
    const GitRun& tracked = runner.run({QStringLiteral("status"), QStringLiteral("--porcelain=v2"), QStringLiteral("-z"), QStringLiteral("--untracked-files=no")});
    if (!tracked.ok()) {
        failed(result, runner.last());
        return result;
    }
    if (!parseGitStatus(tracked.outputBytes).empty())
        return refusal("uncommitted-changes", QStringLiteral("Commit your changes first, or they would come along to %1.").arg(name),
                       std::move(result.steps));
    if (!runner.run({QStringLiteral("switch"), name}).ok()) {
        failed(result, runner.last());
        return result;
    }
    result.outcome = GitOutcome::Done;
    result.message = QStringLiteral("Switched to %1.").arg(name);
    return result;
}

GitResult fetchRemotes(const GitTool& git, const QString& root)
{
    GitResult result;
    Runner runner(git, root, result.steps);
    const std::optional<QStringList> listed = remotes(runner, result);
    if (!listed)
        return result;
    if (listed->isEmpty())
        return refusal("no-remote", QStringLiteral("This repository has no remote to fetch from."), std::move(result.steps));
    if (!runner.run({QStringLiteral("fetch"), QStringLiteral("--prune")}).ok()) {
        failed(result, runner.last());
        return result;
    }
    result.outcome = GitOutcome::Done;
    result.message = QStringLiteral("Fetched.");
    return result;
}

GitResult pullFastForward(const GitTool& git, const QString& root, int lockWaitMs)
{
    GitResult result;
    Runner runner(git, root, result.steps);
    const std::optional<QStringList> listed = remotes(runner, result);
    if (!listed)
        return result;
    if (listed->isEmpty())
        return refusal("no-remote", QStringLiteral("This repository has no remote to pull from."), std::move(result.steps));
    const std::optional<Where> here = where(runner, result);
    if (!here)
        return result;
    if (!here->inProgress.isEmpty())
        return refusal("in-progress", inProgressMessage(here->inProgress), std::move(result.steps));
    if (here->branch.isEmpty())
        return refusal("detached", QStringLiteral("Make a branch first: Loom pulls into a branch."), std::move(result.steps));
    const QString upstream = upstreamOf(runner);
    if (upstream.isEmpty())
        return refusal("no-upstream", QStringLiteral("%1 has no upstream branch to pull from. Push it first.").arg(here->branch),
                       std::move(result.steps));

    const std::unique_ptr<QLockFile> lock = takeWriteLock(root, lockWaitMs, result);
    if (!lock)
        return result;
    const GitRun& pulled = runner.run({QStringLiteral("pull"), QStringLiteral("--ff-only")});
    if (!pulled.ok()) {
        const GitStep step = runner.last();
        failed(result, step);
        if (step.run.started) {
            // `--is-ancestor` exits 1 when it isn't: the branch has commits its upstream hasn't.
            const GitRun& ancestor = runner.run({QStringLiteral("merge-base"), QStringLiteral("--is-ancestor"), QStringLiteral("HEAD"), QStringLiteral("@{upstream}")});
            if (ancestor.started && ancestor.exitCode == 1) {
                result.outcome = GitOutcome::Refused;
                result.code = QStringLiteral("cant-fast-forward");
                result.message = QStringLiteral("%1 and its remote have both moved on, so Loom can't fast-forward. Merge them in a terminal.")
                                     .arg(here->branch);
            }
        }
        return result;
    }
    result.outcome = GitOutcome::Done;
    result.message = QStringLiteral("Pulled %1 into %2.").arg(upstream, here->branch);
    return result;
}

PushPlan planPush(const GitTool& git, const QString& root)
{
    PushPlan plan;
    GitResult failure;
    Runner runner(git, root, plan.steps);
    const auto fail = [&](const QString& code, const QString& message) {
        plan.code = code;
        plan.message = message;
        return plan;
    };
    const std::optional<QStringList> listed = remotes(runner, failure);
    if (!listed)
        return fail(failure.code, failure.message);
    plan.remotes = *listed;
    if (plan.remotes.isEmpty())
        return fail(QStringLiteral("no-remote"), QStringLiteral("This repository has no remote to push to."));
    const std::optional<Where> here = where(runner, failure);
    if (!here)
        return fail(failure.code, failure.message);
    plan.branch = here->branch;
    if (plan.branch.isEmpty())
        return fail(QStringLiteral("detached"), QStringLiteral("Make a branch first: a push needs a branch to name."));
    if (!here->hasCommits)
        return fail(QStringLiteral("nothing-to-push"), QStringLiteral("Nothing to push: %1 has no commits yet.").arg(plan.branch));
    plan.upstream = upstreamOf(runner);
    const QStringList log = plan.upstream.isEmpty()
        ? QStringList{QStringLiteral("log"), QStringLiteral("--oneline"), QStringLiteral("HEAD"), QStringLiteral("--not"), QStringLiteral("--remotes")}
        : QStringList{QStringLiteral("log"), QStringLiteral("--oneline"), QStringLiteral("@{upstream}..HEAD")};
    const std::optional<QString> commits = runner.output(log);
    if (!commits) {
        failed(failure, runner.last());
        return fail(failure.code, failure.message);
    }
    plan.commits = commits->split(u'\n', Qt::SkipEmptyParts);
    if (plan.commits.isEmpty())
        return fail(QStringLiteral("nothing-to-push"),
                    plan.upstream.isEmpty() ? QStringLiteral("Nothing to push: every commit of %1 is on a remote already.").arg(plan.branch)
                                            : QStringLiteral("Nothing to push: %1 has every commit of %2.").arg(plan.upstream, plan.branch));
    plan.ok = true;
    return plan;
}

GitResult pushCommits(const GitTool& git, const QString& root, const QString& remote)
{
    PushPlan plan = planPush(git, root);
    GitResult result;
    result.steps = std::move(plan.steps);
    if (!plan.ok) {
        const bool refused = plan.code == u"no-remote" || plan.code == u"detached" || plan.code == u"nothing-to-push";
        result.outcome = refused ? GitOutcome::Refused : GitOutcome::CantWork;
        result.code = plan.code;
        result.message = plan.message;
        return result;
    }
    Runner runner(git, root, result.steps);
    QStringList push = {QStringLiteral("push")};
    QString to = plan.upstream;
    if (plan.upstream.isEmpty()) {
        QString target = remote;
        if (target.isEmpty() && plan.remotes.size() == 1)
            target = plan.remotes.first();
        if (target.isEmpty())
            return refusal("choose-remote", QStringLiteral("Choose the remote to push %1 to.").arg(plan.branch), std::move(result.steps));
        if (!plan.remotes.contains(target))
            return refusal("no-remote", QStringLiteral("There is no remote called %1.").arg(target), std::move(result.steps));
        push << QStringLiteral("-u") << target << plan.branch;
        to = target + u'/' + plan.branch;
    }
    if (!runner.run(push).ok()) {
        failed(result, runner.last());
        return result;
    }
    result.outcome = GitOutcome::Done;
    result.message = QStringLiteral("Pushed %1 to %2.").arg(counted(plan.commits.size(), "commit", "commits"), to);
    return result;
}

} // namespace loom
