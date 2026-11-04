// Reading a project's statements and proofs, and the problems that need more than one file
// (Specs/m1-core.md, "Statements", "Proofs", "IDs" and "Validation").

#include "LoomCore/ProjectContents.h"

#include "ProjectFixture.h"

#include <catch2/catch_test_macros.hpp>

#include <tuple>
#include <vector>

using loomtest::ProjectFixture;

namespace {

using P = std::tuple<QString, QString, int, QString>;

/// Reads the project and returns every problem as (file, code, line, message).
std::vector<P> problemsOf(const ProjectFixture& project)
{
    const loom::OpenedProject opened = loom::openProject(project.root);
    REQUIRE(opened.status == loom::OpenStatus::Opened);
    const loom::ContentsLoading loading = loom::loadContents(project.root);
    REQUIRE(loading.ok);
    std::vector<P> out;
    for (const loom::Problem& problem : loom::checkContents(opened.config, loading.contents)) {
        CHECK(problem.severity == loom::Severity::Error);
        out.emplace_back(problem.file, problem.code, problem.line, problem.message);
    }
    return out;
}

} // namespace

TEST_CASE("No Statements or Proofs folder is the same as empty ones", "[contents]")
{
    const ProjectFixture project;
    const loom::ContentsLoading loading = loom::loadContents(project.root);
    CHECK(loading.ok);
    CHECK(loading.contents.statements.empty());
    CHECK(loading.contents.proofs.empty());
    CHECK(problemsOf(project).empty());
}

TEST_CASE("A valid project reads without problems", "[contents]")
{
    const ProjectFixture project;
    project.statement("2M9Q", "definition");
    project.statement("5HN8", "lemma");
    project.statement("4KX7", "theorem", "tags = [\"model-system\"]\nstated_using = [\"2M9Q\"]\naliases = [\"thm:energy-decay\"]\n");
    project.statement("N7ZE", "lemma", "superseded_by = \"4KX7\"\n");
    project.statement("F2NS", "lemma", "refuted_by = \"4KX7.a\"\nrefuted_on = 2026-11-06\n");
    project.proof("4KX7", "4KX7.a", "4KX7", "[\"5HN8\"]");
    project.write("Proofs/4KX7/4KX7.a.verbose.tex", "+++\nproof = \"4KX7.a\"\nfingerprint = \"3c8e1f0a92d4\"\ncreated = 2026-11-05\n+++\nSteps.\n");
    project.write("Proofs/4KX7/4KX7.a.checks.toml", "");

    CHECK(problemsOf(project).empty());

    const loom::ProjectContents contents = loom::loadContents(project.root).contents;
    CHECK(contents.statements.size() == 5);
    REQUIRE(contents.proofs.size() == 1);
    CHECK(contents.proofs[0].id == "4KX7.a");
    CHECK(contents.proofs[0].folder == "4KX7");
    REQUIRE(contents.verbose.size() == 1);
    CHECK(contents.verbose[0].proofId == "4KX7.a");
    REQUIRE(contents.checks.size() == 1);
    CHECK(contents.checks[0].path == "Proofs/4KX7/4KX7.a.checks.toml");
    CHECK(contents.checks[0].proofId == "4KX7.a");

    SECTION("IDs typed by a person")
    {
        CHECK(loom::resolveStatement(contents, "4KX7") == "4KX7");
        CHECK(loom::resolveStatement(contents, "4kx7") == "4KX7");
        CHECK(loom::resolveStatement(contents, "2m9q") == "2M9Q");
        CHECK(loom::resolveStatement(contents, "thm:energy-decay") == "4KX7");
        CHECK_FALSE(loom::resolveStatement(contents, "ZZZZ"));
        CHECK_FALSE(loom::resolveStatement(contents, "thm:nothing"));
        CHECK(loom::resolveProof(contents, "4kx7.A") == "4KX7.a");
        CHECK_FALSE(loom::resolveProof(contents, "4KX7.b"));
    }
}

