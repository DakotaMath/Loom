// Creating and changing statements, proofs and tags (Specs/m1-core.md, "The `loom` command":
// new-tag, new-statement, edit-statement, new-proof, edit-proof).

#include "LoomCore/Edits.h"

#include "LoomCore/Ids.h"
#include "LoomCore/ProjectToml.h"
#include "LoomCore/StatementsAndProofs.h"

#include "ProjectFixture.h"

#include <QDir>
#include <QFileInfo>
#include <QLockFile>

#include <catch2/catch_test_macros.hpp>

using loomtest::ProjectFixture;

namespace {

/// Options with a fixed date and a seeded random source.
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

/// A statement request that is fine in ProjectFixture.
loom::NewStatement theorem()
{
    loom::NewStatement request;
    request.kind = "theorem";
    request.title = "Energy decay for the model system";
    request.body = "Let $u_0\\in L^2$.";
    return request;
}

} // namespace

TEST_CASE("new-statement writes the spec's layout", "[edits]")
{
    const ProjectFixture project;
    project.statement("2M9Q", "definition", "aliases = [\"def:energy\"]\n");
    Options o;

    loom::NewStatement request = theorem();
    request.tags = {"model-system"};
    request.statedUsing = {"def:energy"};
    request.origin = "spec:S-004";
    const loom::ChangeResult result = loom::createStatement(project.root, request, o.options);

    REQUIRE(result.outcome == loom::ChangeOutcome::Done);
    CHECK(result.id == firstTag());
    CHECK(result.path == "Statements/" + result.id + ".tex");
    CHECK(project.read(result.path) ==
          "+++\nid = \"" + result.id.toUtf8() + "\"\nkind = \"theorem\"\ntitle = \"Energy decay for the model system\"\n"
          "tags = [\"model-system\"]\nstated_using = [\"2M9Q\"]\norigin = \"spec:S-004\"\ncreated = 2026-11-04\n+++\n"
          "Let $u_0\\in L^2$.\n");
}

TEST_CASE("new-statement never reuses a tag", "[edits]")
{
    const ProjectFixture project;
    project.statement(firstTag(), "lemma");
    Options o;
    const loom::ChangeResult result = loom::createStatement(project.root, theorem(), o.options);
    REQUIRE(result.outcome == loom::ChangeOutcome::Done);
    CHECK(result.id != firstTag());
}

TEST_CASE("new-statement refuses what the project's rules refuse, and writes nothing", "[edits]")
{
    const ProjectFixture project;
    Options o;
    loom::NewStatement request = theorem();

    SECTION("an undeclared tag")
    {
        request.tags = {"energy-estimates"};
        const loom::ChangeResult result = loom::createStatement(project.root, request, o.options);
        CHECK(result.outcome == loom::ChangeOutcome::Refused);
        CHECK(result.code == "tag-unknown");
        REQUIRE(result.problems.size() == 1);
        CHECK(result.problems[0].message == "`energy-estimates` isn't one of the project's tags. Add it with `loom new-tag`, or fix the spelling.");
        CHECK(result.message == "Loom didn't create the statement, because it would add this problem:\n"
                                "Statements/" + firstTag() + ".tex:5: error: `energy-estimates` isn't one of the project's tags. "
                                "Add it with `loom new-tag`, or fix the spelling.");
    }
    SECTION("an unknown kind")
    {
        request.kind = "axiom";
        const loom::ChangeResult result = loom::createStatement(project.root, request, o.options);
        CHECK(result.outcome == loom::ChangeOutcome::Refused);
        CHECK(result.code == "bad-value");
    }
    SECTION("a stated_using that names nothing")
    {
        request.statedUsing = {"ZZZZ"};
        const loom::ChangeResult result = loom::createStatement(project.root, request, o.options);
        CHECK(result.outcome == loom::ChangeOutcome::Refused);
        CHECK(result.code == "ref-unknown");
        CHECK(result.message == "`ZZZZ` isn't a statement or paper stub in this project.");
    }
    SECTION("an empty body")
    {
        request.body = " \n";
        const loom::ChangeResult result = loom::createStatement(project.root, request, o.options);
        CHECK(result.outcome == loom::ChangeOutcome::Refused);
        CHECK(result.code == "body-empty");
    }
    CHECK_FALSE(QFileInfo::exists(project.root + "/Statements"));
}

