// Tests for composing a build (Specs/m2-statements-review.md, "Building LaTeX"): the wrapper
// document for each kind and mode, verbose versions, the line-preserving copies, `\paperref` and
// `\cite`, references outside the build, and refusals. No TeX is needed.

#include "DemoProject.h"
#include "PaperFixture.h"
#include "ProjectFixture.h"
#include "TestSupport.h"

#include "LoomCore/BuildDocument.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProjectToml.h"
#include "LoomCore/Status.h"

#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace loom;
using namespace loomtest;

namespace {

/// A project read as a build reads it. Reads files only.
BuildSource sourceOf(const QString& root)
{
    BuildSource source;
    source.root = root;
    source.config = openProject(root).config;
    ContentsLoading loading = loadContents(root);
    REQUIRE(loading.ok);
    source.contents = std::move(loading.contents);
    source.graph = buildGraph(source.contents);
    return source;
}

/// The bytes of one file of a build, or an empty optional.
std::optional<QByteArray> fileOf(const BuildDocument& doc, const QString& path)
{
    for (const BuildFile& file : doc.files) {
        if (file.path == path)
            return file.bytes;
    }
    return std::nullopt;
}

/// `main.tex` as text.
QString mainOf(const BuildDocument& doc)
{
    const std::optional<QByteArray> bytes = fileOf(doc, QStringLiteral("main.tex"));
    REQUIRE(bytes);
    return QString::fromUtf8(*bytes);
}

/// The codes of a build's problems, in order.
QStringList codesOf(const BuildDocument& doc)
{
    QStringList codes;
    for (const Problem& p : doc.problems)
        codes << p.code;
    return codes;
}

/// Writes a statement with a given title and body; `extra` lines go after `title`.
void writeStatement(const ProjectFixture& fx, const QString& tag, const QByteArray& kind, const QByteArray& title,
                    const QByteArray& body, const QByteArray& extra = {})
{
    fx.write(QStringLiteral("Statements/%1.tex").arg(tag), "+++\nid = \"" + tag.toUtf8() + "\"\nkind = \"" + kind + "\"\ntitle = \""
                                                                + title + "\"\n" + extra + "created = 2026-11-03\n+++\n" + body);
}

/// Writes a stub of SmithJones24 with one locator.
void writeStub(const ProjectFixture& fx, const QString& tag, const QByteArray& version, const QByteArray& at,
               const QByteArray& checked, const QByteArray& extra = {})
{
    fx.write(QStringLiteral("Papers/SmithJones24/Statements/%1.toml").arg(tag),
             "id = \"" + tag.toUtf8() + "\"\nkind = \"theorem\"\ngist = \"A result\"\n" + extra + "extracted_from = \"" + version
                 + "\"\nextracted_by = \"dakota\"\ncreated = 2026-11-03\n\n[locators]\n" + version + " = { at = \"" + at
                 + "\", checked_against_pdf = " + checked + " }\n");
}

/// The project of the spec's example: 4KX7 is a theorem stated using the definition 2M9Q, with
/// one complete proof 4KX7.a from premises 5HN8 and 9D3F, and 9D3F's text cites the paper
/// result 7RT2.
struct SpecExample : ProjectFixture {
    SpecExample()
    {
        write(QStringLiteral("loom.toml"),
              "format = 1\nname = \"Demo\"\ncreated = 2026-11-03\n\n[tex]\npreamble = [\"TeX/preamble.tex\", \"TeX/macros.tex\"]\n\n"
              "[[tags]]\nid = \"model-system\"\nkind = \"equation\"\ntitle = \"The model system\"\n");
        write(QStringLiteral("TeX/macros.tex"), "% macros\n");
        writeStatement(*this, QStringLiteral("2M9Q"), "definition", "Weak solution of the model system", "Definition.\n");
        writeStatement(*this, QStringLiteral("5HN8"), "lemma", "A uniform bound for the energy", "Bound.\n");
        writeStatement(*this, QStringLiteral("9D3F"), "lemma", "Decay of the tail", "As in \\paperref{7RT2}, the tail decays.\n");
        writeStatement(*this, QStringLiteral("6BWT"), "lemma", "A conditional decay estimate", "Estimate.\n");
        writeStatement(*this, QStringLiteral("4KX7"), "theorem", "Energy decay for the model system", "Decay.\n",
                       "stated_using = [\"2M9Q\"]\n");
        proof(QStringLiteral("4KX7"), QStringLiteral("4KX7.a"), "4KX7", "[\"5HN8\", \"9D3F\"]");
        write(QStringLiteral("Papers/SmithJones24/paper.toml"), smithJones24());
        writeStub(*this, QStringLiteral("7RT2"), "published", "Theorem 1.2", "2026-11-03");
    }
};

} // namespace

