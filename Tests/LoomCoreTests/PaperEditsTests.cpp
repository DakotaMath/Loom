// Papers in a project (Specs/m1-core.md, "Papers", "IDs", "Validation" and "The `loom`
// command"): the problems that need several files, stubs as nodes of the graph, and what
// `loom new-paper`, `new-stub`, `rekey` and `build-bib` do.

// Shows the problem tuples in failure messages.
#define CATCH_CONFIG_ENABLE_TUPLE_STRINGMAKER

#include "LoomCore/PaperEdits.h"

#include "LoomCore/Fingerprint.h"
#include "LoomCore/Ids.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProofChecks.h"

#include "PaperFixture.h"
#include "ProjectFixture.h"

#include <QDir>
#include <QFileInfo>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <tuple>
#include <vector>

using loomtest::ProjectFixture;
using loomtest::smithJones24;
using loomtest::stubToml;

namespace {

using P = std::tuple<QString, QString, int, QString, bool>; // file, code, line, message, is an error

struct Options {
    QRandomGenerator random{20261103};
    loom::ChangeOptions options;

    Options()
    {
        options.today = QDate(2026, 11, 4);
        options.random = &random;
    }
};

/// The tag the seeded generator draws first in an empty project.
QString firstTag()
{
    QRandomGenerator random(20261103);
    return *loom::generateTag([](const QString&) { return false; }, random);
}

std::vector<P> problemsOf(const ProjectFixture& project)
{
    const loom::OpenedProject opened = loom::openProject(project.root);
    REQUIRE(opened.status == loom::OpenStatus::Opened);
    const loom::ContentsLoading loading = loom::loadContents(project.root);
    REQUIRE(loading.ok);
    std::vector<P> out;
    for (const loom::Problem& problem : loom::checkContents(opened.config, loading.contents))
        out.emplace_back(problem.file, problem.code, problem.line, problem.message, problem.severity == loom::Severity::Error);
    return out;
}

/// SmithJones24 with its reading session, and its two stubs: 7RT2 cited, 3WQ6 extracted.
void writeSmithJones24(const ProjectFixture& project)
{
    project.write("Papers/SmithJones24/paper.toml", smithJones24());
    QDir().mkpath(project.root + "/Papers/SmithJones24/Sessions/2026-10-18");
    project.write("Papers/SmithJones24/Statements/7RT2.toml", stubToml("7RT2", "theorem", true, "paper_premises = [\"3WQ6\"]\n"));
    project.write("Papers/SmithJones24/Statements/3WQ6.toml", stubToml("3WQ6", "lemma", false));
}

/// A draft: the entry without the lines starting with any of `drop`.
QByteArray without(const QByteArray& text, const QList<QByteArray>& drop)
{
    QList<QByteArray> kept;
    for (const QByteArray& line : text.split('\n')) {
        if (std::none_of(drop.begin(), drop.end(), [&](const QByteArray& d) { return line.startsWith(d); }))
            kept << line;
    }
    return kept.join('\n');
}

/// SmithJones23: the arXiv preprint of SmithJones24, before it was published.
QByteArray preprint()
{
    QByteArray text = smithJones24();
    text = text.left(text.indexOf("\n[[versions]]\nid = \"published\""));
    text.replace("key = \"SmithJones24\"\naliases = [\"SmithJones23\"]\ncite = \"published\"",
                 "key = \"SmithJones23\"\ncite = \"arxiv-v1\"");
    return text;
}

/// The published version's table, as smithJones24() has it.
QByteArray publishedVersion()
{
    const QByteArray text = smithJones24();
    const qsizetype start = text.indexOf("\n[[versions]]\nid = \"published\"");
    return text.mid(start, text.indexOf("\n[[reading]]") - start);
}

} // namespace

