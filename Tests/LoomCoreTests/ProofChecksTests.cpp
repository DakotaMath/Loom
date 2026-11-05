// A proof's checks in its project (Specs/m1-core.md, "Checks"): its fingerprint, adding checks
// (`loom add-check`), and refusing edits that would outdate checks (`--outdate-checks`).

#include "LoomCore/ProofChecks.h"

#include "LoomCore/Edits.h"
#include "LoomCore/Fingerprint.h"

#include "ProjectFixture.h"

#include <QFileInfo>
#include <QTimeZone>

#include <catch2/catch_test_macros.hpp>

using loomtest::ProjectFixture;

namespace {

/// Change options at a fixed time, 2026-11-04 10:12:44 Central, plus `minutes`.
loom::ChangeOptions at(int minutes)
{
    loom::ChangeOptions options;
    options.today = QDate(2026, 11, 4);
    options.now = QDateTime(QDate(2026, 11, 4), QTime(10, 12, 44), QTimeZone::fromSecondsAheadOfUtc(-6 * 3600)).addSecs(minutes * 60);
    return options;
}

/// A project with theorem 4KX7, proved by 4KX7.a from lemma 5HN8, and lemma 9D3F with a proof.
struct CheckedProject : ProjectFixture {
    CheckedProject()
    {
        statement("4KX7", "theorem");
        statement("5HN8", "lemma");
        statement("9D3F", "lemma");
        proof("4KX7", "4KX7.a", "4KX7", "[\"5HN8\"]");
        proof("9D3F", "9D3F.a", "9D3F", "[]");
    }

    /// The fingerprint of `id` as the project is now.
    QString fingerprintOf(const QString& id) const
    {
        return loom::proofFingerprint(loom::loadContents(root).contents, id).value_or(QString());
    }

    /// An agent's pass of `id` with the right fingerprint and a record.
    loom::NewCheck agentPass(const QString& id) const
    {
        loom::NewCheck request;
        request.by = "agent:claude-opus-5-5";
        request.verdict = "pass";
        request.fingerprint = fingerprintOf(id);
        request.record = "Records/2026-11/job-0142.md";
        return request;
    }

    /// Makes `id` checked: an agent's pass at `minutes`, then Dakota's a minute later.
    void makeChecked(const QString& id, int minutes = 0) const
    {
        REQUIRE(loom::addCheck(root, id, agentPass(id), false, at(minutes)).change.outcome == loom::ChangeOutcome::Done);
        loom::NewCheck pass;
        pass.by = "dakota";
        pass.verdict = "pass";
        const loom::CheckAdded added = loom::addCheck(root, id, pass, true, at(minutes + 1));
        REQUIRE(added.change.outcome == loom::ChangeOutcome::Done);
        REQUIRE(added.standing.checked);
    }
};

} // namespace

TEST_CASE("A proof's fingerprint in its project", "[proof-checks]")
{
    const CheckedProject project;
    const loom::ProjectContents contents = loom::loadContents(project.root).contents;

    loom::FingerprintInput input;
    input.level = "complete";
    input.statementTag = "4KX7";
    input.statementBody = "Body of 4KX7.\n";
    input.premises = {{"5HN8", QByteArray("Body of 5HN8.\n")}};
    input.proofBody = "Proof.\n";
    CHECK(loom::proofFingerprint(contents, "4KX7.a") == loom::fingerprint(input));

    SECTION("a premise that names nothing leaves no fingerprint, and says why")
    {
        project.proof("4KX7", "4KX7.b", "4KX7", "[\"ZZZZ\"]");
        QString why;
        CHECK_FALSE(loom::proofFingerprint(loom::loadContents(project.root).contents, "4KX7.b", &why));
        CHECK(why == "Loom can't compute the fingerprint of 4KX7.b: its premise `ZZZZ` isn't a statement in this project.");
    }
}

