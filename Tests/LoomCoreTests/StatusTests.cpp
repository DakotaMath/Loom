// The graph and computed status (Specs/m1-core.md, "How status is computed" and "The worked
// example"): every status of the worked example, the explanations, levels, cycles, recomputing
// after a change, and refuting a statement.

#include "LoomCore/Edits.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProofChecks.h"
#include "LoomCore/Status.h"

#include "PaperFixture.h"
#include "ProjectFixture.h"

#include <QTimeZone>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <tuple>
#include <vector>

using loomtest::ProjectFixture;
using loom::Status;

namespace {

/// The worked example's project folder, in the source tree (Tests/Projects/WorkedExample).
const QString workedExample = QStringLiteral(LOOM_WORKED_EXAMPLE);

/// Reads a project and builds its graph.
loom::ProjectGraph graphOf(const QString& root)
{
    const loom::ContentsLoading loading = loom::loadContents(root);
    REQUIRE(loading.ok);
    return loom::buildGraph(loading.contents);
}

/// The status of `tag` in `graph`; fails the test if there is no such statement.
Status statusOf(const loom::ProjectGraph& graph, const QString& tag)
{
    const loom::StatementNode* node = graph.statement(tag);
    REQUIRE(node);
    return node->status;
}

/// The explanation of `tag` in `graph`.
QString explanationOf(const loom::ProjectGraph& graph, const QString& tag)
{
    const loom::StatementNode* node = graph.statement(tag);
    REQUIRE(node);
    return node->explanation;
}

/// Change options at 2026-11-04 10:00 Central, plus `minutes`.
loom::ChangeOptions at(int minutes)
{
    loom::ChangeOptions options;
    options.today = QDate(2026, 11, 4);
    options.now = QDateTime(QDate(2026, 11, 4), QTime(10, 0, 0), QTimeZone::fromSecondsAheadOfUtc(-6 * 3600)).addSecs(minutes * 60);
    return options;
}

/// Adds a check of `id` by `by` with the proof's current fingerprint.
void addCheck(const ProjectFixture& project, const QString& id, const QString& by, const QString& verdict, int minutes,
              const QString& changes = {})
{
    loom::NewCheck request;
    request.by = by;
    request.verdict = verdict;
    request.changes = changes;
    if (by != u"dakota") {
        request.fingerprint = loom::proofFingerprint(loom::loadContents(project.root).contents, id).value_or(QString());
        request.record = "Records/2026-11/job-0001.md";
    }
    REQUIRE(loom::addCheck(project.root, id, request, by == u"dakota", at(minutes)).change.outcome == loom::ChangeOutcome::Done);
}

/// Makes `id` checked: an agent's pass, then Dakota's a minute later.
void makeChecked(const ProjectFixture& project, const QString& id, int minutes = 0)
{
    addCheck(project, id, "agent:claude-opus-5-5", "pass", minutes);
    addCheck(project, id, "dakota", "pass", minutes + 1);
}

} // namespace

