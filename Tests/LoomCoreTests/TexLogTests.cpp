// Tests for reading LaTeX logs and mapping their problems back to the project
// (Specs/m2-statements-review.md, "Problems from a build"), on logs saved from real builds of the
// demo project (Tests/LoomCoreTests/TexLogs/) and on small made-up ones.

#include "TestSupport.h"

#include "LoomCore/BuildDocument.h"
#include "LoomCore/TexLog.h"

#include <catch2/catch_test_macros.hpp>

using namespace loom;
using namespace loomtest;

namespace {

/// A saved log.
QByteArray savedLog(const char* name)
{
    return readFile(QStringLiteral(LOOM_TEX_LOGS "/") + QString::fromLatin1(name));
}

/// A build document for mapping only: its target, and `main.tex`'s lines owned by the target
/// except those given.
BuildDocument documentFor(const QString& target, const QString& targetFile, int mainLines,
                          const std::vector<std::pair<int, GeneratedPart>>& parts = {})
{
    BuildDocument doc;
    doc.target = target;
    doc.targetFile = targetFile;
    doc.mainLines.assign(static_cast<std::size_t>(mainLines), GeneratedPart{target, targetFile});
    for (const auto& [line, part] : parts)
        doc.mainLines[static_cast<std::size_t>(line - 1)] = part;
    return doc;
}

/// `<file>:<line>: <code>: <message>`, for comparing problems in one go.
QStringList describe(const std::vector<Problem>& problems)
{
    QStringList lines;
    for (const Problem& p : problems)
        lines << QStringLiteral("%1:%2: %3: %4").arg(p.file).arg(p.line).arg(p.code, p.message);
    return lines;
}

} // namespace

TEST_CASE("An unclosed brace, from the latex-error overlay")
{
    const std::vector<TexLogEntry> entries = parseTexLog(savedLog("unclosed-brace.log"));
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].severity == Severity::Error);
    CHECK(entries[0].file == u"src/Statements/K3AB.tex");
    CHECK(entries[0].line == 13);
    CHECK(entries[0].message == u"Missing } inserted.");
    const BuildDocument doc = documentFor(QStringLiteral("K3AB"), QStringLiteral("Statements/K3AB.tex"), 30);
    CHECK(describe(mapTexLog(entries, doc)) == QStringList{"Statements/K3AB.tex:13: tex-error: Missing } inserted."});
}

TEST_CASE("Errors in a preamble, a proof and the generated part; warnings in a statement")
{
    const std::vector<TexLogEntry> entries = parseTexLog(savedLog("errors-and-warnings.log"));
    QStringList raw;
    for (const TexLogEntry& e : entries)
        raw << QStringLiteral("%1 %2:%3 [%4] %5").arg(e.code, e.file).arg(e.line).arg(e.buildFile, e.message);
    CHECK(raw == QStringList{
              "tex-error src/TeX/macros.tex:4 [src/TeX/macros.tex] Undefined control sequence `\\undefinedinpreamble`.",
              "tex-error main.tex:19 [main.tex] Undefined control sequence `\\badtitlemacro`.",
              "tex-error src/Proofs/C4JK/C4JK.a.tex:8 [src/Proofs/C4JK/C4JK.a.tex] Undefined control sequence `\\fracc`.",
              "tex-error main.tex:36 [main.tex] Undefined control sequence `\\badtitlemacro`.",
              "tex-ref-undefined src/Statements/G8TV.tex:13 [src/Statements/G8TV.tex] nowhere",
              "tex-ref-undefined src/Statements/G8TV.tex:13 [src/Statements/G8TV.tex] nowhere",
              "tex-cite-undefined src/Statements/G8TV.tex:13 [src/Statements/G8TV.tex] Nobody99",
              "tex-warning src/Statements/G8TV.tex:13 [src/Statements/G8TV.tex] Package demo Warning: Something odd",
          });

    // main.tex line 36 is G8TV's \begin{loomstatement}; line 19 is the header, the target's.
    const BuildDocument doc = documentFor(QStringLiteral("G8TV"), QStringLiteral("Statements/G8TV.tex"), 45,
                                          {{36, {QStringLiteral("G8TV"), QStringLiteral("Statements/G8TV.tex")}}});
    // With errors, latexmk stopped before references were resolved: those warnings are left out.
    // The title's error is reported once, though LaTeX met it twice.
    CHECK(describe(mapTexLog(entries, doc)) == QStringList{
              "Proofs/C4JK/C4JK.a.tex:8: tex-error: Undefined control sequence `\\fracc`.",
              "Statements/G8TV.tex:1: tex-error-generated: LaTeX stopped in the part Loom writes around G8TV: Undefined control "
              "sequence `\\badtitlemacro`. This is usually a problem in the preamble or a macro used in the title.",
              "TeX/macros.tex:4: tex-error: Undefined control sequence `\\undefinedinpreamble`.",
              "Statements/G8TV.tex:13: tex-warning: Package demo Warning: Something odd",
          });
}

TEST_CASE("hyperref's notes on outside references are left out; a warning after a box keeps its file")
{
    const std::vector<TexLogEntry> entries = parseTexLog(savedLog("hyperref-and-boxes.log"));
    const BuildDocument doc = documentFor(QStringLiteral("K3AB"), QStringLiteral("Statements/K3AB.tex"), 30);
    CHECK(describe(mapTexLog(entries, doc)) == QStringList{"Statements/K3AB.tex:16: tex-warning: Package demo Warning: After the box"});
}