TEST_CASE("Problems that need several files", "[contents]")
{
    const ProjectFixture project;
    project.statement("2M9Q", "definition");
    project.statement("4KX7", "theorem", "aliases = [\"thm:energy-decay\"]\n");
    project.statement("5HN8", "lemma", "tags = [\"no-such-tag\"]\n");
    project.statement("6BWT", "lemma", "stated_using = [\"6BWT\", \"ZZZZ\", \"thm:energy-decay\", \"2m9q\"]\n");
    project.statement("9D3F", "lemma", "aliases = [\"thm:energy-decay\"]\n");
    project.statement("A1A1", "lemma", "stated_using = [\"B2B2\"]\n");
    project.statement("B2B2", "lemma", "stated_using = [\"A1A1\"]\n");
    project.statement("C3C3", "lemma", "superseded_by = \"C3C3\"\n");
    project.statement("D4D4", "lemma", "superseded_by = \"D5D5\"\n");
    project.statement("E5E5", "lemma", "superseded_by = \"E6E6\"\n");
    project.statement("E6E6", "lemma", "superseded_by = \"E5E5\"\n");
    project.write("Statements/F7F7.tex", "+++\nid = \"F8F8\"\nkind = \"lemma\"\ntitle = \"T\"\ncreated = 2026-11-03\n+++\nBody\n");
    project.statement("F8F8", "lemma");
    project.statement("R2HW", "question", "refuted_by = \"4KX7.z\"\nrefuted_on = 2026-11-06\n");
    project.write("Statements/README.md", "notes");
    project.write("Statements/4KX7.tex~", "backup");
    project.proof("2M9Q", "2M9Q.a", "2M9Q", "[]");
    project.proof("4KX7", "4KX7.a", "5HN8", "[\"2M9Q\", \"ZZZZ\", \"thm:energy-decay\"]");
    project.proof("4KX7", "4KX7.b", "thm:energy-decay", "[]");
    project.write("Proofs/4KX7/4KX7.c.verbose.tex", "+++\nproof = \"4KX7.c\"\nfingerprint = \"3c8e1f0a92d4\"\ncreated = 2026-11-05\n+++\nSteps.\n");
    project.write("Proofs/4KX7/notes.txt", "notes");
    project.proof("ZZZZ", "ZZZZ.a", "ZZZZ", "[]");
    project.write("Proofs/notes", "notes");

    const QString onlyProvable = QStringLiteral("only lemmas, propositions, theorems, corollaries and conjectures");
    CHECK(problemsOf(project) == std::vector<P>{
        {"Proofs/2M9Q/2M9Q.a.tex", "proof-of-no-status", 3, "2M9Q is a definition; " + onlyProvable + " have proofs."},
        {"Proofs/4KX7/4KX7.a.tex", "proves-mismatch", 3, "4KX7.a says it proves 5HN8, but it's filed under 4KX7."},
        {"Proofs/4KX7/4KX7.a.tex", "premise-no-status", 4, "2M9Q is a definition, so it can't be a premise. Definitions belong in the statement's `stated_using`."},
        {"Proofs/4KX7/4KX7.a.tex", "ref-unknown", 4, "`ZZZZ` in `premises` isn't a statement or paper stub in this project."},
        {"Proofs/4KX7/4KX7.a.tex", "ref-alias", 4, "`thm:energy-decay` is an old name of 4KX7. Write `4KX7` here."},
        {"Proofs/4KX7/4KX7.b.tex", "ref-alias", 3, "`thm:energy-decay` is an old name of 4KX7. Write `4KX7` here."},
        {"Proofs/4KX7/4KX7.c.verbose.tex", "verbose-orphan", 1, "`Proofs/4KX7/4KX7.c.verbose.tex` is the verbose version of 4KX7.c, which doesn't exist."},
        {"Proofs/4KX7/notes.txt", "stray-file", 1, "`Proofs/4KX7/notes.txt` doesn't belong in Proofs/4KX7/: it holds only the proofs of 4KX7, their verbose versions and their checks."},
        {"Proofs/ZZZZ/ZZZZ.a.tex", "proof-folder-unknown", 1, "`Proofs/ZZZZ/` is for proofs of ZZZZ, but there is no statement ZZZZ."},
        {"Proofs/notes", "stray-file", 1, "`Proofs/notes` doesn't belong in Proofs/: it holds only a folder for each statement's proofs, named by its tag."},
        {"Statements/4KX7.tex~", "stray-file", 1, "`Statements/4KX7.tex~` doesn't belong in Statements/: it holds only statements, each as `<tag>.tex`."},
        {"Statements/5HN8.tex", "tag-unknown", 5, "`no-such-tag` isn't one of the project's tags. Add it with `loom new-tag`, or fix the spelling."},
        {"Statements/6BWT.tex", "stated-using-self", 5, "6BWT can't be stated using itself."},
        {"Statements/6BWT.tex", "ref-unknown", 5, "`ZZZZ` in `stated_using` isn't a statement or paper stub in this project."},
        {"Statements/6BWT.tex", "ref-alias", 5, "`thm:energy-decay` is an old name of 4KX7. Write `4KX7` here."},
        {"Statements/6BWT.tex", "id-syntax", 5, "`2m9q` isn't a valid tag: four characters from 0–9 and A–Z without I, L, O and U. Did you mean `2M9Q`?"},
        {"Statements/9D3F.tex", "alias-duplicate", 5, "`thm:energy-decay` is an alias of both 4KX7 and 9D3F."},
        {"Statements/A1A1.tex", "stated-using-cycle", 5, "`stated_using` goes round in a circle: A1A1 → B2B2 → A1A1."},
        {"Statements/C3C3.tex", "superseded-invalid", 5, "C3C3 is superseded by C3C3, which is itself."},
        {"Statements/D4D4.tex", "superseded-invalid", 5, "D4D4 is superseded by D5D5, which doesn't exist."},
        {"Statements/E5E5.tex", "superseded-invalid", 5, "E5E5 is superseded by E6E6, which leads round in a circle: E5E5 → E6E6 → E5E5."},
        {"Statements/F7F7.tex", "id-mismatch", 2, "The id is `F8F8` but the file is named `F7F7`; they must match."},
        {"Statements/F8F8.tex", "id-duplicate", 2, "The tag `F8F8` is used by both Statements/F7F7.tex and Statements/F8F8.tex."},
        {"Statements/R2HW.tex", "refuted-unknown", 5, "R2HW is refuted by `4KX7.z`, which isn't a proof in this project."},
        {"Statements/R2HW.tex", "refuted-no-status", 5, "R2HW is a question; " + onlyProvable + " can be refuted."},
        {"Statements/README.md", "stray-file", 1, "`Statements/README.md` doesn't belong in Statements/: it holds only statements, each as `<tag>.tex`."},
    });
}

TEST_CASE("A statement file that resolves outside the project isn't read", "[contents][paths]")
{
    const ProjectFixture project;
    QTemporaryDir outside;
    REQUIRE(outside.isValid());
    loomtest::writeFile(outside.path() + "/4KX7.tex", "+++\nid = \"4KX7\"\n+++\nBody\n");
    if (!loomtest::makeDirectoryLink(outside.path(), project.root + "/Statements"))
        SKIP("This system can't make a directory link.");

    const loom::ContentsLoading loading = loom::loadContents(project.root);
    CHECK(loading.ok);
    CHECK(loading.contents.statements.empty());
    REQUIRE(loading.contents.listingProblems.size() == 1);
    CHECK(loading.contents.listingProblems[0].code == "bad-path");
    CHECK(loading.contents.listingProblems[0].file == "Statements/4KX7.tex");
}
