// The git panel's operations in LoomCore (Specs/m2-statements-review.md, "The git panel";
// Verification, "Git operations": "Each operation and each refusal; commit through the hook,
// refused and passed; push to a bare repository"). Each test runs real git on a copy of the demo
// project (Tests/Support/DemoProject), whose commit guard is this build's `loom`.

#include "LoomCore/GitOperations.h"

#include "DemoProject.h"
#include "TestSupport.h"

#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using loom::ChangedFile;
using loom::FileChange;
using loom::GitOutcome;
using loomtest::gitOutput;
using loomtest::readFile;
using loomtest::writeFile;

namespace {

/// A copy of the demo project, with helpers for the tests below.
struct Demo {
    loomtest::DemoProject project;

    Demo()
    {
        INFO(project.error().toStdString());
        REQUIRE(project.ok());
    }

    const QString& root() const { return project.root(); }
    const loom::GitTool& git() const { return project.git(); }
    QString path(const QString& relative) const { return root() + u'/' + relative; }
    QString git(const QStringList& arguments) const { return gitOutput(git(), arguments, root()); }

    /// The current state. Fails the test if it can't be read.
    loom::GitState state() const
    {
        loom::GitState state = loom::readGitState(git(), root());
        INFO(state.message.toStdString());
        REQUIRE(state.ok);
        return state;
    }

    /// The changed file at `path`, as the state lists it. Fails the test if it isn't listed.
    ChangedFile file(const QString& relative) const
    {
        const std::vector<ChangedFile> files = state().files;
        const auto found = std::find_if(files.cbegin(), files.cend(), [&](const ChangedFile& file) { return file.path == relative; });
        INFO("not listed: " << relative.toStdString());
        REQUIRE(found != files.cend());
        return *found;
    }

    /// Appends a line to a file.
    void append(const QString& relative, const QByteArray& line) const { writeFile(path(relative), readFile(path(relative)) + line); }
};

/// A bare repository in a temporary folder, to push to.
struct Remote {
    QTemporaryDir temp;
    QString path;

    explicit Remote(const loom::GitTool& git)
    {
        REQUIRE(temp.isValid());
        path = temp.path() + "/remote.git";
        REQUIRE(QDir().mkpath(path));
        gitOutput(git, {"init", "-q", "--bare", "-b", "main"}, path);
    }
};

/// The paths of the state's files.
QStringList paths(const std::vector<ChangedFile>& files)
{
    QStringList list;
    for (const ChangedFile& file : files)
        list << file.path;
    return list;
}

} // namespace

TEST_CASE("The state of a freshly committed project: its branch, no changes, no remote", "[git-panel][git]")
{
    Demo demo;
    const loom::GitState state = demo.state();
    CHECK(state.branch == "main");
    CHECK_FALSE(state.detached());
    CHECK(state.hasCommits);
    CHECK(state.head == demo.git({"rev-parse", "--short", "HEAD"}));
    CHECK(state.files.empty());
    CHECK(state.remotes.isEmpty());
    CHECK(state.upstream.isEmpty());
    CHECK(state.branches == QStringList{"main"});
    CHECK(state.inProgress.isEmpty());
    // Every command it ran is reported, for Output.
    REQUIRE_FALSE(state.steps.empty());
    CHECK(state.steps.front().arguments.first() == "status");
    CHECK(state.steps.front().folder == demo.root());
}

TEST_CASE("Changed files are new, changed or deleted, with untracked files marked, ignored ones left out", "[git-panel][git]")
{
    Demo demo;
    demo.append("Statements/A2DF.tex", "More.\n");
    REQUIRE(QFile::remove(demo.path("Statements/B3GH.tex")));
    writeFile(demo.path("Notes/idea one.md"), "An idea.\n");
    writeFile(demo.path(".loom/scratch.txt"), "Ignored.\n");
    demo.git({"mv", "Statements/C4JK.tex", "Statements/C4JZ.tex"});

    const loom::GitState state = demo.state();
    CHECK(paths(state.files)
          == QStringList{"Notes/idea one.md", "Statements/A2DF.tex", "Statements/B3GH.tex", "Statements/C4JK.tex", "Statements/C4JZ.tex"});
    CHECK(demo.file("Statements/A2DF.tex").change == FileChange::Changed);
    CHECK(demo.file("Statements/B3GH.tex").change == FileChange::Deleted);
    CHECK(demo.file("Notes/idea one.md").change == FileChange::New);
    CHECK(demo.file("Notes/idea one.md").untracked);
    // A rename staged with `git mv`: the new path is new, the old one deleted, both in the index.
    CHECK(demo.file("Statements/C4JZ.tex").change == FileChange::New);
    CHECK_FALSE(demo.file("Statements/C4JZ.tex").untracked);
    CHECK(demo.file("Statements/C4JK.tex").change == FileChange::Deleted);
    CHECK(state.trackedChanges());
}