TEST_CASE("A project Loom can't work on", "[edits]")
{
    Options o;

    SECTION("no project")
    {
        QTemporaryDir empty;
        const loom::ChangeResult result = loom::createStatement(empty.path(), theorem(), o.options);
        CHECK(result.outcome == loom::ChangeOutcome::CantWork);
        CHECK(result.code == "not-a-project");
    }
    SECTION("a newer format: nothing is written, not even the lock")
    {
        const ProjectFixture project;
        project.write("loom.toml", "format = 2\n");
        const loom::ChangeResult result = loom::createStatement(project.root, theorem(), o.options);
        CHECK(result.outcome == loom::ChangeOutcome::CantWork);
        CHECK(result.code == "format-too-new");
        CHECK_FALSE(QFileInfo::exists(project.root + "/.loom"));
    }
    SECTION("another loom holds the lock")
    {
        const ProjectFixture project;
        REQUIRE(QDir().mkpath(project.root + "/.loom"));
        QLockFile held(project.root + "/.loom/write.lock");
        REQUIRE(held.tryLock());
        o.options.lockWaitMs = 50;
        const loom::ChangeResult result = loom::createStatement(project.root, theorem(), o.options);
        CHECK(result.outcome == loom::ChangeOutcome::CantWork);
        CHECK(result.code == "locked");
        CHECK(result.message == "Another loom command is changing this project. Try again when it has finished.");
        CHECK_FALSE(QFileInfo::exists(project.root + "/Statements"));
    }
}

TEST_CASE("new-proof takes the next letter", "[edits]")
{
    const ProjectFixture project;
    project.statement("4KX7", "theorem");
    project.statement("5HN8", "lemma", "aliases = [\"lem:five\"]\n");
    Options o;

    loom::NewProof request;
    request.statement = "4kx7";
    request.level = "complete";
    request.body = "By Lemma \\ref{5HN8}.";
    request.premises = {"lem:five"};

    loom::ChangeResult result = loom::createProof(project.root, request, o.options);
    REQUIRE(result.outcome == loom::ChangeOutcome::Done);
    CHECK(result.id == "4KX7.a");
    CHECK(result.path == "Proofs/4KX7/4KX7.a.tex");
    CHECK(project.read(result.path) ==
          "+++\nid = \"4KX7.a\"\nproves = \"4KX7\"\npremises = [\"5HN8\"]\nlevel = \"complete\"\ncreated = 2026-11-04\n+++\n"
          "By Lemma \\ref{5HN8}.\n");

    request.premises = {};
    result = loom::createProof(project.root, request, o.options);
    REQUIRE(result.outcome == loom::ChangeOutcome::Done);
    CHECK(result.id == "4KX7.b");

    SECTION("a deleted proof's letter isn't reused while its checks remain")
    {
        project.write("Proofs/4KX7/4KX7.z.checks.toml", "");
        result = loom::createProof(project.root, request, o.options);
        CHECK(result.id == "4KX7.aa");
    }
}

TEST_CASE("new-proof refuses what the rules refuse", "[edits]")
{
    const ProjectFixture project;
    project.statement("2M9Q", "definition");
    project.statement("4KX7", "theorem");
    Options o;
    loom::NewProof request;
    request.statement = "4KX7";
    request.level = "complete";
    request.body = "Proof.";

    SECTION("a proof of a definition")
    {
        request.statement = "2M9Q";
        CHECK(loom::createProof(project.root, request, o.options).code == "proof-of-no-status");
    }
    SECTION("a definition as a premise")
    {
        request.premises = {"2M9Q"};
        const loom::ChangeResult result = loom::createProof(project.root, request, o.options);
        CHECK(result.outcome == loom::ChangeOutcome::Refused);
        CHECK(result.code == "premise-no-status");
    }
    SECTION("an unknown level")
    {
        request.level = "done";
        CHECK(loom::createProof(project.root, request, o.options).code == "bad-value");
    }
    SECTION("a statement that doesn't exist")
    {
        request.statement = "ZZZZ";
        const loom::ChangeResult result = loom::createProof(project.root, request, o.options);
        CHECK(result.code == "ref-unknown");
        CHECK(result.message == "`ZZZZ` isn't a statement in this project.");
    }
    CHECK_FALSE(QFileInfo::exists(project.root + "/Proofs"));
}