TEST_CASE("add-check appends an agent's check", "[proof-checks]")
{
    const CheckedProject project;
    const QString fp = project.fingerprintOf("4KX7.a");
    const loom::CheckAdded added = loom::addCheck(project.root, "4kx7.A", project.agentPass("4KX7.a"), false, at(0));

    REQUIRE(added.change.outcome == loom::ChangeOutcome::Done);
    CHECK(added.change.id == "4KX7.a");
    CHECK(added.change.path == "Proofs/4KX7/4KX7.a.checks.toml");
    CHECK(added.index == 0);
    CHECK(added.standing.countingAgent == 0);
    CHECK_FALSE(added.standing.checked);
    CHECK(project.read("Proofs/4KX7/4KX7.a.checks.toml") ==
          "[[checks]]\nby = \"agent:claude-opus-5-5\"\ndate = 2026-11-04T10:12:44-06:00\nfingerprint = \"" + fp.toUtf8()
              + "\"\nverdict = \"pass\"\nrecord = \"Records/2026-11/job-0142.md\"\n");

    SECTION("Dakota's newer pass makes the proof checked")
    {
        loom::NewCheck pass;
        pass.by = "dakota";
        pass.verdict = "pass";
        const loom::CheckAdded second = loom::addCheck(project.root, "4KX7.a", pass, true, at(30));
        REQUIRE(second.change.outcome == loom::ChangeOutcome::Done);
        CHECK(second.index == 1);
        CHECK(second.standing.countingDakota == 1);
        CHECK(second.standing.checked);
        CHECK(loom::proofChecking(loom::loadContents(project.root).contents, "4KX7.a").standing.checked);
    }
    SECTION("a fail stops the proof supporting its statement")
    {
        loom::NewCheck fail = project.agentPass("4KX7.a");
        fail.verdict = "fail";
        const loom::CheckAdded second = loom::addCheck(project.root, "4KX7.a", fail, false, at(30));
        REQUIRE(second.change.outcome == loom::ChangeOutcome::Done);
        CHECK_FALSE(second.standing.supports);
    }
}

TEST_CASE("add-check keeps the entries already there byte for byte", "[proof-checks]")
{
    const CheckedProject project;
    const QString fp = project.fingerprintOf("9D3F.a");
    const QByteArray before = "# Written by hand.\n[[checks]]\nby   = \"agent:claude-opus-5-5\"\ndate = 2026-11-03T09:00:00-06:00\n"
                              "fingerprint = \"" + fp.toUtf8() + "\"\nverdict = \"pass\"\nrecord = 'Records/2026-11/job-0001.md'";
    project.write("Proofs/9D3F/9D3F.a.checks.toml", before);
    loom::NewCheck pass;
    pass.by = "dakota";
    pass.verdict = "needs-changes";
    pass.changes = "Say where k = 0 is used.\r\nAnd why it is enough.";
    const loom::CheckAdded added = loom::addCheck(project.root, "9D3F.a", pass, true, at(0));

    REQUIRE(added.change.outcome == loom::ChangeOutcome::Done);
    CHECK(added.index == 1);
    CHECK(project.read("Proofs/9D3F/9D3F.a.checks.toml") ==
          before + "\n\n[[checks]]\nby = \"dakota\"\ndate = 2026-11-04T10:12:44-06:00\nfingerprint = \"" + fp.toUtf8()
              + "\"\nverdict = \"needs-changes\"\nchanges = \"\"\"\nSay where k = 0 is used.\nAnd why it is enough.\n\"\"\"\n");
}

