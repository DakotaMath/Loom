// Check histories (Specs/m1-core.md, "Checks"): the verdict table, reading and writing the file,
// and merging two versions.

#include "LoomCore/Checks.h"

#include "TestSupport.h"

#include <QTimeZone>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace {

const QString nowFp = QStringLiteral("3c8e1f0a92d4");
const QString oldFp = QStringLiteral("77b2e9c0d415");

/// 2026-11-04 10:00 at Central time, plus `minutes`.
QDateTime at(int minutes)
{
    return QDateTime(QDate(2026, 11, 4), QTime(10, 0), QTimeZone::fromSecondsAheadOfUtc(-6 * 3600)).addSecs(minutes * 60);
}

/// An agent's check (with a record) at minute `minutes`, by `model`.
loom::Check agent(const char* verdict, bool now, int minutes, const char* model = "claude-opus-5-5")
{
    loom::Check check;
    check.by = QStringLiteral("agent:") + QString::fromLatin1(model);
    check.date = at(minutes);
    check.fingerprint = now ? nowFp : oldFp;
    check.verdict = QString::fromLatin1(verdict);
    check.record = QStringLiteral("Records/2026-11/job-0142.md");
    return check;
}

/// Dakota's check at minute `minutes`; a needs-changes says what to change.
loom::Check dakota(const char* verdict, bool now, int minutes)
{
    loom::Check check;
    check.by = QStringLiteral("dakota");
    check.date = at(minutes);
    check.fingerprint = now ? nowFp : oldFp;
    check.verdict = QString::fromLatin1(verdict);
    if (check.verdict == u"needs-changes")
        check.changes = QStringLiteral("Start the induction at k = 1.");
    return check;
}

/// The problems as `code line message` lines, for comparing whole lists.
QStringList describe(const std::vector<loom::Problem>& problems)
{
    QStringList lines;
    for (const loom::Problem& p : problems)
        lines << QStringLiteral("%1 %2 %3").arg(p.code).arg(p.line).arg(p.message);
    return lines;
}

} // namespace

TEST_CASE("The verdict table", "[checks]")
{
    // Specs/m1-core.md, "Checks", the verdict table, row by row; entries oldest first.
    struct Row {
        const char* name;
        std::vector<loom::Check> checks;
        bool checked;
        bool supports;
    };
    const Row rows[] = {
        {"(none)", {}, false, true},
        {"A:pass now", {agent("pass", true, 1)}, false, true},
        {"A:pass now, D:pass now", {agent("pass", true, 1), dakota("pass", true, 2)}, true, true},
        {"D:pass now, A:pass now", {dakota("pass", true, 1), agent("pass", true, 2)}, false, true},
        {"A:pass now, D:pass now, A:pass now", {agent("pass", true, 1), dakota("pass", true, 2), agent("pass", true, 3)}, false, true},
        {"A:pass now, D:pass now, A:needs-changes now",
         {agent("pass", true, 1), dakota("pass", true, 2), agent("needs-changes", true, 3)}, false, true},
        {"A:pass now, D:pass now, A:fail now", {agent("pass", true, 1), dakota("pass", true, 2), agent("fail", true, 3)}, false, false},
        {"A:pass now, D:needs-changes now", {agent("pass", true, 1), dakota("needs-changes", true, 2)}, false, true},
        {"A:pass now, D:pass now, D:needs-changes now",
         {agent("pass", true, 1), dakota("pass", true, 2), dakota("needs-changes", true, 3)}, false, true},
        {"A:pass now, D:fail now", {agent("pass", true, 1), dakota("fail", true, 2)}, false, false},
        {"A:pass now, D:fail now, D:pass now", {agent("pass", true, 1), dakota("fail", true, 2), dakota("pass", true, 3)}, true, true},
        {"A:pass old, D:pass old", {agent("pass", false, 1), dakota("pass", false, 2)}, false, true},
        {"A:pass old, D:pass old, A:pass now", {agent("pass", false, 1), dakota("pass", false, 2), agent("pass", true, 3)}, false, true},
        {"A:fail old, A:pass now, D:pass now", {agent("fail", false, 1), agent("pass", true, 2), dakota("pass", true, 3)}, true, true},
        {"A:pass now (agent X), A:needs-changes now (agent Y), A:pass now (agent X)",
         {agent("pass", true, 1, "x"), agent("needs-changes", true, 2, "y"), agent("pass", true, 3, "x")}, false, true},
        {"A:pass now, A:needs-changes now, D:pass now",
         {agent("pass", true, 1), agent("needs-changes", true, 2), dakota("pass", true, 3)}, false, true},
    };
    for (const Row& row : rows) {
        DYNAMIC_SECTION(row.name)
        {
            const loom::CheckStanding standing = loom::standingOf(row.checks, nowFp);
            CHECK(standing.checked == row.checked);
            CHECK(standing.supports == row.supports);
        }
    }

    SECTION("D:needs-changes now, without `changes`: invalid")
    {
        const loom::ChecksReading reading = loom::readChecks(
            "[[checks]]\nby = \"dakota\"\ndate = 2026-11-04T16:03:09-06:00\nfingerprint = \"3c8e1f0a92d4\"\nverdict = \"needs-changes\"\n",
            "Proofs/4KX7/4KX7.a.checks.toml");
        CHECK(describe(reading.problems) == QStringList{
            "check-changes-missing 1 Check 1 is Dakota's needs-changes but doesn't say what to change. Add `changes`."});
    }
}

