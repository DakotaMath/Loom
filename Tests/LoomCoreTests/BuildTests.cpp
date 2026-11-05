// Tests for building (Specs/m2-statements-review.md, "Building LaTeX" and "Building another
// revision"): real builds of the demo project with latexmk (tagged [tex]: skipped without TeX, and
// failed instead under LOOM_REQUIRE_TEX=1), reading a commit without touching the working tree,
// and the clean-up of build folders and revisions.

#include "DemoProject.h"
#include "TestSupport.h"

#include "LoomCore/Build.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/Revision.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMap>
#include <QStandardPaths>
#include <QThread>

#include <catch2/catch_test_macros.hpp>

using namespace loom;
using namespace loomtest;

namespace {

/// Skips the test when latexmk isn't found (Specs/m2-statements-review.md, "Dependencies and
/// toolchain"), unless LOOM_REQUIRE_TEX=1 asks for TeX, as CI's Linux job does: then it fails.
#define REQUIRE_TEX()                                                                                                        \
    do {                                                                                                                     \
        if (QStandardPaths::findExecutable(QStringLiteral("latexmk")).isEmpty()) {                                         \
            if (qEnvironmentVariableIntValue("LOOM_REQUIRE_TEX") == 1)                                                       \
                FAIL("latexmk not found, and LOOM_REQUIRE_TEX=1");                                                           \
            SKIP("latexmk not found");                                                                                       \
        }                                                                                                                    \
    } while (false)

/// A build of the demo, with a test's timeout.
BuildRequest requestFor(const DemoProject& demo, const QString& target, BuildMode mode = BuildMode::Alone, bool verbose = false)
{
    BuildRequest request;
    request.root = demo.root();
    request.target = target;
    request.mode = mode;
    request.verbose = verbose;
    request.git = demo.git();
    return request;
}

/// `<file>:<line>: <code>` of each problem.
QStringList whereOf(const BuildResult& result)
{
    QStringList where;
    for (const Problem& p : result.problems)
        where << QStringLiteral("%1:%2: %3").arg(p.file).arg(p.line).arg(p.code);
    return where;
}

/// Every file of a folder with its bytes, `.git` and `.loom` left out: to show a working tree
/// wasn't touched.
QMap<QString, QByteArray> snapshot(const QString& root)
{
    QMap<QString, QByteArray> files;
    for (const QString& path : filesUnder(root)) {
        if (!path.startsWith(QStringLiteral(".loom/")))
            files.insert(path, readFile(root + u'/' + path));
    }
    return files;
}

} // namespace

TEST_CASE("The demo builds in every mode", "[tex]")
{
    REQUIRE_TEX();
    const DemoProject demo;
    INFO(demo.error().toStdString());
    REQUIRE(demo.ok());
    const QStringList targets = {"A2DF", "B3GH", "C4JK", "D5MN", "E6PQ", "F7RS", "G8TV", "H9WX", "J2YZ", "K3AB", "D5MN.a"};
    for (const BuildMode mode : {BuildMode::Alone, BuildMode::Premises, BuildMode::Chain}) {
        for (const QString& target : targets) {
            if (mode != BuildMode::Alone && target != u"E6PQ" && target != u"F7RS" && target != u"H9WX" && target != u"D5MN.a")
                continue;
            CAPTURE(target, buildModeName(mode));
            const BuildResult result = buildTarget(requestFor(demo, target, mode));
            INFO(QString::fromUtf8(result.log.right(3000)).toStdString());
            CHECK(result.outcome == BuildOutcome::Succeeded);
            CHECK(result.errors() == 0);
            CHECK(result.pdf == buildFolder(target, mode, false) + QStringLiteral("/main.pdf"));
            CHECK(QFileInfo::exists(demo.root() + u'/' + result.pdf));
            CHECK(QFileInfo::exists(demo.root() + u'/' + result.lastGoodPdf));
        }
    }
    const BuildResult verbose = buildTarget(requestFor(demo, QStringLiteral("E6PQ"), BuildMode::Chain, true));
    CHECK(verbose.outcome == BuildOutcome::Succeeded);
    CHECK(verbose.folder == u".loom/build/E6PQ/chain-verbose");
    // F7RS cites a stub its paper's cited version has no locator for: a warning, still built.
    const BuildResult f7rs = buildTarget(requestFor(demo, QStringLiteral("F7RS")));
    CHECK(whereOf(f7rs) == QStringList{"Proofs/F7RS/F7RS.a.tex:8: paperref-no-locator"});
}

