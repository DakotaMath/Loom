// `loom install-hooks` (Specs/m1-core.md, "The project folder": the pre-commit hook that runs
// `loom validate --staged`, the `loom-checks` merge driver, and a hook Loom didn't write kept as
// `pre-commit.before-loom`), and new-project installing both before its first commit.

#include "LoomCore/Hooks.h"
#include "LoomCore/NewProject.h"

#include "TestSupport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using loomtest::gitOutput;
using loomtest::readFile;
using loomtest::writeFile;

namespace {

/// A git repository in a temporary folder, with a test identity.
struct Repository {
    QTemporaryDir temp;
    QString root;
    loom::GitTool git;

    /// @param project Whether the repository is a Loom project: `loom.toml` at its top.
    explicit Repository(bool project = true)
    {
        REQUIRE(temp.isValid());
        root = temp.path() + "/Project";
        git = loomtest::testGit(temp.path() + "/gitconfig");
        writeFile(root + "/README.md", "A repository.\n");
        if (project)
            writeFile(root + "/loom.toml", "format = 1\nname = \"Demo\"\ncreated = 2026-11-04\n");
        gitOutput(git, {"init", "-q", "-b", "main"}, root);
    }

    /// Where git runs the pre-commit hook from when nothing says otherwise.
    QString hook() const { return root + "/.git/hooks/pre-commit"; }

    /// Whether the repository's own config has the merge driver; false until installHooks() ran.
    bool hasMergeDriver() const
    {
        return loom::runGit(git, {"config", "--local", "--get", "merge.loom-checks.driver"}, root).ok();
    }
};

/// The spec's `hooks-path-outside` message for hooks run from `folder` (Specs/m1-core.md,
/// "`loom install-hooks`", A1).
QString hooksPathOutside(const QString& folder)
{
    return QStringLiteral("Git runs hooks from `%1` (`core.hooksPath`), outside this project, so Loom can't install its "
                          "commit guard there. Run `git config --local core.hooksPath .git/hooks` here, then `loom "
                          "install-hooks` again.")
        .arg(QDir::toNativeSeparators(folder));
}

} // namespace

TEST_CASE("the pre-commit hook runs loom validate --staged, from PATH or where loom was", "[hooks]")
{
    const QByteArray hook = loom::preCommitHook("/opt/Loom's tools/loom");
    CHECK(hook.startsWith("#!/bin/sh\n"));
    CHECK(loom::isLoomHook(hook));
    CHECK_FALSE(hook.contains('\r'));
    CHECK(hook.contains("exec loom validate --staged\n"));
    // A quote in the path is closed, escaped and reopened.
    CHECK(hook.contains("loom='/opt/Loom'\\''s tools/loom'\n"));
#ifdef Q_OS_WIN
    // Git's sh reads `C:/…`; `\` is a separator only on Windows, so only there is it turned into `/`.
    CHECK(loom::preCommitHook("C:\\Program Files\\Loom\\loom.exe").contains("loom='C:/Program Files/Loom/loom.exe'\n"));
#endif
    CHECK(hook.contains("exec \"$loom\" validate --staged\n"));
    CHECK(hook.contains("Loom's checks couldn't run because \\`loom\\` wasn't found. Put Loom on PATH, or run \\`loom install-hooks\\` again."));
    CHECK(hook.endsWith("exit 1\n"));

    CHECK(loom::preCommitHook({}).contains("loom=''\n"));
    CHECK_FALSE(loom::isLoomHook("#!/bin/sh\nexec make check\n"));
}

