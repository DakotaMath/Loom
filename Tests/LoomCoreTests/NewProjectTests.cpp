// Creating a project (Specs/m1-core.md, "The project folder"; Verification, "New project layout":
// "writes exactly the files listed, with the listed contents; one commit; …; refuses a non-empty
// folder; stops cleanly without git identity"). The hook and merge driver come with #9 and #6.

#include "LoomCore/NewProject.h"
#include "LoomCore/ProjectToml.h"

#include "TestSupport.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

using loomtest::filesUnder;
using loomtest::gitOutput;
using loomtest::readFile;
using loomtest::writeFile;

namespace {

/// A temporary folder to create projects in, and a git with a test identity.
struct Workspace {
    QTemporaryDir temp;
    QString dir;
    loom::GitTool git;

    explicit Workspace(bool withIdentity = true)
    {
        REQUIRE(temp.isValid());
        dir = temp.path();
        git = loomtest::testGit(dir + "/gitconfig", withIdentity);
    }

    /// A request for a project called Demo in `folder`.
    loom::NewProjectRequest request(const QString& folder) const
    {
        loom::NewProjectRequest request;
        request.folder = folder;
        request.name = "Demo";
        request.idea = "Find the sharp decay rate.";
        request.created = QDate(2026, 11, 3);
        request.git = git;
        return request;
    }

    /// The stand-in git (Tests/LoomCoreTests/FakeGit.cpp), failing as `mode` says.
    loom::GitTool fakeGit(const char* mode) const
    {
        loom::GitTool fake = git;
        fake.program = QStringLiteral(LOOM_FAKE_GIT);
        fake.environment.insert("LOOM_FAKE_GIT_MODE", mode);
        return fake;
    }
};

/// The files the spec lists, with their contents, apart from loom.toml.
const std::vector<std::pair<QString, QByteArray>>& kitFiles()
{
    static const std::vector<std::pair<QString, QByteArray>> files = {
        {".gitattributes",
         "# Written by Loom.\n"
         "* text=auto eol=lf\n"
         "*.pdf binary\n"
         "*.png binary\n"
         "*.h5 filter=lfs diff=lfs merge=lfs -text\n"
         "*.checks.toml merge=loom-checks\n"},
        {".gitignore",
         "# Written by Loom.\n"
         ".loom/\n"
         "Papers/*/local/\n"},
        {"CLAUDE.md",
         "# This is a Loom project\n"
         "\n"
         "Loom owns the files in this folder. Change them only with `loom` commands, or with plain edits\n"
         "that `loom validate` accepts. Run `loom validate` before every commit; the pre-commit hook runs it too.\n"
         "Never write a check by `dakota`, never set `refuted_on` or `checked_against_pdf`, and never mark a\n"
         "field verified: those are Dakota's clicks.\n"},
        {"TeX/macros.tex", "% Macros for this project, e.g. \\newcommand{\\R}{\\mathbb{R}}\n"},
        {"TeX/preamble.tex",
         "% Packages for this project. Loom adds the theorem environments itself.\n"
         "\\usepackage{amsmath,amssymb,amsthm}\n"},
    };
    return files;
}

/// Checks that nothing was created: `folder` doesn't exist, or is still as `before` lists.
void requireUntouched(const QString& folder, const QStringList& before = {})
{
    if (before.isEmpty())
        CHECK_FALSE(QFileInfo::exists(folder));
    else
        CHECK(filesUnder(folder) == before);
    CHECK_FALSE(QFileInfo::exists(folder + "/.git"));
}

} // namespace