TEST_CASE("git status's porcelain v2 is read record by record", "[git-panel]")
{
    QByteArray output;
    for (const char* record : {"# branch.oid 1234", "# branch.head main",
                               "1 .M N... 100644 100644 100644 aaa bbb With spaces/a b.tex",
                               "1 A. N... 000000 100644 100644 000 bbb New.tex",
                               "1 D. N... 100644 000000 000000 aaa 000 Gone.tex",
                               "2 R. N... 100644 100644 100644 aaa aaa R100 To.tex", "From.tex",
                               "u UU N... 100644 100644 100644 100644 a b c Both.tex",
                               "? Untracked.md", "! Ignored.log"}) {
        output += record;
        output += '\0';
    }
    const std::vector<ChangedFile> files = loom::parseGitStatus(output);
    REQUIRE(paths(files) == QStringList{"Both.tex", "From.tex", "Gone.tex", "New.tex", "To.tex", "Untracked.md", "With spaces/a b.tex"});
    CHECK(files[0].change == FileChange::Changed);
    CHECK(files[1].change == FileChange::Deleted);
    CHECK(files[2].change == FileChange::Deleted);
    CHECK(files[3].change == FileChange::New);
    CHECK(files[4].change == FileChange::New);
    CHECK((files[5].change == FileChange::New && files[5].untracked));
    CHECK(files[6].change == FileChange::Changed);
}

TEST_CASE("A click shows git's diff against HEAD, or a new file as it is", "[git-panel][git]")
{
    Demo demo;
    demo.append("Statements/A2DF.tex", "A line added.\n");
    writeFile(demo.path("Notes/new.md"), "Brand new.\n");
    writeFile(demo.path("Notes/blob.bin"), QByteArray("\x01\x00\x02", 3));
    std::vector<loom::GitStep> steps;

    const QString diff = loom::fileDiff(demo.git(), demo.root(), demo.file("Statements/A2DF.tex"), steps);
    CHECK(diff.contains("+A line added."));
    CHECK(diff.contains("diff --git a/Statements/A2DF.tex b/Statements/A2DF.tex"));
    REQUIRE(steps.size() == 1);
    CHECK(steps[0].arguments == QStringList{"diff", "HEAD", "--", "Statements/A2DF.tex"});

    CHECK(loom::fileDiff(demo.git(), demo.root(), demo.file("Notes/new.md"), steps) == "A new file:\n\nBrand new.\n");
    CHECK(loom::fileDiff(demo.git(), demo.root(), demo.file("Notes/blob.bin"), steps) == "A new binary file, 3 bytes.");
}

TEST_CASE("Commit puts in only the ticked files, adding new ones, through the commit guard", "[git-panel][git]")
{
    Demo demo;
    demo.append("Statements/A2DF.tex", "Ticked.\n");
    demo.append("Statements/B3GH.tex", "Not ticked.\n");
    writeFile(demo.path("Notes/new.md"), "New and ticked.\n");
    const std::vector<ChangedFile> ticked = {demo.file("Statements/A2DF.tex"), demo.file("Notes/new.md")};

    const loom::GitResult result = loom::commitFiles(demo.git(), demo.root(), ticked, "Tidy A2DF\n\nAnd a note.");
    INFO(result.message.toStdString() << "\n" << result.output.toStdString());
    REQUIRE(result.done());
    const QString hash = demo.git({"rev-parse", "--short", "HEAD"});
    CHECK(result.message == QString("Committed 2 files to main as %1.").arg(hash));
    CHECK(demo.git({"log", "-1", "--format=%B"}) == "Tidy A2DF\n\nAnd a note.");
    CHECK(demo.git({"show", "--name-only", "--format=", "HEAD"}) == "Notes/new.md\nStatements/A2DF.tex");
    CHECK(paths(demo.state().files) == QStringList{"Statements/B3GH.tex"});
    // `git add`, then `git commit`, then reading the new hash.
    QStringList commands;
    for (const loom::GitStep& step : result.steps)
        commands << step.arguments.first();
    CHECK(commands.contains("add"));
    CHECK(commands.contains("commit"));
}

