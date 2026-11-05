// Saving the editor's text (Specs/m2-statements-review.md, "Saving" and "Verbose versions";
// Verification, "Saving editor text": "`file-changed`, `id-changed`, outdated checks with and
// without confirmation, a raw save fixing a broken file, canonical front matter, the body byte for
// byte").

#include "LoomCore/EditorSaves.h"

#include "LoomCore/Edits.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProofChecks.h"
#include "LoomCore/StatementsAndProofs.h"

#include "ProjectFixture.h"

#include <QFile>
#include <QTimeZone>

#include <catch2/catch_test_macros.hpp>

using loomtest::ProjectFixture;

namespace {

/// Change options at a fixed time, 2026-11-05 10:00 Central, plus `minutes`.
loom::ChangeOptions at(int minutes = 0)
{
    loom::ChangeOptions options;
    options.today = QDate(2026, 11, 5);
    options.now = QDateTime(QDate(2026, 11, 5), QTime(10, 0, 0), QTimeZone::fromSecondsAheadOfUtc(-6 * 3600)).addSecs(minutes * 60);
    return options;
}

/// Makes `id` checked: an agent's pass, then Dakota's (confirmed).
/// Side effects: writes the proof's check history; fails the test if either is refused.
void makeChecked(const ProjectFixture& project, const QString& id)
{
    loom::NewCheck agent;
    agent.by = "agent:claude-opus-5-5";
    agent.verdict = "pass";
    agent.fingerprint = loom::proofFingerprint(loom::loadContents(project.root).contents, id).value_or(QString());
    agent.record = "Records/2026-11/job-0142.md";
    REQUIRE(loom::addCheck(project.root, id, agent, false, at(1)).change.outcome == loom::ChangeOutcome::Done);
    loom::NewCheck dakota;
    dakota.by = "dakota";
    dakota.verdict = "pass";
    REQUIRE(loom::addCheck(project.root, id, dakota, true, at(2)).standing.checked);
}

/// The statement ProjectFixture::statement() writes for 4KX7, as a document.
loom::Statement statement4kx7()
{
    return loom::readStatement(QByteArray("+++\nid = \"4KX7\"\nkind = \"theorem\"\ntitle = \"T\"\ncreated = 2026-11-03\n+++\nBody of 4KX7.\n"),
                               "Statements/4KX7.tex")
        .document;
}

} // namespace

TEST_CASE("Saving a statement writes canonical front matter and the body as typed", "[editor-saves]")
{
    ProjectFixture project;
    project.statement("4KX7", "theorem");
    const QString path = "Statements/4KX7.tex";
    const QString base = loom::fileBase(project.read(path));

    // Fields out of order, CRLF line endings, a comment, and a body with trailing spaces and no
    // final newline: the front matter comes back canonical, the body exactly as typed (LF).
    const QByteArray typed = "+++\r\ntitle = \"Energy decay\" # renamed\r\nid = \"4KX7\"\r\ncreated = 2026-11-03\r\n"
                             "kind = \"theorem\"\r\n+++\r\nNew body.  \r\nSecond line";
    const loom::ChangeResult saved = loom::saveStatementText(project.root, path, typed, base, {}, at());
    REQUIRE(saved.outcome == loom::ChangeOutcome::Done);
    CHECK(saved.changed == QStringList{"title", "body"});
    CHECK(saved.id == "4KX7");
    CHECK(saved.path == path);

    loom::Statement expected = statement4kx7();
    expected.title = "Energy decay";
    expected.body = "New body.  \nSecond line";
    CHECK(project.read(path) == loom::formatStatement(expected));
    CHECK(project.read(path).endsWith("+++\nNew body.  \nSecond line"));
    CHECK(saved.written == project.read(path));

    SECTION("saving the same text again writes nothing and changes nothing")
    {
        const QByteArray now = project.read(path);
        const loom::ChangeResult again = loom::saveStatementText(project.root, path, now, loom::fileBase(now), {}, at());
        CHECK(again.outcome == loom::ChangeOutcome::Done);
        CHECK(again.changed.isEmpty());
        CHECK(project.read(path) == now);
    }
}