TEST_CASE("install-hooks writes the hook and registers the merge driver", "[hooks]")
{
    const Repository repo;
    const loom::HooksResult result = loom::installHooks(repo.root, "/opt/loom/bin/loom", repo.git);
    INFO(result.message.toStdString());
    REQUIRE(result.outcome == loom::HooksOutcome::Installed);
    CHECK(result.hookPath == repo.hook());
    CHECK(result.movedHook.isEmpty());
    CHECK(readFile(repo.hook()) == loom::preCommitHook("/opt/loom/bin/loom"));
#ifndef Q_OS_WIN
    // Windows has no executable bit; Git for Windows runs the hook anyway.
    CHECK(QFile::permissions(repo.hook()).testFlag(QFileDevice::ExeOwner));
#endif
    CHECK(gitOutput(repo.git, {"config", "--local", "--get", "merge.loom-checks.name"}, repo.root) == "Loom check history");
    CHECK(gitOutput(repo.git, {"config", "--local", "--get", "merge.loom-checks.driver"}, repo.root) == "loom merge-checks %O %A %B");

    // Again: Loom's own hook is simply rewritten.
    const loom::HooksResult again = loom::installHooks(repo.root, "/usr/bin/loom", repo.git);
    REQUIRE(again.outcome == loom::HooksOutcome::Installed);
    CHECK(again.movedHook.isEmpty());
    CHECK(readFile(repo.hook()) == loom::preCommitHook("/usr/bin/loom"));
    CHECK_FALSE(QFileInfo::exists(repo.hook() + ".before-loom"));
}

TEST_CASE("install-hooks keeps a hook it didn't write", "[hooks]")
{
    const Repository repo;
    writeFile(repo.hook(), "#!/bin/sh\nexec make check\n");
    const loom::HooksResult result = loom::installHooks(repo.root, "/usr/bin/loom", repo.git);
    REQUIRE(result.outcome == loom::HooksOutcome::Installed);
    CHECK(result.movedHook == repo.hook() + ".before-loom");
    CHECK(readFile(repo.hook() + ".before-loom") == "#!/bin/sh\nexec make check\n");
    CHECK(loom::isLoomHook(readFile(repo.hook())));

    // A second foreign hook, with the kept one in the way: refused, and both are left alone.
    writeFile(repo.hook(), "#!/bin/sh\nexec make lint\n");
    const loom::HooksResult refused = loom::installHooks(repo.root, "/usr/bin/loom", repo.git);
    CHECK(refused.outcome == loom::HooksOutcome::Refused);
    CHECK(refused.code == "hook-backup-exists");
    CHECK(readFile(repo.hook()) == "#!/bin/sh\nexec make lint\n");
    CHECK(readFile(repo.hook() + ".before-loom") == "#!/bin/sh\nexec make check\n");
}

TEST_CASE("install-hooks stops at an existing hook it can't read", "[hooks]")
{
    // A folder where the hook should be: Loom can't tell whose it is, so it writes nothing.
    const Repository repo;
    REQUIRE(QDir().mkpath(repo.hook()));
    const loom::HooksResult result = loom::installHooks(repo.root, "/usr/bin/loom", repo.git);
    CHECK(result.outcome == loom::HooksOutcome::CantWork);
    CHECK(result.code == "write-failed");
    CHECK(result.message == "Loom couldn't read `" + QDir::toNativeSeparators(repo.hook()) + "`.");
    CHECK(QFileInfo(repo.hook()).isDir());
    CHECK_FALSE(repo.hasMergeDriver());
}

TEST_CASE("new-project says when install-hooks fails, and leaves what it made", "[hooks][new-project]")
{
    // The stand-in git "succeeds" at `git init` without making a repository, so install-hooks
    // finds none.
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    loom::NewProjectRequest request;
    request.folder = temp.path() + "/Demo";
    request.name = "Demo";
    request.git.program = QStringLiteral(LOOM_FAKE_GIT);
    request.git.environment.insert("GIT_CEILING_DIRECTORIES", temp.path());
    request.loomProgram = QStringLiteral(LOOM_PROGRAM);
    const loom::NewProjectResult result = loom::createProject(request);
    CHECK(result.outcome == loom::NewProjectOutcome::CantWork);
    CHECK(result.code == "hooks-failed");
    const QString native = QDir::toNativeSeparators(request.folder);
    CHECK(result.message == "Loom wrote the project files, but `loom install-hooks` failed: `" + native
                            + "` isn't a git repository, so Loom has nowhere to install its hook. Loom left what it made in `"
                            + native + "` for you to look at.");
    CHECK(QFileInfo::exists(request.folder + "/loom.toml"));
}