TEST_CASE("The wrapper of the spec's example, mode premises")
{
    const SpecExample fx;
    const BuildDocument doc = composeBuild(sourceOf(fx.root), QStringLiteral("4KX7"), BuildMode::Premises, false);
    REQUIRE(doc.refusal.isEmpty());
    // As the spec's example, without its two explanatory comments; the outside labels are in tag order.
    CHECK(mainOf(doc) == QStringLiteral(
              "% Written by Loom for 4KX7, mode premises. Don't edit: Loom rewrites it on every build.\n"
              "\\documentclass[11pt]{article}\n"
              "\\input{src/TeX/preamble.tex}\n"
              "\\input{src/TeX/macros.tex}\n"
              "\\usepackage{loom}\n"
              "\\loompaperref{7RT2}{SmithJones24}{Theorem 1.2}\n"
              "\\loomoutside{6BWT}{A conditional decay estimate}\n"
              "\\loomoutside{7RT2}{SmithJones24, Theorem 1.2}\n"
              "\\begin{document}\n"
              "\\loomheader{4KX7 \u00b7 Energy decay for the model system}{with its direct premises}\n"
              "\n"
              "\\loomsection{Stated using}\n"
              "\\begin{loomstatement}{definition}{2M9Q}{Weak solution of the model system}\n"
              "\\input{src/Statements/2M9Q.tex}\n"
              "\\end{loomstatement}\n"
              "\n"
              "\\loomsection{Premises}\n"
              "\\begin{loomstatement}{lemma}{5HN8}{A uniform bound for the energy}\n"
              "\\input{src/Statements/5HN8.tex}\n"
              "\\end{loomstatement}\n"
              "\\begin{loomstatement}{lemma}{9D3F}{Decay of the tail}\n"
              "\\input{src/Statements/9D3F.tex}\n"
              "\\end{loomstatement}\n"
              "\n"
              "\\loomsection{The statement}\n"
              "\\begin{loomstatement}{theorem}{4KX7}{Energy decay for the model system}\n"
              "\\input{src/Statements/4KX7.tex}\n"
              "\\end{loomstatement}\n"
              "\\begin{loomproof}{4KX7.a}{complete}{5HN8, 9D3F}\n"
              "\\input{src/Proofs/4KX7/4KX7.a.tex}\n"
              "\\end{loomproof}\n"
              "\n"
              "\\bibliographystyle{amsplain}\n"
              "\\bibliography{refs}\n"
              "\\end{document}\n"));
    CHECK(doc.statements == QStringList{"2M9Q", "5HN8", "9D3F", "4KX7"});
    CHECK(doc.proofs == QStringList{"4KX7.a"});
    CHECK(doc.problems.empty());

    // The files: main.tex, the preambles, the copies, refs.bib.
    QStringList paths;
    for (const BuildFile& file : doc.files)
        paths << file.path;
    CHECK(paths == QStringList{"main.tex", "src/TeX/preamble.tex", "src/TeX/macros.tex", "src/Statements/2M9Q.tex",
                               "src/Statements/5HN8.tex", "src/Statements/9D3F.tex", "src/Statements/4KX7.tex",
                               "src/Proofs/4KX7/4KX7.a.tex", "refs.bib"});
    CHECK(fileOf(doc, QStringLiteral("refs.bib"))->contains("@article{SmithJones24,"));

    // Each line of main.tex belongs to a part: the statement's lines to it, the proof's to it.
    const QStringList lines = mainOf(doc).split(u'\n');
    REQUIRE(doc.mainLines.size() == static_cast<std::size_t>(lines.size() - 1));
    const auto ownerOf = [&](const QString& text) {
        const qsizetype i = lines.indexOf(text);
        REQUIRE(i >= 0);
        return doc.mainLines[static_cast<std::size_t>(i)];
    };
    CHECK(ownerOf(QStringLiteral("\\begin{loomstatement}{lemma}{9D3F}{Decay of the tail}")).file == u"Statements/9D3F.tex");
    CHECK(ownerOf(QStringLiteral("\\input{src/Proofs/4KX7/4KX7.a.tex}")).id == u"4KX7.a");
    CHECK(ownerOf(QStringLiteral("\\loomoutside{6BWT}{A conditional decay estimate}")).id == u"6BWT");
    CHECK(ownerOf(QStringLiteral("\\loompaperref{7RT2}{SmithJones24}{Theorem 1.2}")).file == u"Papers/SmithJones24/Statements/7RT2.toml");
    CHECK(ownerOf(QStringLiteral("\\documentclass[11pt]{article}")).id == u"4KX7");
}

