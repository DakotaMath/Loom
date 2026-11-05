// *Check my setup* (Specs/m2-statements-review.md, "Settings"; Verification, "Setup checks": "Found
// and missing tools (with a PATH of stand-ins)"). The stand-ins are copies of loom-fake-tool
// (FakeTool.cpp), each named for the tool it plays.

#include "LoomCore/NewProject.h"
#include "LoomCore/SetupCheck.h"

#include "TestSupport.h"

#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace {

/// What an executable's file name ends with here.
QString exeSuffix()
{
#ifdef Q_OS_WIN
    return QStringLiteral(".exe");
#else
    return {};
#endif
}

/// A folder of stand-ins, and a request whose PATH is only that folder.
struct StandIns {
    QTemporaryDir temp;
    QString bin;
    loom::SetupRequest request;

    /// Copies a stand-in for each tool named into `<temp>/bin`.
    /// Side effects: creates the folder and the copies.
    explicit StandIns(const QStringList& tools = {"git", "latexmk", "pdflatex", "synctex", "loom"})
    {
        REQUIRE(temp.isValid());
        bin = temp.path() + "/bin";
        REQUIRE(QDir().mkpath(bin));
        for (const QString& tool : tools)
            add(tool, bin);
        request.environment.insert("PATH", QDir::toNativeSeparators(bin));
        request.environment.remove("LOOM_FAKE_GIT_VERSION");
        request.environment.remove("LOOM_FAKE_NO_IDENTITY");
        request.environment.remove("LOOM_FAKE_NO_LFS");
        request.environment.remove("LOOM_FAKE_LATEX");
    }

    /// Copies the stand-in for `tool` into `folder`.
    /// @returns Its path, with `/`. Side effects: writes the copy.
    QString add(const QString& tool, const QString& folder) const
    {
        const QString path = folder + u'/' + tool + exeSuffix();
        QFile::remove(path);
        REQUIRE(QFile::copy(QStringLiteral(LOOM_FAKE_TOOL), path));
        QFile::setPermissions(path, QFile::permissions(path) | QFile::ExeOwner | QFile::ReadOwner);
        return path;
    }

    /// The path of a stand-in in `bin`, as findProgram() gives it.
    QString path(const QString& tool) const { return QDir::cleanPath(bin + u'/' + tool + exeSuffix()); }
};

/// The item with `key`; fails the test (and gives an empty item) if there isn't one.
loom::SetupItem item(const std::vector<loom::SetupItem>& items, const char* key)
{
    for (const loom::SetupItem& one : items) {
        if (one.key == QString::fromLatin1(key))
            return one;
    }
    FAIL_CHECK("no item " << key);
    return {};
}

/// Each item's key, in order.
QStringList keys(const std::vector<loom::SetupItem>& items)
{
    QStringList out;
    for (const loom::SetupItem& one : items)
        out << one.key;
    return out;
}

const QStringList allKeys = {"git", "git-identity", "git-lfs", "loom", "latexmk", "pdflatex", "synctex", "test-build"};

} // namespace

TEST_CASE("setup check: every tool found, in the spec's order", "[setup]")
{
    const StandIns tools;
    std::vector<loom::SetupItem> seen;
    const std::vector<loom::SetupItem> items = loom::checkSetup(tools.request, [&](const loom::SetupItem& one) { seen.push_back(one); });
    CHECK(keys(items) == allKeys);
    CHECK(keys(seen) == allKeys);
    for (const loom::SetupItem& one : items) {
        INFO(one.key.toStdString() << ": " << one.message.toStdString());
        CHECK(one.state == loom::SetupState::Found);
    }
    CHECK(item(items, "git").version == "git version 2.47.1.fake");
    CHECK(item(items, "git").path == tools.path("git"));
    CHECK(item(items, "git").name == "git");
    CHECK(item(items, "git-identity").version == "Ada Example <ada@example.com>");
    CHECK(item(items, "git-lfs").version == "git-lfs/3.6.0 (fake)");
    CHECK(item(items, "loom").version == "loom 0.1.0");
    CHECK(item(items, "loom").path == tools.path("loom"));
    CHECK(item(items, "latexmk").version == "Latexmk, John Collins, fake. Version 4.86");
    CHECK(item(items, "pdflatex").version == "pdfTeX 3.141592653-2.6-1.40.27 (fake)");
    CHECK(item(items, "synctex").version == "This is SyncTeX command line utility, version 1.5 (fake)");
    CHECK(item(items, "test-build").version.startsWith("Built in "));
}

