// `loom validate` (Specs/m1-core.md, "Validation"; Verification, "Worked example" and
// "Validation messages": "A test project with one of every problem gives exactly the expected
// list"). The JSON of the same list is checked through `loom` in Tests/CMake/ValidateCli.cmake.

#include "LoomCore/Validation.h"

#include "TestSupport.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <utility>
#include <vector>

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

/// Points the process's temporary folder (QDir::tempPath()) somewhere else while it lives.
struct TemporaryFolder {
    static constexpr const char* names[] = {"TMP", "TEMP", "TMPDIR"};
    /// Each variable's value before, and whether it was set.
    std::vector<std::pair<QByteArray, bool>> saved;

    /// Sets `TMP`, `TEMP` and `TMPDIR` to `folder`. Side effects: changes the environment.
    explicit TemporaryFolder(const QString& folder)
    {
        for (const char* name : names) {
            saved.emplace_back(qgetenv(name), qEnvironmentVariableIsSet(name));
            qputenv(name, QDir::toNativeSeparators(folder).toLocal8Bit());
        }
    }
    /// Puts the environment back as it was.
    ~TemporaryFolder()
    {
        for (std::size_t i = 0; i < saved.size(); ++i) {
            if (saved[i].second)
                qputenv(names[i], saved[i].first);
            else
                qunsetenv(names[i]);
        }
    }
    TemporaryFolder(const TemporaryFolder&) = delete;
    TemporaryFolder& operator=(const TemporaryFolder&) = delete;
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

    // Every code of the spec's tables appears, except `format-too-new`, which stops validation;
    // so do `paperref-not-stub` and `refuted-self`, which wait for the spec (M1 review, S22 and S26).
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
        "not-utf8", "paper-key-duplicate", "paper-key-mismatch", "paper-key-rule", "paperref-not-stub", "pdf-path",
        "preamble-missing", "premise-cycle", "premise-duplicate", "premise-false", "premise-no-status",
        "premise-self", "premise-superseded", "proof-folder-unknown", "proof-id-mismatch",
        "proof-of-no-status", "proof-of-stub", "proves-mismatch", "ref-alias", "ref-dangling",
        "ref-not-listed", "ref-unknown", "refuted-but-proved", "refuted-incomplete",
        "refuted-no-status", "refuted-self", "refuted-unknown", "session-missing", "stated-using-cycle",
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

TEST_CASE("validate in a repository with no commit yet has nothing to compare with", "[validate]")
{
    // The first commit through the pre-commit hook.
    const Copy copy(QStringLiteral(LOOM_WORKED_EXAMPLE));
    gitOutput(copy.git, {"init", "-q", "-b", "main"}, copy.root);
    CHECK(copy.validate(false).outcome == loom::ValidationOutcome::Checked);
    gitOutput(copy.git, {"add", "--all"}, copy.root);
    const loom::ValidationResult staged = copy.validate(true);
    CHECK(staged.outcome == loom::ValidationOutcome::Checked);
    CHECK(staged.errors() == 2); // The worked example's two premise cycles.
}

TEST_CASE("validate reports a git failure reading the last commit", "[validate]")
{
    // Only "no repository" and "no commit yet" mean there is nothing to compare with; any other
    // failure would silently skip `check-changed` and `deleted-since-commit`.
    const Copy copy(QStringLiteral(LOOM_WORKED_EXAMPLE));
    const char* mode = GENERATE("head-fails", "dubious");
    INFO(mode);
    loom::ValidationRequest request;
    request.root = copy.root;
    request.git.program = QStringLiteral(LOOM_FAKE_GIT);
    request.git.environment.insert("LOOM_FAKE_GIT_MODE", mode);
    const loom::ValidationResult result = loom::validateProject(request);
    CHECK(result.outcome == loom::ValidationOutcome::CantWork);
    CHECK(result.code == "git-failed");
    CHECK(result.message.startsWith("Loom couldn't read the last commit to compare the checks with it"));
}

TEST_CASE("validate without --staged makes no temporary folder", "[validate]")
{
    // validateProject() needs a temporary folder only for the staged copy. Point the temporary
    // folder at an empty one of the test's own and watch its modification time, which making
    // and removing a folder in it changes.
    const Copy copy(QStringLiteral(LOOM_WORKED_EXAMPLE));
    copy.commitAll();
    const QString temporary = copy.temp.path() + "/Temporary";
    REQUIRE(QDir().mkpath(temporary));
    const TemporaryFolder redirected(temporary);
    REQUIRE(QDir::cleanPath(QDir::tempPath()) == QDir::cleanPath(temporary));

    const auto changedBy = [&](bool staged) {
        const QDateTime before = QFileInfo(temporary).lastModified();
        QThread::msleep(50);
        const loom::ValidationResult result = copy.validate(staged);
        CHECK(result.outcome == loom::ValidationOutcome::Checked);
        return QFileInfo(temporary).lastModified() != before;
    };
    CHECK_FALSE(changedBy(false));
    CHECK(changedBy(true)); // The way of watching works.
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
        "check-changed:1 A check committed earlier (dakota, 2026-11-04T11:00:00-06:00) was changed, moved or removed. "
        "Checks are never edited or reordered; add a new one instead."});
    QString edited = b;
    edited.replace("pass", "fail");
    CHECK(problems(a + u'\n' + edited) == QStringList{
        "check-changed:8 A check committed earlier (dakota, 2026-11-04T11:00:00-06:00) was changed, moved or removed. "
        "Checks are never edited or reordered; add a new one instead."});
}

