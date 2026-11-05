// Status changes (Specs/m2-statements-review.md, "What LoomCore gains in M2": "Status changes", and
// "Rules: Status changes"): raises and drops, the order they are listed in, and the changes every
// ChangeResult carries, done or refused for one of Dakota's clicks, so his dialogs can name them.

#include "LoomCore/StatusChanges.h"

#include "LoomCore/EditorSaves.h"
#include "LoomCore/Edits.h"
#include "LoomCore/ProofChecks.h"
#include "LoomCore/ProjectContents.h"

#include "ProjectFixture.h"

#include <QTimeZone>

#include <catch2/catch_test_macros.hpp>

using loom::Status;
using loom::StatusDirection;
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

/// A graph node with only what statusChanges() reads.
loom::StatementNode node(const QString& id, Status status, const QString& title = QStringLiteral("T"))
{
    loom::StatementNode n;
    n.id = id;
    n.title = title;
    n.status = status;
    return n;
}

/// The changes as `ID before→after mark` strings, for comparing in one line.
QStringList described(const std::vector<loom::StatusChange>& changes)
{
    QStringList out;
    for (const loom::StatusChange& change : changes) {
        const char* mark = change.direction == StatusDirection::Raise ? " up" : change.direction == StatusDirection::Drop ? " down" : "";
        out << QStringLiteral("%1 %2>%3%4").arg(change.id, loom::statusName(change.before), loom::statusName(change.after),
                                              QString::fromLatin1(mark));
    }
    return out;
}

/// Theorem 4KX7 proved by 4KX7.a from lemma 5HN8 (proved by 5HN8.a); 5HN8.a carries an agent's pass.
struct ChainProject : ProjectFixture {
    ChainProject()
    {
        statement("4KX7", "theorem");
        statement("5HN8", "lemma");
        statement("2M9Q", "definition");
        proof("4KX7", "4KX7.a", "4KX7", "[\"5HN8\"]");
        proof("5HN8", "5HN8.a", "5HN8", "[]");
    }

    QString fingerprintOf(const QString& id) const
    {
        return loom::proofFingerprint(loom::loadContents(root).contents, id).value_or(QString());
    }

    /// Adds a check; Dakota's are confirmed.
    void check(const QString& id, const QString& by, int minutes) const
    {
        loom::NewCheck request;
        request.by = by;
        request.verdict = QStringLiteral("pass");
        if (by != u"dakota") {
            request.fingerprint = fingerprintOf(id);
            request.record = QStringLiteral("Records/2026-11/job-0142.md");
        }
        REQUIRE(loom::addCheck(root, id, request, by == u"dakota", at(minutes)).change.outcome == loom::ChangeOutcome::Done);
    }
};

} // namespace

TEST_CASE("Which way a status moves", "[status-changes]")
{
    // "Raises go up the order proved > conditional > unchecked > sketch > heuristic > open".
    CHECK(loom::statusDirection(Status::Unchecked, Status::Proved) == StatusDirection::Raise);
    CHECK(loom::statusDirection(Status::Open, Status::Heuristic) == StatusDirection::Raise);
    CHECK(loom::statusDirection(Status::Sketch, Status::Conditional) == StatusDirection::Raise);
    CHECK(loom::statusDirection(Status::Proved, Status::Unchecked) == StatusDirection::Drop);
    CHECK(loom::statusDirection(Status::Proved, Status::Conditional) == StatusDirection::Drop);
    CHECK(loom::statusDirection(Status::Heuristic, Status::Open) == StatusDirection::Drop);
    // "and cited > extracted".
    CHECK(loom::statusDirection(Status::Extracted, Status::Cited) == StatusDirection::Raise);
    CHECK(loom::statusDirection(Status::Cited, Status::Extracted) == StatusDirection::Drop);
    // "a change to or from false, or between no status and a status, is neither".
    CHECK(loom::statusDirection(Status::Open, Status::False) == StatusDirection::Neither);
    CHECK(loom::statusDirection(Status::False, Status::Proved) == StatusDirection::Neither);
    CHECK(loom::statusDirection(Status::None, Status::Open) == StatusDirection::Neither);
    CHECK(loom::statusDirection(Status::Proved, Status::None) == StatusDirection::Neither);
    // The two orders don't compare.
    CHECK(loom::statusDirection(Status::Cited, Status::Proved) == StatusDirection::Neither);
    CHECK(loom::statusDirection(Status::Open, Status::Extracted) == StatusDirection::Neither);
    CHECK(loom::statusDirection(Status::Proved, Status::Proved) == StatusDirection::Neither);
}

