// Tests for statement previews (Specs/m2-statements-review.md, "IDs in the editor", "Previews": "the
// statement built alone (without proofs) at a small page size, cut to its content", "cached by the
// hash of the statement's copy and the preambles"): what a preview's document holds, and real
// builds of the demo's previews (tagged [tex]). Cutting the picture to its content is the app's.

#include "DemoProject.h"
#include "ProjectFixture.h"
#include "TestSupport.h"

#include "LoomCore/Build.h"
#include "LoomCore/BuildDocument.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProjectToml.h"
#include "LoomCore/Status.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <catch2/catch_test_macros.hpp>

using namespace loom;
using namespace loomtest;

namespace {

/// Skips without latexmk, or fails under LOOM_REQUIRE_TEX=1 (as BuildTests.cpp does).
#define REQUIRE_TEX()                                                                                                        \
    do {                                                                                                                     \
        if (QStandardPaths::findExecutable(QStringLiteral("latexmk")).isEmpty()) {                                         \
            if (qEnvironmentVariableIntValue("LOOM_REQUIRE_TEX") == 1)                                                       \
                FAIL("latexmk not found, and LOOM_REQUIRE_TEX=1");                                                           \
            SKIP("latexmk not found");                                                                                       \
        }                                                                                                                    \
    } while (false)

/// A project read as a build reads it.
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

/// `main.tex` of a document, or an empty string (which every check below then fails).
QString mainOf(const BuildDocument& doc)
{
    for (const BuildFile& file : doc.files) {
        if (file.path == u"main.tex")
            return QString::fromUtf8(file.bytes);
    }
    return {};
}

/// Writes a statement with a title and body.
void writeStatement(const ProjectFixture& fx, const QString& tag, const QByteArray& kind, const QByteArray& title, const QByteArray& body)
{
    fx.write(QStringLiteral("Statements/%1.tex").arg(tag),
             "+++\nid = \"" + tag.toUtf8() + "\"\nkind = \"" + kind + "\"\ntitle = \"" + title + "\"\ncreated = 2026-11-03\n+++\n" + body);
}

/// A preview of the demo.
BuildRequest previewOf(const DemoProject& demo, const QString& tag)
{
    BuildRequest request;
    request.root = demo.root();
    request.target = tag;
    request.preview = true;
    return request;
}

} // namespace

TEST_CASE("A preview is the statement alone: no proofs, no header, no page number")
{
    const ProjectFixture fx;
    writeStatement(fx, QStringLiteral("4KX7"), "theorem", "Energy decay", "Decay, by \\ref{5HN8}.\n");
    writeStatement(fx, QStringLiteral("5HN8"), "lemma", "A bound", "Bound.\n");
    fx.proof(QStringLiteral("4KX7"), QStringLiteral("4KX7.a"), "4KX7", "[\"5HN8\"]");

    const BuildDocument doc = composePreview(sourceOf(fx.root), QStringLiteral("4KX7"));
    REQUIRE(doc.refusal.isEmpty());
    CHECK(doc.preview);
    CHECK(doc.statements == QStringList{"4KX7"});
    CHECK(doc.proofs.isEmpty());
    const QString main = mainOf(doc);
    CHECK(main.contains(QStringLiteral("\\begin{loomstatement}{theorem}{4KX7}{Energy decay}")));
    CHECK(main.contains(QStringLiteral("\\pagestyle{empty}")));
    CHECK_FALSE(main.contains(QStringLiteral("\\loomheader")));
    CHECK_FALSE(main.contains(QStringLiteral("loomproof")));
    // Every other ID is labelled, without its title; the proof isn't in the preview, so it is too.
    CHECK(main.contains(QStringLiteral("\\loomoutside{5HN8}{}\n")));
    CHECK(main.contains(QStringLiteral("\\loomoutside{4KX7.a}{}\n")));
}

TEST_CASE("A preview's inputs change with what it shows, not with other statements' titles")
{
    const ProjectFixture fx;
    writeStatement(fx, QStringLiteral("4KX7"), "theorem", "Energy decay", "Decay.\n");
    writeStatement(fx, QStringLiteral("5HN8"), "lemma", "A bound", "Bound.\n");
    const auto files = [&] { return composePreview(sourceOf(fx.root), QStringLiteral("4KX7")).files.size(); };
    const auto main = [&] { return mainOf(composePreview(sourceOf(fx.root), QStringLiteral("4KX7"))); };
    const QString before = main();
    REQUIRE(files() == 3); // main.tex, the preamble, the copy.

    writeStatement(fx, QStringLiteral("5HN8"), "lemma", "A better bound", "Bound.\n");
    CHECK(main() == before);
    writeStatement(fx, QStringLiteral("4KX7"), "theorem", "Energy decay, sharper", "Decay.\n");
    CHECK(main() != before);
}

TEST_CASE("A stub or an unknown tag has no preview build")
{
    const ProjectFixture fx;
    writeStatement(fx, QStringLiteral("4KX7"), "theorem", "Energy decay", "Decay.\n");
    fx.proof(QStringLiteral("4KX7"), QStringLiteral("4KX7.a"), "4KX7", "[]");
    const BuildSource source = sourceOf(fx.root);
    CHECK(composePreview(source, QStringLiteral("ZZZZ")).refusal == QStringLiteral("ref-unknown"));
    CHECK(composePreview(source, QStringLiteral("4KX7.a")).refusal == QStringLiteral("ref-unknown"));
    CHECK(composePreview(source, QStringLiteral("4kx7")).refusal == QStringLiteral("ref-unknown"));
    CHECK(previewFile(QStringLiteral("4KX7"), QStringLiteral("0123456789abcdef"))
          == QStringLiteral(".loom/build/preview/4KX7-0123456789abcdef.pdf"));
}

TEST_CASE("Previews are built once, kept under their hash, and rebuilt when the statement changes", "[tex]")
{
    REQUIRE_TEX();
    const DemoProject demo;
    INFO(demo.error().toStdString());
    REQUIRE(demo.ok());

    const BuildResult first = buildTarget(previewOf(demo, QStringLiteral("C4JK")));
    INFO(first.message.toStdString());
    REQUIRE(first.outcome == BuildOutcome::Succeeded);
    CHECK_FALSE(first.reused);
    CHECK(first.pdf.startsWith(QStringLiteral(".loom/build/preview/C4JK-")));
    CHECK(first.pdf.endsWith(QStringLiteral(".pdf")));
    CHECK(QFileInfo::exists(demo.root() + u'/' + first.pdf));
    CHECK(first.lastGoodPdf.isEmpty());

    const BuildResult second = buildTarget(previewOf(demo, QStringLiteral("C4JK")));
    CHECK(second.outcome == BuildOutcome::Succeeded);
    CHECK(second.reused);
    CHECK(second.pdf == first.pdf);

    // A change to the statement makes a new preview, and the old one goes.
    const QString c4jk = demo.root() + QStringLiteral("/Statements/C4JK.tex");
    writeFile(c4jk, readFile(c4jk) + "Also, the tails never increase.\n");
    const BuildResult third = buildTarget(previewOf(demo, QStringLiteral("C4JK")));
    REQUIRE(third.outcome == BuildOutcome::Succeeded);
    CHECK_FALSE(third.reused);
    CHECK(third.pdf != first.pdf);
    CHECK(QFileInfo::exists(demo.root() + u'/' + third.pdf));
    CHECK_FALSE(QFileInfo::exists(demo.root() + u'/' + first.pdf));

    // A stub isn't built.
    CHECK(buildTarget(previewOf(demo, QStringLiteral("7RT2"))).code == QStringLiteral("build-stub"));
}