TEST_CASE("add-check refuses what the rules don't allow, writing nothing", "[proof-checks]")
{
    const CheckedProject project;
    const QString path = "Proofs/4KX7/4KX7.a.checks.toml";
    const QString fp = project.fingerprintOf("4KX7.a");
    loom::NewCheck request = project.agentPass("4KX7.a");
    bool confirmed = false;
    loom::ChangeOptions options = at(0);
    QString code;
    QString message;

    SECTION("an agent without a fingerprint")
    {
        request.fingerprint.clear();
        code = "fingerprint-needed";
        message = "An agent's check needs `--fingerprint`: what `loom fingerprint 4KX7.a` printed before you read the proof.";
    }
    SECTION("a proof changed after it was read")
    {
        request.fingerprint = "3c8e1f0a92d4";
        code = "proof-changed";
        message = "4KX7.a changed after you read it (you read 3c8e1f0a92d4, it is now " + fp + "). Read it again before checking it.";
    }
    SECTION("an agent without a record")
    {
        request.record.clear();
        code = "check-record-missing";
    }
    SECTION("a verdict that isn't one")
    {
        request.verdict = "accept";
        code = "check-verdict";
    }
    SECTION("a `by` that is neither Dakota nor an agent")
    {
        request.by = "Claude";
        code = "check-by";
    }
    SECTION("Dakota's check without his confirmation")
    {
        request = {};
        request.by = "dakota";
        request.verdict = "pass";
        code = "dakota-only";
        message = "Only Dakota can add Dakota's checks, and only at a terminal.";
    }
    SECTION("Dakota's needs-changes that doesn't say what to change")
    {
        request = {};
        request.by = "dakota";
        request.verdict = "needs-changes";
        request.changes = "  \n";
        confirmed = true;
        code = "check-changes-missing";
    }
    SECTION("changes on a pass")
    {
        request.changes = "Nothing.";
        code = "check-changes-unexpected";
    }
    SECTION("a check dated before the last one")
    {
        REQUIRE(loom::addCheck(project.root, "4KX7.a", request, false, at(10)).change.outcome == loom::ChangeOutcome::Done);
        project.write("expected", project.read(path));
        code = "check-order";
    }
    SECTION("a proof that doesn't exist")
    {
        const loom::CheckAdded added = loom::addCheck(project.root, "4KX7.b", request, false, options);
        CHECK(added.change.code == "ref-unknown");
        CHECK(added.change.message == "`4KX7.b` isn't a proof in this project.");
        return;
    }

    const bool hadFile = QFileInfo::exists(project.root + u'/' + path);
    const loom::CheckAdded added = loom::addCheck(project.root, "4KX7.a", request, confirmed, options);
    CHECK(added.change.outcome == loom::ChangeOutcome::Refused);
    CHECK(added.change.code == code);
    if (!message.isEmpty())
        CHECK(added.change.message == message);
    if (hadFile)
        CHECK(project.read(path) == project.read("expected"));
    else
        CHECK_FALSE(QFileInfo::exists(project.root + u'/' + path));
}

TEST_CASE("add-check refuses a proof without a fingerprint, a history it can't rewrite, and a stale fingerprint from Dakota",
          "[proof-checks]")
{
    const CheckedProject project;
    loom::NewCheck dakota;
    dakota.by = "dakota";
    dakota.verdict = "pass";

    SECTION("a premise that names nothing: no fingerprint, so no check")
    {
        project.proof("4KX7", "4KX7.b", "4KX7", "[\"ZZZZ\"]");
        const loom::CheckAdded added = loom::addCheck(project.root, "4KX7.b", dakota, true, at(0));
        CHECK(added.change.outcome == loom::ChangeOutcome::Refused);
        CHECK(added.change.code == "no-fingerprint");
        CHECK(added.change.message == "Loom can't compute the fingerprint of 4KX7.b: its premise `ZZZZ` isn't a statement in this project.");
        CHECK_FALSE(QFileInfo::exists(project.root + "/Proofs/4KX7/4KX7.b.checks.toml"));
    }
    SECTION("an existing history that isn't TOML is never rewritten")
    {
        const QByteArray broken = "[[checks]\nby = \"dakota\"\n";
        project.write("Proofs/4KX7/4KX7.a.checks.toml", broken);
        const loom::CheckAdded added = loom::addCheck(project.root, "4KX7.a", dakota, true, at(0));
        CHECK(added.change.outcome == loom::ChangeOutcome::Refused);
        CHECK(added.change.code == "not-editable");
        CHECK(project.read("Proofs/4KX7/4KX7.a.checks.toml") == broken);
    }
    SECTION("Dakota's optional fingerprint must be the current one too")
    {
        dakota.fingerprint = "3c8e1f0a92d4";
        const loom::CheckAdded added = loom::addCheck(project.root, "4KX7.a", dakota, true, at(0));
        CHECK(added.change.outcome == loom::ChangeOutcome::Refused);
        CHECK(added.change.code == "proof-changed");
        CHECK_FALSE(QFileInfo::exists(project.root + "/Proofs/4KX7/4KX7.a.checks.toml"));
    }
}