TEST_CASE("The worked example: every status", "[status][worked-example]")
{
    const loom::ProjectGraph graph = graphOf(workedExample);

    // Specs/m1-core.md, "The worked example", the table's last column.
    const std::map<QString, Status> expected = {
        {"7RT2", Status::Cited},       {"3WQ6", Status::Extracted},   {"2M9Q", Status::None},
        {"R2HW", Status::None},        {"S6KT", Status::Open},        {"5HN8", Status::Proved},
        {"9D3F", Status::Proved},      {"4KX7", Status::Proved},      {"6BWT", Status::Conditional},
        {"8CJA", Status::Unchecked},   {"AH3K", Status::Unchecked},   {"B7MP", Status::Unchecked},
        {"C5RE", Status::Unchecked},   {"CQ2R", Status::Heuristic},   {"W5HC", Status::Sketch},
        {"D4VS", Status::Sketch},      {"D9XF", Status::Open},        {"E4GV", Status::Conditional},
        {"E8TB", Status::Conditional}, {"G5TZ", Status::Proved},      {"F2NS", Status::False},
        {"T9GB", Status::Conditional}, {"H8KD", Status::Proved},      {"J3RW", Status::Proved},
        {"K6YM", Status::Conditional}, {"M1PQ", Status::Conditional}, {"N7ZE", Status::Proved},
        {"P3QA", Status::Proved},
    };
    REQUIRE(graph.statements.size() == expected.size());
    for (const auto& [tag, status] : expected) {
        INFO(tag.toStdString());
        CHECK(statusOf(graph, tag) == status);
    }

    // The counts in the spec's `loom status --json` example.
    CHECK(graph.count(Status::Proved) == 8);
    CHECK(graph.count(Status::Conditional) == 6);
    CHECK(graph.count(Status::Unchecked) == 4);
    CHECK(graph.count(Status::Sketch) == 2);
    CHECK(graph.count(Status::Heuristic) == 1);
    CHECK(graph.count(Status::Open) == 2);
    CHECK(graph.count(Status::Cited) == 1);
    CHECK(graph.count(Status::Extracted) == 1);
    CHECK(graph.count(Status::False) == 1);
    CHECK(graph.count(Status::None) == 2);

    SECTION("what conditional statements rest on, and the deciding proofs")
    {
        const std::map<QString, std::pair<QString, QStringList>> conditional = {
            {"6BWT", {"6BWT.a", {"3WQ6"}}},
            {"E4GV", {"E4GV.a", {"8CJA"}}},
            {"E8TB", {"E8TB.b", {"8CJA"}}}, // b: fewer premises neither proved nor cited.
            {"T9GB", {"T9GB.a", {"F2NS"}}},
            {"K6YM", {"K6YM.a", {"M1PQ"}}},
            {"M1PQ", {"M1PQ.a", {"K6YM"}}},
        };
        for (const auto& [tag, decided] : conditional) {
            INFO(tag.toStdString());
            CHECK(graph.statement(tag)->decidingProof == decided.first);
            CHECK(graph.statement(tag)->restsOn == decided.second);
        }
        CHECK(graph.statement("J3RW")->decidingProof == "J3RW.b"); // H8KD is proved via J3RW.b.
        CHECK(graph.statement("W5HC")->decidingProof == "W5HC.b");
        CHECK(graph.statement("S6KT")->decidingProof.isEmpty());
        CHECK(graph.statement("4KX7")->restsOn.isEmpty());
    }

    SECTION("what each proof gives")
    {
        CHECK(graph.proof("4KX7.a")->gives == loom::Gives::CompleteChecked);
        CHECK(graph.proof("8CJA.a")->gives == loom::Gives::CompleteUnchecked);
        CHECK(graph.proof("W5HC.a")->gives == loom::Gives::Heuristic);
        CHECK(graph.proof("W5HC.b")->gives == loom::Gives::Sketch);
        CHECK(graph.proof("D9XF.a")->gives == loom::Gives::Nothing); // failed
        CHECK(graph.proof("D9XF.b")->gives == loom::Gives::Nothing); // an agent's fail
    }

    SECTION("cycles of premises")
    {
        CHECK(graph.cycles == std::vector<QStringList>{{"H8KD", "J3RW"}, {"K6YM", "M1PQ"}});
    }

    SECTION("edges")
    {
        CHECK(graph.statedUsing == std::vector<std::pair<QString, QString>>{{"4KX7", "2M9Q"}});
        CHECK(graph.paperPremises.empty());
        CHECK(graph.statement("7RT2")->paper == "SmithJones24");
        CHECK(graph.statement("N7ZE")->supersededBy == "4KX7");
    }
}

TEST_CASE("The worked example: its problems are the premise cycles", "[status][worked-example]")
{
    // The other problems the spec expects of it (`refuted-but-proved`, `premise-false`,
    // `premise-superseded`) and the files' own checks are `loom validate`'s (#9).
    const loom::OpenedProject opened = loom::openProject(workedExample);
    REQUIRE(opened.status == loom::OpenStatus::Opened);
    CHECK(opened.problems.empty());
    const loom::ContentsLoading loading = loom::loadContents(workedExample);
    REQUIRE(loading.ok);

    std::vector<std::tuple<QString, QString, int, QString>> problems;
    for (const loom::Problem& p : loom::checkContents(opened.config, loading.contents))
        problems.emplace_back(p.code, p.file, p.line, p.message);
    CHECK(problems == std::vector<std::tuple<QString, QString, int, QString>>{
        {"premise-cycle", "Proofs/H8KD/H8KD.a.tex", 4,
         "Premises go round in a circle: H8KD → J3RW → H8KD. A statement can't help prove itself."},
        {"premise-cycle", "Proofs/K6YM/K6YM.a.tex", 4,
         "Premises go round in a circle: K6YM → M1PQ → K6YM. A statement can't help prove itself."},
    });
}

