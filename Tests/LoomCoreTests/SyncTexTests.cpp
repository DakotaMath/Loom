// Tests for SyncTeX and the open statement's page (Specs/m2-statements-review.md, "SyncTeX and the
// open statement's page"): reading `main.aux` and `synctex`'s output (no TeX needed, on output
// saved from TeX Live 2026), and forward and inverse search on a real build of the demo (tagged
// [tex]: skipped without TeX, failed instead under LOOM_REQUIRE_TEX=1).

#include "DemoProject.h"

#include "LoomCore/Build.h"
#include "LoomCore/SyncTex.h"

#include <QFile>
#include <QStandardPaths>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace loom;
using Catch::Matchers::WithinAbs;

namespace {

/// What `synctex view` printed for a line of D5MN's body in a premises build: three records.
const QByteArray viewOutput = "This is SyncTeX command line utility, version 1.5\n"
                              "SyncTeX result begin\n"
                              "Output:main.pdf\n"
                              "Page:1\n"
                              "x:159.301056\n"
                              "y:402.315826\n"
                              "h:125.798241\n"
                              "v:405.043091\n"
                              "W:358.655029\n"
                              "H:10.909073\n"
                              "before:\n"
                              "offset:-1\n"
                              "middle:\n"
                              "after:\n"
                              "Output:main.pdf\n"
                              "Page:2\n"
                              "x:400.718292\n"
                              "y:388.766632\n"
                              "h:125.798241\n"
                              "v:391.493896\n"
                              "W:358.655029\n"
                              "H:10.909073\n"
                              "SyncTeX result end\n";

/// What `synctex edit` printed, with the file it gives.
QByteArray editOutput(const QString& input)
{
    return "This is SyncTeX command line utility, version 1.5\r\n"
           "SyncTeX result begin\r\n"
           "Output:main.pdf\r\n"
           "Input:" + input.toUtf8() + "\r\n"
           "Line:9\r\n"
           "Column:-1\r\n"
           "Offset:0\r\n"
           "Context:\r\n"
           "SyncTeX result end\r\n";
}

} // namespace

TEST_CASE("A label's page comes from main.aux")
{
    const QByteArray aux = "\\relax\n"
                           "\\newlabel{A2DF}{{A2DF}{1}{}{}{}}\n"
                           "\\newlabel{D5MN.a}{{D5MN.a}{3}}\n"
                           "\\newlabel{D5MN}{{\\textsf {D5MN}}{2}{Tame {sequences}}{loom.D5MN}{}}\n";
    CHECK(auxLabelPage(aux, QStringLiteral("A2DF")) == 1);
    CHECK(auxLabelPage(aux, QStringLiteral("D5MN")) == 2);   // Braces inside the text; hyperref's groups.
    CHECK(auxLabelPage(aux, QStringLiteral("D5MN.a")) == 3); // Not D5MN's line.
    CHECK(auxLabelPage(aux, QStringLiteral("K3AB")) == 0);
    CHECK(auxLabelPage(QByteArray("\\newlabel{X}{{X}"), QStringLiteral("X")) == 0); // Cut short.
}

TEST_CASE("synctex view's first record is the box")
{
    const std::optional<SyncTexBox> box = parseSynctexView(viewOutput);
    REQUIRE(box);
    CHECK(box->page == 1);
    CHECK_THAT(box->x, WithinAbs(125.798241, 1e-6));
    CHECK_THAT(box->y, WithinAbs(405.043091 - 10.909073, 1e-6));
    CHECK_THAT(box->width, WithinAbs(358.655029, 1e-6));
    CHECK_THAT(box->height, WithinAbs(10.909073, 1e-6));

    // A point without a size: x and y.
    const std::optional<SyncTexBox> point = parseSynctexView("SyncTeX result begin\nOutput:main.pdf\nPage:4\nx:10.5\ny:20\nh:0\nv:0\nW:0\nH:0\nSyncTeX result end\n");
    REQUIRE(point);
    CHECK(point->page == 4);
    CHECK(point->x == 10.5);
    CHECK(point->y == 20);
    CHECK(point->width == 0);

    CHECK_FALSE(parseSynctexView("This is SyncTeX command line utility, version 1.5\nSyncTeX: No tag for src/x.tex\n"));
    CHECK_FALSE(parseSynctexView(QByteArray()));
}