TEST_CASE("Which checks count", "[checks]")
{
    SECTION("the newest matching check of each, by index")
    {
        const std::vector<loom::Check> checks = {agent("pass", true, 1), dakota("pass", true, 2), agent("pass", false, 3),
                                                 dakota("fail", false, 4)};
        const loom::CheckStanding standing = loom::standingOf(checks, nowFp);
        CHECK(standing.countingAgent == 0);
        CHECK(standing.countingDakota == 1);
        CHECK(standing.checked);
    }
    SECTION("nothing counts without a current fingerprint")
    {
        const loom::CheckStanding standing = loom::standingOf({agent("fail", true, 1)}, QString());
        CHECK_FALSE(standing.countingAgent);
        CHECK(standing.supports);
    }
    SECTION("equal dates: the later in the file is the newer")
    {
        CHECK(loom::standingOf({agent("pass", true, 1), dakota("pass", true, 1)}, nowFp).checked);
        CHECK_FALSE(loom::standingOf({dakota("pass", true, 1), agent("pass", true, 1)}, nowFp).checked);
        const loom::CheckStanding two = loom::standingOf({agent("fail", true, 1), agent("pass", true, 1)}, nowFp);
        CHECK(two.countingAgent == 1);
        CHECK(two.supports);
    }
    SECTION("newer is by the instant, whatever the offsets")
    {
        loom::Check first = agent("pass", true, 0);
        first.date = QDateTime(QDate(2026, 11, 4), QTime(11, 0), QTimeZone::fromSecondsAheadOfUtc(-5 * 3600)); // 10:00 Central
        loom::Check second = dakota("pass", true, 0);
        second.date = QDateTime(QDate(2026, 11, 4), QTime(10, 30), QTimeZone::fromSecondsAheadOfUtc(-6 * 3600));
        CHECK(loom::standingOf({first, second}, nowFp).checked);
    }
}