TEST_CASE("Explanations: one sentence for each status", "[status][explanations]")
{
    const loom::ProjectGraph graph = graphOf(workedExample);

    // The spec's examples, where the worked example has the same shape.
    CHECK(explanationOf(graph, "4KX7") == "4KX7 proved: 4KX7.a is complete and checked, and its premises 5HN8, 9D3F are proved.");
    CHECK(explanationOf(graph, "6BWT")
          == "6BWT conditional: 6BWT.a is complete and checked, but rests on 3WQ6, which is extracted (not yet checked "
             "against the paper's PDF).");
    CHECK(explanationOf(graph, "8CJA") == "8CJA unchecked: 8CJA.a is complete and has an agent pass, but no pass from Dakota.");
    CHECK(explanationOf(graph, "D9XF") == "D9XF open: no proof supports it (D9XF.a failed; an agent check says D9XF.b is wrong).");
    CHECK(explanationOf(graph, "F2NS") == "F2NS false: refuted by G5TZ.a, accepted 2026-11-06.");

    // The rest of the worked example.
    CHECK(explanationOf(graph, "5HN8") == "5HN8 proved: 5HN8.a is complete and checked, and has no premises.");
    CHECK(explanationOf(graph, "9D3F") == "9D3F proved: 9D3F.a is complete and checked, and its premise 7RT2 is cited.");
    CHECK(explanationOf(graph, "J3RW") == "J3RW proved: J3RW.b is complete and checked, and has no premises.");
    CHECK(explanationOf(graph, "H8KD") == "H8KD proved: H8KD.a is complete and checked, and its premise J3RW is proved.");
    CHECK(explanationOf(graph, "T9GB") == "T9GB conditional: T9GB.a is complete and checked, but rests on F2NS, which is false.");
    CHECK(explanationOf(graph, "K6YM") == "K6YM conditional: K6YM.a is complete and checked, but rests on M1PQ, which is conditional.");
    CHECK(explanationOf(graph, "AH3K")
          == "AH3K unchecked: AH3K.a is complete, but has no agent pass of this version (it changed after its last agent check).");
    CHECK(explanationOf(graph, "B7MP")
          == "B7MP unchecked: Dakota asked for changes to B7MP.a: \"Step 2 divides by the energy, which may vanish. "
             "Treat the zero-energy case first.\"");
    CHECK(explanationOf(graph, "C5RE")
          == "C5RE unchecked: C5RE.a is complete and has an agent pass, but no pass from Dakota newer than it.");
    CHECK(explanationOf(graph, "W5HC") == "W5HC sketch: W5HC.b is a sketch.");
    CHECK(explanationOf(graph, "CQ2R") == "CQ2R heuristic: CQ2R.a is a heuristic argument.");
    CHECK(explanationOf(graph, "S6KT") == "S6KT open: it has no proofs.");
    CHECK(explanationOf(graph, "7RT2") == "7RT2 cited: Theorem 1.2, p. 4 in SmithJones24 (arxiv-v1), checked against the PDF on 2026-10-20.");
    CHECK(explanationOf(graph, "3WQ6") == "3WQ6 extracted: Lemma 2.1, p. 7 in SmithJones24 (arxiv-v1), not yet checked against the PDF.");
    CHECK(explanationOf(graph, "2M9Q") == "2M9Q has no status: it is a definition.");
}