TEST_CASE("Made-up logs: unbalanced text in boxes and errors, continuation lines, ! errors, TeX's own files")
{
    const BuildDocument doc = documentFor(QStringLiteral("4KX7"), QStringLiteral("Statements/4KX7.tex"), 10,
                                          {{5, {QStringLiteral("4KX7.a"), QStringLiteral("Proofs/4KX7/4KX7.a.tex")}}});
    SECTION("an overfull box quoting an unbalanced parenthesis doesn't lose track of the file")
    {
        const QByteArray log = "(./main.tex (./src/Statements/4KX7.tex\n"
                               "Overfull \\hbox (12.0pt too wide) in paragraph at lines 7--8\n"
                               "[]\\OT1/cmr/m/n/10.95 (((((unbalanced\n"
                               "\n"
                               "Package demo Warning: Inside on input line 8.\n"
                               "\n"
                               ") (./src/Proofs/4KX7/4KX7.a.tex\n"
                               "LaTeX Warning: Reference `x' on page 1 undefined on input line 9.\n"
                               "\n"
                               "))\n";
        CHECK(describe(mapTexLog(parseTexLog(log), doc)) == QStringList{
                  "Proofs/4KX7/4KX7.a.tex:9: tex-ref-undefined: `\\ref{x}` points to nothing in this build or the project.",
                  "Statements/4KX7.tex:8: tex-warning: Package demo Warning: Inside",
              });
    }
    SECTION("a package warning over several lines, and a LaTeX error over two")
    {
        const QByteArray log = "(./main.tex (./src/Statements/4KX7.tex\n"
                               "Package natbib Warning: Something long\n"
                               "(natbib)                that goes on on input line 7.\n"
                               "\n"
                               "./src/Statements/4KX7.tex:8: LaTeX Error: Unicode character x (U+0078)\n"
                               "               not set up for use with LaTeX.\n"
                               "\n"
                               "See the LaTeX manual or LaTeX Companion for explanation.\n"
                               "Type  H <return>  for immediate help.\n"
                               " ...                                              \n"
                               "                                                  \n"
                               "l.8 (((\n"
                               "     x\n"
                               "))\n";
        const std::vector<TexLogEntry> entries = parseTexLog(log);
        REQUIRE(entries.size() == 2);
        CHECK(entries[0].message == u"Package natbib Warning: Something long that goes on");
        CHECK(entries[0].line == 7);
        CHECK(entries[1].message == u"Unicode character x (U+0078) not set up for use with LaTeX.");
        CHECK(entries[1].line == 8);
    }
    SECTION("an error without file and line takes them from the stack and the context")
    {
        const QByteArray log = "(./main.tex (./src/Statements/4KX7.tex\n"
                               "! Emergency stop.\n"
                               "<*> main.tex\n"
                               "l.9 \\end\n"
                               "     \n";
        const std::vector<TexLogEntry> entries = parseTexLog(log);
        REQUIRE(entries.size() == 1);
        CHECK(entries[0].file == u"src/Statements/4KX7.tex");
        CHECK(entries[0].line == 9);
        CHECK(entries[0].message == u"Emergency stop.");
    }
    SECTION("an error inside one of TeX's own files points at the build file that loaded it")
    {
        const QByteArray log = "(./main.tex (./src/TeX/preamble.tex (c:/texlive/2026/texmf-dist/tex/latex/foo/foo.sty\n"
                               "c:/texlive/2026/texmf-dist/tex/latex/foo/foo.sty:12: Package foo Error: Broken.\n"
                               "\n"
                               "l.12 \\broken\n"
                               "     \n";
        CHECK(describe(mapTexLog(parseTexLog(log), doc)) == QStringList{"TeX/preamble.tex:1: tex-error: Package foo Error: Broken."});
    }
    SECTION("an error on a proof's generated line is that proof's")
    {
        const QByteArray log = "(./main.tex\n./main.tex:5: Missing $ inserted.\n<inserted text> \n                $\nl.5 x\n";
        CHECK(describe(mapTexLog(parseTexLog(log), doc)) == QStringList{
                  "Proofs/4KX7/4KX7.a.tex:1: tex-error-generated: LaTeX stopped in the part Loom writes around 4KX7.a: Missing $ "
                  "inserted. This is usually a problem in the preamble or a macro used in the title."});
    }
    SECTION("summaries are left out")
    {
        const QByteArray log = "(./main.tex\nLaTeX Warning: There were undefined references.\n\n"
                               "LaTeX Warning: Label(s) may have changed. Rerun to get cross-references right.\n\n"
                               "LaTeX Font Warning: Some font shapes were not available, defaults substituted.\n)\n";
        CHECK(parseTexLog(log).empty());
    }
}

TEST_CASE("Problems are sorted errors first and de-duplicated")
{
    std::vector<Problem> problems = {
        {Severity::Warning, QStringLiteral("tex-warning"), QStringLiteral("B.tex"), 2, QStringLiteral("w")},
        {Severity::Error, QStringLiteral("tex-error"), QStringLiteral("B.tex"), 5, QStringLiteral("e")},
        {Severity::Warning, QStringLiteral("tex-warning"), QStringLiteral("B.tex"), 2, QStringLiteral("w")},
        {Severity::Error, QStringLiteral("tex-error"), QStringLiteral("A.tex"), 9, QStringLiteral("e")},
    };
    sortUniqueProblems(problems);
    CHECK(describe(problems) == QStringList{"A.tex:9: tex-error: e", "B.tex:5: tex-error: e", "B.tex:2: tex-warning: w"});
}