TEST_CASE("A project with a paper and its stubs reads without errors", "[papers][contents]")
{
    const ProjectFixture project;
    writeSmithJones24(project);
    project.statement("2M9Q", "definition");
    project.statement("9D3F", "lemma", "stated_using = [\"7RT2\"]\n");
    project.proof("9D3F", "9D3F.a", "9D3F", "[\"7RT2\", \"3WQ6\"]");
    project.write("Papers/SmithJones24/notes.md", "Notes.\n");
    project.write("Papers/SmithJones24/local/arxiv-v1.pdf", "%PDF");

    CHECK(problemsOf(project).empty());

    const loom::ProjectContents contents = loom::loadContents(project.root).contents;
    REQUIRE(contents.papers.size() == 1);
    CHECK(contents.papers[0].sessions == QStringList{"Sessions/2026-10-18"});
    REQUIRE(contents.stubs.size() == 2);
    CHECK(contents.stub("7RT2")->paperFolder == "SmithJones24");
    CHECK(contents.paperWithKey("SmithJones23") == &contents.papers[0]);
    CHECK(loom::resolveStatement(contents, "7rt2") == QString("7RT2"));
    CHECK(loom::tagsInUse(contents).contains("3WQ6"));
}

TEST_CASE("A stub premise is in the fingerprint with no text", "[papers][contents]")
{
    const ProjectFixture project;
    writeSmithJones24(project);
    project.statement("9D3F", "lemma");
    project.proof("9D3F", "9D3F.a", "9D3F", "[\"7RT2\"]");
    const loom::ProjectContents contents = loom::loadContents(project.root).contents;

    loom::FingerprintInput input;
    input.statementTag = "9D3F";
    input.statementBody = "Body of 9D3F.\n";
    input.premises.push_back({"7RT2", std::nullopt});
    input.proofBody = "Proof.\n";
    CHECK(loom::fingerprintBytes(input).contains("premise 7RT2\n0\n\nproof\n"));
    CHECK(loom::proofFingerprint(contents, "9D3F.a") == loom::fingerprint(input));
}

