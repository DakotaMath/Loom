// `loom install-hooks` (Specs/m1-core.md, "The project folder": the pre-commit hook that runs
// `loom validate --staged`, the `loom-checks` merge driver, and a hook Loom didn't write kept as
// `pre-commit.before-loom`), and new-project installing both before its first commit.

#include "LoomCore/Hooks.h"
#include "LoomCore/NewProject.h"

#include "TestSupport.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

using loomtest::gitOutput;
using loomtest::readFile;
using loomtest::writeFile;

namespace {

/// A git repository in a temporary folder, with a test identity.
struct Repository {
    QTemporaryDir temp;
    QString root;
    loom::GitTool git;

    Repository()
    {
        REQUIRE(temp.isValid());
        root = temp.path() + "/Project";
        git = loomtest::testGit(temp.path() + "/gitconfig");
        writeFile(root + "/README.md", "A repository.\n");
        gitOutput(git, {"init", "-q", "-b", "main"}, root);
    }

    QString hook() const { return root + "/.git/hooks/pre-commit"; }
};

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