TEST_CASE("A check history is written in canonical form and reads back the same", "[checks]")
{
    loom::Check needsChanges = dakota("needs-changes", true, 363);
    needsChanges.date = QDateTime(QDate(2026, 11, 4), QTime(16, 3, 9), QTimeZone::fromSecondsAheadOfUtc(-6 * 3600));
    needsChanges.changes = QStringLiteral("Step 3 uses Lemma 7RT2 with k = 0, but 7RT2 needs k ≥ 1.\nStart at k = 1.\n");
    needsChanges.note = QStringLiteral("Notes/4KX7.md");
    loom::Check first = agent("pass", true, 0);
    first.date = QDateTime(QDate(2026, 11, 4), QTime(10, 12, 44), QTimeZone::fromSecondsAheadOfUtc(-6 * 3600));

    const QByteArray bytes = loom::formatChecks({first, needsChanges});
    CHECK(bytes ==
          "[[checks]]\nby = \"agent:claude-opus-5-5\"\ndate = 2026-11-04T10:12:44-06:00\nfingerprint = \"3c8e1f0a92d4\"\n"
          "verdict = \"pass\"\nrecord = \"Records/2026-11/job-0142.md\"\n"
          "\n"
          "[[checks]]\nby = \"dakota\"\ndate = 2026-11-04T16:03:09-06:00\nfingerprint = \"3c8e1f0a92d4\"\n"
          "verdict = \"needs-changes\"\nchanges = \"\"\"\nStep 3 uses Lemma 7RT2 with k = 0, but 7RT2 needs k \xe2\x89\xa5 1.\nStart at k = 1.\n\"\"\"\n"
          "note = \"Notes/4KX7.md\"\n");

    const loom::ChecksReading reading = loom::readChecks(bytes, "Proofs/4KX7/4KX7.a.checks.toml");
    CHECK(reading.problems.empty());
    CHECK(reading.lossless);
    REQUIRE(reading.checks.size() == 2);
    CHECK(loom::sameCheck(reading.checks[0], first));
    CHECK(loom::sameCheck(reading.checks[1], needsChanges));
    CHECK(reading.lines[1].header == 8);
    CHECK(reading.lines[1].of("verdict") == 12);

    SECTION("any `changes` text comes back exactly")
    {
        loom::Check odd = needsChanges;
        odd.changes = QStringLiteral("No final line break,\nbut \"quotes\", a \\ and a CR\r.");
        const loom::ChecksReading back = loom::readChecks(loom::formatCheck(odd), "x.checks.toml");
        REQUIRE(back.checks.size() == 1);
        CHECK(back.checks[0].changes == odd.changes);
    }
    SECTION("a zero offset is written Z")
    {
        loom::Check utc = first;
        utc.date = QDateTime(QDate(2026, 11, 4), QTime(16, 12, 44), QTimeZone::fromSecondsAheadOfUtc(0));
        CHECK(loom::formatCheck(utc).contains("date = 2026-11-04T16:12:44Z\n"));
    }
    SECTION("an empty file is an empty history")
    {
        const loom::ChecksReading empty = loom::readChecks("", "x.checks.toml");
        CHECK(empty.parsed);
        CHECK(empty.lossless);
        CHECK(empty.checks.empty());
        CHECK(empty.problems.empty());
        CHECK(loom::formatChecks({}).isEmpty());
    }
}