TEST_CASE("new-project writes exactly the spec's files and makes one commit", "[new-project]")
{
    const Workspace workspace;
    const QString folder = workspace.dir + "/Demo";

    const loom::NewProjectResult result = loom::createProject(workspace.request(folder));
    INFO(result.message.toStdString());
    REQUIRE(result.outcome == loom::NewProjectOutcome::Created);
    CHECK(result.path == folder);
    CHECK(result.code.isEmpty());

    QStringList expected = {"loom.toml"};
    for (const auto& [path, bytes] : kitFiles())
        expected << path;
    expected.sort();
    CHECK(filesUnder(folder) == expected);

    CHECK(readFile(folder + "/loom.toml") ==
          "format = 1\n"
          "name = \"Demo\"\n"
          "idea = \"\"\"\n"
          "Find the sharp decay rate.\n"
          "\"\"\"\n"
          "created = 2026-11-03\n"
          "\n"
          "[tex]\n"
          "preamble = [\"TeX/preamble.tex\", \"TeX/macros.tex\"]\n");
    for (const auto& [path, bytes] : kitFiles()) {
        INFO(path.toStdString());
        CHECK(readFile(folder + "/" + path) == bytes);
    }

    // No empty folders (Specs/m1-core.md, D6): only TeX/, which has files, and .git/.
    CHECK(QDir(folder).entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot) == QStringList{".git", "TeX"});

    const loom::GitTool& git = workspace.git;
    CHECK(gitOutput(git, {"rev-list", "--count", "HEAD"}, folder) == "1");
    CHECK(gitOutput(git, {"log", "-1", "--format=%s"}, folder) == "Create project Demo");
    CHECK(gitOutput(git, {"log", "-1", "--format=%an <%ae>"}, folder) == "Loom Test <test@example.com>");
    CHECK(gitOutput(git, {"branch", "--show-current"}, folder) == "main");
    CHECK(gitOutput(git, {"ls-files"}, folder).split('\n') == expected);
    CHECK(gitOutput(git, {"status", "--porcelain"}, folder).isEmpty());
    // `git lfs install --local` writes the LFS filter into the repo's own config.
    CHECK(gitOutput(git, {"config", "--local", "--get", "filter.lfs.clean"}, folder).startsWith("git-lfs clean"));

    const loom::OpenedProject opened = loom::openProject(folder);
    CHECK(opened.status == loom::OpenStatus::Opened);
    CHECK(opened.problems.empty());
    CHECK(opened.config.name == "Demo");
    CHECK(opened.config.idea == "Find the sharp decay rate.\n");
}

TEST_CASE("new-project works in an existing empty folder, and without an idea", "[new-project]")
{
    const Workspace workspace;
    const QString folder = workspace.dir + "/Empty";
    REQUIRE(QDir().mkdir(folder));

    loom::NewProjectRequest request = workspace.request(folder);
    request.idea.clear();
    const loom::NewProjectResult result = loom::createProject(request);
    INFO(result.message.toStdString());
    REQUIRE(result.outcome == loom::NewProjectOutcome::Created);
    CHECK_FALSE(readFile(folder + "/loom.toml").contains("idea"));
    CHECK(loom::openProject(folder).problems.empty());
}

TEST_CASE("new-project refuses a folder it can't use, and writes nothing", "[new-project]")
{
    const Workspace workspace;

    SECTION("a folder with a file in it")
    {
        const QString folder = workspace.dir + "/Busy";
        writeFile(folder + "/notes.txt", "mine");
        const loom::NewProjectResult result = loom::createProject(workspace.request(folder));
        CHECK(result.outcome == loom::NewProjectOutcome::Refused);
        CHECK(result.code == "folder-not-empty");
        CHECK(result.message == "`" + QDir::toNativeSeparators(folder) + "` isn't empty. Loom creates projects only in a new or empty folder.");
        requireUntouched(folder, {"notes.txt"});
    }

    SECTION("a folder with only a hidden file in it")
    {
        const QString folder = workspace.dir + "/Hidden";
        writeFile(folder + "/.hidden", "mine");
        CHECK(loom::createProject(workspace.request(folder)).code == "folder-not-empty");
        requireUntouched(folder, {".hidden"});
    }

    SECTION("a file")
    {
        const QString file = workspace.dir + "/file.txt";
        writeFile(file, "mine");
        const loom::NewProjectResult result = loom::createProject(workspace.request(file));
        CHECK(result.outcome == loom::NewProjectOutcome::Refused);
        CHECK(result.code == "not-a-folder");
        CHECK(readFile(file) == "mine");
    }

    SECTION("a folder whose parent doesn't exist")
    {
        const QString folder = workspace.dir + "/Missing/Demo";
        const loom::NewProjectResult result = loom::createProject(workspace.request(folder));
        CHECK(result.outcome == loom::NewProjectOutcome::CantWork);
        CHECK(result.code == "parent-missing");
        CHECK_FALSE(QFileInfo::exists(workspace.dir + "/Missing"));
    }

    SECTION("no name")
    {
        loom::NewProjectRequest request = workspace.request(workspace.dir + "/Demo");
        request.name.clear();
        CHECK(loom::createProject(request).code == "name-empty");
        requireUntouched(workspace.dir + "/Demo");
    }
}