TEST_CASE("Commit refuses nothing ticked, an empty message, a detached HEAD and an operation in progress", "[git-panel][git]")
{
    Demo demo;
    demo.append("Statements/A2DF.tex", "More.\n");
    const std::vector<ChangedFile> ticked = {demo.file("Statements/A2DF.tex")};
    const QString before = demo.git({"rev-parse", "HEAD"});

    SECTION("nothing ticked")
    {
        const loom::GitResult result = loom::commitFiles(demo.git(), demo.root(), {}, "A message");
        CHECK(result.outcome == GitOutcome::Refused);
        CHECK(result.code == "nothing-ticked");
        CHECK(result.steps.empty());
    }
    SECTION("an empty message")
    {
        const loom::GitResult result = loom::commitFiles(demo.git(), demo.root(), ticked, " \n\t");
        CHECK(result.code == "empty-message");
        CHECK(result.steps.empty());
    }
    SECTION("a detached HEAD")
    {
        demo.git({"switch", "-q", "--detach"});
        const loom::GitResult result = loom::commitFiles(demo.git(), demo.root(), ticked, "A message");
        CHECK(result.outcome == GitOutcome::Refused);
        CHECK(result.code == "detached");
        CHECK(result.message == "Make a branch first: a commit here would be left on no branch.");
        CHECK(demo.state().detached());
    }
    SECTION("a merge in progress")
    {
        writeFile(demo.path(".git/MERGE_HEAD"), before.toUtf8() + "\n");
        CHECK(demo.state().inProgress == "merge");
        const loom::GitResult result = loom::commitFiles(demo.git(), demo.root(), ticked, "A message");
        CHECK(result.code == "in-progress");
        CHECK(result.message == "A merge is in progress. Finish it in a terminal.");
    }
    CHECK(demo.git({"rev-parse", "HEAD"}) == before);
}

TEST_CASE("A commit the guard refuses lists loom validate's problems and leaves the index as it was", "[git-panel][git]")
{
    Demo demo;
    QString broken = QString::fromUtf8(readFile(demo.path("Statements/A2DF.tex"))).replace("kind = \"definition\"", "kind = \"bogus\"");
    writeFile(demo.path("Statements/A2DF.tex"), broken.toUtf8());
    writeFile(demo.path("Notes/new.md"), "New.\n");
    const std::vector<ChangedFile> ticked = {demo.file("Statements/A2DF.tex"), demo.file("Notes/new.md")};
    const QString before = demo.git({"rev-parse", "HEAD"});

    const loom::GitResult refused = loom::commitFiles(demo.git(), demo.root(), ticked, "Break it");
    INFO(refused.output.toStdString());
    CHECK(refused.outcome == GitOutcome::Refused);
    CHECK(refused.code == "hook-refused");
    REQUIRE_FALSE(refused.problems.empty());
    CHECK(refused.problems.front().file == "Statements/A2DF.tex");
    CHECK(refused.problems.front().severity == loom::Severity::Error);
    CHECK(refused.problems.front().line == 3);
    CHECK(refused.message.startsWith("Loom's commit guard refused the commit"));
    CHECK(refused.output.contains("Statements/A2DF.tex:3: error:"));
    CHECK(demo.git({"rev-parse", "HEAD"}) == before);
    // The new file it added for the commit is untracked again.
    CHECK(demo.file("Notes/new.md").untracked);

    // Fixed, the same commit goes in.
    writeFile(demo.path("Statements/A2DF.tex"), broken.replace("kind = \"bogus\"", "kind = \"definition\"").toUtf8() + "Fixed.\n");
    const loom::GitResult passed = loom::commitFiles(demo.git(), demo.root(), ticked, "Fix it");
    INFO(passed.output.toStdString());
    CHECK(passed.done());
    CHECK(demo.state().files.empty());
}