TEST_CASE("setup check: nothing on PATH", "[setup]")
{
    const StandIns tools(QStringList{});
    const std::vector<loom::SetupItem> items = loom::checkSetup(tools.request);
    REQUIRE(keys(items) == allKeys);
    CHECK(item(items, "git").state == loom::SetupState::Missing);
    CHECK(item(items, "git").message == "Install Git for Windows.");
    CHECK(item(items, "git-identity").state == loom::SetupState::Skipped);
    CHECK(item(items, "git-lfs").state == loom::SetupState::Skipped);
    CHECK(item(items, "loom").message == "The commit guard runs `loom` from PATH. Add Loom's folder to PATH.");
    CHECK(item(items, "latexmk").message == "Install TeX Live (it includes latexmk and Perl). With MiKTeX, also install Perl.");
    CHECK(item(items, "pdflatex").message == "Install TeX Live.");
    // synctex is a warning, not a cross.
    CHECK(item(items, "synctex").state == loom::SetupState::Warning);
    CHECK(item(items, "synctex").message == "Without synctex, clicking in the PDF can't find the source; everything else works.");
    CHECK(item(items, "test-build").state == loom::SetupState::Skipped);
}

TEST_CASE("setup check: a git older than 2.40", "[setup]")
{
    StandIns tools;
    tools.request.environment.insert("LOOM_FAKE_GIT_VERSION", "git version 2.39.5");
    const loom::SetupItem git = item(loom::checkSetup(tools.request), "git");
    CHECK(git.state == loom::SetupState::Missing);
    CHECK(git.message == "This is git version 2.39.5; Loom needs git 2.40 or newer. Install Git for Windows.");
    CHECK(git.path == tools.path("git"));

    tools.request.environment.insert("LOOM_FAKE_GIT_VERSION", "git version 2.40.0.windows.1");
    CHECK(item(loom::checkSetup(tools.request), "git").state == loom::SetupState::Found);
}

TEST_CASE("setup check: git without an identity or Git LFS gives M1's messages", "[setup]")
{
    StandIns tools;
    tools.request.environment.insert("LOOM_FAKE_NO_IDENTITY", "1");
    tools.request.environment.insert("LOOM_FAKE_NO_LFS", "1");
    const std::vector<loom::SetupItem> items = loom::checkSetup(tools.request);
    CHECK(item(items, "git").state == loom::SetupState::Found);
    CHECK(item(items, "git-identity").state == loom::SetupState::Missing);
    CHECK(item(items, "git-identity").message == loom::gitIdentityMissingMessage());
    CHECK(item(items, "git-lfs").state == loom::SetupState::Missing);
    CHECK(item(items, "git-lfs").message == loom::gitLfsMissingMessage());
}

TEST_CASE("setup check: a failed test build shows the log's error", "[setup]")
{
    StandIns tools;
    tools.request.environment.insert("LOOM_FAKE_LATEX", "fail");
    const loom::SetupItem build = item(loom::checkSetup(tools.request), "test-build");
    CHECK(build.state == loom::SetupState::Missing);
    CHECK(build.message == "! Undefined control sequence.\nl.2 \\begin{document}\\foo");
}

TEST_CASE("setup check: a test build that outlasts the compile timeout is stopped", "[setup]")
{
    StandIns tools;
    tools.request.environment.insert("LOOM_FAKE_LATEX", "slow");
    tools.request.compileTimeoutMs = 1000;
    const loom::SetupItem build = item(loom::checkSetup(tools.request), "test-build");
    CHECK(build.state == loom::SetupState::Missing);
    CHECK(build.message.startsWith("It didn't finish within the compile timeout (1 seconds)."));
}

TEST_CASE("setup check: a full path overrides PATH", "[setup]")
{
    StandIns tools(QStringList{"git", "pdflatex", "synctex", "loom"});
    const QString other = tools.temp.path() + "/TeX Live/bin";
    REQUIRE(QDir().mkpath(other));
    const QString latexmk = tools.add("latexmk", other);
    tools.request.latexmk = QDir::toNativeSeparators(latexmk);
    const std::vector<loom::SetupItem> items = loom::checkSetup(tools.request);
    CHECK(item(items, "latexmk").state == loom::SetupState::Found);
    CHECK(item(items, "latexmk").path == latexmk);
    CHECK(item(items, "test-build").state == loom::SetupState::Found);

    tools.request.latexmk = tools.temp.path() + "/nowhere/latexmk";
    CHECK(item(loom::checkSetup(tools.request), "latexmk").state == loom::SetupState::Missing);
}

TEST_CASE("setup check: stopping leaves the rest unchecked", "[setup]")
{
    StandIns tools;
    tools.request.stop = std::make_shared<std::atomic_bool>(true);
    CHECK(loom::checkSetup(tools.request).empty());
}

TEST_CASE("findProgram looks on the environment's PATH, or at a path", "[setup]")
{
    const StandIns tools(QStringList{"git"});
    CHECK(loom::findProgram("git", tools.request.environment) == tools.path("git"));
    CHECK(loom::findProgram("latexmk", tools.request.environment).isEmpty());
    CHECK(loom::findProgram(tools.path("git"), tools.request.environment) == tools.path("git"));
#ifdef Q_OS_WIN
    // `.exe` may be left off a path, as in a Settings field.
    CHECK(loom::findProgram(tools.bin + "/git", tools.request.environment) == tools.path("git"));
#endif
    CHECK(loom::findProgram("", tools.request.environment).isEmpty());
}