TEST_CASE("edit-statement changes fields and keeps everything else", "[edits]")
{
    const ProjectFixture project;
    project.statement("2M9Q", "definition");
    const QByteArray body = "\\begin{verbatim}\n+++\n\\end{verbatim}\nNo final newline";
    project.write("Statements/4KX7.tex",
        "+++\nid = \"4KX7\"\nkind = \"lemma\"\n# Dakota's comment.\ntitle = \"Old\"\naliases = [\"lem:old\"]\ncreated = 2026-11-03\n+++\n" + body);
    Options o;

    SECTION("title, tags, stated_using, verified; the body byte for byte")
    {
        loom::StatementEdit edit;
        edit.title = "New";
        edit.kind = "theorem";
        edit.addTags = {"model-system"};
        edit.addStatedUsing = {"2m9q"};
        edit.verified = true;
        const loom::ChangeResult result = loom::editStatement(project.root, "lem:old", edit, o.options);
        REQUIRE(result.outcome == loom::ChangeOutcome::Done);
        CHECK(result.id == "4KX7");
        CHECK(result.changed == QStringList{"kind", "title", "tags", "stated_using", "verified"});
        CHECK(project.read("Statements/4KX7.tex") ==
              "+++\nid = \"4KX7\"\nkind = \"theorem\"\ntitle = \"New\"\ntags = [\"model-system\"]\nstated_using = [\"2M9Q\"]\n"
              "aliases = [\"lem:old\"]\ncreated = 2026-11-03\nverified = 2026-11-04\n+++\n" + body);

        loom::StatementEdit undo;
        undo.removeTags = {"model-system"};
        undo.removeStatedUsing = {"2M9Q"};
        CHECK(loom::editStatement(project.root, "4KX7", undo, o.options).changed == QStringList{"tags", "stated_using"});
    }
    SECTION("a new body")
    {
        loom::StatementEdit edit;
        edit.body = "New text.\r\nSecond line.";
        const loom::ChangeResult result = loom::editStatement(project.root, "4KX7", edit, o.options);
        CHECK(result.changed == QStringList{"body"});
        CHECK(project.read("Statements/4KX7.tex").endsWith("+++\nNew text.\nSecond line.\n"));
    }
    SECTION("nothing to change writes nothing")
    {
        const QByteArray before = project.read("Statements/4KX7.tex");
        loom::StatementEdit edit;
        edit.title = "Old";
        const loom::ChangeResult result = loom::editStatement(project.root, "4KX7", edit, o.options);
        CHECK(result.outcome == loom::ChangeOutcome::Done);
        CHECK(result.changed.isEmpty());
        CHECK(project.read("Statements/4KX7.tex") == before);
    }
    SECTION("refusals")
    {
        const QByteArray before = project.read("Statements/4KX7.tex");
        loom::StatementEdit edit;
        SECTION("removing a tag it doesn't have")
        {
            edit.removeTags = {"model-system"};
            const loom::ChangeResult result = loom::editStatement(project.root, "4KX7", edit, o.options);
            CHECK(result.code == "not-there");
            CHECK(result.message == "`tags` of 4KX7 doesn't list `model-system`.");
        }
        SECTION("an undeclared tag")
        {
            edit.addTags = {"no-such-tag"};
            CHECK(loom::editStatement(project.root, "4KX7", edit, o.options).code == "tag-unknown");
        }
        SECTION("superseded by itself")
        {
            edit.supersededBy = "4KX7";
            CHECK(loom::editStatement(project.root, "4KX7", edit, o.options).code == "superseded-invalid");
        }
        SECTION("an empty body")
        {
            edit.body = "\n";
            CHECK(loom::editStatement(project.root, "4KX7", edit, o.options).code == "body-empty");
        }
        SECTION("a statement that doesn't exist")
        {
            edit.title = "X";
            CHECK(loom::editStatement(project.root, "ZZZZ", edit, o.options).code == "ref-unknown");
        }
        CHECK(project.read("Statements/4KX7.tex") == before);
    }
}

TEST_CASE("edit-statement rewrites a CRLF file as LF", "[edits]")
{
    const ProjectFixture project;
    project.write("Statements/4KX7.tex", "+++\r\nid = \"4KX7\"\r\nkind = \"lemma\"\r\ntitle = \"Old\"\r\ncreated = 2026-11-03\r\n+++\r\nA\r\nB\r\n");
    Options o;
    loom::StatementEdit edit;
    edit.title = "New";
    REQUIRE(loom::editStatement(project.root, "4KX7", edit, o.options).outcome == loom::ChangeOutcome::Done);
    CHECK(project.read("Statements/4KX7.tex") == "+++\nid = \"4KX7\"\nkind = \"lemma\"\ntitle = \"New\"\ncreated = 2026-11-03\n+++\nA\nB\n");
}

TEST_CASE("Loom won't rewrite a file it couldn't fully read", "[edits]")
{
    const ProjectFixture project;
    const QByteArray text = "+++\nid = \"4KX7\"\nkind = \"lemma\"\ntitle = \"Old\"\ncolour = \"blue\"\ncreated = 2026-11-03\n+++\nBody\n";
    project.write("Statements/4KX7.tex", text);
    Options o;
    loom::StatementEdit edit;
    edit.title = "New";
    const loom::ChangeResult result = loom::editStatement(project.root, "4KX7", edit, o.options);
    CHECK(result.outcome == loom::ChangeOutcome::Refused);
    CHECK(result.code == "not-editable");
    CHECK(result.message.startsWith("Loom can't change Statements/4KX7.tex until these problems are fixed"));
    CHECK(result.message.contains("Statements/4KX7.tex:5: error: `colour` isn't a field of a statement."));
    CHECK(project.read("Statements/4KX7.tex") == text);
}

