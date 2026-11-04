// `loom validate` (Specs/m1-core.md, "Validation"; Verification, "Worked example" and
// "Validation messages": "A test project with one of every problem gives exactly the expected
// list"). The JSON of the same list is checked through `loom` in Tests/CMake/ValidateCli.cmake.

#include "LoomCore/Validation.h"

#include "TestSupport.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

using loomtest::gitOutput;
using loomtest::readFile;
using loomtest::writeFile;

namespace {

/// Copies a folder's files into another, which is created.
void copyFolder(const QString& from, const QString& to)
{
    const QDir source(from);
    QDirIterator it(from, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        writeFile(to + u'/' + source.relativeFilePath(path), readFile(path));
    }
}

/// A copy of a test project in a temporary folder, and a git with a test identity.
struct Copy {
    QTemporaryDir temp;
    QString root;
    loom::GitTool git;

    explicit Copy(const QString& project)
    {
        REQUIRE(temp.isValid());
        root = temp.path() + "/Project";
        copyFolder(project, root);
        git = loomtest::testGit(temp.path() + "/gitconfig");
    }

    /// Makes the copy a git repository and commits everything in it.
    void commitAll() const
    {
        gitOutput(git, {"init", "-q", "-b", "main"}, root);
        gitOutput(git, {"add", "--all"}, root);
        gitOutput(git, {"commit", "-q", "-m", "Test"}, root);
    }

    /// Validates the copy.
    loom::ValidationResult validate(bool staged = false) const
    {
        loom::ValidationRequest request;
        request.root = root;
        request.staged = staged;
        request.git = git;
        return loom::validateProject(request);
    }
};

/// Each problem as `<code> <file>:<line>: <severity>: <message>`.
QStringList lines(const loom::ValidationResult& result)
{
    QStringList out;
    for (const loom::Problem& problem : result.problems)
        out << problem.code + u' ' + loom::problemLine(problem);
    return out;
}

} // namespace

TEST_CASE("validate finds exactly the worked example's problems", "[validate]")
{
    // Specs/m1-core.md, "The worked example": "Expected validation problems (and no others)".
    const Copy copy(QStringLiteral(LOOM_WORKED_EXAMPLE));
    copy.commitAll();
    const loom::ValidationResult result = copy.validate();
    REQUIRE(result.outcome == loom::ValidationOutcome::Checked);
    CHECK(lines(result) == QStringList{
        "premise-cycle Proofs/H8KD/H8KD.a.tex:4: error: Premises go round in a circle: H8KD → J3RW → H8KD. A statement can't help prove itself.",
        "premise-cycle Proofs/K6YM/K6YM.a.tex:4: error: Premises go round in a circle: K6YM → M1PQ → K6YM. A statement can't help prove itself.",
        "premise-superseded Proofs/P3QA/P3QA.a.tex:4: warning: P3QA.a uses N7ZE, which is superseded by 4KX7.",
        "premise-false Proofs/T9GB/T9GB.a.tex:4: warning: T9GB.a uses F2NS, which is false.",
        "refuted-but-proved Statements/F2NS.tex:5: warning: F2NS is marked false, but F2NS.a would prove it. One of them is wrong.",
    });
    CHECK(result.errors() == 2);
    CHECK(result.warnings() == 3);
}