TEST_CASE("new-project stops before writing anything without git, Git LFS or a git identity", "[new-project]")
{
    SECTION("no git")
    {
        const Workspace workspace;
        const QString folder = workspace.dir + "/Demo";
        loom::NewProjectRequest request = workspace.request(folder);
        request.git.program = workspace.dir + "/no-such-git";
        const loom::NewProjectResult result = loom::createProject(request);
        CHECK(result.outcome == loom::NewProjectOutcome::CantWork);
        CHECK(result.code == "git-missing");
        CHECK(result.message == "Loom needs git, and `git` didn't run. Install Git for Windows and try again.");
        requireUntouched(folder);
    }

    SECTION("no Git LFS")
    {
        const Workspace workspace;
        const QString folder = workspace.dir + "/Demo";
        loom::NewProjectRequest request = workspace.request(folder);
        request.git = workspace.fakeGit("no-lfs");
        const loom::NewProjectResult result = loom::createProject(request);
        CHECK(result.outcome == loom::NewProjectOutcome::CantWork);
        CHECK(result.code == "git-lfs-missing");
        CHECK(result.message == "Loom needs Git LFS, and `git lfs` didn't run. Git for Windows includes it; reinstall Git with LFS ticked.");
        requireUntouched(folder);
    }

    SECTION("no identity")
    {
        const Workspace workspace(false);
        const QString folder = workspace.dir + "/Demo";
        const loom::NewProjectResult result = loom::createProject(workspace.request(folder));
        CHECK(result.outcome == loom::NewProjectOutcome::CantWork);
        CHECK(result.code == "git-identity-missing");
        CHECK(result.message == QString::fromUtf8(
            "Git doesn't know your name and email yet, so Loom can't make the first commit. "
            "Run `git config --global user.name \"…\"` and `git config --global user.email …`, then try again."));
        requireUntouched(folder);
    }

    SECTION("an identity in a repository around the folder doesn't count")
    {
        // The new repository won't read that repository's config, so its commit would fail.
        const Workspace workspace(false);
        const QString outer = workspace.dir + "/Outer";
        REQUIRE(QDir().mkdir(outer));
        gitOutput(workspace.git, {"init", "--quiet"}, outer);
        gitOutput(workspace.git, {"config", "user.name", "Outer Repo"}, outer);
        gitOutput(workspace.git, {"config", "user.email", "outer@example.com"}, outer);

        const QString folder = outer + "/Demo";
        CHECK(loom::createProject(workspace.request(folder)).code == "git-identity-missing");
        requireUntouched(folder);
    }
}

TEST_CASE("new-project inside another repository makes its own", "[new-project]")
{
    const Workspace workspace;
    const QString outer = workspace.dir + "/Outer";
    REQUIRE(QDir().mkdir(outer));
    gitOutput(workspace.git, {"init", "--quiet"}, outer);

    loom::NewProjectRequest request = workspace.request(outer + "/Demo");
    // A variable pointing git at the outer repository is ignored.
    request.git.environment.insert("GIT_DIR", outer + "/.git");
    const loom::NewProjectResult result = loom::createProject(request);
    INFO(result.message.toStdString());
    REQUIRE(result.outcome == loom::NewProjectOutcome::Created);
    CHECK(QFileInfo(outer + "/Demo/.git").isDir());
    CHECK(gitOutput(workspace.git, {"rev-list", "--count", "HEAD"}, outer + "/Demo") == "1");
    CHECK_FALSE(loom::runGit(workspace.git, {"rev-parse", "--verify", "HEAD"}, outer).ok());
}

TEST_CASE("new-project says which git step failed, and leaves what it made", "[new-project]")
{
    const Workspace workspace;
    const QString folder = workspace.dir + "/Demo";
    loom::NewProjectRequest request = workspace.request(folder);
    request.git = workspace.fakeGit("init-fails");

    const loom::NewProjectResult result = loom::createProject(request);
    CHECK(result.outcome == loom::NewProjectOutcome::CantWork);
    CHECK(result.code == "git-failed");
    CHECK(result.message ==
          "Loom wrote the project files, but `git init -b main` failed: fatal: the fake git won't init. Loom left what it made in `"
              + QDir::toNativeSeparators(folder) + "` for you to look at.");
    CHECK(QFileInfo(folder + "/loom.toml").isFile());
    CHECK(QFileInfo(folder + "/CLAUDE.md").isFile());
}