TEST_CASE("Status changes between two graphs", "[status-changes]")
{
    loom::ProjectGraph before;
    loom::ProjectGraph after;
    before.statements = {node("1AAA", Status::Proved), node("2BBB", Status::Unchecked), node("3CCC", Status::Open),
                         node("4DDD", Status::Extracted), node("5EEE", Status::Sketch), node("6FFF", Status::Proved),
                         node("7GGG", Status::Heuristic, "Old title"), node("8HHH", Status::Open)};
    after.statements = {node("1AAA", Status::Unchecked), node("2BBB", Status::Proved), node("3CCC", Status::False),
                        node("4DDD", Status::Cited), node("5EEE", Status::Sketch), node("6FFF", Status::Conditional),
                        node("7GGG", Status::Open, "New title"), node("9JJJ", Status::Open)};

    const std::vector<loom::StatusChange> changes = loom::statusChanges(before, after);
    // "Sorted drops first, then raises, then the rest, each by tag"; an unchanged status isn't listed,
    // and a statement in one graph only has no status in the other.
    CHECK(described(changes)
          == QStringList{"1AAA proved>unchecked down", "6FFF proved>conditional down", "7GGG heuristic>open down",
                         "2BBB unchecked>proved up", "4DDD extracted>cited up", "3CCC open>false", "8HHH open>", "9JJJ >open"});
    // The title is the later one's when it is there.
    CHECK(changes[2].title == "New title");
    CHECK(loom::statusChanges(before, before).empty());
}

TEST_CASE("Dakota's check carries what it would do before he confirms it", "[status-changes]")
{
    const ChainProject project;
    project.check("5HN8.a", "agent:claude-opus-5-5", 0);
    project.check("5HN8.a", "dakota", 1);
    project.check("4KX7.a", "agent:claude-opus-5-5", 2);
    const QByteArray before = project.read("Proofs/4KX7/4KX7.a.checks.toml");

    loom::NewCheck pass;
    pass.by = "dakota";
    pass.verdict = "pass";
    pass.fingerprint = project.fingerprintOf("4KX7.a");

    // Unconfirmed: refused, nothing written, but it says what it would do.
    const loom::CheckAdded asked = loom::addCheck(project.root, "4KX7.a", pass, false, at(3));
    CHECK(asked.change.outcome == loom::ChangeOutcome::Refused);
    CHECK(asked.change.code == "dakota-only");
    CHECK(asked.change.message == "Only Dakota can add Dakota's checks, and only at a terminal.");
    CHECK(described(asked.change.statusChanges) == QStringList{"4KX7 unchecked>proved up"});
    CHECK(asked.standing.checked);
    CHECK(asked.check.byDakota());
    CHECK(asked.check.verdict == "pass");
    CHECK(project.read("Proofs/4KX7/4KX7.a.checks.toml") == before);

    // Confirmed: done, with the same changes.
    const loom::CheckAdded done = loom::addCheck(project.root, "4KX7.a", pass, true, at(3));
    REQUIRE(done.change.outcome == loom::ChangeOutcome::Done);
    CHECK(described(done.change.statusChanges) == QStringList{"4KX7 unchecked>proved up"});

    // A later needs-changes drops it back.
    loom::NewCheck changes = pass;
    changes.verdict = "needs-changes";
    changes.changes = "Step 3 needs the bound from 5HN8 stated.";
    const loom::CheckAdded dropped = loom::addCheck(project.root, "4KX7.a", changes, false, at(4));
    CHECK(dropped.change.code == "dakota-only");
    CHECK_FALSE(dropped.standing.checked);
    CHECK(described(dropped.change.statusChanges) == QStringList{"4KX7 proved>unchecked down"});
}