TEST_CASE("install-hooks needs a git repository", "[hooks]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    loom::GitTool git = loomtest::testGit(temp.path() + "/gitconfig");
    git.environment.insert("GIT_CEILING_DIRECTORIES", temp.path());
    writeFile(temp.path() + "/Project/loom.toml", "format = 1\n");
    const loom::HooksResult result = loom::installHooks(temp.path() + "/Project", "/usr/bin/loom", git);
    CHECK(result.outcome == loom::HooksOutcome::CantWork);
    CHECK(result.code == "not-a-repository");
}

TEST_CASE("install-hooks refuses a folder without loom.toml, writing nothing", "[hooks]")
{
    // A repository that isn't a Loom project: Loom's hook would refuse every commit in it (A1).
    const Repository repo(false);
    writeFile(repo.hook(), "#!/bin/sh\nexec make check\n");
    const loom::HooksResult result = loom::installHooks(repo.root, "/usr/bin/loom", repo.git);
    CHECK(result.outcome == loom::HooksOutcome::CantWork);
    CHECK(result.code == "not-a-project");
    CHECK(result.message == "`" + QDir::toNativeSeparators(repo.root) + "` isn't a Loom project: it has no loom.toml.");
    CHECK(readFile(repo.hook()) == "#!/bin/sh\nexec make check\n");
    CHECK_FALSE(QFileInfo::exists(repo.hook() + ".before-loom"));
    CHECK_FALSE(repo.hasMergeDriver());
}

TEST_CASE("install-hooks refuses a project that isn't the top of its repository", "[hooks]")
{
    // The project is a folder inside a bigger repository, whose hooks belong to that repository (A1).
    const Repository repo(false);
    const QString inner = repo.root + "/Inner";
    writeFile(inner + "/loom.toml", "format = 1\nname = \"Inner\"\ncreated = 2026-11-04\n");
    const loom::HooksResult result = loom::installHooks(inner, "/usr/bin/loom", repo.git);
    CHECK(result.outcome == loom::HooksOutcome::CantWork);
    CHECK(result.code == "hooks-not-top-level");
    CHECK(result.message == "`" + QDir::toNativeSeparators(inner) + "` isn't the top of its git repository, so its hooks "
                            "belong to another project. Run `loom install-hooks` in a Loom project.");
    CHECK_FALSE(QFileInfo::exists(repo.hook()));
    CHECK_FALSE(repo.hasMergeDriver());
}

TEST_CASE("install-hooks refuses when git runs hooks from outside the repository", "[hooks]")
{
    // `git rev-parse --git-path hooks/pre-commit` follows core.hooksPath. Loom must never write
    // there (A1): that folder's hooks run for other repositories too.
    const Repository repo;
    const QString shared = repo.temp.path() + "/SharedHooks";
    writeFile(shared + "/pre-commit", "#!/bin/sh\nexec company-check\n");
    const auto untouched = [&] {
        CHECK(loomtest::filesUnder(shared) == QStringList{"pre-commit"});
        CHECK(readFile(shared + "/pre-commit") == "#!/bin/sh\nexec company-check\n");
        CHECK_FALSE(QFileInfo::exists(repo.hook()));
        CHECK_FALSE(repo.hasMergeDriver());
    };

    SECTION("set in the repository's own config")
    {
        gitOutput(repo.git, {"config", "--local", "core.hooksPath", shared}, repo.root);
        const loom::HooksResult result = loom::installHooks(repo.root, "/usr/bin/loom", repo.git);
        CHECK(result.outcome == loom::HooksOutcome::CantWork);
        CHECK(result.code == "hooks-path-outside");
        CHECK(result.message == hooksPathOutside(shared));
        untouched();
    }
    SECTION("set in the global config, until the repository's own config points back at .git/hooks")
    {
        const QString global = repo.temp.path() + "/gitconfig";
        writeFile(global, readFile(global) + "[core]\n\thooksPath = " + shared.toUtf8() + "\n");
        const loom::HooksResult result = loom::installHooks(repo.root, "/usr/bin/loom", repo.git);
        CHECK(result.outcome == loom::HooksOutcome::CantWork);
        CHECK(result.code == "hooks-path-outside");
        CHECK(result.message == hooksPathOutside(shared));
        untouched();

        // The way out the message gives.
        gitOutput(repo.git, {"config", "--local", "core.hooksPath", ".git/hooks"}, repo.root);
        const loom::HooksResult fixed = loom::installHooks(repo.root, "/usr/bin/loom", repo.git);
        INFO(fixed.message.toStdString());
        REQUIRE(fixed.outcome == loom::HooksOutcome::Installed);
        CHECK(loom::isLoomHook(readFile(repo.hook())));
        CHECK(loomtest::filesUnder(shared) == QStringList{"pre-commit"});
    }
}