TEST_CASE("synctex edit's file maps back to the project")
{
    const QString folder = QStringLiteral("C:/Projects/My Project/.loom/build/D5MN/premises");
    // Absolute, as TeX Live on Windows writes it, with a ./ part and Windows' slashes.
    std::optional<SyncTexPlace> place = parseSynctexEdit(editOutput(folder + QStringLiteral("/./src/Statements/D5MN.tex")), folder);
    REQUIRE(place);
    CHECK(place->file == u"Statements/D5MN.tex");
    CHECK(place->line == 9);
    place = parseSynctexEdit(editOutput(QStringLiteral("C:\\Projects\\My Project\\.loom\\build\\D5MN\\premises\\src\\Proofs\\D5MN\\D5MN.a.tex")), folder);
    REQUIRE(place);
    CHECK(place->file == u"Proofs/D5MN/D5MN.a.tex");
    // Relative to the build folder.
    place = parseSynctexEdit(editOutput(QStringLiteral("./src/TeX/macros.tex")), folder);
    REQUIRE(place);
    CHECK(place->file == u"TeX/macros.tex");
    // main.tex is left for generatedPlace().
    place = parseSynctexEdit(editOutput(folder + QStringLiteral("/./main.tex")), folder);
    REQUIRE(place);
    CHECK(place->file == u"main.tex");
    // TeX's own files, and other folders, aren't places.
    CHECK_FALSE(parseSynctexEdit(editOutput(QStringLiteral("c:/texlive/2026/texmf-dist/tex/latex/amsmath.sty")), folder));
    CHECK_FALSE(parseSynctexEdit(editOutput(QStringLiteral("C:/Projects/Other/src/Statements/D5MN.tex")), folder));
    CHECK_FALSE(parseSynctexEdit(editOutput(QStringLiteral("./src/../other.tex")), folder));
    CHECK_FALSE(parseSynctexEdit("SyncTeX result begin\nSyncTeX result end\n", folder));
}

TEST_CASE("A line of main.tex points at the body below its heading")
{
    const QByteArray mainTex = "\\begin{document}\n"                                              // 1
                               "\\loomheader{D5MN}{alone}\n"                                       // 2
                               "\n"                                                                // 3
                               "\\begin{loomstatement}{lemma}{D5MN}{Tame sequences are bounded}\n" // 4
                               "\\input{src/Statements/D5MN.tex}\n"                                // 5
                               "\\end{loomstatement}\n"                                            // 6
                               "\\end{document}\n";                                                // 7
    const auto copy = [](const QString& path) {
        return path == u"src/Statements/D5MN.tex" ? QByteArray("%\n%\n%\n%\nEvery tame sequence is bounded.\n") : QByteArray();
    };
    for (const int line : {3, 4, 5}) {
        CAPTURE(line);
        const std::optional<SyncTexPlace> place = generatedPlace(mainTex, line, copy);
        REQUIRE(place);
        CHECK(place->file == u"Statements/D5MN.tex");
        CHECK(place->line == 5); // The body's first line, after the front matter's four.
    }
    CHECK_FALSE(generatedPlace(mainTex, 2, copy));
    CHECK_FALSE(generatedPlace(mainTex, 6, copy));
    CHECK_FALSE(generatedPlace(mainTex, 0, copy));
    CHECK_FALSE(generatedPlace(mainTex, 99, copy));
}

TEST_CASE("Forward and inverse search on a real build", "[tex]")
{
    if (QStandardPaths::findExecutable(QStringLiteral("latexmk")).isEmpty()
        || QStandardPaths::findExecutable(QStringLiteral("synctex")).isEmpty()) {
        if (qEnvironmentVariableIntValue("LOOM_REQUIRE_TEX") == 1)
            FAIL("latexmk or synctex not found, and LOOM_REQUIRE_TEX=1");
        SKIP("latexmk not found");
    }
    const loomtest::DemoProject demo;
    INFO(demo.error().toStdString());
    REQUIRE(demo.ok());
    BuildRequest request;
    request.root = demo.root();
    request.target = QStringLiteral("D5MN");
    request.mode = BuildMode::Premises;
    request.git = demo.git();
    const BuildResult result = buildTarget(request);
    REQUIRE(result.outcome == BuildOutcome::Succeeded);
    const QString folder = demo.root() + u'/' + result.folder;

    // The open statement's page, from main.aux.
    QFile aux(folder + QStringLiteral("/main.aux"));
    REQUIRE(aux.open(QIODevice::ReadOnly));
    CHECK(auxLabelPage(aux.readAll(), QStringLiteral("D5MN")) == 1);

    // D5MN's body is line 9 of its file.
    SyncTexBox box;
    REQUIRE(synctexForward(SyncTexTool(), folder, QStringLiteral("Statements/D5MN.tex"), 9, 0, box) == SyncTexOutcome::Found);
    CHECK(box.page == 1);
    CHECK(box.width > 0);
    SyncTexPlace place;
    REQUIRE(synctexInverse(SyncTexTool(), folder, box.page, box.x + box.width / 2, box.y + box.height / 2, place) == SyncTexOutcome::Found);
    CHECK(place.file == u"Statements/D5MN.tex");
    CHECK(place.line == 9);

    // A line no build file has: nothing found. Without synctex: Missing.
    CHECK(synctexForward(SyncTexTool(), folder, QStringLiteral("Statements/K3AB.tex"), 9, 0, box) == SyncTexOutcome::NotFound);
    SyncTexTool missing;
    missing.program = QStringLiteral("loom-no-such-synctex");
    CHECK(synctexForward(missing, folder, QStringLiteral("Statements/D5MN.tex"), 9, 0, box) == SyncTexOutcome::Missing);
}