TEST_CASE("A save refuses a file that changed on disk since the editor loaded it", "[editor-saves]")
{
    ProjectFixture project;
    project.statement("4KX7", "theorem");
    const QString path = "Statements/4KX7.tex";
    const QByteArray loaded = project.read(path);
    const QByteArray mine = QByteArray(loaded).replace("Body of 4KX7.", "My edit.");

    SECTION("changed")
    {
        const QByteArray theirs = QByteArray(loaded).replace("Body of 4KX7.", "An agent's edit.");
        project.write(path, theirs);
        const loom::ChangeResult refused = loom::saveStatementText(project.root, path, mine, loom::fileBase(loaded), {}, at());
        CHECK(refused.outcome == loom::ChangeOutcome::Refused);
        CHECK(refused.code == "file-changed");
        CHECK(refused.message == "Statements/4KX7.tex changed on disk while you were editing it, so Loom didn't save over it.");
        CHECK(project.read(path) == theirs);
    }
    SECTION("deleted")
    {
        REQUIRE(QFile::remove(project.root + "/" + path));
        const loom::ChangeResult refused = loom::saveStatementText(project.root, path, mine, loom::fileBase(loaded), {}, at());
        CHECK(refused.code == "file-changed");
        CHECK(!QFile::exists(project.root + "/" + path));

        // *Restore it with my text*: no base, since the editor knows there is no file.
        const loom::ChangeResult restored = loom::saveStatementText(project.root, path, mine, QString(), {}, at());
        CHECK(restored.outcome == loom::ChangeOutcome::Done);
        CHECK(project.read(path).endsWith("+++\nMy edit.\n"));
    }
    SECTION("created, while the editor thought there was none")
    {
        const loom::ChangeResult refused = loom::saveStatementText(project.root, path, mine, QString(), {}, at());
        CHECK(refused.code == "file-changed");
        CHECK(project.read(path) == loaded);
    }
}

TEST_CASE("A save refuses a changed tag and anything that adds an error, keeping the file", "[editor-saves]")
{
    ProjectFixture project;
    project.statement("4KX7", "theorem");
    const QString path = "Statements/4KX7.tex";
    const QByteArray loaded = project.read(path);
    const QString base = loom::fileBase(loaded);

    SECTION("id-changed")
    {
        const QByteArray typed = QByteArray(loaded).replace("id = \"4KX7\"", "id = \"4KX8\"");
        const loom::ChangeResult refused = loom::saveStatementText(project.root, path, typed, base, {}, at());
        CHECK(refused.code == "id-changed");
        CHECK(refused.message == "A statement's tag is permanent; Loom can't change 4KX7 to 4KX8.");
    }
    SECTION("an error, with its line")
    {
        const QByteArray typed = QByteArray(loaded).replace("kind = \"theorem\"", "kind = \"theorm\"");
        const loom::ChangeResult refused = loom::saveStatementText(project.root, path, typed, base, {}, at());
        CHECK(refused.outcome == loom::ChangeOutcome::Refused);
        REQUIRE(!refused.problems.empty());
        CHECK(refused.problems.front().code == "bad-value");
        CHECK(refused.problems.front().file == path);
        CHECK(refused.problems.front().line == 3);
    }
    SECTION("front matter that doesn't parse")
    {
        const QByteArray typed = QByteArray(loaded).replace("title = \"T\"", "title = \"T");
        const loom::ChangeResult refused = loom::saveStatementText(project.root, path, typed, base, {}, at());
        CHECK(refused.outcome == loom::ChangeOutcome::Refused);
        REQUIRE(!refused.problems.empty());
        CHECK(refused.problems.front().line == 4);
    }
    SECTION("an empty body")
    {
        const QByteArray typed = QByteArray(loaded).replace("Body of 4KX7.\n", "\n");
        CHECK(loom::saveStatementText(project.root, path, typed, base, {}, at()).code == "body-empty");
    }
    CHECK(project.read(path) == loaded);
}

TEST_CASE("A raw save can fix a file whose front matter doesn't parse", "[editor-saves]")
{
    ProjectFixture project;
    const QString path = "Statements/4KX7.tex";
    project.write(path, "+++\nid = \"4KX7\"\nkind = \"theorem\"\ntitle = \"T\ncreated = 2026-11-03\n+++\nBody.\n");
    const QString base = loom::fileBase(project.read(path));

    SECTION("fixed in full: written canonical")
    {
        const QByteArray typed = "+++\nid = \"4KX7\"\nkind = \"theorem\"\ntitle = \"T\"\ncreated = 2026-11-03\n+++\nBody.\n";
        const loom::ChangeResult saved = loom::saveStatementText(project.root, path, typed, base, {}, at());
        REQUIRE(saved.outcome == loom::ChangeOutcome::Done);
        CHECK(project.read(path) == typed);
        CHECK(loom::loadContents(project.root).contents.statement("4KX7")->reading.lossless);
    }
    SECTION("still broken in the same way: written as typed, since it adds no error")
    {
        const QByteArray typed = "+++\nid = \"4KX7\"\nkind = \"theorem\"\ntitle = \"T\ncreated = 2026-11-03\n+++\nA better body.\n";
        const loom::ChangeResult saved = loom::saveStatementText(project.root, path, typed, base, {}, at());
        CHECK(saved.outcome == loom::ChangeOutcome::Done);
        CHECK(project.read(path) == typed);
    }
}