TEST_CASE("A git that doesn't finish in time is told apart from a git that isn't there", "[hooks][git]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    loom::GitTool slow;
    slow.program = QStringLiteral(LOOM_FAKE_GIT);
    slow.environment.insert("LOOM_FAKE_GIT_MODE", "slow");
    slow.timeoutMs = 500;
    loom::GitTool missing;
    missing.program = temp.path() + "/no-such-git";

    const loom::GitRun late = loom::runGit(slow, {"rev-parse", "--show-toplevel"}, temp.path());
    const loom::GitRun absent = loom::runGit(missing, {"rev-parse", "--show-toplevel"}, temp.path());
    CHECK_FALSE(late.ok());
    CHECK_FALSE(absent.ok());
    CHECK(late.timedOut);
    CHECK_FALSE(absent.timedOut);
    CHECK(loom::gitDidntRunMessage(absent, {"rev-parse", "--show-toplevel"})
          == "Loom needs git, and `git` didn't run. Install Git for Windows and try again.");
    const QString lateMessage = "`git rev-parse --show-toplevel` didn't finish in time, so Loom stopped it. Git may be waiting for something, "
                                "such as another git's lock or a slow network drive; try again.";
    CHECK(loom::gitDidntRunMessage(late, {"rev-parse", "--show-toplevel"}) == lateMessage);

    // install-hooks says so, rather than to install Git.
    writeFile(temp.path() + "/Project/loom.toml", "format = 1\nname = \"Demo\"\ncreated = 2026-11-04\n");
    const loom::HooksResult result = loom::installHooks(temp.path() + "/Project", "/usr/bin/loom", slow);
    CHECK(result.outcome == loom::HooksOutcome::CantWork);
    CHECK(result.code == "git-timeout");
    CHECK(result.message == lateMessage);
}

TEST_CASE("install-hooks in a linked worktree writes the hook into the main repository's hooks (A10)", "[hooks]")
{
    // Git keeps a linked worktree's hooks in the common git directory, so that is "inside".
    const Repository repo;
    gitOutput(repo.git, {"add", "--all"}, repo.root);
    gitOutput(repo.git, {"commit", "-q", "--no-verify", "-m", "Start"}, repo.root);
    const QString linked = repo.temp.path() + "/Linked";
    gitOutput(repo.git, {"worktree", "add", "-q", "-b", "other", linked}, repo.root);

    const loom::HooksResult result = loom::installHooks(linked, "/usr/bin/loom", repo.git);
    INFO(result.message.toStdString());
    REQUIRE(result.outcome == loom::HooksOutcome::Installed);
    CHECK(QFileInfo(result.hookPath).canonicalFilePath() == QFileInfo(repo.hook()).canonicalFilePath());
    CHECK(loom::isLoomHook(readFile(repo.hook())));
}

TEST_CASE("install-hooks stops a git that doesn't finish at any step, with git-timeout (A16)", "[hooks][git]")
{
    // Every git command install-hooks runs, not only the first; the message names the whole command.
    const auto [slowAt, command] = GENERATE(table<const char*, const char*>({
        {"rev-parse --git-path", "git rev-parse --git-path hooks/pre-commit"},
        {"rev-parse --git-common-dir", "git rev-parse --git-common-dir"},
        {"config --local", "git config --local merge.loom-checks.name \"Loom check history\""},
    }));
    INFO(slowAt);
    const Repository repo;
    loom::GitTool git = repo.git;
    git.program = QStringLiteral(LOOM_FAKE_GIT);
    git.environment.insert("LOOM_FAKE_GIT_MODE", "slow-at");
    git.environment.insert("LOOM_FAKE_GIT_SLOW", slowAt);
    git.timeoutMs = 2000;
    const loom::HooksResult result = loom::installHooks(repo.root, "/usr/bin/loom", git);
    CHECK(result.outcome == loom::HooksOutcome::CantWork);
    CHECK(result.code == "git-timeout");
    CHECK(result.message == "`" + QString::fromUtf8(command) + "` didn't finish in time, so Loom stopped it. Git may be "
                            "waiting for something, such as another git's lock or a slow network drive; try again.");
}