TEST_CASE("An edit may fix a problem without fixing every other one", "[edits]")
{
    const ProjectFixture project;
    project.statement("4KX7", "theorem");
    project.statement("5HN8", "lemma", "tags = [\"no-such-tag\"]\n");
    project.proof("4KX7", "4KX7.a", "4KX7", "[\"ZZZZ\"]");
    Options o;

    loom::ProofEdit edit;
    edit.removePremises = {"zzzz"};
    edit.addPremises = {"5HN8"};
    edit.level = "sketch";
    const loom::ChangeResult result = loom::editProof(project.root, "4KX7.a", edit, o.options);
    REQUIRE(result.outcome == loom::ChangeOutcome::Done);
    CHECK(result.changed == QStringList{"premises", "level"});
    const loom::Proof proof = loom::readProof(project.read("Proofs/4KX7/4KX7.a.tex"), "Proofs/4KX7/4KX7.a.tex").document;
    CHECK(proof.premises == QStringList{"5HN8"});
    CHECK(proof.level == "sketch");
}

TEST_CASE("edit-proof refusals", "[edits]")
{
    const ProjectFixture project;
    project.statement("4KX7", "theorem");
    project.statement("5HN8", "lemma");
    project.proof("4KX7", "4KX7.a", "4KX7", "[\"5HN8\"]");
    const QByteArray before = project.read("Proofs/4KX7/4KX7.a.tex");
    Options o;
    loom::ProofEdit edit;

    SECTION("a premise listed already")
    {
        edit.addPremises = {"5hn8"};
        const loom::ChangeResult result = loom::editProof(project.root, "4KX7.a", edit, o.options);
        CHECK(result.code == "already-there");
        CHECK(result.message == "`premises` of 4KX7.a already lists 5HN8.");
    }
    SECTION("the proved statement as a premise")
    {
        edit.addPremises = {"4KX7"};
        CHECK(loom::editProof(project.root, "4KX7.a", edit, o.options).code == "premise-self");
    }
    SECTION("a proof that doesn't exist")
    {
        edit.verified = true;
        const loom::ChangeResult result = loom::editProof(project.root, "4KX7.b", edit, o.options);
        CHECK(result.code == "ref-unknown");
        CHECK(result.message == "`4KX7.b` isn't a proof in this project.");
    }
    CHECK(project.read("Proofs/4KX7/4KX7.a.tex") == before);
}

TEST_CASE("new-tag adds a [[tags]] table and keeps the rest of loom.toml", "[edits]")
{
    const ProjectFixture project;
    const QByteArray before = "# Dakota's notes on the project.\n" + project.read("loom.toml").trimmed(); // No final newline.
    project.write("loom.toml", before);
    Options o;

    const loom::ChangeResult result = loom::addTag(project.root, "energy-estimates", "technique", "Energy estimates", o.options);
    REQUIRE(result.outcome == loom::ChangeOutcome::Done);
    CHECK(result.id == "energy-estimates");
    CHECK(project.read("loom.toml") ==
          before + "\n\n[[tags]]\nid = \"energy-estimates\"\nkind = \"technique\"\ntitle = \"Energy estimates\"\n");
    const loom::OpenedProject opened = loom::openProject(project.root);
    CHECK(opened.problems.empty());
    REQUIRE(opened.config.tags.size() == 2);
    CHECK(opened.config.tags[1].id == "energy-estimates");

    SECTION("refusals")
    {
        const QByteArray now = project.read("loom.toml");
        CHECK(loom::addTag(project.root, "energy-estimates", "technique", "Again", o.options).code == "tag-duplicate");
        CHECK(loom::addTag(project.root, "Energy", "technique", "E", o.options).code == "tag-syntax");
        CHECK(loom::addTag(project.root, "decay", "lemma", "E", o.options).code == "bad-value");
        CHECK(loom::addTag(project.root, "decay", "other", "", o.options).code == "wrong-type");
        CHECK(project.read("loom.toml") == now);
    }
}

TEST_CASE("new-tag won't touch a loom.toml it can't read", "[edits]")
{
    const ProjectFixture project;
    project.write("loom.toml", "format = 1\nname = \"Demo\"\ncreated = 2026-11-03\ntags = []\n\n[tex]\npreamble = [\"TeX/preamble.tex\"]\n");
    Options o;
    const loom::ChangeResult result = loom::addTag(project.root, "decay", "other", "Decay", o.options);
    CHECK(result.outcome == loom::ChangeOutcome::Refused);
    CHECK(result.code == "toml-syntax");
}