TEST_CASE("Dakota is asked about a check only once the rules allow it", "[status-changes]")
{
    const ChainProject project;
    loom::NewCheck request;
    request.by = "dakota";
    request.verdict = "pass";

    SECTION("a proof that changed after it was shown")
    {
        request.fingerprint = "000000000000";
        CHECK(loom::addCheck(project.root, "4KX7.a", request, false, at(0)).change.code == "proof-changed");
    }
    SECTION("needs-changes without the changes")
    {
        request.verdict = "needs-changes";
        CHECK(loom::addCheck(project.root, "4KX7.a", request, false, at(0)).change.code == "check-changes-missing");
    }
}

TEST_CASE("A refutation carries what it would do before Dakota confirms it", "[status-changes]")
{
    const ChainProject project;
    const QByteArray before = project.read("Statements/5HN8.tex");

    const loom::ChangeResult asked = loom::refuteStatement(project.root, "5HN8", "4KX7.a", false, at(0));
    CHECK(asked.code == "dakota-only");
    CHECK(asked.message == "Only Dakota can refute a statement, and only at a terminal.");
    CHECK(described(asked.statusChanges) == QStringList{"5HN8 unchecked>false"});
    CHECK(project.read("Statements/5HN8.tex") == before);

    // A rule's refusal comes first: a definition has no status to make false.
    CHECK(loom::refuteStatement(project.root, "2M9Q", "4KX7.a", false, at(0)).code == "refuted-no-status");

    const loom::ChangeResult done = loom::refuteStatement(project.root, "5HN8", "4KX7.a", true, at(0));
    REQUIRE(done.outcome == loom::ChangeOutcome::Done);
    CHECK(described(done.statusChanges) == QStringList{"5HN8 unchecked>false"});
}

TEST_CASE("Saves and edits carry their status changes", "[status-changes]")
{
    const ChainProject project;
    project.check("5HN8.a", "agent:claude-opus-5-5", 0);
    project.check("5HN8.a", "dakota", 1);
    project.check("4KX7.a", "agent:claude-opus-5-5", 2);
    project.check("4KX7.a", "dakota", 3);

    SECTION("a save that outdates checks, asked and confirmed")
    {
        const QString path = QStringLiteral("Statements/5HN8.tex");
        const QByteArray loaded = project.read(path);
        QByteArray text = loaded;
        text.replace("Body of 5HN8.", "A new body of 5HN8.");
        const loom::ChangeResult asked = loom::saveStatementText(project.root, path, text, loom::fileBase(loaded), {}, at(4));
        CHECK(asked.code == "outdates-checks");
        CHECK(described(asked.statusChanges) == QStringList{"4KX7 proved>unchecked down", "5HN8 proved>unchecked down"});

        loom::SaveConfirmations confirmed;
        confirmed.outdateChecks = asked.outdatedChecks;
        const loom::ChangeResult done = loom::saveStatementText(project.root, path, text, loom::fileBase(loaded), confirmed, at(4));
        REQUIRE(done.outcome == loom::ChangeOutcome::Done);
        CHECK(described(done.statusChanges) == described(asked.statusChanges));
    }
    SECTION("marking a proof failed")
    {
        loom::ProofEdit edit;
        edit.level = "failed";
        const loom::ChangeResult asked = loom::editProof(project.root, "5HN8.a", edit, at(4));
        CHECK(asked.code == "dakota-only");
        CHECK(described(asked.statusChanges) == QStringList{"4KX7 proved>conditional down", "5HN8 proved>open down"});
    }
    SECTION("a change that leaves every status as it was")
    {
        loom::StatementEdit edit;
        edit.title = "A better title";
        const loom::ChangeResult done = loom::editStatement(project.root, "4KX7", edit, at(4));
        REQUIRE(done.outcome == loom::ChangeOutcome::Done);
        CHECK(done.statusChanges.empty());
    }
}