TEST_CASE("Problem lines in loom validate's form are read from git's output", "[git-panel]")
{
    const std::vector<loom::Problem> problems = loom::parseProblemLines(
        "Loom's commit guard:\r\nStatements/K3AB.tex:12: error: The proof is missing.\r\nloom.toml:1: warning: Old.\nnot a problem: here\n");
    REQUIRE(problems.size() == 2);
    CHECK(problems[0].file == "Statements/K3AB.tex");
    CHECK(problems[0].line == 12);
    CHECK(problems[0].severity == loom::Severity::Error);
    CHECK(problems[0].message == "The proof is missing.");
    CHECK(problems[1].file == "loom.toml");
    CHECK(problems[1].severity == loom::Severity::Warning);
}

TEST_CASE("New branch takes the changes along and refuses bad or existing names", "[git-panel][git]")
{
    Demo demo;
    // A branch checked out before, so `@{-1}` names something (Wiki/gotchas/git/check-ref-format-branch-expands.md).
    demo.git({"switch", "-q", "-c", "before"});
    demo.git({"switch", "-q", "main"});
    REQUIRE(demo.git({"check-ref-format", "--branch", "@{-1}"}) == "before");
    demo.append("Statements/A2DF.tex", "Comes along.\n");

    CHECK(loom::createBranch(demo.git(), demo.root(), "a..b").code == "bad-branch-name");
    CHECK(loom::createBranch(demo.git(), demo.root(), "@{-1}").code == "bad-branch-name");
    CHECK(loom::createBranch(demo.git(), demo.root(), "").code == "bad-branch-name");
    const loom::GitResult exists = loom::createBranch(demo.git(), demo.root(), "main");
    CHECK(exists.code == "branch-exists");
    CHECK(exists.message == "There is a branch called main already.");

    const loom::GitResult made = loom::createBranch(demo.git(), demo.root(), "tail-bounds");
    INFO(made.message.toStdString());
    REQUIRE(made.done());
    const loom::GitState state = demo.state();
    CHECK(state.branch == "tail-bounds");
    CHECK(state.branches == QStringList{"before", "main", "tail-bounds"});
    CHECK(paths(state.files) == QStringList{"Statements/A2DF.tex"});
}

TEST_CASE("Switch branch refuses uncommitted changes to tracked files, and holds the write lock", "[git-panel][git]")
{
    Demo demo;
    demo.git({"branch", "other"});

    SECTION("a change to a tracked file")
    {
        demo.append("Statements/A2DF.tex", "Uncommitted.\n");
        const loom::GitResult result = loom::switchBranch(demo.git(), demo.root(), "other");
        CHECK(result.outcome == GitOutcome::Refused);
        CHECK(result.code == "uncommitted-changes");
        CHECK(result.message == "Commit your changes first, or they would come along to other.");
        CHECK(demo.state().branch == "main");
    }
    SECTION("an untracked file comes along, as git does")
    {
        writeFile(demo.path("Notes/new.md"), "New.\n");
        CHECK(loom::switchBranch(demo.git(), demo.root(), "other").done());
        CHECK(demo.state().branch == "other");
    }
    SECTION("a branch that isn't there")
    {
        CHECK(loom::switchBranch(demo.git(), demo.root(), "nowhere").code == "no-such-branch");
    }
    SECTION("the write lock held by another loom")
    {
        REQUIRE(QDir().mkpath(demo.path(".loom")));
        QLockFile held(demo.path(".loom/write.lock"));
        REQUIRE(held.tryLock(0));
        const loom::GitResult result = loom::switchBranch(demo.git(), demo.root(), "other", 100);
        CHECK(result.code == "locked");
        CHECK(result.message == "Another loom command is changing this project. Try again when it has finished.");
        CHECK(demo.state().branch == "main");
    }
}