TEST_CASE("Explanations of less common cases", "[status][explanations]")
{
    const ProjectFixture project;
    project.statement("4KX7", "theorem");
    project.statement("5HN8", "lemma");
    project.proof("5HN8", "5HN8.a", "5HN8", "[]");
    project.proof("4KX7", "4KX7.a", "4KX7", "[\"5HN8\"]");

    SECTION("Dakota's fail")
    {
        addCheck(project, "4KX7.a", "agent:claude-opus-5-5", "pass", 0);
        addCheck(project, "4KX7.a", "dakota", "fail", 1);
        const loom::ProjectGraph graph = graphOf(project.root);
        CHECK(statusOf(graph, "4KX7") == Status::Open);
        CHECK(explanationOf(graph, "4KX7") == "4KX7 open: no proof supports it (Dakota's check says 4KX7.a is wrong).");
    }
    SECTION("an agent's needs-changes")
    {
        addCheck(project, "4KX7.a", "agent:claude-opus-5-5", "needs-changes", 0);
        CHECK(explanationOf(graphOf(project.root), "4KX7")
              == "4KX7 unchecked: an agent asked for changes to 4KX7.a (see Records/2026-11/job-0001.md).");
    }
    SECTION("no checks at all")
    {
        CHECK(explanationOf(graphOf(project.root), "4KX7") == "4KX7 unchecked: 4KX7.a is complete, but has no agent pass.");
    }
    SECTION("Dakota's pass only, no agent check")
    {
        addCheck(project, "4KX7.a", "dakota", "pass", 0);
        CHECK(explanationOf(graphOf(project.root), "4KX7") == "4KX7 unchecked: 4KX7.a is complete, but has no agent pass.");
    }
    SECTION("a fingerprint Loom can't compute")
    {
        // Checked, then a premise's front matter breaks: no check can match, and the sentence
        // says why rather than claiming the proof changed.
        makeChecked(project, "4KX7.a");
        project.write("Statements/5HN8.tex", "+++\nid = \"5HN8\"\nkind = lemma\n+++\nBody of 5HN8.\n");
        const loom::ProjectGraph graph = graphOf(project.root);
        CHECK(statusOf(graph, "4KX7") == Status::Unchecked);
        CHECK(graph.proof("4KX7.a")->checking.fingerprintProblem
              == "Loom can't compute the fingerprint of 4KX7.a: Loom can't read the front matter of its premise 5HN8 "
                 "(Statements/5HN8.tex).");
        CHECK(explanationOf(graph, "4KX7")
              == "4KX7 unchecked: 4KX7.a is complete, but no check of it can count. Loom can't compute the fingerprint of "
                 "4KX7.a: Loom can't read the front matter of its premise 5HN8 (Statements/5HN8.tex).");
    }
    SECTION("premises both proved and cited, and several resting premises")
    {
        project.write("Papers/SmithJones24/paper.toml", loomtest::smithJones24());
        project.write("Papers/SmithJones24/Statements/7RT2.toml", loomtest::stubToml("7RT2", "theorem", true));
        project.write("Papers/SmithJones24/Statements/3WQ6.toml", loomtest::stubToml("3WQ6", "lemma", false));
        project.statement("9D3F", "lemma");
        project.proof("9D3F", "9D3F.a", "9D3F", "[\"7RT2\", \"5HN8\"]");
        project.statement("6BWT", "lemma");
        project.proof("6BWT", "6BWT.a", "6BWT", "[\"3WQ6\", \"4KX7\"]");
        makeChecked(project, "5HN8.a", 0);
        makeChecked(project, "9D3F.a", 2);
        makeChecked(project, "6BWT.a", 4);
        const loom::ProjectGraph graph = graphOf(project.root);
        CHECK(explanationOf(graph, "9D3F")
              == "9D3F proved: 9D3F.a is complete and checked, and its premises are proved (5HN8) or cited (7RT2).");
        CHECK(explanationOf(graph, "6BWT")
              == "6BWT conditional: 6BWT.a is complete and checked, but rests on 3WQ6, which is extracted (not yet checked "
                 "against the paper's PDF) and 4KX7, which is unchecked.");
    }
}

