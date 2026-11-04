// Path safety (Specs/m1-core.md, "Path safety"; Verification: "`..`, absolute paths and symlinks
// leaving the project are refused").

#include "LoomCore/ProjectFiles.h"

#include "TestSupport.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

using loomtest::readFile;
using loomtest::writeFile;

TEST_CASE("Project paths are relative, with /, without . or ..", "[paths]")
{
    for (const char* good : {"loom.toml", "TeX/preamble.tex", ".gitignore", "Papers/Smith2020/local/paper.pdf", "Ünïcode/ß.tex"})
        CHECK(loom::isProjectPath(QString::fromUtf8(good)));

    for (const char* bad : {"", "/etc/passwd", "C:/Windows/win.ini", "C:loom.toml", "TeX\\preamble.tex", "../outside.tex",
                            "TeX/../../outside.tex", "TeX/../loom.toml", "./loom.toml", "TeX/./macros.tex", "TeX//macros.tex",
                            "TeX/", "loom.toml:stream"}) {
        INFO(bad);
        CHECK_FALSE(loom::isProjectPath(QString::fromUtf8(bad)));
    }
}

TEST_CASE("A project path resolves inside the project, existing or not", "[paths]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString root = temp.path();
    writeFile(root + "/TeX/preamble.tex", "x");

    CHECK(loom::resolveProjectPath(root, "TeX/preamble.tex") == root + "/TeX/preamble.tex");
    CHECK(loom::resolveProjectPath(root, "Statements/4KX7.tex") == root + "/Statements/4KX7.tex");
    CHECK(loom::resolveProjectPath(root, "New/Deeper/file.tex") == root + "/New/Deeper/file.tex");
    CHECK_FALSE(loom::resolveProjectPath(root, "../outside.tex"));
    CHECK_FALSE(loom::resolveProjectPath(root, QDir::cleanPath(root + "/TeX/preamble.tex")));
    CHECK_FALSE(loom::resolveProjectPath(root + "/missing", "loom.toml"));
}

TEST_CASE("Reading and writing project files", "[paths]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString root = temp.path() + "/Project";
    REQUIRE(QDir().mkdir(root));

    SECTION("a write creates the folders above the file and replaces it whole")
    {
        CHECK(loom::writeProjectFile(root, "Statements/4KX7.tex", "first\n").isEmpty());
        CHECK(loom::writeProjectFile(root, "Statements/4KX7.tex", "second\n").isEmpty());
        CHECK(readFile(root + "/Statements/4KX7.tex") == "second\n");
        CHECK(loom::readProjectFile(root, "Statements/4KX7.tex") == QByteArray("second\n"));
    }

    SECTION("a path leaving the project is refused, and nothing is written")
    {
        const QString message = loom::writeProjectFile(root, "../escaped.tex", "x");
        CHECK(message == "`../escaped.tex` must be a path inside the project, written with `/`, without `..`.");
        CHECK_FALSE(QFileInfo::exists(temp.path() + "/escaped.tex"));
        writeFile(temp.path() + "/secret.txt", "secret");
        CHECK_FALSE(loom::readProjectFile(root, "../secret.txt"));
    }

    SECTION("a missing file can't be read")
    {
        CHECK_FALSE(loom::readProjectFile(root, "loom.toml"));
    }
}

TEST_CASE("A link that leaves the project is refused", "[paths]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString root = temp.path() + "/Project";
    const QString outside = temp.path() + "/Outside";
    writeFile(root + "/TeX/preamble.tex", "x");
    writeFile(outside + "/secret.tex", "secret");

    if (!loomtest::makeDirectoryLink(outside, root + "/Link"))
        SKIP("This system can make neither a symbolic link nor a junction.");

    CHECK_FALSE(loom::resolveProjectPath(root, "Link"));
    CHECK_FALSE(loom::resolveProjectPath(root, "Link/secret.tex"));
    CHECK_FALSE(loom::resolveProjectPath(root, "Link/new/file.tex"));
    CHECK_FALSE(loom::readProjectFile(root, "Link/secret.tex"));
    CHECK_FALSE(loom::writeProjectFile(root, "Link/written.tex", "x").isEmpty());
    CHECK_FALSE(QFileInfo::exists(outside + "/written.tex"));
}

TEST_CASE("A link that stays inside the project is followed", "[paths]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString root = temp.path() + "/Project";
    writeFile(root + "/TeX/preamble.tex", "x");

    if (!loomtest::makeDirectoryLink(root + "/TeX", root + "/Alias"))
        SKIP("This system can make neither a symbolic link nor a junction.");

    CHECK(loom::resolveProjectPath(root, "Alias/preamble.tex") == root + "/Alias/preamble.tex");
    CHECK(loom::readProjectFile(root, "Alias/preamble.tex") == QByteArray("x"));
}

TEST_CASE("A dangling link is refused, wherever it would have led", "[paths]")
{
    // Its target can't be checked, and could be created outside the project later.
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString root = temp.path() + "/Project";
    const QString gone = temp.path() + "/Gone";
    writeFile(root + "/TeX/preamble.tex", "x");
    REQUIRE(QDir().mkpath(gone));
    if (!loomtest::makeDirectoryLink(gone, root + "/Link"))
        SKIP("This system can make neither a symbolic link nor a junction.");
    REQUIRE(QDir(gone).removeRecursively());

    CHECK_FALSE(loom::resolveProjectPath(root, "Link"));
    CHECK_FALSE(loom::resolveProjectPath(root, "Link/new.tex"));
    CHECK_FALSE(loom::writeProjectFile(root, "Link/new.tex", "x").isEmpty());
    CHECK_FALSE(QFileInfo::exists(gone));
}

TEST_CASE("A project reached through a link still works", "[paths]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString real = temp.path() + "/Real";
    writeFile(real + "/TeX/preamble.tex", "x");

    if (!loomtest::makeDirectoryLink(real, temp.path() + "/Linked"))
        SKIP("This system can make neither a symbolic link nor a junction.");

    CHECK(loom::readProjectFile(temp.path() + "/Linked", "TeX/preamble.tex") == QByteArray("x"));
}