TEST_CASE("Every kind gets its environment, and the alone mode has no section headings")
{
    for (const QString& kind : statementKinds) {
        CAPTURE(kind);
        const ProjectFixture fx;
        fx.statement(QStringLiteral("4KX7"), kind.toUtf8());
        const BuildDocument doc = composeBuild(sourceOf(fx.root), QStringLiteral("4KX7"), BuildMode::Alone, false);
        const QString main = mainOf(doc);
        CHECK(main.contains(QStringLiteral("\\begin{loomstatement}{%1}{4KX7}{T}\n\\input{src/Statements/4KX7.tex}\n").arg(kind)));
        CHECK(main.contains(QStringLiteral("\\loomheader{4KX7 \u00b7 T}{alone}")));
        CHECK_FALSE(main.contains(QStringLiteral("\\loomsection")));
        CHECK_FALSE(main.contains(QStringLiteral("\\bibliography")));
    }
}

TEST_CASE("A statement alone shows all its proofs in letter order, failed attempts last")
{
    const ProjectFixture fx;
    fx.statement(QStringLiteral("4KX7"), "lemma");
    fx.proof(QStringLiteral("4KX7"), QStringLiteral("4KX7.a"), "4KX7", "[]", "failed");
    fx.proof(QStringLiteral("4KX7"), QStringLiteral("4KX7.b"), "4KX7", "[]", "complete");
    fx.proof(QStringLiteral("4KX7"), QStringLiteral("4KX7.c"), "4KX7", "[]", "sketch");
    const BuildDocument doc = composeBuild(sourceOf(fx.root), QStringLiteral("4KX7"), BuildMode::Alone, false);
    CHECK(doc.proofs == QStringList{"4KX7.b", "4KX7.c", "4KX7.a"});
    CHECK(mainOf(doc).contains(QStringLiteral("\\begin{loomproof}{4KX7.a}{failed}{}\n")));
}

TEST_CASE("A proof target shows its statement and that proof only")
{
    const ProjectFixture fx;
    fx.statement(QStringLiteral("4KX7"), "lemma");
    fx.proof(QStringLiteral("4KX7"), QStringLiteral("4KX7.a"), "4KX7", "[]");
    fx.proof(QStringLiteral("4KX7"), QStringLiteral("4KX7.b"), "4KX7", "[]");
    const BuildDocument doc = composeBuild(sourceOf(fx.root), QStringLiteral("4kx7.B"), BuildMode::Alone, false);
    CHECK(doc.target == u"4KX7.b");
    CHECK(doc.targetFile == u"Proofs/4KX7/4KX7.b.tex");
    CHECK(doc.statements == QStringList{"4KX7"});
    CHECK(doc.proofs == QStringList{"4KX7.b"});
    const QString main = mainOf(doc);
    CHECK(main.contains(QStringLiteral("\\loomheader{4KX7.b \u00b7 T}{alone}")));
    CHECK(main.contains(QStringLiteral("\\loomoutside{4KX7.a}{Proof 4KX7.a of 4KX7}")));
    CHECK_FALSE(main.contains(QStringLiteral("\\loomoutside{4KX7.b}")));
}