TEST_CASE("Levels rank complete, then sketch, then heuristic", "[status][levels]")
{
    const ProjectFixture project;
    project.statement("4KX7", "theorem");
    project.proof("4KX7", "4KX7.a", "4KX7", "[]", "heuristic");
    CHECK(statusOf(graphOf(project.root), "4KX7") == Status::Heuristic);
    project.proof("4KX7", "4KX7.b", "4KX7", "[]", "sketch");
    CHECK(statusOf(graphOf(project.root), "4KX7") == Status::Sketch);
    project.proof("4KX7", "4KX7.c", "4KX7", "[]", "complete");
    CHECK(statusOf(graphOf(project.root), "4KX7") == Status::Unchecked);
    project.proof("4KX7", "4KX7.d", "4KX7", "[]", "failed");
    const loom::ProjectGraph graph = graphOf(project.root);
    CHECK(statusOf(graph, "4KX7") == Status::Unchecked);
    CHECK(graph.statement("4KX7")->decidingProof == "4KX7.c");
    CHECK(graph.statement("4KX7")->proofs == QStringList{"4KX7.a", "4KX7.b", "4KX7.c", "4KX7.d"});
}

TEST_CASE("The rules that come before proofs", "[status][rules]")
{
    const ProjectFixture project;
    project.write("Papers/SmithJones24/paper.toml", loomtest::smithJones24());

    SECTION("a stub of a kind without status has none (rule 1 before rule 2)")
    {
        project.write("Papers/SmithJones24/Statements/7RT2.toml", loomtest::stubToml("7RT2", "definition", true));
        CHECK(statusOf(graphOf(project.root), "7RT2") == Status::None);
    }
    SECTION("refuted_by without refuted_on isn't false")
    {
        project.statement("G5TZ", "proposition");
        project.proof("G5TZ", "G5TZ.a", "G5TZ", "[]");
        project.statement("F2NS", "lemma", "refuted_by = \"G5TZ.a\"\n");
        CHECK(statusOf(graphOf(project.root), "F2NS") == Status::Open);
    }
    SECTION("a refuted statement with a checked proof is false, and not proved")
    {
        project.statement("G5TZ", "proposition");
        project.proof("G5TZ", "G5TZ.a", "G5TZ", "[]");
        project.statement("F2NS", "lemma", "refuted_by = \"G5TZ.a\"\nrefuted_on = 2026-11-06\n");
        project.proof("F2NS", "F2NS.a", "F2NS", "[]");
        makeChecked(project, "F2NS.a");
        project.statement("T9GB", "lemma");
        project.proof("T9GB", "T9GB.a", "T9GB", "[\"F2NS\"]");
        makeChecked(project, "T9GB.a", 2);
        const loom::ProjectGraph graph = graphOf(project.root);
        CHECK(statusOf(graph, "F2NS") == Status::False);
        CHECK(statusOf(graph, "T9GB") == Status::Conditional);
        CHECK(graph.statement("T9GB")->restsOn == QStringList{"F2NS"});
    }
}

TEST_CASE("A cycle of premises proves nothing on its own, and is an error", "[status][cycles]")
{
    const ProjectFixture project;
    project.statement("H8KD", "lemma");
    project.statement("J3RW", "lemma");
    project.statement("K6YM", "lemma");
    project.proof("H8KD", "H8KD.a", "H8KD", "[\"J3RW\"]");
    project.proof("J3RW", "J3RW.a", "J3RW", "[\"K6YM\"]");
    project.proof("K6YM", "K6YM.a", "K6YM", "[\"H8KD\"]");
    makeChecked(project, "H8KD.a", 0);
    makeChecked(project, "J3RW.a", 2);
    makeChecked(project, "K6YM.a", 4);

    const loom::ProjectGraph graph = graphOf(project.root);
    CHECK(graph.cycles == std::vector<QStringList>{{"H8KD", "J3RW", "K6YM"}});
    for (const char* tag : {"H8KD", "J3RW", "K6YM"})
        CHECK(statusOf(graph, tag) == Status::Conditional);

    const loom::OpenedProject opened = loom::openProject(project.root);
    const std::vector<loom::Problem> problems = loom::checkContents(opened.config, loom::loadContents(project.root).contents);
    REQUIRE(problems.size() == 1);
    CHECK(problems[0].code == "premise-cycle");
    CHECK(problems[0].file == "Proofs/H8KD/H8KD.a.tex");
    CHECK(problems[0].message == "Premises go round in a circle: H8KD → J3RW → K6YM → H8KD. A statement can't help prove itself.");

    SECTION("a proof from outside the cycle proves them all")
    {
        project.proof("K6YM", "K6YM.b", "K6YM", "[]");
        makeChecked(project, "K6YM.b", 6);
        const loom::ProjectGraph after = graphOf(project.root);
        for (const char* tag : {"H8KD", "J3RW", "K6YM"})
            CHECK(statusOf(after, tag) == Status::Proved);
        CHECK(after.statement("K6YM")->decidingProof == "K6YM.b");
    }

    SECTION("a new proof that would close a cycle is refused")
    {
        project.statement("M1PQ", "lemma");
        project.proof("M1PQ", "M1PQ.a", "M1PQ", "[]");
        loom::NewProof request;
        request.statement = "H8KD";
        request.level = "sketch";
        request.body = "Proof.";
        request.premises = {"M1PQ"};
        CHECK(loom::createProof(project.root, request, at(10)).outcome == loom::ChangeOutcome::Done);
        loom::NewProof closing = request;
        closing.statement = "M1PQ";
        closing.premises = {"H8KD"};
        const loom::ChangeResult result = loom::createProof(project.root, closing, at(11));
        CHECK(result.outcome == loom::ChangeOutcome::Refused);
        CHECK(result.code == "premise-cycle");
    }
}