TEST_CASE("Fetch, pull and push refuse a repository with no remote", "[git-panel][git]")
{
    Demo demo;
    CHECK(loom::fetchRemotes(demo.git(), demo.root()).code == "no-remote");
    CHECK(loom::pullFastForward(demo.git(), demo.root()).code == "no-remote");
    CHECK(loom::planPush(demo.git(), demo.root()).code == "no-remote");
    CHECK(loom::pushCommits(demo.git(), demo.root()).code == "no-remote");
}

TEST_CASE("Push lists the commits first, sets an upstream the first time, and then has nothing to push", "[git-panel][git]")
{
    Demo demo;
    Remote remote(demo.git());
    demo.git({"remote", "add", "origin", remote.path});

    loom::PushPlan plan = loom::planPush(demo.git(), demo.root());
    INFO(plan.message.toStdString());
    REQUIRE(plan.ok);
    CHECK(plan.upstream.isEmpty());
    CHECK(plan.remotes == QStringList{"origin"});
    CHECK(plan.commits == QStringList{demo.git({"log", "--oneline", "-1"})});
    CHECK(plan.steps.back().arguments == QStringList{"log", "--oneline", "HEAD", "--not", "--remotes"});

    const loom::GitResult pushed = loom::pushCommits(demo.git(), demo.root());
    INFO(pushed.output.toStdString());
    REQUIRE(pushed.done());
    CHECK(pushed.message == "Pushed 1 commit to origin/main.");
    CHECK(pushed.steps.back().arguments == QStringList{"push", "-u", "origin", "main"});
    CHECK(gitOutput(demo.git(), {"rev-parse", "main"}, remote.path) == demo.git({"rev-parse", "HEAD"}));
    CHECK(demo.state().upstream == "origin/main");

    plan = loom::planPush(demo.git(), demo.root());
    CHECK_FALSE(plan.ok);
    CHECK(plan.code == "nothing-to-push");
    CHECK(plan.message == "Nothing to push: origin/main has every commit of main.");

    // Two commits later, with the upstream: `@{upstream}..HEAD`, then plain `git push`.
    for (const char* line : {"One.\n", "Two.\n"}) {
        demo.append("Statements/A2DF.tex", line);
        REQUIRE(loom::commitFiles(demo.git(), demo.root(), {demo.file("Statements/A2DF.tex")}, line).done());
    }
    CHECK(demo.state().ahead == 2);
    plan = loom::planPush(demo.git(), demo.root());
    REQUIRE(plan.ok);
    CHECK(plan.commits.size() == 2);
    CHECK(plan.commits.first().endsWith("Two."));
    CHECK(plan.steps.back().arguments == QStringList{"log", "--oneline", "@{upstream}..HEAD"});
    const loom::GitResult again = loom::pushCommits(demo.git(), demo.root());
    REQUIRE(again.done());
    CHECK(again.steps.back().arguments == QStringList{"push"});
    CHECK(again.message == "Pushed 2 commits to origin/main.");
}

TEST_CASE("Push asks which remote when there are several and no upstream", "[git-panel][git]")
{
    Demo demo;
    Remote first(demo.git());
    Remote second(demo.git());
    demo.git({"remote", "add", "origin", first.path});
    demo.git({"remote", "add", "backup", second.path});
    CHECK(loom::planPush(demo.git(), demo.root()).remotes == QStringList{"backup", "origin"});
    CHECK(loom::pushCommits(demo.git(), demo.root()).code == "choose-remote");
    CHECK(loom::pushCommits(demo.git(), demo.root(), "elsewhere").code == "no-remote");
    const loom::GitResult pushed = loom::pushCommits(demo.git(), demo.root(), "backup");
    REQUIRE(pushed.done());
    CHECK(demo.state().upstream == "backup/main");
}