TEST_CASE("Problems of papers and stubs that need several files", "[papers][contents]")
{
    const ProjectFixture project;
    writeSmithJones24(project);
    // A stub using a statement's tag, a premise from another paper, and versions the paper lacks.
    project.statement("5HN8", "lemma");
    project.write("Papers/SmithJones24/Statements/5HN8.toml", stubToml("5HN8", "lemma", false));
    QByteArray stub = stubToml("8CJA", "lemma", false, "paper_premises = [\"K6YM\", \"8CJA\"]\n");
    stub.replace("extracted_from = \"arxiv-v1\"", "extracted_from = \"arxiv-v4\"");
    stub += "arxiv-v4 = { at = \"Lemma 2\", checked_against_pdf = false }\n";
    project.write("Papers/SmithJones24/Statements/8CJA.toml", stub);
    // A second paper: its folder doesn't match its key, it reuses an old key, and its tag is undeclared.
    QByteArray other = smithJones24();
    other.replace("tags = [\"model-system\"]", "tags = [\"heat-flow\"]");
    other.replace("session = \"Sessions/2026-10-18\"", "session = \"Sessions/2026-10-19\"");
    project.write("Papers/Other/paper.toml", other);
    project.write("Papers/Other/Statements/K6YM.toml", stubToml("K6YM", "lemma", false));
    // A proof of a stub, stray files, and a paper folder without its entry.
    project.proof("7RT2", "7RT2.a", "7RT2", "[]");
    project.write("Papers/README.md", "Papers.\n");
    project.write("Papers/SmithJones24/draft.toml", "x = 1\n");
    project.write("Papers/SmithJones24/Statements/notes.txt", "x\n");
    project.write("Papers/Empty/notes.md", "x\n");

    const std::vector<P> expected = {
        {"Papers/Empty", "missing-field", 1, "`paper.toml` is missing; every paper folder needs one.", true},
        {"Papers/Other/paper.toml", "paper-key-mismatch", 1, "The key is `SmithJones24` but the folder is `Other`; they must match.", true},
        {"Papers/Other/paper.toml", "tag-unknown", 6, "`heat-flow` isn't one of the project's tags. Add it with `loom new-tag`, or fix the spelling.", true},
        {"Papers/README.md", "stray-file", 1, "`Papers/README.md` doesn't belong in Papers/: it holds only a folder for each paper, named by its key.", true},
        {"Papers/SmithJones24/Statements/5HN8.toml", "id-duplicate", 1, "The tag `5HN8` is used by both Statements/5HN8.tex and Papers/SmithJones24/Statements/5HN8.toml.", true},
        {"Papers/SmithJones24/Statements/8CJA.toml", "stub-paper-premise", 4, "`K6YM` in `paper_premises` isn't a result of the same paper.", true},
        {"Papers/SmithJones24/Statements/8CJA.toml", "stub-paper-premise", 4, "`8CJA` in `paper_premises` isn't a result of the same paper.", true},
        {"Papers/SmithJones24/Statements/8CJA.toml", "version-unknown", 5, "`extracted_from` names version `arxiv-v4`, which this paper doesn't have.", true},
        {"Papers/SmithJones24/Statements/8CJA.toml", "version-unknown", 11, "`locators` names version `arxiv-v4`, which this paper doesn't have.", true},
        {"Papers/SmithJones24/Statements/notes.txt", "stray-file", 1, "`Papers/SmithJones24/Statements/notes.txt` doesn't belong in Papers/SmithJones24/Statements/: it holds only the paper's stubs, each as `<tag>.toml`.", true},
        {"Papers/SmithJones24/draft.toml", "stray-file", 1, "`Papers/SmithJones24/draft.toml` doesn't belong in Papers/SmithJones24/: it holds only paper.toml, Statements/, notes.md, Sessions/ and local/.", true},
        {"Papers/SmithJones24/paper.toml", "paper-key-duplicate", 1, "`SmithJones24` is the key or an old key of more than one paper.", true},
        {"Papers/SmithJones24/paper.toml", "paper-key-duplicate", 2, "`SmithJones23` is the key or an old key of more than one paper.", true},
        {"Proofs/7RT2/7RT2.a.tex", "proof-of-stub", 3, "7RT2 is a result from SmithJones24; it's cited from the paper, not proved here.", true},
        {"Papers/Other/paper.toml", "session-missing", 44, "Reading of 2026-10-18 links `Sessions/2026-10-19`, which doesn't exist.", false},
    };
    CHECK(problemsOf(project) == expected);
}

TEST_CASE("A paper whose key doesn't follow the rule is warned about", "[papers][contents]")
{
    const ProjectFixture project;
    project.write("Papers/SmithJones23/paper.toml", preprint() + publishedVersion());
    const std::vector<P> expected = {
        {"Papers/SmithJones23/paper.toml", "paper-key-rule", 1,
         "By the key rule this paper's key is `SmithJones24`, not `SmithJones23`. Run `loom rekey SmithJones23`.", false},
    };
    CHECK(problemsOf(project) == expected);
}

TEST_CASE("new-statement and new-stub never take a stub's tag", "[papers][edits]")
{
    const ProjectFixture project;
    writeSmithJones24(project);
    project.write("Papers/SmithJones24/Statements/" + firstTag() + ".toml", stubToml(firstTag(), "lemma", false));
    Options o;
    loom::NewStatement request;
    request.kind = "lemma";
    request.title = "T";
    request.body = "Text.";
    const loom::ChangeResult result = loom::createStatement(project.root, request, o.options);
    REQUIRE(result.outcome == loom::ChangeOutcome::Done);
    CHECK(result.id != firstTag());
}