TEST_CASE("An error is reported at its source line, and the last good PDF is kept", "[tex]")
{
    REQUIRE_TEX();
    DemoProject demo;
    INFO(demo.error().toStdString());
    REQUIRE(demo.ok());
    const BuildResult good = buildTarget(requestFor(demo, QStringLiteral("K3AB")));
    REQUIRE(good.outcome == BuildOutcome::Succeeded);
    const QByteArray lastGood = readFile(demo.root() + u'/' + good.lastGoodPdf);

    REQUIRE(demo.applyOverlay(QStringLiteral("latex-error")).isEmpty());
    const BuildResult bad = buildTarget(requestFor(demo, QStringLiteral("K3AB")));
    CHECK(bad.outcome == BuildOutcome::Failed);
    CHECK(whereOf(bad) == QStringList{"Statements/K3AB.tex:13: tex-error"});
    CHECK(bad.problems.at(0).message == u"Missing } inserted.");
    CHECK(bad.lastGoodPdf == good.lastGoodPdf);
    CHECK(readFile(demo.root() + u'/' + bad.lastGoodPdf) == lastGood);
    CHECK(bad.log.contains("Missing } inserted."));
    CHECK(bad.command == u"latexmk -norc -pdf -interaction=nonstopmode -file-line-error -synctex=1 main.tex");

    // A failed build isn't reused: the same inputs run latexmk again.
    const BuildResult again = buildTarget(requestFor(demo, QStringLiteral("K3AB")));
    CHECK_FALSE(again.reused);
    CHECK(again.outcome == BuildOutcome::Failed);
}

TEST_CASE("An unchanged build is reused; a change builds again", "[tex]")
{
    REQUIRE_TEX();
    const DemoProject demo;
    INFO(demo.error().toStdString());
    REQUIRE(demo.ok());
    const BuildResult first = buildTarget(requestFor(demo, QStringLiteral("D5MN"), BuildMode::Premises));
    REQUIRE(first.outcome == BuildOutcome::Succeeded);
    CHECK_FALSE(first.reused);
    const QByteArray record = readFile(demo.root() + u'/' + first.folder + QStringLiteral("/loom-build.toml"));
    CHECK(record.contains("target = \"D5MN\"\nmode = \"premises\"\nverbose = false\ninputs = \""));
    CHECK(record.contains("result = \"succeeded\""));

    const BuildResult second = buildTarget(requestFor(demo, QStringLiteral("D5MN"), BuildMode::Premises));
    CHECK(second.reused);
    CHECK(second.outcome == BuildOutcome::Succeeded);
    CHECK(second.pdf == first.pdf);

    // Any input that changes builds again: here a premise's body (in the copies).
    const QString c4jk = demo.root() + QStringLiteral("/Statements/C4JK.tex");
    writeFile(c4jk, readFile(c4jk) + "Also, the tails never increase.\n");
    const BuildResult third = buildTarget(requestFor(demo, QStringLiteral("D5MN"), BuildMode::Premises));
    CHECK_FALSE(third.reused);
    CHECK(third.outcome == BuildOutcome::Succeeded);
}