TEST_CASE("new-project stops before writing anything when git's global config sets core.hooksPath", "[hooks][new-project]")
{
    // Both `git lfs install --local` and the commit guard would write into that shared folder (A1).
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString shared = temp.path() + "/SharedHooks";
    writeFile(shared + "/pre-commit", "#!/bin/sh\nexec company-check\n");
    loom::NewProjectRequest request;
    request.folder = temp.path() + "/Demo";
    request.name = "Demo";
    request.git = loomtest::testGit(temp.path() + "/gitconfig");
    writeFile(temp.path() + "/gitconfig", readFile(temp.path() + "/gitconfig") + "[core]\n\thooksPath = " + shared.toUtf8() + "\n");
    request.loomProgram = QStringLiteral(LOOM_PROGRAM);

    const loom::NewProjectResult result = loom::createProject(request);
    CHECK(result.outcome == loom::NewProjectOutcome::CantWork);
    CHECK(result.code == "hooks-path-outside");
    // Its own message: install-hooks' way out needs a repository, which doesn't exist yet.
    CHECK(result.message == "Git runs hooks from `" + QDir::toNativeSeparators(shared) + "` (`core.hooksPath` in your "
                            "global git config), so a new project's commit guard and Git LFS hooks would go there. Unset "
                            "it with `git config --global --unset core.hooksPath`, then try again.");
    CHECK_FALSE(QFileInfo::exists(request.folder));
    CHECK(loomtest::filesUnder(shared) == QStringList{"pre-commit"});
    CHECK(readFile(shared + "/pre-commit") == "#!/bin/sh\nexec company-check\n");
}

TEST_CASE("new-project installs the hook, which then guards every commit", "[hooks][new-project]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    loom::NewProjectRequest request;
    request.folder = temp.path() + "/Demo";
    request.name = "Demo";
    request.created = QDate(2026, 11, 4);
    request.git = loomtest::testGit(temp.path() + "/gitconfig");
    request.loomProgram = QStringLiteral(LOOM_PROGRAM);
    const loom::NewProjectResult created = loom::createProject(request);
    INFO(created.message.toStdString());
    REQUIRE(created.outcome == loom::NewProjectOutcome::Created);
    const QString& root = created.path;
    CHECK(readFile(root + "/.git/hooks/pre-commit") == loom::preCommitHook(QStringLiteral(LOOM_PROGRAM)));
    CHECK(gitOutput(request.git, {"config", "--local", "--get", "merge.loom-checks.driver"}, root) == "loom merge-checks %O %A %B");

    // A statement with broken front matter can't be committed; fixed, it can.
    const QByteArray good = "+++\nid = \"4KX7\"\nkind = \"lemma\"\ntitle = \"T\"\ncreated = 2026-11-04\n+++\nBody.\n";
    QByteArray broken = good;
    broken.replace("kind = ", "knd = ");
    writeFile(root + "/Statements/4KX7.tex", broken);
    gitOutput(request.git, {"add", "--all"}, root);
    const loom::GitRun refused = loom::runGit(request.git, {"commit", "-q", "-m", "Broken"}, root);
    CHECK_FALSE(refused.ok());
    CHECK(refused.errors.contains("Statements/4KX7.tex:3: error: `knd` isn't a field of a statement."));
    writeFile(root + "/Statements/4KX7.tex", good);
    gitOutput(request.git, {"add", "--all"}, root);
    const loom::GitRun committed = loom::runGit(request.git, {"commit", "-q", "-m", "Fixed"}, root);
    INFO(committed.errors.toStdString());
    CHECK(committed.ok());
}