TEST_CASE("Problems a check history shows on its own", "[checks]")
{
    const QByteArray bytes =
        "[[checks]]\n"                                  // 1  check 1
        "by = \"agent:Claude\"\n"                        // 2
        "date = 2026-11-04T10:12:44-06:00\n"            // 3
        "fingerprint = \"3C8E1F0A92D4\"\n"               // 4
        "verdict = \"accept\"\n"                         // 5
        "record = \"Records/2026-11/job-0142.md\"\n"     // 6
        "colour = \"blue\"\n"                            // 7
        "\n"                                             // 8
        "[[checks]]\n"                                  // 9  check 2
        "by = \"agent:claude-opus-5-5\"\n"               // 10
        "date = 2026-11-04T09:00:00-06:00\n"            // 11
        "fingerprint = \"3c8e1f0a92d4\"\n"               // 12
        "verdict = \"pass\"\n"                           // 13
        "changes = \"Nothing.\"\n"                       // 14
        "\n"                                             // 15
        "[[checks]]\n"                                  // 16 check 3
        "by = \"dakota\"\n"                              // 17
        "date = 2026-11-04T16:03:09-06:00\n"            // 18
        "fingerprint = \"3c8e1f0a92d4\"\n"               // 19
        "verdict = \"needs-changes\"\n"                  // 20
        "note = \"../notes.md\"\n"                       // 21
        "\n"                                             // 22
        "[[checks]]\n"                                  // 23 check 4
        "by = \"robot\"\n"                               // 24
        "fingerprint = \"3c8e1f0a92d4\"\n"               // 25
        "verdict = \"pass\"\n"                           // 26
        "\n"                                             // 27
        "[[checks]]\n"                                  // 28 check 5
        "by = \"dakota\"\n"                              // 29
        "date = 2026-11-04T17:00:00\n"                  // 30
        "fingerprint = \"3c8e1f0a92d4\"\n"               // 31
        "verdict = \"pass\"\n"                           // 32
        "changes = \"\"\n";                              // 33
    const loom::ChecksReading reading = loom::readChecks(bytes, "Proofs/4KX7/4KX7.a.checks.toml");
    CHECK(reading.parsed);
    CHECK_FALSE(reading.lossless);
    CHECK(reading.checks.size() == 5);
    CHECK(describe(reading.problems) == QStringList{
        "check-by 2 Check 1: `by` must be `dakota` or `agent:<model>`, not `agent:Claude`.",
        "check-fingerprint 4 Check 1: the fingerprint must be 12 characters from 0–9 and a–f.",
        "check-verdict 5 Check 1: `accept` isn't a verdict. Use pass, needs-changes or fail.",
        "unknown-field 7 `colour` isn't a field of a check. Is it misspelt? The fields are: by, date, fingerprint, verdict, changes, record, note.",
        "check-record-missing 9 Check 2 is by an agent and has no `record`: every agent check links its reasoning.",
        "check-order 11 Check 2 is dated before check 1. Checks are kept in date order.",
        "check-changes-unexpected 14 Check 2 has `changes`, but only a needs-changes check says what to change.",
        "check-changes-missing 16 Check 3 is Dakota's needs-changes but doesn't say what to change. Add `changes`.",
        "bad-path 21 `../notes.md` must be a path inside the project, written with `/`, without `..`.",
        "missing-field 23 `date` is missing; every check needs one.",
        "check-by 24 Check 4: `by` must be `dakota` or `agent:<model>`, not `robot`.",
        "wrong-type 30 `date` should be a date and time to the second with its offset from UTC, like 2026-11-04T15:32:10-06:00, not a date and time without an offset.",
        "wrong-type 33 `changes` should be a non-empty string, not an empty string.",
    });

    SECTION("the same entry twice")
    {
        const QByteArray entry = loom::formatCheck(agent("pass", true, 1));
        const loom::ChecksReading twice = loom::readChecks(entry + "\n" + entry, "x.checks.toml");
        CHECK(describe(twice.problems) == QStringList{"check-duplicate 8 Checks 1 and 2 are the same entry twice."});
    }
    SECTION("anything but checks")
    {
        const loom::ChecksReading other = loom::readChecks("checks = 3\nnote = \"x\"\n", "x.checks.toml");
        CHECK(describe(other.problems) == QStringList{
            "wrong-type 1 `checks` should be a list of checks, each `[[checks]]`, not a whole number.",
            "unknown-field 2 `note` isn't a field of a check history. Is it misspelt? The fields are: checks."});
        CHECK_FALSE(other.lossless);
    }
    SECTION("an element of checks that isn't a table")
    {
        const loom::ChecksReading other = loom::readChecks("checks = [1]\n", "x.checks.toml");
        CHECK_FALSE(other.lossless);
        CHECK(other.checks.empty());
        CHECK_FALSE(other.problems.empty());
    }
    SECTION("dates with fractions of a second, or without a time")
    {
        // D1: a check's date is to the second, with its offset.
        const QByteArray entry = loom::formatCheck(agent("pass", true, 1));
        REQUIRE(entry.contains("date = 2026-11-04T10:01:00-06:00\n"));
        QByteArray fraction = entry;
        fraction.replace("date = 2026-11-04T10:01:00-06:00", "date = 2026-11-04T10:01:00.5-06:00");
        QByteArray plain = entry;
        plain.replace("date = 2026-11-04T10:01:00-06:00", "date = 2026-11-04");
        for (const QByteArray& variant : {fraction, plain}) {
            const loom::ChecksReading read = loom::readChecks(variant, "x.checks.toml");
            INFO(variant.toStdString());
            CHECK_FALSE(read.lossless);
            const QStringList problems = describe(read.problems);
            REQUIRE(problems.size() == 1);
            CHECK(problems[0].startsWith("wrong-type 3 `date` should be a date and time to the second with its offset from UTC"));
        }
    }
    SECTION("changes that are only spaces")
    {
        QByteArray entry = loom::formatCheck(dakota("needs-changes", true, 1));
        REQUIRE(entry.contains("changes = \"Start the induction at k = 1.\""));
        entry.replace("changes = \"Start the induction at k = 1.\"", "changes = \"  \\n \"");
        const QStringList problems = describe(loom::readChecks(entry, "x.checks.toml").problems);
        REQUIRE(problems.size() == 1);
        CHECK(problems[0].startsWith("wrong-type 6 `changes` should be a non-empty string"));
    }
    SECTION("not TOML")
    {
        const loom::ChecksReading broken = loom::readChecks("[[checks]\n", "x.checks.toml");
        CHECK_FALSE(broken.parsed);
        REQUIRE(broken.problems.size() == 1);
        CHECK(broken.problems[0].code == "toml-syntax");
    }
    SECTION("not UTF-8")
    {
        const loom::ChecksReading binary = loom::readChecks("by = \"\xff\"\n", "x.checks.toml");
        CHECK(describe(binary.problems) == QStringList{
            "not-utf8 1 This file isn't valid UTF-8 (the first bad byte is at offset 6). Save it as UTF-8."});
    }
}