TEST_CASE("Edits that would outdate checks need Dakota's confirmation", "[proof-checks][edits]")
{
    const CheckedProject project;
    project.makeChecked("4KX7.a");
    const QByteArray statementBefore = project.read("Statements/4KX7.tex");
    const QByteArray proofBefore = project.read("Proofs/4KX7/4KX7.a.tex");
    const QString refusal = "Loom didn't change %1, because it would outdate the checks of 4KX7.a: it is checked, and what its "
                            "checks read would change. Only Dakota can outdate checks (`--outdate-checks`, at a terminal).";

    SECTION("the proved statement's body")
    {
        loom::StatementEdit edit;
        edit.body = "A stronger statement.";
        loom::ChangeResult result = loom::editStatement(project.root, "4KX7", edit, at(5));
        CHECK(result.outcome == loom::ChangeOutcome::Refused);
        CHECK(result.code == "outdates-checks");
        CHECK(result.outdatedChecks == QStringList{"4KX7.a"});
        CHECK(result.message == refusal.arg("4KX7"));
        CHECK(project.read("Statements/4KX7.tex") == statementBefore);

        edit.outdateChecks = QStringList{"4KX7.a"};
        result = loom::editStatement(project.root, "4KX7", edit, at(5));
        CHECK(result.outcome == loom::ChangeOutcome::Done);
        CHECK(result.outdatedChecks == QStringList{"4KX7.a"});
        CHECK_FALSE(loom::proofChecking(loom::loadContents(project.root).contents, "4KX7.a").standing.checked);
    }
    SECTION("a premise's body")
    {
        loom::StatementEdit edit;
        edit.body = "A weaker lemma.";
        const loom::ChangeResult result = loom::editStatement(project.root, "5HN8", edit, at(5));
        CHECK(result.code == "outdates-checks");
        CHECK(result.message == refusal.arg("5HN8"));
    }
    SECTION("the proof's body")
    {
        loom::ProofEdit edit;
        edit.body = "A different proof.";
        const loom::ChangeResult result = loom::editProof(project.root, "4KX7.a", edit, at(5));
        CHECK(result.code == "outdates-checks");
        CHECK(result.message == refusal.arg("4KX7.a"));
        CHECK(project.read("Proofs/4KX7/4KX7.a.tex") == proofBefore);
    }
    SECTION("the proof's premises")
    {
        loom::ProofEdit edit;
        edit.addPremises = {"9D3F"};
        CHECK(loom::editProof(project.root, "4KX7.a", edit, at(5)).code == "outdates-checks");
    }
    SECTION("the proof's level (A18)")
    {
        loom::ProofEdit edit;
        edit.level = "sketch";
        loom::ChangeResult result = loom::editProof(project.root, "4KX7.a", edit, at(5));
        CHECK(result.outcome == loom::ChangeOutcome::Refused);
        CHECK(result.code == "outdates-checks");
        CHECK(result.outdatedChecks == QStringList{"4KX7.a"});
        CHECK(result.message == refusal.arg("4KX7.a"));
        CHECK(project.read("Proofs/4KX7/4KX7.a.tex") == proofBefore);

        edit.outdateChecks = QStringList{"4KX7.a"};
        result = loom::editProof(project.root, "4KX7.a", edit, at(5));
        CHECK(result.outcome == loom::ChangeOutcome::Done);
        CHECK(result.changed == QStringList{"level"});
        CHECK(result.outdatedChecks == QStringList{"4KX7.a"});
        CHECK_FALSE(loom::proofChecking(loom::loadContents(project.root).contents, "4KX7.a").standing.checked);
    }
    SECTION("marking a checked proof failed needs both of Dakota's clicks (A16, A18)")
    {
        loom::ProofEdit edit;
        edit.level = "failed";
        CHECK(loom::editProof(project.root, "4KX7.a", edit, at(5)).code == "dakota-only");
        edit.failedConfirmed = true;
        CHECK(loom::editProof(project.root, "4KX7.a", edit, at(5)).code == "outdates-checks");
        CHECK(project.read("Proofs/4KX7/4KX7.a.tex") == proofBefore);
        edit.outdateChecks = QStringList{"4KX7.a"};
        CHECK(loom::editProof(project.root, "4KX7.a", edit, at(5)).outcome == loom::ChangeOutcome::Done);
    }
    SECTION("what isn't in the fingerprint edits freely")
    {
        loom::StatementEdit title;
        title.title = "A new title";
        title.verified = true;
        CHECK(loom::editStatement(project.root, "4KX7", title, at(5)).outcome == loom::ChangeOutcome::Done);
        loom::ProofEdit verified;
        verified.verified = true;
        const loom::ChangeResult result = loom::editProof(project.root, "4KX7.a", verified, at(5));
        CHECK(result.outcome == loom::ChangeOutcome::Done);
        CHECK(result.outdatedChecks.isEmpty());
        loom::StatementEdit trailing;
        trailing.body = "Body of 4KX7.   \r\n\r\n"; // Whitespace the fingerprint ignores.
        CHECK(loom::editStatement(project.root, "4KX7", trailing, at(5)).outcome == loom::ChangeOutcome::Done);
        CHECK(loom::proofChecking(loom::loadContents(project.root).contents, "4KX7.a").standing.checked);
    }
    SECTION("a proof that isn't checked edits freely")
    {
        REQUIRE(loom::addCheck(project.root, "9D3F.a", project.agentPass("9D3F.a"), false, at(5)).change.outcome
                == loom::ChangeOutcome::Done);
        loom::ProofEdit edit;
        edit.body = "A different proof.";
        CHECK(loom::editProof(project.root, "9D3F.a", edit, at(6)).outcome == loom::ChangeOutcome::Done);
    }
    SECTION("the refusal names every proof it would outdate")
    {
        project.proof("9D3F", "9D3F.b", "9D3F", "[\"5HN8\"]");
        project.makeChecked("9D3F.b", 10);
        loom::StatementEdit edit;
        edit.body = "A weaker lemma.";
        const loom::ChangeResult result = loom::editStatement(project.root, "5HN8", edit, at(20));
        CHECK(result.outdatedChecks == QStringList{"4KX7.a", "9D3F.b"});
        CHECK(result.message.contains("the checks of 4KX7.a and 9D3F.b: they are checked, and what their checks read would change."));
    }
}

TEST_CASE("Checks of a proof that doesn't exist are an error", "[proof-checks][contents]")
{
    const CheckedProject project;
    project.write("Proofs/4KX7/4KX7.c.checks.toml", "");
    const loom::OpenedProject opened = loom::openProject(project.root);
    const std::vector<loom::Problem> problems = loom::checkContents(opened.config, loom::loadContents(project.root).contents);
    REQUIRE(problems.size() == 1);
    CHECK(problems[0].code == "checks-orphan");
    CHECK(problems[0].file == "Proofs/4KX7/4KX7.c.checks.toml");
    CHECK(problems[0].message == "`Proofs/4KX7/4KX7.c.checks.toml` holds checks of 4KX7.c, which doesn't exist.");
}