TEST_CASE("Proofs may use stubs as premises; stubs aren't edited as statements", "[papers][edits]")
{
    const ProjectFixture project;
    writeSmithJones24(project);
    project.statement("9D3F", "lemma");
    Options o;

    loom::NewProof proof;
    proof.statement = "9D3F";
    proof.level = "complete";
    proof.body = "By the paper.";
    proof.premises = {"7rt2"};
    const loom::ChangeResult created = loom::createProof(project.root, proof, o.options);
    REQUIRE(created.outcome == loom::ChangeOutcome::Done);
    CHECK(project.read("Proofs/9D3F/9D3F.a.tex").contains("premises = [\"7RT2\"]"));

    proof.statement = "7RT2";
    proof.premises = {};
    const loom::ChangeResult ofStub = loom::createProof(project.root, proof, o.options);
    CHECK(ofStub.outcome == loom::ChangeOutcome::Refused);
    CHECK(ofStub.code == "proof-of-stub");

    loom::StatementEdit edit;
    edit.title = "New";
    const loom::ChangeResult edited = loom::editStatement(project.root, "7RT2", edit, o.options);
    CHECK(edited.outcome == loom::ChangeOutcome::Refused);
    CHECK(edited.message == "7RT2 is a result from SmithJones24, not a statement of this project: its stub is "
                            "`Papers/SmithJones24/Statements/7RT2.toml`.");
}

TEST_CASE("new-paper writes the entry from a draft, computing its key", "[papers][edits]")
{
    const ProjectFixture project;
    Options o;
    const QByteArray draft = without(smithJones24(), {"key = ", "cite = ", "created = "});
    const loom::PaperChange result = loom::createPaper(project.root, draft, "draft.toml", o.options);

    REQUIRE(result.change.outcome == loom::ChangeOutcome::Done);
    CHECK(result.change.id == "SmithJones24");
    CHECK(result.change.path == "Papers/SmithJones24/paper.toml");
    CHECK(result.renamed.empty());
    QByteArray expected = smithJones24();
    expected.replace("created = 2026-10-18", "created = 2026-11-04");
    CHECK(project.read("Papers/SmithJones24/paper.toml") == expected);
}

TEST_CASE("new-paper defaults cite to the newest arXiv version when there is no published one", "[papers][edits]")
{
    const ProjectFixture project;
    Options o;
    const QByteArray draft = without(preprint(), {"key = ", "cite = "}) + "\n[[versions]]\nid = \"arxiv-v2\"\nkind = \"arxiv\"\n";
    const loom::PaperChange result = loom::createPaper(project.root, draft, "draft.toml", o.options);
    REQUIRE(result.change.outcome == loom::ChangeOutcome::Done);
    CHECK(result.change.id == "SmithJones23");
    CHECK(project.read("Papers/SmithJones23/paper.toml").contains("\ncite = \"arxiv-v2\"\n"));
}

TEST_CASE("new-paper refuses a wrong key, a draft with errors, and a paper with no key", "[papers][edits]")
{
    const ProjectFixture project;
    Options o;

    QByteArray wrongKey = smithJones24();
    wrongKey.replace("key = \"SmithJones24\"", "key = \"Smith24\"");
    const loom::PaperChange wrong = loom::createPaper(project.root, wrongKey, "draft.toml", o.options);
    CHECK(wrong.change.outcome == loom::ChangeOutcome::Refused);
    CHECK(wrong.change.code == "paper-key-rule");
    CHECK(wrong.change.message == "The draft's key is `Smith24`, but by the key rule it is `SmithJones24`. Leave `key` out, and Loom fills it in.");

    QByteArray broken = smithJones24();
    broken.replace("relevance = 1", "relevance = 0");
    const loom::PaperChange errors = loom::createPaper(project.root, broken, "draft.toml", o.options);
    CHECK(errors.change.outcome == loom::ChangeOutcome::Refused);
    CHECK(errors.change.code == "bad-value");
    CHECK(errors.change.message.startsWith("Loom didn't create the paper: the draft has problems to fix first:\ndraft.toml:4: error:"));

    QByteArray noYear = without(preprint(), {"posted = "});
    const loom::PaperChange keyless = loom::createPaper(project.root, without(noYear, {"key = "}), "draft.toml", o.options);
    CHECK(keyless.change.outcome == loom::ChangeOutcome::Refused);
    CHECK(keyless.change.message == "Can't make a key for this paper: it has no year. Add a published year, an arXiv posting date, or a date.");

    CHECK_FALSE(QFileInfo::exists(project.root + "/Papers"));
}