TEST_CASE("A build that runs out of time is stopped with everything it started", "[tex]")
{
    REQUIRE_TEX();
    DemoProject demo;
    INFO(demo.error().toStdString());
    REQUIRE(demo.ok());
    // pdfLaTeX loops for ever, writing a new file every tenth of a second while it runs.
    const QString k3ab = demo.root() + QStringLiteral("/Statements/K3AB.tex");
    QByteArray bytes = readFile(k3ab);
    bytes += "\\newwrite\\loomtick \\newcount\\loomnext \\newcount\\loomn\n"
             "\\loop \\ifnum\\pdfelapsedtime>\\loomnext \\advance\\loomn by 1 "
             "\\immediate\\openout\\loomtick=tick-\\the\\loomn.txt \\immediate\\closeout\\loomtick "
             "\\advance\\loomnext by 6554 \\fi \\iftrue\\repeat\n";
    writeFile(k3ab, bytes);

    BuildRequest request = requestFor(demo, QStringLiteral("K3AB"));
    request.timeoutMs = 4000;
    QElapsedTimer clock;
    clock.start();
    const BuildResult result = buildTarget(request);
    CHECK(clock.elapsed() < 30'000);
    CHECK(result.outcome == BuildOutcome::CantWork);
    CHECK(result.code == u"tex-timeout");
    CHECK(result.message.startsWith(QStringLiteral("The build didn't finish in 4 seconds, so Loom stopped it.")));
    CHECK(whereOf(result) == QStringList{"Statements/K3AB.tex:1: tex-timeout"});

    // pdfLaTeX ran (it wrote ticks), and stopped: no new ticks appear.
    const QDir folder(demo.root() + u'/' + buildFolder(QStringLiteral("K3AB"), BuildMode::Alone, false));
    const auto ticks = [&] { return folder.entryList({QStringLiteral("tick-*.txt")}, QDir::Files).size(); };
    const qsizetype before = ticks();
    CHECK(before > 0);
    QThread::msleep(1500);
    CHECK(ticks() == before);
    // On Windows a running pdflatex would hold main.log open, and it couldn't be removed.
    CHECK((!QFileInfo::exists(folder.filePath(QStringLiteral("main.log"))) || QFile::remove(folder.filePath(QStringLiteral("main.log")))));
}

TEST_CASE("Without latexmk, the build says so and stops")
{
    const DemoProject demo;
    INFO(demo.error().toStdString());
    REQUIRE(demo.ok());
    BuildRequest request = requestFor(demo, QStringLiteral("C4JK"));
    request.latexmk = QStringLiteral("loom-test-no-such-latexmk");
    const BuildResult result = buildTarget(request);
    CHECK(result.outcome == BuildOutcome::CantWork);
    CHECK(result.code == u"tex-missing");
    CHECK(result.message == u"Loom couldn't run `latexmk`. Install TeX Live, then use Check my setup.");
    // The folder was written, and records that the build couldn't run, so the next one runs.
    CHECK(QFileInfo::exists(demo.root() + QStringLiteral("/.loom/build/C4JK/alone/main.tex")));
    CHECK(QFileInfo::exists(demo.root() + QStringLiteral("/.loom/build/C4JK/alone/loom.sty")));
    CHECK(readFile(demo.root() + QStringLiteral("/.loom/build/C4JK/alone/loom-build.toml")).contains("result = \"couldnt-run\""));
}

TEST_CASE("Refusals and a project that isn't one")
{
    const DemoProject demo;
    INFO(demo.error().toStdString());
    REQUIRE(demo.ok());
    BuildResult result = buildTarget(requestFor(demo, QStringLiteral("7RT2")));
    CHECK(result.outcome == BuildOutcome::Refused);
    CHECK(result.code == u"build-stub");
    result = buildTarget(requestFor(demo, QStringLiteral("ZZZZ")));
    CHECK(result.outcome == BuildOutcome::Refused);
    CHECK(result.code == u"ref-unknown");
    CHECK_FALSE(QFileInfo::exists(demo.root() + QStringLiteral("/.loom/build")));

    BuildRequest nowhere = requestFor(demo, QStringLiteral("C4JK"));
    nowhere.root = demo.root() + QStringLiteral("/Statements");
    result = buildTarget(nowhere);
    CHECK(result.outcome == BuildOutcome::CantWork);
    CHECK(result.code == u"not-a-project");
}

TEST_CASE("A commit is read without touching the working tree, and cached")
{
    DemoProject demo(QStringLiteral("review"));
    INFO(demo.error().toStdString());
    REQUIRE(demo.ok());
    const QMap<QString, QByteArray> before = snapshot(demo.root());
    const QString status = gitOutput(demo.git(), {"status", "--porcelain"}, demo.root());

    const RevisionTree tree = readRevision(demo.root(), QStringLiteral("HEAD"), demo.git());
    REQUIRE(tree.outcome == RevisionOutcome::Ready);
    CHECK_FALSE(tree.reused);
    CHECK(tree.hash == gitOutput(demo.git(), {"rev-parse", "HEAD"}, demo.root()));
    CHECK(tree.folder == QStringLiteral(".loom/rev/%1/tree").arg(tree.hash));
    const QString at = demo.root() + u'/' + tree.folder;
    // The commit's version, not the overlay's; the overlay's new statement isn't there.
    CHECK(readFile(at + QStringLiteral("/Statements/C4JK.tex")) == readFile(QStringLiteral(LOOM_DEMO_PROJECT "/Statements/C4JK.tex")));
    CHECK_FALSE(QFileInfo::exists(at + QStringLiteral("/Statements/M7QR.tex")));
    // What builds read is there; nothing else is.
    const QStringList files = filesUnder(at);
    CHECK(files.contains(QStringLiteral("loom.toml")));
    CHECK(files.contains(QStringLiteral("TeX/macros.tex")));
    CHECK(files.contains(QStringLiteral("Proofs/E6PQ/E6PQ.a.verbose.tex")));
    CHECK(files.contains(QStringLiteral("Papers/SmithJones24/paper.toml")));
    CHECK(files.contains(QStringLiteral("Papers/SmithJones24/Statements/7RT2.toml")));
    CHECK_FALSE(files.contains(QStringLiteral("Papers/SmithJones24/notes.md")));
    CHECK_FALSE(files.contains(QStringLiteral("Records/2026-11/demo-0001.md")));
    CHECK_FALSE(files.contains(QStringLiteral(".gitignore")));

    // The working tree and the index are as they were.
    CHECK(snapshot(demo.root()) == before);
    CHECK(gitOutput(demo.git(), {"status", "--porcelain"}, demo.root()) == status);

    // Used again as it is.
    writeFile(at + QStringLiteral("/marker"), "kept");
    const RevisionTree again = readRevision(demo.root(), QStringLiteral("HEAD"), demo.git());
    CHECK(again.reused);
    CHECK(QFileInfo::exists(at + QStringLiteral("/marker")));

    const RevisionTree missing = readRevision(demo.root(), QStringLiteral("no-such-branch"), demo.git());
    CHECK(missing.outcome == RevisionOutcome::CantWork);
    CHECK(missing.code == u"git-failed");
}

TEST_CASE("Building at HEAD while the working tree differs", "[tex]")
{
    REQUIRE_TEX();
    DemoProject demo(QStringLiteral("review"));
    INFO(demo.error().toStdString());
    REQUIRE(demo.ok());
    const QMap<QString, QByteArray> before = snapshot(demo.root());
    BuildRequest request = requestFor(demo, QStringLiteral("C4JK"));
    request.rev = QStringLiteral("HEAD");
    const BuildResult result = buildTarget(request);
    CHECK(result.outcome == BuildOutcome::Succeeded);
    const QString hash = gitOutput(demo.git(), {"rev-parse", "HEAD"}, demo.root());
    CHECK(result.rev == hash);
    CHECK(result.folder == QStringLiteral(".loom/rev/%1/build/C4JK/alone").arg(hash));
    // The copy is the committed body, not the working tree's.
    CHECK_FALSE(readFile(demo.root() + u'/' + result.folder + QStringLiteral("/src/Statements/C4JK.tex")).contains("never increase"));
    CHECK(snapshot(demo.root()) == before);
    CHECK(buildTarget(request).reused);

    // A statement that is new since the commit isn't there.
    request.target = QStringLiteral("M7QR");
    const BuildResult fresh = buildTarget(request);
    CHECK(fresh.outcome == BuildOutcome::Refused);
    CHECK(fresh.code == u"ref-unknown");
    CHECK(fresh.message == QStringLiteral("`M7QR` isn't a statement or proof at `HEAD` (%1).").arg(hash.left(7)));
}

TEST_CASE("A repository without a commit has no HEAD to build")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    REQUIRE(copyFolder(QStringLiteral(LOOM_DEMO_PROJECT), temp.path()).isEmpty());
    const GitTool git = testGit(temp.filePath(QStringLiteral(".gitconfig-test")));
    gitOutput(git, {"init", "-b", "main"}, temp.path());
    CHECK(readRevision(temp.path(), QStringLiteral("HEAD"), git).outcome == RevisionOutcome::NoCommits);

    BuildRequest request;
    request.root = temp.path();
    request.target = QStringLiteral("C4JK");
    request.rev = QStringLiteral("HEAD");
    request.git = git;
    const BuildResult result = buildTarget(request);
    CHECK(result.outcome == BuildOutcome::CantWork);
    CHECK(result.code == u"no-commits");
}