TEST_CASE("Changing one proof or check recomputes everything downstream", "[status][recompute]")
{
    const ProjectFixture project;
    project.statement("5HN8", "lemma");
    project.statement("4KX7", "theorem");
    project.statement("6BWT", "corollary");
    project.proof("5HN8", "5HN8.a", "5HN8", "[]");
    project.proof("4KX7", "4KX7.a", "4KX7", "[\"5HN8\"]");
    project.proof("6BWT", "6BWT.a", "6BWT", "[\"4KX7\"]");
    makeChecked(project, "4KX7.a", 0);
    makeChecked(project, "6BWT.a", 2);

    loom::ProjectGraph graph = graphOf(project.root);
    CHECK(statusOf(graph, "5HN8") == Status::Unchecked);
    CHECK(statusOf(graph, "4KX7") == Status::Conditional);
    CHECK(statusOf(graph, "6BWT") == Status::Conditional);
    CHECK(graph.statement("6BWT")->restsOn == QStringList{"4KX7"});

    // A check at the bottom proves the whole chain.
    makeChecked(project, "5HN8.a", 4);
    graph = graphOf(project.root);
    CHECK(statusOf(graph, "5HN8") == Status::Proved);
    CHECK(statusOf(graph, "4KX7") == Status::Proved);
    CHECK(statusOf(graph, "6BWT") == Status::Proved);

    // Editing the bottom proof outdates its checks, and everything above is conditional again.
    loom::ProofEdit edit;
    edit.body = QByteArray("A better proof.\n");
    edit.outdateChecks = QStringList{"5HN8.a"};
    REQUIRE(loom::editProof(project.root, "5HN8.a", edit, at(6)).outcome == loom::ChangeOutcome::Done);
    graph = graphOf(project.root);
    CHECK(statusOf(graph, "5HN8") == Status::Unchecked);
    CHECK(statusOf(graph, "4KX7") == Status::Conditional);
    CHECK(statusOf(graph, "6BWT") == Status::Conditional);

    // An agent's fail on it: nothing supports 5HN8 now.
    addCheck(project, "5HN8.a", "agent:claude-opus-5-5", "fail", 8);
    graph = graphOf(project.root);
    CHECK(statusOf(graph, "5HN8") == Status::Open);
    CHECK(graph.statement("4KX7")->explanation
          == "4KX7 conditional: 4KX7.a is complete and checked, but rests on 5HN8, which is open.");
}