TEST_CASE("Empty values in a check history are missing values", "[checks]")
{
    // Specs/m1-core.md, "Front matter", A2. `changes` is free text, so it keeps its own rule.
    const QByteArray bytes =
        "[[checks]]\n"         // 1
        "by = \"\"\n"          // 2
        "date = \"\"\n"        // 3
        "fingerprint = \"\"\n" // 4
        "verdict = \"\"\n"     // 5
        "record = \"\"\n"      // 6
        "note = \"\"\n";       // 7
    CHECK(describe(loom::readChecks(bytes, "x.checks.toml").problems) == QStringList{
        "missing-field 2 `by` is empty; every check needs one.",
        "missing-field 3 `date` is empty; every check needs one.",
        "missing-field 4 `fingerprint` is empty; every check needs one.",
        "missing-field 5 `verdict` is empty; every check needs one.",
        "missing-field 6 `record` is empty; give it a value or leave it out.",
        "missing-field 7 `note` is empty; give it a value or leave it out."});
}

TEST_CASE("A check's author can't end in a line break", "[checks]")
{
    // Qt's `$` also matches before a final line break, so the rule must be anchored at the very end.
    CHECK_FALSE(loom::isCheckAuthor(QStringLiteral("agent:claude\n")));
    CHECK_FALSE(loom::isCheckAuthor(QStringLiteral("dakota\n")));
    CHECK(loom::isCheckAuthor(QStringLiteral("agent:claude-opus-5-5")));
    const QByteArray bytes = "[[checks]]\nby = \"agent:claude\\n\"\ndate = 2026-11-04T10:12:44-06:00\nfingerprint = \"3c8e1f0a92d4\"\n"
                             "verdict = \"pass\"\nrecord = \"Records/2026-11/job-0142.md\"\n";
    CHECK(describe(loom::readChecks(bytes, "x.checks.toml").problems) == QStringList{
        "check-by 2 Check 1: `by` must be `dakota` or `agent:<model>`, not `agent:claude\n`."});
}