TEST_CASE("new-paper letters a shared key, renaming the paper already there", "[papers][edits]")
{
    const ProjectFixture project;
    writeSmithJones24(project);
    Options o;
    QByteArray draft = without(smithJones24(), {"key = ", "aliases = ", "cite = ", "created = "});
    draft.replace("posted = { value = 2023-01-15", "posted = { value = 2024-01-10");
    draft.replace("Energy decay for a model parabolic system", "A second paper");
    const loom::PaperChange result = loom::createPaper(project.root, draft, "draft.toml", o.options);

    REQUIRE(result.change.outcome == loom::ChangeOutcome::Done);
    CHECK(result.change.id == "SmithJones24b");
    REQUIRE(result.renamed.size() == 1);
    CHECK(result.renamed[0].from == "SmithJones24");
    CHECK(result.renamed[0].to == "SmithJones24a");

    CHECK_FALSE(QFileInfo::exists(project.root + "/Papers/SmithJones24"));
    QByteArray renamed = smithJones24();
    renamed.replace("key = \"SmithJones24\"\naliases = [\"SmithJones23\"]", "key = \"SmithJones24a\"\naliases = [\"SmithJones23\", \"SmithJones24\"]");
    CHECK(project.read("Papers/SmithJones24a/paper.toml") == renamed);
    CHECK(project.read("Papers/SmithJones24a/Statements/7RT2.toml") == stubToml("7RT2", "theorem", true, "paper_premises = [\"3WQ6\"]\n"));
    CHECK(QFileInfo(project.root + "/Papers/SmithJones24a/Sessions/2026-10-18").isDir());
    CHECK(project.read("Papers/SmithJones24b/paper.toml").startsWith("key = \"SmithJones24b\"\ncite = \"published\"\n"));
    for (const P& problem : problemsOf(project))
        CHECK_FALSE(std::get<4>(problem));
}

TEST_CASE("new-paper never replaces a folder that is already there", "[papers][edits]")
{
    // Specs/m1-core.md, `new-paper` and the edge cases (A3): whatever is in `Papers/<Key>/`, and
    // whatever case its name is in, it is refused with `paper-exists` and left as it was.
    const ProjectFixture project;
    Options o;
    const QByteArray draft = without(smithJones24(), {"key = ", "aliases = ", "cite = ", "created = "});
    QString folder;
    QByteArray existing;

    SECTION("an entry that doesn't parse")
    {
        folder = "SmithJones24";
        existing = "key = \"SmithJones24\"\nhalf an edit\n";
    }
    SECTION("an entry whose key disagrees with its folder")
    {
        folder = "SmithJones24";
        existing = smithJones24();
        existing.replace("key = \"SmithJones24\"", "key = \"SmithJones2\"");
    }
    SECTION("a folder whose name differs only in case")
    {
        // On Windows it is the same folder; Loom compares ignoring case on every system.
        folder = "smithjones24";
        existing = "key = \"smithjones24\"\nhalf an edit\n";
    }
    project.write("Papers/" + folder + "/paper.toml", existing);
    project.write("Papers/" + folder + "/notes.md", "Mine.\n");

    const loom::PaperChange result = loom::createPaper(project.root, draft, "draft.toml", o.options);
    CHECK(result.change.outcome == loom::ChangeOutcome::Refused);
    CHECK(result.change.code == "paper-exists");
    CHECK(result.change.message == "`Papers/" + folder + "/` already exists. Fix or remove what's there first; "
                                   "`loom validate` shows its problems.");
    CHECK(result.renamed.empty());
    CHECK(loomtest::filesUnder(project.root + "/Papers") == QStringList{folder + "/notes.md", folder + "/paper.toml"});
    CHECK(project.read("Papers/" + folder + "/paper.toml") == existing);
}