TEST_CASE("A save that outdates checks needs Dakota's confirmation, naming the proofs", "[editor-saves][dakota]")
{
    ProjectFixture project;
    project.statement("4KX7", "theorem");
    project.statement("5HN8", "lemma");
    project.proof("4KX7", "4KX7.a", "4KX7", "[\"5HN8\"]");
    makeChecked(project, "4KX7.a");
    const QString path = "Statements/5HN8.tex";
    const QByteArray loaded = project.read(path);
    const QByteArray typed = QByteArray(loaded).replace("Body of 5HN8.", "A stronger bound.");

    const loom::ChangeResult asked = loom::saveStatementText(project.root, path, typed, loom::fileBase(loaded), {}, at());
    CHECK(asked.outcome == loom::ChangeOutcome::Refused);
    CHECK(asked.code == "outdates-checks");
    CHECK(asked.outdatedChecks == QStringList{"4KX7.a"});
    CHECK(project.read(path) == loaded);

    SECTION("confirmed")
    {
        loom::SaveConfirmations confirmed;
        confirmed.outdateChecks = asked.outdatedChecks;
        const loom::ChangeResult saved = loom::saveStatementText(project.root, path, typed, loom::fileBase(loaded), confirmed, at());
        CHECK(saved.outcome == loom::ChangeOutcome::Done);
        CHECK(saved.outdatedChecks == QStringList{"4KX7.a"});
        CHECK(project.read(path).endsWith("A stronger bound.\n"));
        CHECK(!loom::proofChecking(loom::loadContents(project.root).contents, "4KX7.a").standing.checked);
    }
    SECTION("confirmed for other proofs than it outdates now")
    {
        loom::SaveConfirmations confirmed;
        confirmed.outdateChecks = QStringList{};
        CHECK(loom::saveStatementText(project.root, path, typed, loom::fileBase(loaded), confirmed, at()).code == "outdates-checks");
        CHECK(project.read(path) == loaded);
    }
    SECTION("a change outside the fingerprint needs no confirmation")
    {
        const QByteArray retitled = QByteArray(loaded).replace("title = \"T\"", "title = \"A uniform bound\"");
        CHECK(loom::saveStatementText(project.root, path, retitled, loom::fileBase(loaded), {}, at()).outcome
              == loom::ChangeOutcome::Done);
    }
}

TEST_CASE("Saving a proof: its ID and statement are permanent, and failed is Dakota's click", "[editor-saves][dakota]")
{
    ProjectFixture project;
    project.statement("4KX7", "theorem");
    project.statement("5HN8", "lemma");
    project.proof("4KX7", "4KX7.a", "4KX7", "[]");
    const QString path = "Proofs/4KX7/4KX7.a.tex";
    const QByteArray loaded = project.read(path);
    const QString base = loom::fileBase(loaded);

    SECTION("id-changed")
    {
        const QByteArray typed = QByteArray(loaded).replace("id = \"4KX7.a\"", "id = \"4KX7.b\"");
        CHECK(loom::saveProofText(project.root, path, typed, base, {}, at()).message
              == "A proof's ID is permanent; Loom can't change 4KX7.a to 4KX7.b.");
    }
    SECTION("proves changed")
    {
        const QByteArray typed = QByteArray(loaded).replace("proves = \"4KX7\"", "proves = \"5HN8\"");
        const loom::ChangeResult refused = loom::saveProofText(project.root, path, typed, base, {}, at());
        CHECK(refused.code == "id-changed");
        CHECK(refused.message == "4KX7.a proves 4KX7, and which statement a proof proves is permanent; Loom can't change it to 5HN8.");
    }
    SECTION("premises and body")
    {
        const QByteArray typed = QByteArray(loaded).replace("premises = []", "premises = [\"5HN8\"]").replace("Proof.", "By 5HN8.");
        const loom::ChangeResult saved = loom::saveProofText(project.root, path, typed, base, {}, at());
        REQUIRE(saved.outcome == loom::ChangeOutcome::Done);
        CHECK(saved.changed == QStringList{"premises", "body"});
        CHECK(project.read(path) == typed);
    }
    SECTION("failed, on a checked proof: one refusal names both clicks")
    {
        makeChecked(project, "4KX7.a");
        const QByteArray typed = QByteArray(loaded).replace("level = \"complete\"", "level = \"failed\"");
        const loom::ChangeResult asked = loom::saveProofText(project.root, path, typed, base, {}, at());
        CHECK(asked.code == "dakota-only");
        CHECK(asked.outdatedChecks == QStringList{"4KX7.a"});
        CHECK(project.read(path) == loaded);

        loom::SaveConfirmations onlyFailed;
        onlyFailed.failedConfirmed = true;
        CHECK(loom::saveProofText(project.root, path, typed, base, onlyFailed, at()).code == "outdates-checks");

        loom::SaveConfirmations both = onlyFailed;
        both.outdateChecks = asked.outdatedChecks;
        CHECK(loom::saveProofText(project.root, path, typed, base, both, at()).outcome == loom::ChangeOutcome::Done);
        CHECK(project.read(path).contains("level = \"failed\""));
    }
}