TEST_CASE("Merging two check lists", "[checks][merge]")
{
    const loom::Check a1 = agent("pass", true, 1);
    const loom::Check a2 = agent("needs-changes", true, 2, "other-model");
    const loom::Check d3 = dakota("pass", true, 3);
    const loom::Check a4 = agent("pass", true, 4);
    const auto merged = [](const std::vector<loom::Check>& base, const std::vector<loom::Check>& ours,
                           const std::vector<loom::Check>& theirs) {
        return loom::mergeChecks(loom::formatChecks(base), loom::formatChecks(ours), loom::formatChecks(theirs));
    };

    SECTION("entries from both sides interleave by date")
    {
        const loom::ChecksMerge merge = merged({a1}, {a1, d3}, {a1, a2, a4});
        REQUIRE(merge.ok);
        CHECK(merge.merged == loom::formatChecks({a1, a2, d3, a4}));
        CHECK_FALSE(merge.restored);
    }
    SECTION("an entry both sides added appears once")
    {
        const loom::ChecksMerge merge = merged({a1}, {a1, d3}, {a1, d3, a4});
        REQUIRE(merge.ok);
        CHECK(merge.merged == loom::formatChecks({a1, d3, a4}));
    }
    SECTION("for equal dates, ours come first")
    {
        loom::Check ours = dakota("pass", true, 5);
        loom::Check theirs = agent("fail", true, 5);
        CHECK(merged({a1}, {a1, ours}, {a1, theirs}).merged == loom::formatChecks({a1, ours, theirs}));
        CHECK(merged({a1}, {a1, theirs}, {a1, ours}).merged == loom::formatChecks({a1, theirs, ours}));
    }
    SECTION("a deleted entry is put back")
    {
        loom::ChecksMerge merge = merged({a1, a2}, {a2, d3}, {a1, a2});
        REQUIRE(merge.ok);
        CHECK(merge.merged == loom::formatChecks({a1, a2, d3}));
        CHECK(merge.restored);

        merge = merged({a1}, {}, {});
        CHECK(merge.merged == loom::formatChecks({a1}));
        CHECK(merge.restored);
    }
    SECTION("two new histories, with an empty base")
    {
        const loom::ChecksMerge merge = loom::mergeChecks("", loom::formatChecks({a2}), loom::formatChecks({a1}));
        REQUIRE(merge.ok);
        CHECK(merge.merged == loom::formatChecks({a1, a2}));
    }
    SECTION("an undated entry is left for a person, since its place can't be found")
    {
        const QByteArray undated = "[[checks]]\nby = \"dakota\"\nfingerprint = \"" + nowFp.toUtf8() + "\"\nverdict = \"pass\"\n";
        for (int side = 0; side < 3; ++side) {
            QByteArray versions[3] = {loom::formatChecks({a1}), loom::formatChecks({a1, d3}), loom::formatChecks({a1, a4})};
            versions[side] += "\n" + undated;
            const loom::ChecksMerge merge = loom::mergeChecks(versions[0], versions[1], versions[2]);
            INFO(side);
            CHECK_FALSE(merge.ok);
            CHECK(merge.merged.isEmpty());
            CHECK(merge.message.startsWith(QStringLiteral("Loom couldn't merge the check histories: %1 version isn't one it can read in full")
                                               .arg(side == 0 ? "the common ancestor's" : side == 1 ? "our" : "their")));
            CHECK(merge.message.endsWith("Resolve the conflict by hand, keeping every check."));
        }
    }
    SECTION("a base entry one side edited comes back, and the edited one stays as new")
    {
        // Rule 1: no entry is ever edited; an edit reads as removing one entry and adding another.
        loom::Check edited = a1;
        edited.verdict = QStringLiteral("fail");
        const loom::ChecksMerge merge = merged({a1}, {edited}, {a1, d3});
        REQUIRE(merge.ok);
        CHECK(merge.restored);
        CHECK(merge.merged == loom::formatChecks({edited, a1, d3}));
    }
    SECTION("a version Loom can't read in full is left for a person")
    {
        loom::ChecksMerge merge = loom::mergeChecks(loom::formatChecks({a1}), "[[checks]\n", loom::formatChecks({a1}));
        CHECK_FALSE(merge.ok);
        CHECK(merge.message.startsWith("Loom couldn't merge the check histories: our version isn't one it can read in full (line 1: "));

        merge = loom::mergeChecks(loom::formatChecks({a1}), loom::formatChecks({a1}), loom::formatChecks({a1}) + "colour = \"red\"\n");
        CHECK_FALSE(merge.ok);
        CHECK(merge.message.contains("their version"));
    }
}