TEST_CASE("The demo's chain: deciding proofs, stubs as stubs, dependency order")
{
    const BuildSource source = sourceOf(QStringLiteral(LOOM_DEMO_PROJECT));
    const BuildDocument doc = composeBuild(source, QStringLiteral("E6PQ"), BuildMode::Chain, false);
    REQUIRE(doc.refusal.isEmpty());
    // 7RT2 sorts first by tag; D5MN comes after C4JK, which its proof is from.
    CHECK(doc.statements == QStringList{"7RT2", "A2DF", "B3GH", "C4JK", "D5MN", "E6PQ"});
    CHECK(doc.proofs == QStringList{"C4JK.a", "D5MN.a", "E6PQ.a"});
    const QString main = mainOf(doc);
    CHECK(main.contains(QStringLiteral("\\loomsection{The chain}\n\\begin{loomstatement}{theorem}{7RT2}{SmithJones24, Theorem 1.2}\n"
                                       "For every p at least 1, the termwise product of a p-summable sequence and a bounded sequence is "
                                       "p-summable\n\\end{loomstatement}\n")));
    CHECK(main.contains(QStringLiteral("\\begin{loomstatement}{definition}{A2DF}{Tame sequence}\n\\input{src/Statements/A2DF.tex}\n"
                                       "\\end{loomstatement}\n\\begin{loomstatement}{definition}{B3GH}")));
    CHECK(main.contains(QStringLiteral("\\loomsection{The statement}\n")));
    CHECK(main.contains(QStringLiteral("\\loompaperref{7RT2}{SmithJones24}{Theorem 1.2}\n")));
    CHECK(doc.problems.empty());

    SECTION("premises mode: what it's stated using, then the proofs' premises, without their proofs")
    {
        const BuildDocument premises = composeBuild(source, QStringLiteral("F7RS"), BuildMode::Premises, false);
        CHECK(premises.statements == QStringList{"A2DF", "3WQ6", "F7RS"});
        CHECK(premises.proofs == QStringList{"F7RS.a"});
        // The cited version has no locator for 3WQ6.
        CHECK(codesOf(premises) == QStringList{"paperref-no-locator"});
        CHECK(premises.problems[0].file == u"Proofs/F7RS/F7RS.a.tex");
        CHECK(premises.problems[0].line == 8);
        CHECK(mainOf(premises).contains(QStringLiteral("\\loompaperref{3WQ6}{SmithJones24}{}")));
        CHECK(mainOf(premises).contains(QStringLiteral("{SmithJones24, arxiv-v1: Lemma 2.1}")));
    }
    SECTION("a statement without a deciding proof comes without one")
    {
        const BuildDocument chain = composeBuild(source, QStringLiteral("H9WX"), BuildMode::Chain, false);
        CHECK(chain.statements == QStringList{"7RT2", "A2DF", "B3GH", "C4JK", "D5MN", "E6PQ", "H9WX"});
        CHECK(chain.proofs == QStringList{"C4JK.a", "D5MN.a", "E6PQ.a", "H9WX.a"});
    }
}