TEST_CASE("A rename that fails part way is undone, every rename before it too", "[papers][edits]")
{
    // The key rule, step 4: letters go by posting order the first time. If the second rename
    // fails and the first stayed done, a retry would letter the papers in the wrong order.
    const ProjectFixture project;
    QByteArray later = smithJones24();
    later.replace("aliases = [\"SmithJones23\"]\n", "");
    later.replace("posted = { value = 2023-01-15", "posted = { value = 2024-01-10");
    project.write("Papers/SmithJones24/paper.toml", later);
    project.write("Papers/SmithJones24/Statements/7RT2.toml", stubToml("7RT2", "theorem", true));
    const QByteArray earlier = preprint() + publishedVersion();
    project.write("Papers/SmithJones23/paper.toml", earlier);
    // The plan is SmithJones24 → SmithJones24b, then SmithJones23 → SmithJones24a; something is
    // already where the second rename goes.
    project.write("Papers/SmithJones24a/notes.md", "In the way.\n");
    Options o;

    const loom::PaperChange result = loom::rekeyPaper(project.root, "SmithJones23", o.options);
    CHECK(result.change.outcome == loom::ChangeOutcome::CantWork);
    CHECK(result.change.code == "write-failed");
    CHECK(result.renamed.empty());
    CHECK(loomtest::filesUnder(project.root + "/Papers") ==
          QStringList{"SmithJones23/paper.toml", "SmithJones24/Statements/7RT2.toml", "SmithJones24/paper.toml",
                      "SmithJones24a/notes.md"});
    CHECK(project.read("Papers/SmithJones24/paper.toml") == later);
    CHECK(project.read("Papers/SmithJones23/paper.toml") == earlier);
}

TEST_CASE("new-stub writes a stub with a new tag and an unchecked locator", "[papers][edits]")
{
    const ProjectFixture project;
    writeSmithJones24(project);
    project.statement("2M9Q", "definition");
    Options o;
    loom::NewStub request;
    request.paper = "SmithJones23"; // An old key works too.
    request.kind = "lemma";
    request.gist = "Energy is nonincreasing";
    request.version = "published";
    request.at = "Lemma 2.4, p. 9";
    request.by = "agent:claude-opus-5-5";
    request.title = "Monotone energy";
    request.statedUsing = {"2m9q"};
    request.paperPremises = {"3WQ6"};
    const loom::ChangeResult result = loom::createStub(project.root, request, o.options);

    REQUIRE(result.outcome == loom::ChangeOutcome::Done);
    CHECK(result.id == firstTag());
    CHECK(result.path == "Papers/SmithJones24/Statements/" + firstTag() + ".toml");
    CHECK(project.read(result.path) ==
          "id = \"" + firstTag().toUtf8() + "\"\nkind = \"lemma\"\ntitle = \"Monotone energy\"\ngist = \"Energy is nonincreasing\"\n"
          "stated_using = [\"2M9Q\"]\npaper_premises = [\"3WQ6\"]\nextracted_from = \"published\"\n"
          "extracted_by = \"agent:claude-opus-5-5\"\ncreated = 2026-11-04\n\n[locators]\n"
          "published = { at = \"Lemma 2.4, p. 9\", checked_against_pdf = false }\n");
}

TEST_CASE("new-stub refuses what would add an error, writing nothing", "[papers][edits]")
{
    const ProjectFixture project;
    writeSmithJones24(project);
    Options o;
    loom::NewStub request;
    request.paper = "SmithJones24";
    request.kind = "lemma";
    request.gist = "Energy is nonincreasing";
    request.version = "published";
    request.at = "Lemma 2.4";
    request.by = "agent:claude-opus-5-5";

    loom::NewStub unknownPaper = request;
    unknownPaper.paper = "Nobody99";
    CHECK(loom::createStub(project.root, unknownPaper, o.options).code == "ref-unknown");

    loom::NewStub unknownVersion = request;
    unknownVersion.version = "arxiv-v3";
    const loom::ChangeResult version = loom::createStub(project.root, unknownVersion, o.options);
    CHECK(version.outcome == loom::ChangeOutcome::Refused);
    CHECK(version.code == "version-unknown");

    loom::NewStub badBy = request;
    badBy.by = "claude";
    CHECK(loom::createStub(project.root, badBy, o.options).code == "bad-value");

    loom::NewStub noGist = request;
    noGist.gist = " ";
    CHECK(loom::createStub(project.root, noGist, o.options).code == "stub-gist-empty");

    CHECK_FALSE(QFileInfo::exists(project.root + "/Papers/SmithJones24/Statements/" + firstTag() + ".toml"));
}