TEST_CASE("refute sets refuted_by and refuted_on, and the statement becomes false", "[status][refute]")
{
    const ProjectFixture project;
    project.statement("G5TZ", "proposition");
    project.proof("G5TZ", "G5TZ.a", "G5TZ", "[]");
    project.statement("F2NS", "lemma");
    project.statement("2M9Q", "definition");

    SECTION("only with Dakota's confirmation")
    {
        const loom::ChangeResult result = loom::refuteStatement(project.root, "F2NS", "G5TZ.a", false, at(0));
        CHECK(result.outcome == loom::ChangeOutcome::Refused);
        CHECK(result.code == "dakota-only");
        CHECK(statusOf(graphOf(project.root), "F2NS") == Status::Open);
    }
    SECTION("confirmed")
    {
        const loom::ChangeResult result = loom::refuteStatement(project.root, "f2ns", "g5tz.A", true, at(0));
        REQUIRE(result.outcome == loom::ChangeOutcome::Done);
        CHECK(result.id == "F2NS");
        CHECK(result.changed == QStringList{"refuted_by", "refuted_on"});
        CHECK(project.read("Statements/F2NS.tex")
              == "+++\nid = \"F2NS\"\nkind = \"lemma\"\ntitle = \"T\"\nrefuted_by = \"G5TZ.a\"\nrefuted_on = 2026-11-04\n"
                 "created = 2026-11-03\n+++\nBody of F2NS.\n");
        const loom::ProjectGraph graph = graphOf(project.root);
        CHECK(statusOf(graph, "F2NS") == Status::False);
        CHECK(explanationOf(graph, "F2NS") == "F2NS false: refuted by G5TZ.a, accepted 2026-11-04.");

        const loom::ChangeResult again = loom::refuteStatement(project.root, "F2NS", "G5TZ.a", true, at(1));
        CHECK(again.outcome == loom::ChangeOutcome::Refused);
        CHECK(again.code == "already-there");
    }
    SECTION("a proof that isn't there")
    {
        CHECK(loom::refuteStatement(project.root, "F2NS", "G5TZ.b", true, at(0)).code == "ref-unknown");
    }
    SECTION("a kind without status")
    {
        const loom::ChangeResult result = loom::refuteStatement(project.root, "2M9Q", "G5TZ.a", true, at(0));
        CHECK(result.outcome == loom::ChangeOutcome::Refused);
        CHECK(result.code == "refuted-no-status");
    }
    SECTION("a stub")
    {
        project.write("Papers/SmithJones24/paper.toml", loomtest::smithJones24());
        project.write("Papers/SmithJones24/Statements/7RT2.toml", loomtest::stubToml("7RT2", "theorem", true));
        const loom::ChangeResult result = loom::refuteStatement(project.root, "7RT2", "G5TZ.a", true, at(0));
        CHECK(result.outcome == loom::ChangeOutcome::Refused);
        CHECK(result.code == "ref-unknown");
    }
    SECTION("one of its own proofs")
    {
        const QByteArray before = project.read("Statements/G5TZ.tex");
        const loom::ChangeResult result = loom::refuteStatement(project.root, "G5TZ", "G5TZ.a", true, at(0));
        CHECK(result.outcome == loom::ChangeOutcome::Refused);
        CHECK(result.code == "refuted-self");
        CHECK(result.message
              == "G5TZ.a is a proof of G5TZ itself, so it can't refute it. A statement is refuted by a proof of another "
                 "statement, such as its negation.");
        CHECK(project.read("Statements/G5TZ.tex") == before);
    }
}

TEST_CASE("A stub's paper is named by its key", "[status]")
{
    // Specs/m1-core.md, the `loom status` JSON: "`paper` is the key for stubs". The key and the
    // folder differ only alongside `paper-key-mismatch`.
    const ProjectFixture project;
    QByteArray paper = loomtest::smithJones24();
    paper.replace("key = \"SmithJones24\"", "key = \"SmithJones25\"");
    project.write("Papers/SmithJones24/paper.toml", paper);
    project.write("Papers/SmithJones24/Statements/7RT2.toml", loomtest::stubToml("7RT2", "theorem", true));
    const loom::ProjectGraph graph = graphOf(project.root);
    CHECK(graph.statement("7RT2")->paper == "SmithJones25");
    CHECK(explanationOf(graph, "7RT2") == "7RT2 cited: Theorem 1.2, p. 4 in SmithJones25 (arxiv-v1), checked against the PDF on 2026-10-18.");
}