TEST_CASE("A circle of premises in the chain is placed in tag order and warns build-cycle")
{
    const ProjectFixture fx;
    fx.statement(QStringLiteral("4KX7"), "theorem");
    fx.statement(QStringLiteral("H8KD"), "lemma");
    fx.statement(QStringLiteral("J3RW"), "lemma");
    fx.statement(QStringLiteral("2M9Q"), "lemma");
    fx.proof(QStringLiteral("4KX7"), QStringLiteral("4KX7.a"), "4KX7", "[\"J3RW\", \"2M9Q\"]");
    fx.proof(QStringLiteral("J3RW"), QStringLiteral("J3RW.a"), "J3RW", "[\"H8KD\"]");
    fx.proof(QStringLiteral("H8KD"), QStringLiteral("H8KD.a"), "H8KD", "[\"J3RW\", \"2M9Q\"]");
    fx.proof(QStringLiteral("2M9Q"), QStringLiteral("2M9Q.a"), "2M9Q", "[]");
    const BuildDocument doc = composeBuild(sourceOf(fx.root), QStringLiteral("4KX7"), BuildMode::Chain, false);
    // 2M9Q first (the circle needs it), then the circle in tag order.
    CHECK(doc.statements == QStringList{"2M9Q", "H8KD", "J3RW", "4KX7"});
    REQUIRE(codesOf(doc) == QStringList{"build-cycle"});
    CHECK(doc.problems[0].message
          == QStringLiteral("Premises go round in a circle (H8KD \u2192 J3RW \u2192 H8KD); the build lists them in tag order."));
    CHECK(doc.problems[0].file == u"Statements/4KX7.tex");
}

TEST_CASE("Verbose versions replace proofs; a stale one says so")
{
    QTemporaryDir temp;
    REQUIRE(copyFolder(QStringLiteral(LOOM_DEMO_PROJECT), temp.path()).isEmpty());
    BuildDocument doc = composeBuild(sourceOf(temp.path()), QStringLiteral("E6PQ"), BuildMode::Alone, true);
    CHECK(mainOf(doc).contains(QStringLiteral("\\begin{loomproof}[verbose]{E6PQ.a}{complete}{D5MN, 7RT2}\n"
                                              "\\input{src/Proofs/E6PQ/E6PQ.a.verbose.tex}\n")));
    CHECK(mainOf(doc).contains(QStringLiteral("\\loomheader{E6PQ \u00b7 Products with bounded sequences stay tame}{alone, verbose}")));
    CHECK(fileOf(doc, QStringLiteral("src/Proofs/E6PQ/E6PQ.a.verbose.tex")));
    CHECK_FALSE(fileOf(doc, QStringLiteral("src/Proofs/E6PQ/E6PQ.a.tex")));
    CHECK(doc.problems.empty());

    // Without verbose, the proof itself.
    doc = composeBuild(sourceOf(temp.path()), QStringLiteral("E6PQ"), BuildMode::Alone, false);
    CHECK(mainOf(doc).contains(QStringLiteral("\\begin{loomproof}{E6PQ.a}{complete}{D5MN, 7RT2}\n\\input{src/Proofs/E6PQ/E6PQ.a.tex}\n")));

    // A proof without a verbose version is shown as it is.
    doc = composeBuild(sourceOf(temp.path()), QStringLiteral("D5MN"), BuildMode::Alone, true);
    CHECK(mainOf(doc).contains(QStringLiteral("\\begin{loomproof}{D5MN.a}{complete}{C4JK}\n\\input{src/Proofs/D5MN/D5MN.a.tex}\n")));

    // Stale: written from an older version of the proof.
    const QString verbose = temp.filePath(QStringLiteral("Proofs/E6PQ/E6PQ.a.verbose.tex"));
    QByteArray bytes = readFile(verbose);
    const qsizetype at = bytes.indexOf("fingerprint = \"") + 15;
    bytes.replace(at, 12, "000000000000");
    writeFile(verbose, bytes);
    doc = composeBuild(sourceOf(temp.path()), QStringLiteral("E6PQ"), BuildMode::Alone, true);
    CHECK(mainOf(doc).contains(QStringLiteral("\\begin{loomproof}[stale]{E6PQ.a}{complete}{D5MN, 7RT2}\n")));
    REQUIRE(codesOf(doc) == QStringList{"verbose-stale"});
    CHECK(doc.problems[0].file == u"Proofs/E6PQ/E6PQ.a.verbose.tex");
    CHECK(doc.problems[0].line == 3);
    CHECK(doc.problems[0].message == u"The verbose version of E6PQ.a was written from an older version of the proof.");
}