TEST_CASE("validate reports one of every problem, exactly", "[validate]")
{
    // Tests/Projects/EveryProblem has the problems that one set of files can show; the four that
    // need special bytes or the last commit are made here.
    const Copy copy(QStringLiteral(LOOM_EVERY_PROBLEM));
    writeFile(copy.root + "/Statements/N8T8.tex", "+++\nid = \"N8T8\"\ntitle = \"\xff\"\n+++\nBody.\n");
    writeFile(copy.root + "/Statements/B0M0.tex",
              "\xef\xbb\xbf+++\nid = \"B0M0\"\nkind = \"lemma\"\ntitle = \"T\"\ncreated = 2026-11-04\n+++\nBody of B0M0.\n");
    copy.commitAll();
    REQUIRE(QFile::remove(copy.root + "/Statements/DE1T.tex"));
    QByteArray checks = readFile(copy.root + "/Proofs/CK2A/CK2A.a.checks.toml");
    REQUIRE(checks.contains("verdict = \"pass\""));
    writeFile(copy.root + "/Proofs/CK2A/CK2A.a.checks.toml", checks.replace("verdict = \"pass\"", "verdict = \"fail\""));

    const loom::ValidationResult result = copy.validate();
    REQUIRE(result.outcome == loom::ValidationOutcome::Checked);
    QStringList expected = QString::fromUtf8(readFile(QStringLiteral(LOOM_EVERY_PROBLEM "-expected.txt"))).split(u'\n');
    expected.removeAll(QString());
    CHECK(lines(result) == expected);

    // Every code of the spec's tables appears, except `format-too-new`, which stops validation.
    QStringList codes;
    for (const loom::Problem& problem : result.problems)
        codes << problem.code;
    codes.removeDuplicates();
    codes.sort();
    CHECK(codes == QStringList{
        "alias-duplicate", "alias-invalid", "arxiv-version-mismatch", "authors-missing", "bad-path",
        "bad-value", "body-empty", "bom", "check-by", "check-changed", "check-changes-missing",
        "check-changes-unexpected", "check-duplicate", "check-fingerprint", "check-order",
        "check-record-missing", "check-record-not-found", "check-verdict", "checks-orphan",
        "cite-alias", "cite-unknown", "conjecture-proved", "deleted-since-commit", "field-expected",
        "field-not-for-type", "field-shape", "field-source", "field-verified", "front-matter-missing",
        "front-matter-unclosed", "id-duplicate", "id-mismatch", "id-syntax", "missing-field",
        "not-utf8", "paper-key-duplicate", "paper-key-mismatch", "paper-key-rule", "pdf-path",
        "preamble-missing", "premise-cycle", "premise-duplicate", "premise-false", "premise-no-status",
        "premise-self", "premise-superseded", "proof-folder-unknown", "proof-id-mismatch",
        "proof-of-no-status", "proof-of-stub", "proves-mismatch", "ref-alias", "ref-dangling",
        "ref-not-listed", "ref-unknown", "refuted-but-proved", "refuted-incomplete",
        "refuted-no-status", "refuted-unknown", "session-missing", "stated-using-cycle",
        "stated-using-self", "stray-file", "stub-gist-empty", "stub-locator-missing",
        "stub-paper-premise", "superseded-invalid", "tag-duplicate", "tag-syntax", "tag-unknown",
        "toml-syntax", "unknown-field", "unknown-top-level", "verbose-orphan", "verbose-stale",
        "version-duplicate", "version-id", "version-unknown", "wrong-type"});
}

TEST_CASE("validate stops on a project from a newer Loom", "[validate]")
{
    const Copy copy(QStringLiteral(LOOM_WORKED_EXAMPLE));
    writeFile(copy.root + "/loom.toml", "format = 2\nname = \"Future\"\n");
    const loom::ValidationResult result = copy.validate();
    CHECK(result.outcome == loom::ValidationOutcome::CantWork);
    CHECK(result.code == "format-too-new");
    CHECK(result.message == "This project was made by a newer Loom (format 2). Update Loom to open it.");
}

TEST_CASE("validate --staged checks what git has staged, not the working folder", "[validate]")
{
    const Copy copy(QStringLiteral(LOOM_WORKED_EXAMPLE));
    copy.commitAll();
    const QString path = copy.root + "/Statements/S6KT.tex";
    const QByteArray good = readFile(path);
    QByteArray broken = good;
    broken.replace("kind = ", "knd = ");

    // Broken and staged: both see it.
    writeFile(path, broken);
    gitOutput(copy.git, {"add", "Statements/S6KT.tex"}, copy.root);
    CHECK(copy.validate(true).errors() == 4);
    CHECK(copy.validate(false).errors() == 4);

    // Fixed in the working folder only: the commit would still be broken.
    writeFile(path, good);
    CHECK(copy.validate(true).errors() == 4);
    CHECK(copy.validate(false).errors() == 2);

    // A staged deletion is seen too, and a file only in the working folder isn't.
    gitOutput(copy.git, {"add", "Statements/S6KT.tex"}, copy.root);
    gitOutput(copy.git, {"rm", "-q", "--cached", "Statements/S6KT.tex"}, copy.root);
    writeFile(copy.root + "/Notes.txt", "Not staged.\n");
    QStringList staged;
    for (const loom::Problem& problem : copy.validate(true).problems)
        staged << problem.code;
    CHECK(staged.contains("deleted-since-commit"));
    CHECK_FALSE(staged.contains("unknown-top-level"));
    QStringList working;
    for (const loom::Problem& problem : copy.validate(false).problems)
        working << problem.code;
    CHECK_FALSE(working.contains("deleted-since-commit"));
    CHECK(working.contains("unknown-top-level"));
}