TEST_CASE("rekey renames a preprint once it is published, keeping the old key and every \\cite", "[papers][edits]")
{
    const ProjectFixture project;
    project.write("Papers/SmithJones23/paper.toml", preprint());
    project.write("Papers/SmithJones23/Statements/7RT2.toml", stubToml("7RT2", "theorem", true));
    project.write("Statements/9D3F.tex",
                  "+++\nid = \"9D3F\"\nkind = \"lemma\"\ntitle = \"T\"\ncreated = 2026-11-03\n+++\nAs in \\cite{SmithJones23}.\n");
    Options o;

    // Still a preprint: the key is right.
    const loom::PaperChange early = loom::rekeyPaper(project.root, "SmithJones23", o.options);
    REQUIRE(early.change.outcome == loom::ChangeOutcome::Done);
    CHECK(early.renamed.empty());
    CHECK(early.change.id == "SmithJones23");

    // Published in 2024: renamed, with the old key kept as an alias.
    project.write("Papers/SmithJones23/paper.toml", preprint() + publishedVersion());
    const loom::PaperChange result = loom::rekeyPaper(project.root, "smithjones23", o.options);
    CHECK(result.change.outcome == loom::ChangeOutcome::Refused); // Keys are typed exactly.

    const loom::PaperChange renamed = loom::rekeyPaper(project.root, "SmithJones23", o.options);
    REQUIRE(renamed.change.outcome == loom::ChangeOutcome::Done);
    CHECK(renamed.change.id == "SmithJones24");
    REQUIRE(renamed.renamed.size() == 1);
    CHECK(renamed.renamed[0].from == "SmithJones23");
    CHECK(renamed.renamed[0].to == "SmithJones24");
    CHECK_FALSE(QFileInfo::exists(project.root + "/Papers/SmithJones23"));
    QByteArray expected = preprint() + publishedVersion();
    expected.replace("key = \"SmithJones23\"", "key = \"SmithJones24\"\naliases = [\"SmithJones23\"]");
    CHECK(project.read("Papers/SmithJones24/paper.toml") == expected);
    CHECK(QFileInfo::exists(project.root + "/Papers/SmithJones24/Statements/7RT2.toml"));
    // D9: bodies aren't rewritten; the old key works through the alias.
    CHECK(project.read("Statements/9D3F.tex").endsWith("As in \\cite{SmithJones23}.\n"));
    CHECK(problemsOf(project).empty());

    // Now right, under its new key or its old one.
    const loom::PaperChange again = loom::rekeyPaper(project.root, "SmithJones23", o.options);
    REQUIRE(again.change.outcome == loom::ChangeOutcome::Done);
    CHECK(again.renamed.empty());
    CHECK(again.change.id == "SmithJones24");
}