TEST_CASE("Copies keep every line where it was")
{
    CHECK(lineCopy("Body\n", 7) == "%\n%\n%\n%\n%\n%\nBody\n");
    CHECK(lineCopy("Body\n", 1) == "Body\n");

    const ProjectFixture fx;
    SECTION("CRLF, and a +++ line in the body")
    {
        const QByteArray source = "+++\r\nid = \"4KX7\"\r\nkind = \"lemma\"\r\ntitle = \"T\"\r\ncreated = 2026-11-03\r\n+++\r\n"
                                  "Line 7\r\n+++\r\nLine 9\r\n";
        fx.write(QStringLiteral("Statements/4KX7.tex"), source);
        const BuildDocument doc = composeBuild(sourceOf(fx.root), QStringLiteral("4KX7"), BuildMode::Alone, false);
        const QByteArray copy = *fileOf(doc, QStringLiteral("src/Statements/4KX7.tex"));
        CHECK(copy == "%\n%\n%\n%\n%\n%\nLine 7\n+++\nLine 9\n");
        const QList<QByteArray> sourceLines = source.split('\n');
        const QList<QByteArray> copyLines = copy.split('\n');
        REQUIRE(copyLines.size() == sourceLines.size());
        for (qsizetype i = 6; i < sourceLines.size(); ++i)
            CHECK(copyLines[i] == QByteArray(sourceLines[i]).replace('\r', ""));
        // The source isn't touched.
        CHECK(fx.read(QStringLiteral("Statements/4KX7.tex")) == source);
    }
    SECTION("a byte-order mark")
    {
        fx.write(QStringLiteral("Statements/4KX7.tex"),
                 "\xEF\xBB\xBF+++\nid = \"4KX7\"\nkind = \"lemma\"\ntitle = \"T\"\ncreated = 2026-11-03\n+++\nLine 7\n");
        const BuildDocument doc = composeBuild(sourceOf(fx.root), QStringLiteral("4KX7"), BuildMode::Alone, false);
        CHECK(*fileOf(doc, QStringLiteral("src/Statements/4KX7.tex")) == "%\n%\n%\n%\n%\n%\nLine 7\n");
    }
    SECTION("preambles whole")
    {
        fx.write(QStringLiteral("TeX/preamble.tex"), "\\usepackage{amsmath}\r\n% keep\r\n");
        fx.statement(QStringLiteral("4KX7"), "lemma");
        const BuildDocument doc = composeBuild(sourceOf(fx.root), QStringLiteral("4KX7"), BuildMode::Alone, false);
        CHECK(*fileOf(doc, QStringLiteral("src/TeX/preamble.tex")) == "\\usepackage{amsmath}\r\n% keep\r\n");
    }
}

TEST_CASE("\\paperref: a locator, none, unchecked, unknown, and an old name")
{
    const ProjectFixture fx;
    fx.write(QStringLiteral("Papers/SmithJones24/paper.toml"), smithJones24());
    writeStub(fx, QStringLiteral("7RT2"), "published", "Theorem 1.2", "2026-11-03", "aliases = [\"thm:product\"]\n");
    writeStub(fx, QStringLiteral("3WQ6"), "arxiv-v1", "Lemma 2.1", "false");
    writeStub(fx, QStringLiteral("8VRS"), "published", "Lemma 3", "false");
    writeStatement(fx, QStringLiteral("4KX7"), "lemma", "T",
                   "By \\paperref{7RT2} and \\paperref{3WQ6},\n\\paperref{8VRS} and \\paperref{9ZZZ},\nand \\paperref{thm:product}.\n");
    const BuildDocument doc = composeBuild(sourceOf(fx.root), QStringLiteral("4KX7"), BuildMode::Alone, false);
    const QString main = mainOf(doc);
    CHECK(main.contains(QStringLiteral("\\loompaperref{3WQ6}{SmithJones24}{}\n"
                                       "\\loompaperref{7RT2}{SmithJones24}{Theorem 1.2}\n"
                                       "\\loompaperref{thm:product}{SmithJones24}{Theorem 1.2}\n"
                                       "\\loompaperref{8VRS}{SmithJones24}{Lemma 3}\n")));
    CHECK_FALSE(main.contains(QStringLiteral("9ZZZ")));
    CHECK(main.contains(QStringLiteral("\\bibliography{refs}")));

    REQUIRE(doc.problems.size() == 3);
    // The body starts on line 7.
    CHECK(doc.problems[0].code == u"paperref-no-locator");
    CHECK(doc.problems[0].line == 7);
    CHECK(doc.problems[0].message
          == u"SmithJones24's cited version `published` has no locator for 3WQ6, so the citation has no theorem number.");
    CHECK(doc.problems[1].code == u"paperref-unchecked");
    CHECK(doc.problems[1].line == 8);
    CHECK(doc.problems[1].message == u"The locator \"Lemma 3\" of 8VRS in SmithJones24's `published` isn't checked against its PDF yet.");
    CHECK(doc.problems[2].code == u"paperref-unknown");
    CHECK(doc.problems[2].line == 8);
    CHECK(doc.problems[2].message == u"`\\paperref{9ZZZ}` doesn't name a paper's result.");
    for (const Problem& p : doc.problems) {
        CHECK(p.severity == Severity::Warning);
        CHECK(p.file == u"Statements/4KX7.tex");
    }
}