TEST_CASE("Old revisions and build folders of statements that are gone are cleaned up")
{
    DemoProject demo;
    INFO(demo.error().toStdString());
    REQUIRE(demo.ok());
    const QString first = readRevision(demo.root(), QStringLiteral("HEAD"), demo.git()).hash;
    writeFile(demo.root() + QStringLiteral("/Papers/SmithJones24/notes.md"), "Changed.\n");
    gitOutput(demo.git(), {"commit", "--quiet", "-am", "A second commit"}, demo.root());
    QThread::msleep(1100); // File times of the `used` marks must differ.
    const QString second = readRevision(demo.root(), QStringLiteral("HEAD"), demo.git()).hash;
    REQUIRE(first != second);
    CHECK(pruneRevisions(demo.root(), 1) == QStringList{first});
    CHECK(QFileInfo::exists(demo.root() + QStringLiteral("/.loom/rev/") + second + QStringLiteral("/tree/loom.toml")));
    CHECK_FALSE(QFileInfo::exists(demo.root() + QStringLiteral("/.loom/rev/") + first));

    for (const char* folder : {"C4JK/alone", "C4JK.a/chain", "ZZZZ/alone", "ZZZZ.a/alone", "preview"})
        QDir().mkpath(demo.root() + QStringLiteral("/.loom/build/") + QString::fromLatin1(folder));
    const ProjectContents contents = loadContents(demo.root()).contents;
    CHECK(cleanBuildFolders(demo.root(), contents) == QStringList{"ZZZZ", "ZZZZ.a"});
    CHECK(QFileInfo::exists(demo.root() + QStringLiteral("/.loom/build/C4JK.a/chain")));
    CHECK(QFileInfo::exists(demo.root() + QStringLiteral("/.loom/build/preview")));
}

TEST_CASE("The demo commits through the commit guard, and its overlays apply")
{
    const DemoProject demo(QStringLiteral("review"));
    INFO(demo.error().toStdString());
    REQUIRE(demo.ok());
    CHECK(gitOutput(demo.git(), {"log", "--oneline"}, demo.root()).endsWith(QStringLiteral("The demo project")));
    CHECK(QFileInfo::exists(demo.root() + QStringLiteral("/Statements/M7QR.tex")));
    const QString status = gitOutput(demo.git(), {"status", "--porcelain"}, demo.root());
    CHECK(status.contains(QStringLiteral("?? Statements/M7QR.tex")));
    CHECK(status.contains(QStringLiteral(" M Statements/C4JK.tex")));
    CHECK_FALSE(status.contains(QStringLiteral(".loom")));
}