TEST_CASE("Fetch sees the remote move on; pull fast-forwards, and refuses when both moved on", "[git-panel][git]")
{
    Demo demo;
    Remote remote(demo.git());
    demo.git({"remote", "add", "origin", remote.path});
    REQUIRE(loom::pushCommits(demo.git(), demo.root()).done());

    // Someone else pushes a commit.
    QTemporaryDir elsewhere;
    REQUIRE(elsewhere.isValid());
    gitOutput(demo.git(), {"clone", "-q", remote.path, "clone"}, elsewhere.path());
    const QString clone = elsewhere.path() + "/clone";
    writeFile(clone + "/Notes/theirs.md", "Theirs.\n");
    gitOutput(demo.git(), {"add", "--", "Notes/theirs.md"}, clone);
    gitOutput(demo.git(), {"commit", "-q", "--no-verify", "-m", "Theirs"}, clone);
    gitOutput(demo.git(), {"push", "-q"}, clone);

    const loom::GitResult fetched = loom::fetchRemotes(demo.git(), demo.root());
    REQUIRE(fetched.done());
    CHECK(fetched.steps.back().arguments == QStringList{"fetch", "--prune"});
    CHECK(demo.state().behind == 1);

    SECTION("only the remote moved on: a fast-forward")
    {
        const loom::GitResult pulled = loom::pullFastForward(demo.git(), demo.root());
        INFO(pulled.output.toStdString());
        REQUIRE(pulled.done());
        CHECK(pulled.message == "Pulled origin/main into main.");
        CHECK(QFile::exists(demo.path("Notes/theirs.md")));
        CHECK(demo.state().behind == 0);
    }
    SECTION("both moved on")
    {
        demo.append("Statements/A2DF.tex", "Mine.\n");
        REQUIRE(loom::commitFiles(demo.git(), demo.root(), {demo.file("Statements/A2DF.tex")}, "Mine").done());
        const QString mine = demo.git({"rev-parse", "HEAD"});
        const loom::GitResult pulled = loom::pullFastForward(demo.git(), demo.root());
        CHECK(pulled.outcome == GitOutcome::Refused);
        CHECK(pulled.code == "cant-fast-forward");
        CHECK(pulled.message == "main and its remote have both moved on, so Loom can't fast-forward. Merge them in a terminal.");
        CHECK(demo.git({"rev-parse", "HEAD"}) == mine);
    }
    SECTION("a branch with no upstream")
    {
        REQUIRE(loom::createBranch(demo.git(), demo.root(), "local-only").done());
        const loom::GitResult pulled = loom::pullFastForward(demo.git(), demo.root());
        CHECK(pulled.code == "no-upstream");
        CHECK(pulled.message == "local-only has no upstream branch to pull from. Push it first.");
    }
}

TEST_CASE("A git that doesn't finish in time is stopped and reported as M1 does", "[git-panel][git]")
{
    Demo demo;
    Remote remote(demo.git());
    demo.git({"remote", "add", "origin", remote.path});
    loom::GitTool slow = demo.git();
    slow.program = QStringLiteral(LOOM_FAKE_GIT);
    slow.environment.insert("LOOM_FAKE_GIT_MODE", "slow-at");
    slow.environment.insert("LOOM_FAKE_GIT_SLOW", "fetch --prune");
    slow.timeoutMs = 1000;

    const loom::GitResult result = loom::fetchRemotes(slow, demo.root());
    CHECK(result.outcome == GitOutcome::CantWork);
    CHECK(result.code == "git-timeout");
    CHECK(result.message
          == "`git fetch --prune` didn't finish in time, so Loom stopped it. Git may be waiting for something, such as another git's "
             "lock or a slow network drive; try again.");
    REQUIRE_FALSE(result.steps.empty());
    CHECK(loom::gitStepText(result.steps.back()).contains("Didn't finish in time"));
}

TEST_CASE("Output's text for a git command: folder, command, exit code, time, output cut at 1 MB", "[git-panel]")
{
    loom::GitStep step;
    step.folder = "C:/Projects/Demo";
    step.arguments = {"commit", "-m", "Two words"};
    step.milliseconds = 1250;
    step.run.started = true;
    step.run.exitCode = 1;
    step.run.output = "out\n";
    step.run.errors = "err\n";
    const QString text = loom::gitStepText(step);
    CHECK(text == QString("In %1: git commit -m \"Two words\"\nExit code 1 after 1.25 s.\nout\nerr").arg(QDir::toNativeSeparators("C:/Projects/Demo")));

    step.run.output = QString(2 * 1024 * 1024, u'x');
    step.run.errors.clear();
    const QString cut = loom::gitStepText(step);
    CHECK(cut.endsWith("… (cut at 1 MB)"));
    CHECK(cut.size() < 1024 * 1024 + 200);
}