TEST_CASE("rekey into a key another paper has letters both", "[papers][edits]")
{
    const ProjectFixture project;
    QByteArray later = smithJones24();
    later.replace("aliases = [\"SmithJones23\"]\n", "");
    later.replace("posted = { value = 2023-01-15", "posted = { value = 2024-01-10");
    project.write("Papers/SmithJones24/paper.toml", later);
    project.write("Papers/SmithJones23/paper.toml", preprint() + publishedVersion());
    Options o;

    const loom::PaperChange result = loom::rekeyPaper(project.root, "SmithJones23", o.options);
    REQUIRE(result.change.outcome == loom::ChangeOutcome::Done);
    CHECK(result.change.id == "SmithJones24a"); // Posted 2023-01-15, before the other's 2024-01-10.
    REQUIRE(result.renamed.size() == 2);
    CHECK(result.renamed[0].from == "SmithJones24");
    CHECK(result.renamed[0].to == "SmithJones24b");
    CHECK(result.renamed[1].from == "SmithJones23");
    CHECK(result.renamed[1].to == "SmithJones24a");
    CHECK(QFileInfo::exists(project.root + "/Papers/SmithJones24a/paper.toml"));
    CHECK(QFileInfo::exists(project.root + "/Papers/SmithJones24b/paper.toml"));
    CHECK_FALSE(QFileInfo::exists(project.root + "/Papers/SmithJones24"));
}

TEST_CASE("build-bib writes refs.bib, with its warnings", "[papers][edits]")
{
    const ProjectFixture project;
    writeSmithJones24(project);
    Options o;
    const loom::BibliographyBuilt built = loom::buildBibliography(project.root, {}, o.options);
    REQUIRE(built.change.outcome == loom::ChangeOutcome::Done);
    CHECK(built.change.path == ".loom/build/refs.bib");
    CHECK(project.read(".loom/build/refs.bib").startsWith("@article{SmithJones24,\n"));
    CHECK(built.problems.empty());

    const loom::BibliographyBuilt elsewhere = loom::buildBibliography(project.root, "TeX/refs.bib", o.options);
    REQUIRE(elsewhere.change.outcome == loom::ChangeOutcome::Done);
    CHECK(project.read("TeX/refs.bib") == project.read(".loom/build/refs.bib"));

    const loom::BibliographyBuilt outside = loom::buildBibliography(project.root, "../refs.bib", o.options);
    CHECK(outside.change.outcome == loom::ChangeOutcome::CantWork);
    CHECK(outside.change.code == "bad-path");
}

TEST_CASE("build-bib never writes over the project's own files", "[papers][edits]")
{
    const ProjectFixture project;
    writeSmithJones24(project);
    project.statement("4KX7", "lemma");
    project.proof("4KX7", "4KX7.a", "4KX7", "[]");
    Options o;
    const QStringList files = loomtest::filesUnder(project.root);
    std::vector<QByteArray> before;
    for (const QString& file : files)
        before.push_back(project.read(file));

    // `loom.toml`, a paper, a stub, a statement, a proof, a preamble file, and new files in the
    // folders Loom reads (a stray file there is an error).
    const QStringList outs = {"loom.toml", "Loom.toml", "Papers/SmithJones24/paper.toml",
                              "Papers/SmithJones24/Statements/7RT2.toml", "Papers/SmithJones24/notes.md",
                              "Papers/refs.bib", "Statements/4KX7.tex", "statements/4KX7.tex", "Statements/refs.bib",
                              "Proofs/4KX7/4KX7.a.tex", "Proofs/4KX7/4KX7.a.checks.toml", "Proofs/4KX7/4KX7.a.verbose.tex",
                              "TeX/preamble.tex", "CLAUDE.md", ".loom/write.lock", ".git/config"};
    for (const QString& out : outs) {
        INFO(out.toStdString());
        const loom::BibliographyBuilt built = loom::buildBibliography(project.root, out, o.options);
        CHECK(built.change.outcome == loom::ChangeOutcome::Refused);
        CHECK(built.change.code == "bib-out-project-file");
        CHECK(built.change.message == "`" + out + "` is one of the project's own files, or in a folder Loom keeps them "
                                      "in, so Loom won't write the bibliography there. Give `--out` a path of its own, "
                                      "such as `TeX/refs.bib`.");
        CHECK(loomtest::filesUnder(project.root) == files);
        for (qsizetype i = 0; i < files.size() && i < qsizetype(before.size()); ++i)
            CHECK(project.read(files[i]) == before[std::size_t(i)]);
    }
}