TEST_CASE("\\cite keys: old and wrong-case keys become current in the copies only; refs.bib has what's cited")
{
    const ProjectFixture fx;
    fx.write(QStringLiteral("Papers/SmithJones24/paper.toml"), smithJones24());
    fx.write(QStringLiteral("Papers/Brown20/paper.toml"), QByteArray(smithJones24()).replace("SmithJones24", "Brown20").replace("SmithJones23", "Brown19"));
    const QByteArray body = "See \\cite{SmithJones23} and \\cite[p.~3]{smithjones24, Unknown99}. % \\cite{SmithJones23}\n"
                            "\\nocite{SMITHJONES23}\n";
    writeStatement(fx, QStringLiteral("4KX7"), "lemma", "T", body);
    const BuildDocument doc = composeBuild(sourceOf(fx.root), QStringLiteral("4KX7"), BuildMode::Alone, false);
    CHECK(*fileOf(doc, QStringLiteral("src/Statements/4KX7.tex"))
          == "%\n%\n%\n%\n%\n%\n"
             "See \\cite{SmithJones24} and \\cite[p.~3]{SmithJones24, Unknown99}. % \\cite{SmithJones23}\n"
             "\\nocite{SmithJones24}\n");
    CHECK(fx.read(QStringLiteral("Statements/4KX7.tex")).endsWith(body));
    const QByteArray bib = *fileOf(doc, QStringLiteral("refs.bib"));
    CHECK(bib.contains("@article{SmithJones24,"));
    CHECK_FALSE(bib.contains("Brown20"));

    SECTION("\\nocite{*} cites every paper")
    {
        writeStatement(fx, QStringLiteral("4KX7"), "lemma", "T", "\\nocite{*}\n");
        const BuildDocument all = composeBuild(sourceOf(fx.root), QStringLiteral("4KX7"), BuildMode::Alone, false);
        CHECK(fileOf(all, QStringLiteral("refs.bib"))->contains("@article{Brown20,"));
    }
    SECTION("a paper with errors is left out, as a warning")
    {
        fx.write(QStringLiteral("Papers/SmithJones24/paper.toml"), smithJones24() + "unknown_field = 1\n");
        const BuildDocument skipped = composeBuild(sourceOf(fx.root), QStringLiteral("4KX7"), BuildMode::Alone, false);
        REQUIRE(codesOf(skipped).contains(QStringLiteral("bib-entry-skipped")));
        for (const Problem& p : skipped.problems)
            CHECK(p.severity == Severity::Warning);
        CHECK_FALSE(fileOf(skipped, QStringLiteral("refs.bib"))->contains("SmithJones24"));
    }
}