TEST_CASE("Setting refuted_by in raw text is Dakota's click; clearing it isn't", "[editor-saves][dakota]")
{
    ProjectFixture project;
    project.statement("4KX7", "theorem");
    project.statement("5HN8", "lemma");
    project.proof("5HN8", "5HN8.a", "5HN8", "[]");
    const QString path = "Statements/4KX7.tex";
    const QByteArray loaded = project.read(path);
    const QByteArray typed = QByteArray(loaded).replace("created = 2026-11-03", "refuted_by = \"5HN8.a\"\nrefuted_on = 2026-11-05\ncreated = 2026-11-03");

    CHECK(loom::saveStatementText(project.root, path, typed, loom::fileBase(loaded), {}, at()).code == "dakota-only");
    loom::SaveConfirmations confirmed;
    confirmed.refutationConfirmed = true;
    REQUIRE(loom::saveStatementText(project.root, path, typed, loom::fileBase(loaded), confirmed, at()).outcome
            == loom::ChangeOutcome::Done);
    CHECK(loom::loadContents(project.root).contents.statement("4KX7")->reading.document.refutedBy == "5HN8.a");

    const QByteArray refuted = project.read(path);
    const loom::ChangeResult undone = loom::saveStatementText(project.root, path, loaded, loom::fileBase(refuted), {}, at());
    CHECK(undone.outcome == loom::ChangeOutcome::Done);
    CHECK(undone.changed == QStringList{"refuted_by", "refuted_on"});
}

TEST_CASE("Verbose versions: created, their body saved, marked up to date", "[editor-saves]")
{
    ProjectFixture project;
    project.statement("4KX7", "theorem");
    project.proof("4KX7", "4KX7.a", "4KX7", "[]");
    const QString path = "Proofs/4KX7/4KX7.a.verbose.tex";
    const auto fingerprintNow = [&] { return *loom::proofFingerprint(loom::loadContents(project.root).contents, "4KX7.a"); };
    const QString first = fingerprintNow();

    const loom::ChangeResult created = loom::saveVerboseBody(project.root, "4KX7.a", "Step by step.\n", QString(), at());
    REQUIRE(created.outcome == loom::ChangeOutcome::Done);
    CHECK(created.path == path);
    loom::VerboseProof expected;
    expected.proof = "4KX7.a";
    expected.fingerprint = first;
    expected.created = QDate(2026, 11, 5);
    expected.body = "Step by step.\n";
    CHECK(project.read(path) == loom::formatVerboseProof(expected));

    SECTION("creating again with no base refuses: it exists now")
    {
        CHECK(loom::saveVerboseBody(project.root, "4KX7.a", "Other.\n", QString(), at()).code == "file-changed");
    }
    SECTION("the proof changes: saving the body keeps the old fingerprint until Mark up to date")
    {
        const QByteArray proof = project.read("Proofs/4KX7/4KX7.a.tex");
        project.write("Proofs/4KX7/4KX7.a.tex", QByteArray(proof).replace("Proof.", "A new proof."));
        REQUIRE(fingerprintNow() != first);

        const loom::ChangeResult saved = loom::saveVerboseBody(project.root, "4KX7.a", "More steps.\n", loom::fileBase(project.read(path)), at());
        REQUIRE(saved.outcome == loom::ChangeOutcome::Done);
        CHECK(saved.changed == QStringList{"body"});
        CHECK(project.read(path).contains(("fingerprint = \"" + first + "\"").toUtf8()));

        const QString stale = loom::fileBase(project.read(path));
        CHECK(loom::markVerboseUpToDate(project.root, "4KX7.a", "0000", at()).code == "file-changed");
        const loom::ChangeResult marked = loom::markVerboseUpToDate(project.root, "4KX7.a", stale, at());
        REQUIRE(marked.outcome == loom::ChangeOutcome::Done);
        CHECK(marked.changed == QStringList{"fingerprint"});
        CHECK(project.read(path).contains(("fingerprint = \"" + fingerprintNow() + "\"").toUtf8()));
        CHECK(project.read(path).endsWith("More steps.\n"));
    }
    SECTION("refusals")
    {
        CHECK(loom::saveVerboseBody(project.root, "4KX7.b", "x\n", QString(), at()).code == "ref-unknown");
        CHECK(loom::markVerboseUpToDate(project.root, "4KX7.b", QString(), at()).code == "ref-unknown");
        CHECK(loom::saveVerboseBody(project.root, "4KX7.a", "  \n", loom::fileBase(project.read(path)), at()).outcome
              == loom::ChangeOutcome::Refused);
    }
}