TEST_CASE("validate --staged needs a git repository", "[validate]")
{
    const Copy copy(QStringLiteral(LOOM_WORKED_EXAMPLE));
    loom::GitTool git = copy.git;
    git.environment.insert("GIT_CEILING_DIRECTORIES", copy.temp.path());
    loom::ValidationRequest request;
    request.root = copy.root;
    request.staged = true;
    request.git = git;
    const loom::ValidationResult result = loom::validateProject(request);
    CHECK(result.outcome == loom::ValidationOutcome::CantWork);
    CHECK(result.code == "not-a-repository");

    // Without --staged there is just no commit to compare with.
    request.staged = false;
    CHECK(loom::validateProject(request).outcome == loom::ValidationOutcome::Checked);
}

TEST_CASE("committed checks may move, but not change or go", "[validate]")
{
    // Specs/m1-core.md, "Checks", rule 1, and the edge case of a merge interleaving by date.
    const QString a = "[[checks]]\nby = \"agent:x\"\ndate = 2026-11-04T10:00:00-06:00\nfingerprint = \"0123456789ab\"\n"
                      "verdict = \"pass\"\nrecord = \"R.md\"\n";
    const QString b = "[[checks]]\nby = \"dakota\"\ndate = 2026-11-04T11:00:00-06:00\nfingerprint = \"0123456789ab\"\nverdict = \"pass\"\n";
    const QString c = "[[checks]]\nby = \"agent:y\"\ndate = 2026-11-04T10:30:00-06:00\nfingerprint = \"0123456789ab\"\n"
                      "verdict = \"fail\"\nrecord = \"R.md\"\n";
    loom::CommittedFiles committed;
    committed.paths << "Proofs/4KX7/4KX7.a.checks.toml";
    committed.checks.insert("Proofs/4KX7/4KX7.a.checks.toml", (a + u'\n' + b).toUtf8());

    const auto problems = [&](const QString& now) {
        loom::ProjectContents contents;
        contents.checks.push_back({"Proofs/4KX7/4KX7.a.checks.toml", "4KX7.a", loom::readChecks(now.toUtf8(), "Proofs/4KX7/4KX7.a.checks.toml")});
        QStringList out;
        for (const loom::Problem& problem : loom::checkAgainstCommit(contents, committed))
            out << QStringLiteral("%1:%2 %3").arg(problem.code).arg(problem.line).arg(problem.message);
        return out;
    };
    CHECK(problems(a + u'\n' + b).isEmpty());
    CHECK(problems(a + u'\n' + c + u'\n' + b).isEmpty());
    CHECK(problems(a) == QStringList{
        "check-changed:1 A check committed earlier (dakota, 2026-11-04T11:00:00-06:00) was changed or removed. "
        "Checks are never edited; add a new one instead."});
    QString edited = b;
    edited.replace("pass", "fail");
    CHECK(problems(a + u'\n' + edited) == QStringList{
        "check-changed:8 A check committed earlier (dakota, 2026-11-04T11:00:00-06:00) was changed or removed. "
        "Checks are never edited; add a new one instead."});
}

TEST_CASE("body references: what counts as a reference", "[validate]")
{
    const Copy copy(QStringLiteral(LOOM_WORKED_EXAMPLE));
    const QString path = copy.root + "/Proofs/5HN8/5HN8.a.tex";
    QByteArray proof = readFile(path);
    // 5HN8.a lists no premises. None of these refer to a statement it should list.
    proof.append("See \\reference{9D3F}, \\ref*{9D3F}, % \\ref{9D3F}\n"
                 "\\ref\n{9D3F} and \\label{eq:a} \\ref{eq:a}, \\ref{2M9Q} and \\ref{5HN8}.\n"
                 "Then \\ref [x]{9D3F} and \\paperref{3WQ6} and \\cite{SmithJones24,  Other}.\n");
    writeFile(path, proof);
    QStringList found;
    for (const loom::Problem& problem : copy.validate().problems) {
        if (problem.file == u"Proofs/5HN8/5HN8.a.tex")
            found << QStringLiteral("%1:%2 %3").arg(problem.code).arg(problem.line).arg(problem.message);
    }
    CHECK(found == QStringList{
        "ref-not-listed:12 The text refers to 9D3F (`\\ref`) but `premises` doesn't list it.",
        "ref-not-listed:12 The text refers to 3WQ6 (`\\paperref`) but `premises` doesn't list it."});
}