TEST_CASE("Everything outside the build gets a label: tags, label-safe aliases, proofs")
{
    const ProjectFixture fx;
    fx.statement(QStringLiteral("4KX7"), "lemma", "aliases = [\"thm:decay\", \"bad%alias\"]\n");
    fx.statement(QStringLiteral("5HN8"), "lemma", "aliases = [\"lem:bound\"]\n");
    fx.proof(QStringLiteral("4KX7"), QStringLiteral("4KX7.a"), "4KX7", "[]");
    const BuildDocument doc = composeBuild(sourceOf(fx.root), QStringLiteral("5HN8"), BuildMode::Alone, false);
    const QString main = mainOf(doc);
    CHECK(main.contains(QStringLiteral("\\loomoutside{4KX7}{T}\n\\loomoutside[4KX7]{thm:decay}{T}\n\\loomoutside{4KX7.a}{Proof 4KX7.a of 4KX7}\n")));
    CHECK_FALSE(main.contains(QStringLiteral("bad%alias")));
    // In the build: labelled by its environment, aliases included.
    CHECK(main.contains(QStringLiteral("\\begin{loomstatement}[lem:bound]{lemma}{5HN8}{T}\n")));
    CHECK_FALSE(main.contains(QStringLiteral("\\loomoutside{5HN8}")));
}

TEST_CASE("Refusals and files Loom can't read")
{
    const ProjectFixture fx;
    fx.write(QStringLiteral("Papers/SmithJones24/paper.toml"), smithJones24());
    writeStub(fx, QStringLiteral("7RT2"), "published", "Theorem 1.2", "2026-11-03");
    fx.statement(QStringLiteral("4KX7"), "theorem");
    fx.proof(QStringLiteral("4KX7"), QStringLiteral("4KX7.a"), "4KX7", "[\"5HN8\"]");

    BuildDocument doc = composeBuild(sourceOf(fx.root), QStringLiteral("7RT2"), BuildMode::Alone, false);
    CHECK(doc.refusal == u"build-stub");
    CHECK(doc.refusalMessage == u"7RT2 is a result from SmithJones24; it's built only as part of a statement's chain.");
    doc = composeBuild(sourceOf(fx.root), QStringLiteral("9ZZZ"), BuildMode::Alone, false);
    CHECK(doc.refusal == u"ref-unknown");
    CHECK(doc.refusalMessage == u"`9ZZZ` isn't a statement or proof in this project.");

    // A premise whose front matter doesn't parse stops the build.
    fx.write(QStringLiteral("Statements/5HN8.tex"), "+++\nid = \n+++\nBody.\n");
    doc = composeBuild(sourceOf(fx.root), QStringLiteral("4KX7"), BuildMode::Premises, false);
    CHECK(doc.refusal.isEmpty());
    CHECK(doc.unreadable());
    CHECK(doc.files.empty());
    REQUIRE(codesOf(doc) == QStringList{"build-unreadable"});
    CHECK(doc.problems[0].file == u"Statements/5HN8.tex");
    CHECK(doc.problems[0].message == u"Statements/5HN8.tex can't be built: Loom can't read its front matter (`loom validate` says why).");
    // Alone, the premise isn't read, so the build goes ahead.
    doc = composeBuild(sourceOf(fx.root), QStringLiteral("4KX7"), BuildMode::Alone, false);
    CHECK_FALSE(doc.unreadable());
}

TEST_CASE("Text that isn't LaTeX is escaped, math kept")
{
    CHECK(escapeLatexText(QStringLiteral("p >= 1 & 50% of $x_1$ ~ #_^\\{}|<")) ==
          QStringLiteral("p \\textgreater{}= 1 \\& 50\\% of $x_1$ \\textasciitilde{} \\#\\_\\textasciicircum{}\\textbackslash{}\\{\\}"
                         "\\textbar{}\\textless{}"));
    CHECK(escapeLatexText(QStringLiteral("costs $5")) == QStringLiteral("costs \\$5"));
    CHECK(escapeLatexText(QStringLiteral("$x$"), false) == QStringLiteral("\\$x\\$"));
    CHECK(escapeLatexText(QStringLiteral("a\tb")) == QStringLiteral("a b"));
}

TEST_CASE("Mode names")
{
    CHECK(buildModeName(BuildMode::Premises) == u"premises");
    CHECK(buildModeFromName(QStringLiteral("chain")) == BuildMode::Chain);
    CHECK_FALSE(buildModeFromName(QStringLiteral("Chain")));
    CHECK(loomStyle().contains("\\ProvidesPackage{loom}"));
}