TEST_CASE("committed checks keep their order, even with the same date", "[validate]")
{
    // Specs/m1-core.md, "Checks", rule 1 (A7): committed entries stay in the same order relative
    // to each other. Rule 2 breaks a tie by position, so swapping two entries of the same second
    // would change which check counts: here Dakota's needs-changes would give way to his pass.
    const QString a = "[[checks]]\nby = \"agent:x\"\ndate = 2026-11-04T10:00:00-06:00\nfingerprint = \"0123456789ab\"\n"
                      "verdict = \"pass\"\nrecord = \"R.md\"\n";
    const QString pass = "[[checks]]\nby = \"dakota\"\ndate = 2026-11-04T11:00:00-06:00\nfingerprint = \"0123456789ab\"\nverdict = \"pass\"\n";
    const QString fix = "[[checks]]\nby = \"dakota\"\ndate = 2026-11-04T11:00:00-06:00\nfingerprint = \"0123456789ab\"\n"
                        "verdict = \"needs-changes\"\nchanges = \"Fix step 3.\"\n";
    const QString c = "[[checks]]\nby = \"agent:y\"\ndate = 2026-11-04T11:00:00-06:00\nfingerprint = \"0123456789ab\"\n"
                      "verdict = \"pass\"\nrecord = \"R.md\"\n";
    const QString path = "Proofs/4KX7/4KX7.a.checks.toml";
    loom::CommittedFiles committed;
    committed.paths << path;
    committed.checks.insert(path, (a + u'\n' + pass + u'\n' + fix).toUtf8());

    const auto problems = [&](const QString& now) {
        loom::ProjectContents contents;
        contents.checks.push_back({path, "4KX7.a", loom::readChecks(now.toUtf8(), path)});
        REQUIRE(contents.checks[0].reading.problems.empty()); // No check-order: the dates tie.
        QStringList out;
        for (const loom::Problem& problem : loom::checkAgainstCommit(contents, committed))
            out << QStringLiteral("%1:%2 %3").arg(problem.code).arg(problem.line).arg(problem.message);
        return out;
    };
    CHECK(problems(a + u'\n' + pass + u'\n' + fix).isEmpty());
    // A new entry may sit between committed ones, as a merge interleaves.
    CHECK(problems(a + u'\n' + pass + u'\n' + c + u'\n' + fix).isEmpty());
    // Swapped: reported on the entry that moved.
    CHECK(problems(a + u'\n' + fix + u'\n' + pass) == QStringList{
        "check-changed:8 A check committed earlier (dakota, 2026-11-04T11:00:00-06:00) was changed, moved or removed. "
        "Checks are never edited or reordered; add a new one instead."});
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

TEST_CASE("body references: \\paperref names a paper stub", "[validate]")
{
    // Specs/loom.md: `\paperref{<stub-id>}` cites a paper's result. A statement of the project, by
    // tag or alias, is `\ref`'s; a `\label` is never a stub.
    const Copy copy(QStringLiteral(LOOM_WORKED_EXAMPLE));
    const QString path = copy.root + "/Proofs/5HN8/5HN8.a.tex";
    QByteArray proof = readFile(path);
    proof.append("\\label{eq:b} \\paperref{4KX7}, \\paperref{thm:energy-decay}, \\paperref{ZZZZ}, \\paperref{eq:b}.\n");
    writeFile(path, proof);
    const QString statement = copy.root + "/Statements/4KX7.tex";
    QByteArray text = readFile(statement);
    REQUIRE(text.contains("stated_using = [\"2M9Q\"]\n"));
    writeFile(statement, text.replace("stated_using = [\"2M9Q\"]\n", "stated_using = [\"2M9Q\"]\naliases = [\"thm:energy-decay\"]\n"));
    QStringList found;
    for (const loom::Problem& problem : copy.validate().problems) {
        if (problem.file == u"Proofs/5HN8/5HN8.a.tex")
            found << QStringLiteral("%1:%2:%3 %4").arg(problem.code).arg(problem.line)
                         .arg(problem.severity == loom::Severity::Error ? "error" : "warning", problem.message);
    }
    const QString stub = "it isn't the tag or old name of a paper stub.";
    CHECK(found == QStringList{
        "paperref-not-stub:9:error `\\paperref{4KX7}` names 4KX7, a statement of this project, not a paper's result. Use `\\ref{4KX7}`.",
        "paperref-not-stub:9:error `\\paperref{thm:energy-decay}` names 4KX7, a statement of this project, not a paper's result. Use `\\ref{4KX7}`.",
        "ref-dangling:9:warning `\\paperref{ZZZZ}` points to nothing: " + stub,
        "ref-dangling:9:warning `\\paperref{eq:b}` points to nothing: " + stub});
}
