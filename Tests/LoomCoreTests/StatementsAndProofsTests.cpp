// Statement, proof and verbose proof files: reading, the problems each file shows on its own,
// and the canonical writer (Specs/m1-core.md, "Front matter", "Statements", "Proofs" and
// "Validation").

#include "LoomCore/StatementsAndProofs.h"

#include "TestSupport.h"

#include <catch2/catch_test_macros.hpp>

#include <tuple>
#include <vector>

namespace {

using P = std::tuple<QString, int, QString>;

/// The spec's example statement, byte for byte.
constexpr auto specStatement = R"(+++
id = "4KX7"
kind = "theorem"
title = "Energy decay for the model system"
tags = ["model-system"]
stated_using = ["2M9Q"]
aliases = ["thm:energy-decay"]
origin = "spec:S-004"
created = 2026-11-03
verified = 2026-11-03
+++
Let $u_0\in L^2(\R^n)$ and let $u$ be a weak solution with data $u_0$ ...
)";

/// The spec's example proof.
constexpr auto specProof = R"(+++
id = "4KX7.a"
proves = "4KX7"
premises = ["7RT2", "9D3F"]
level = "complete"
created = 2026-11-03
verified = 2026-11-03
+++
By Fact \ref{7RT2} with $k=0$, ...
)";

/// The spec's example verbose proof, with a body.
constexpr auto specVerbose = R"(+++
proof = "4KX7.a"
fingerprint = "3c8e1f0a92d4"
created = 2026-11-05
+++
Step 1. ...
)";

const QString statementPath = QStringLiteral("Statements/4KX7.tex");
const QString proofPath = QStringLiteral("Proofs/4KX7/4KX7.a.tex");
const QString verbosePath = QStringLiteral("Proofs/4KX7/4KX7.a.verbose.tex");

/// A valid statement with `extra` lines added before `created` (line 5 onwards).
QByteArray statementWith(const QByteArray& extra, const QByteArray& body = "Body\n")
{
    return "+++\nid = \"4KX7\"\nkind = \"theorem\"\ntitle = \"T\"\n" + extra + "created = 2026-11-03\n+++\n" + body;
}

/// A valid proof with `extra` lines added before `created` (line 6 onwards).
QByteArray proofWith(const QByteArray& extra, const QByteArray& premises = "[]")
{
    return "+++\nid = \"4KX7.a\"\nproves = \"4KX7\"\npremises = " + premises + "\nlevel = \"complete\"\n" + extra
         + "created = 2026-11-03\n+++\nBody\n";
}

/// Each problem as (code, line, message), checking the file each names.
std::vector<P> summary(const std::vector<loom::Problem>& problems, const QString& file)
{
    std::vector<P> out;
    for (const loom::Problem& problem : problems) {
        CHECK(problem.file == file);
        out.emplace_back(problem.code, problem.line, problem.message);
    }
    return out;
}

/// The problems of a statement file read as `statementPath`, as summary() gives them.
std::vector<P> statementProblems(const QByteArray& bytes)
{
    return summary(loom::readStatement(bytes, statementPath).problems, statementPath);
}

/// The problems of a proof file read as `proofPath`, as summary() gives them.
std::vector<P> proofProblems(const QByteArray& bytes)
{
    return summary(loom::readProof(bytes, proofPath).problems, proofPath);
}

} // namespace

TEST_CASE("The spec's example statement reads without problems", "[statements]")
{
    const loom::StatementReading reading = loom::readStatement(specStatement, statementPath);
    CHECK(reading.problems.empty());
    CHECK(reading.parsed);
    CHECK(reading.lossless);
    const loom::Statement& s = reading.document;
    CHECK(s.id == "4KX7");
    CHECK(s.kind == "theorem");
    CHECK(s.title == "Energy decay for the model system");
    CHECK(s.tags == QStringList{"model-system"});
    CHECK(s.statedUsing == QStringList{"2M9Q"});
    CHECK(s.aliases == QStringList{"thm:energy-decay"});
    CHECK(s.origin == "spec:S-004");
    CHECK(s.created == QDate(2026, 11, 3));
    CHECK(s.verified == QDate(2026, 11, 3));
    CHECK(s.body == "Let $u_0\\in L^2(\\R^n)$ and let $u$ be a weak solution with data $u_0$ ...\n");
    CHECK(reading.lines.of("stated_using", 0) == 6);
}

TEST_CASE("The spec's examples are in canonical form", "[statements]")
{
    CHECK(loom::formatStatement(loom::readStatement(specStatement, statementPath).document) == QByteArray(specStatement));
    const loom::ProofReading proof = loom::readProof(specProof, proofPath);
    CHECK(proof.problems.empty());
    CHECK(loom::formatProof(proof.document) == QByteArray(specProof));
    const loom::VerboseReading verbose = loom::readVerboseProof(specVerbose, verbosePath);
    CHECK(verbose.problems.empty());
    CHECK(loom::formatVerboseProof(verbose.document) == QByteArray(specVerbose));
}

TEST_CASE("Rewriting keeps the body byte for byte", "[statements]")
{
    const QByteArray front = "+++\nid = \"4KX7\"\nkind = \"lemma\"\ntitle = \"T\"\ncreated = 2026-11-03\n+++\n";

    SECTION("a +++ line in a verbatim block, Unicode, no final newline")
    {
        const QByteArray body = QString::fromUtf8("\\begin{verbatim}\n+++\n\\end{verbatim}\nFür $x ∈ ℝ$: 𝔼|u|² ≤ C.  \n\nEnd").toUtf8();
        const loom::StatementReading reading = loom::readStatement(front + body, statementPath);
        CHECK(reading.problems.empty());
        CHECK(loom::formatStatement(reading.document) == front + body);
    }

    SECTION("CRLF in, LF out")
    {
        QByteArray crlf = front + "Line one.\nLine two.\n";
        crlf.replace("\n", "\r\n");
        CHECK(loom::formatStatement(loom::readStatement(crlf, statementPath).document) == front + "Line one.\nLine two.\n");
    }

    SECTION("a byte-order mark is dropped")
    {
        const loom::StatementReading reading = loom::readStatement("\xEF\xBB\xBF" + front + "Body\n", statementPath);
        CHECK(statementProblems("\xEF\xBB\xBF" + front + "Body\n") == std::vector<P>{
            {"bom", 1, "This file starts with a byte-order mark. Loom will drop it the next time it saves the file."}});
        CHECK(loom::formatStatement(reading.document) == front + "Body\n");
    }

    SECTION("comments in front matter are dropped; fields come out in the spec's order")
    {
        const QByteArray messy = "+++\n# A comment.\ncreated = 2026-11-03\ntitle = 'T'\nkind = \"lemma\"\nid = \"4KX7\"\n+++\nBody\n";
        CHECK(loom::formatStatement(loom::readStatement(messy, statementPath).document) == front + "Body\n");
    }
}

TEST_CASE("The canonical writer", "[statements]")
{
    loom::Statement s;
    s.id = "F2NS";
    s.kind = "lemma";
    s.title = QString::fromUtf8("A \"quoted\" title \\ with a\ttab and é");
    s.tags = {"model-system", "energy-estimates"};
    s.statedUsing = {"2M9Q"};
    s.aliases = {"lem:old"};
    s.origin = "import:Notes/old.tex";
    s.supersededBy = "4KX7";
    s.refutedBy = "G5TZ.a";
    s.refutedOn = QDate(2026, 11, 6);
    s.created = QDate(2026, 11, 3);
    s.verified = QDate(2026, 11, 4);
    s.body = "Body\n";
    CHECK(loom::formatStatement(s) == QString::fromUtf8(
        "+++\n"
        "id = \"F2NS\"\n"
        "kind = \"lemma\"\n"
        "title = \"A \\\"quoted\\\" title \\\\ with a\\ttab and é\"\n"
        "tags = [\"model-system\", \"energy-estimates\"]\n"
        "stated_using = [\"2M9Q\"]\n"
        "aliases = [\"lem:old\"]\n"
        "origin = \"import:Notes/old.tex\"\n"
        "superseded_by = \"4KX7\"\n"
        "refuted_by = \"G5TZ.a\"\n"
        "refuted_on = 2026-11-06\n"
        "created = 2026-11-03\n"
        "verified = 2026-11-04\n"
        "+++\n"
        "Body\n").toUtf8());

    loom::Statement minimal;
    minimal.id = "4KX7";
    minimal.kind = "definition";
    minimal.title = "T";
    minimal.created = QDate(2026, 11, 3);
    minimal.body = "Body\n";
    CHECK(loom::formatStatement(minimal) == "+++\nid = \"4KX7\"\nkind = \"definition\"\ntitle = \"T\"\ncreated = 2026-11-03\n+++\nBody\n");

    loom::Proof p;
    p.id = "4KX7.a";
    p.proves = "4KX7";
    p.level = "sketch";
    p.created = QDate(2026, 11, 3);
    p.body = "Body\n";
    CHECK(loom::formatProof(p) == "+++\nid = \"4KX7.a\"\nproves = \"4KX7\"\npremises = []\nlevel = \"sketch\"\ncreated = 2026-11-03\n+++\nBody\n");
}

TEST_CASE("Body text given to Loom", "[statements]")
{
    CHECK(loom::bodyFromText("Text") == "Text\n");
    CHECK(loom::bodyFromText("Text\n") == "Text\n");
    CHECK(loom::bodyFromText("A\r\nB\r\n") == "A\nB\n");
    CHECK(loom::bodyFromText("\xEF\xBB\xBFText") == "Text\n");
}

TEST_CASE("Statement problems", "[statements]")
{
    SECTION("syntax")
    {
        CHECK(statementProblems("Body\n") == std::vector<P>{
            {"front-matter-missing", 1, "The first line should be `+++`: Loom files start with front matter between two `+++` lines."}});
        const std::vector<P> problems = statementProblems("+++\nid = \"4KX7\"\nkind = \"theorem\"\ntitle = \ncreated = 2026-11-03\n+++\nBody\n");
        REQUIRE(problems.size() == 1);
        CHECK(std::get<0>(problems[0]) == "toml-syntax");
        CHECK(std::get<1>(problems[0]) == 4);
        CHECK(std::get<2>(problems[0]).startsWith("This isn't valid TOML: "));
        CHECK_FALSE(loom::readStatement("Body\n", statementPath).parsed);
    }

    SECTION("an unknown field")
    {
        const loom::StatementReading reading = loom::readStatement(statementWith("colour = \"blue\"\n"), statementPath);
        CHECK(summary(reading.problems, statementPath) == std::vector<P>{
            {"unknown-field", 5, "`colour` isn't a field of a statement. Is it misspelt? The fields are: id, kind, title, tags, "
                                 "stated_using, aliases, origin, superseded_by, refuted_by, refuted_on, created, verified."}});
        CHECK(reading.parsed);
        CHECK_FALSE(reading.lossless);
    }

    SECTION("every required field missing")
    {
        const loom::StatementReading reading = loom::readStatement("+++\n+++\nBody\n", statementPath);
        CHECK(summary(reading.problems, statementPath) == std::vector<P>{
            {"missing-field", 1, "`id` is missing; every statement needs one."},
            {"missing-field", 1, "`kind` is missing; every statement needs one."},
            {"missing-field", 1, "`title` is missing; every statement needs one."},
            {"missing-field", 1, "`created` is missing; every statement needs one."}});
        CHECK(reading.lossless); // Nothing was dropped, so it can be rewritten.
    }

    SECTION("wrong types")
    {
        CHECK(statementProblems(statementWith("verified = \"2026-11-03\"\n")) == std::vector<P>{
            {"wrong-type", 5, "`verified` should be a date, not a string."}});
        CHECK(statementProblems(statementWith("verified = 2026-11-03T10:00:00-06:00\n")) == std::vector<P>{
            {"wrong-type", 5, "`verified` should be a date, not a date and time."}});
        CHECK(statementProblems(statementWith("tags = \"model-system\"\n")) == std::vector<P>{
            {"wrong-type", 5, "`tags` should be a list of tags, not a string."}});
        CHECK(statementProblems(statementWith("stated_using = [\n  \"2M9Q\",\n  7,\n]\n")) == std::vector<P>{
            {"wrong-type", 7, "`stated_using` should be a list of tags, not a list holding a whole number."}});
        CHECK(statementProblems(statementWith("origin = 3\n")) == std::vector<P>{
            {"wrong-type", 5, "`origin` should be a string, not a whole number."}});
        CHECK(statementProblems("+++\nid = \"4KX7\"\nkind = \"theorem\"\ntitle = \"Two\\nlines\"\ncreated = 2026-11-03\n+++\nBody\n") == std::vector<P>{
            {"wrong-type", 4, "`title` should be a string on one line, not a string with line breaks."}});
        CHECK(statementProblems("+++\nid = 4\nkind = \"theorem\"\ntitle = \"T\"\ncreated = 2026-11-03\n+++\nBody\n") == std::vector<P>{
            {"wrong-type", 2, "`id` should be a string, not a whole number."}});
        CHECK_FALSE(loom::readStatement(statementWith("tags = \"model-system\"\n"), statementPath).lossless);
    }

    SECTION("bad values")
    {
        CHECK(statementProblems("+++\nid = \"4KX7\"\nkind = \"axiom\"\ntitle = \"T\"\ncreated = 2026-11-03\n+++\nBody\n") == std::vector<P>{
            {"bad-value", 3, "`kind` can't be `axiom`; it must be one of `definition`, `lemma`, `proposition`, `theorem`, "
                             "`corollary`, `conjecture`, `question`, `remark`, `example`."}});
        for (const char* origin : {"spec:", "import:", "news"})
            CHECK(statementProblems(statementWith("origin = \"" + QByteArray(origin) + "\"\n")) == std::vector<P>{
                {"bad-value", 5, QStringLiteral("`origin` can't be `%1`; it must be one of `new`, `spec:<spec-id>`, `import:<path>`.").arg(origin)}});
        for (const char* origin : {"new", "spec:S-004", "import:Notes/old.tex"})
            CHECK(statementProblems(statementWith("origin = \"" + QByteArray(origin) + "\"\n")).empty());
        CHECK(statementProblems(statementWith("tags = [\"a\", \"b\", \"a\"]\n")) == std::vector<P>{
            {"bad-value", 5, "`tags` can't list `a` twice."}});
        CHECK(statementProblems(statementWith("stated_using = [\"2M9Q\", \"2M9Q\"]\n")) == std::vector<P>{
            {"bad-value", 5, "`stated_using` can't list `2M9Q` twice."}});
    }

    SECTION("the id")
    {
        CHECK(statementProblems("+++\nid = \"4kx7\"\nkind = \"theorem\"\ntitle = \"T\"\ncreated = 2026-11-03\n+++\nBody\n") == std::vector<P>{
            {"id-syntax", 2, "`4kx7` isn't a valid tag: four characters from 0–9 and A–Z without I, L, O and U. Did you mean `4KX7`?"}});
        CHECK(statementProblems("+++\nid = \"4KXU\"\nkind = \"theorem\"\ntitle = \"T\"\ncreated = 2026-11-03\n+++\nBody\n") == std::vector<P>{
            {"id-syntax", 2, "`4KXU` isn't a valid tag: four characters from 0–9 and A–Z without I, L, O and U."}});
        CHECK(statementProblems("+++\nid = \"4KX8\"\nkind = \"theorem\"\ntitle = \"T\"\ncreated = 2026-11-03\n+++\nBody\n") == std::vector<P>{
            {"id-mismatch", 2, "The id is `4KX8` but the file is named `4KX7`; they must match."}});
    }

    SECTION("aliases")
    {
        CHECK(statementProblems(statementWith("aliases = [\"thm:ok\", \"two words\", \"abcd\", \"thm:ok\"]\n")) == std::vector<P>{
            {"bad-value", 5, "`aliases` can't list `thm:ok` twice."},
            {"alias-invalid", 5, "`two words` can't be an alias: it contains whitespace or one of {}[],\"."},
            {"alias-invalid", 5, "`abcd` can't be an alias: it is shaped like a tag."}});
    }

    SECTION("refuting needs both fields")
    {
        CHECK(statementProblems(statementWith("refuted_by = \"G5TZ.a\"\n")) == std::vector<P>{
            {"refuted-incomplete", 5, "4KX7 has `refuted_by` but not `refuted_on`. Refuting a statement needs both: the proof and the date Dakota accepted it."}});
        CHECK(statementProblems(statementWith("refuted_on = 2026-11-06\n")) == std::vector<P>{
            {"refuted-incomplete", 5, "4KX7 has `refuted_on` but not `refuted_by`. Refuting a statement needs both: the proof and the date Dakota accepted it."}});
        CHECK(statementProblems(statementWith("refuted_by = \"G5TZ.a\"\nrefuted_on = 2026-11-06\n")).empty());
    }

    SECTION("an empty body")
    {
        CHECK(statementProblems(statementWith("", " \n\t\n")) == std::vector<P>{
            {"body-empty", 7, "4KX7 has no text below its front matter."}});
        CHECK(statementProblems(statementWith("", "")) == std::vector<P>{
            {"body-empty", 7, "4KX7 has no text below its front matter."}});
    }
}

TEST_CASE("Proof problems", "[proofs]")
{
    SECTION("fields")
    {
        CHECK(proofProblems("+++\nid = \"4KX7.a\"\n+++\nBody\n") == std::vector<P>{
            {"missing-field", 1, "`proves` is missing; every proof needs one."},
            {"missing-field", 1, "`premises` is missing; every proof needs one."},
            {"missing-field", 1, "`level` is missing; every proof needs one."},
            {"missing-field", 1, "`created` is missing; every proof needs one."}});
        CHECK(proofProblems(proofWith("note = \"x\"\n")) == std::vector<P>{
            {"unknown-field", 6, "`note` isn't a field of a proof. Is it misspelt? The fields are: id, proves, premises, level, created, verified."}});
        CHECK(proofProblems(proofWith("", "\"7RT2\"")) == std::vector<P>{
            {"wrong-type", 4, "`premises` should be a list of tags, not a string."}});
        CHECK(proofProblems("+++\nid = \"4KX7.a\"\nproves = \"4KX7\"\npremises = []\nlevel = \"done\"\ncreated = 2026-11-03\n+++\nBody\n") == std::vector<P>{
            {"bad-value", 5, "`level` can't be `done`; it must be one of `sketch`, `heuristic`, `complete`, `failed`."}});
    }

    SECTION("the id")
    {
        for (const char* id : {"4KX7.b", "4KX8.a", "4kx7.a"})
            CHECK(proofProblems("+++\nid = \"" + QByteArray(id) + "\"\nproves = \"4KX7\"\npremises = []\nlevel = \"sketch\"\ncreated = 2026-11-03\n+++\nBody\n") == std::vector<P>{
                {"proof-id-mismatch", 2, QStringLiteral("The proof's id `%1` doesn't match its file name `4KX7.a`, or doesn't start with its folder's tag.").arg(id)}});
        // A file filed under the wrong folder.
        const QString elsewhere = QStringLiteral("Proofs/9D3F/4KX7.a.tex");
        CHECK(summary(loom::readProof(proofWith(""), elsewhere).problems, elsewhere) == std::vector<P>{
            {"proof-id-mismatch", 2, "The proof's id `4KX7.a` doesn't match its file name `4KX7.a`, or doesn't start with its folder's tag."}});
    }

    SECTION("premises")
    {
        CHECK(proofProblems(proofWith("", "[\"7RT2\", \"4KX7\", \"7RT2\"]")) == std::vector<P>{
            {"premise-self", 4, "4KX7.a uses 4KX7, the statement it proves, as a premise."},
            {"premise-duplicate", 4, "7RT2 is listed twice in the premises of 4KX7.a."}});
        CHECK(proofProblems(proofWith("", "[]")).empty());
    }
}

TEST_CASE("Verbose proof problems", "[proofs]")
{
    const auto problems = [](const QByteArray& bytes) { return summary(loom::readVerboseProof(bytes, verbosePath).problems, verbosePath); };
    CHECK(problems("+++\nproof = \"4KX7.b\"\nfingerprint = \"3c8e1f0a92d4\"\ncreated = 2026-11-05\n+++\nSteps\n") == std::vector<P>{
        {"bad-value", 2, "`proof` can't be `4KX7.b`; it must be one of `4KX7.a`."}});
    CHECK(problems("+++\nproof = \"4KX7.a\"\nfingerprint = \"3C8E1F0A92D4\"\ncreated = 2026-11-05\n+++\nSteps\n") == std::vector<P>{
        {"wrong-type", 3, "`fingerprint` should be 12 characters from 0–9 and a–f, not `3C8E1F0A92D4`."}});
    CHECK(problems("+++\nproof = \"4KX7.a\"\n+++\nSteps\n") == std::vector<P>{
        {"missing-field", 1, "`fingerprint` is missing; every verbose proof needs one."},
        {"missing-field", 1, "`created` is missing; every verbose proof needs one."}});
    CHECK(problems("+++\nproof = \"4KX7.a\"\nfingerprint = \"3c8e1f0a92d4\"\ncreated = 2026-11-05\n+++\n") == std::vector<P>{
        {"body-empty", 6, "4KX7.a.verbose has no text below its front matter."}});
}

TEST_CASE("Empty values are missing values", "[statements][proofs]")
{
    // Specs/m1-core.md, "Front matter", A2: a required field, or an optional one that names
    // something, that is present but empty is `missing-field`, worded "`{field}` is empty".
    SECTION("statements")
    {
        CHECK(statementProblems("+++\nid = \"\"\nkind = \"\"\ntitle = \"\"\ncreated = 2026-11-03\n+++\nBody\n") == std::vector<P>{
            {"missing-field", 2, "`id` is empty; every statement needs one."},
            {"missing-field", 3, "`kind` is empty; every statement needs one."},
            {"missing-field", 4, "`title` is empty; every statement needs one."}});
        CHECK(statementProblems(statementWith("origin = \"\"\n")) == std::vector<P>{
            {"missing-field", 5, "`origin` is empty; give it a value or leave it out."}});
        CHECK(statementProblems(statementWith("superseded_by = \"\"\n")) == std::vector<P>{
            {"missing-field", 5, "`superseded_by` is empty; give it a value or leave it out."}});
        CHECK(statementProblems(statementWith("verified = \"\"\n")) == std::vector<P>{
            {"missing-field", 5, "`verified` is empty; give it a value or leave it out."}});
        CHECK(statementProblems("+++\nid = \"4KX7\"\nkind = \"theorem\"\ntitle = \"T\"\ncreated = \"\"\n+++\nBody\n") == std::vector<P>{
            {"missing-field", 5, "`created` is empty; every statement needs one."}});
    }

    SECTION("a refutation by nothing is an error")
    {
        // Before A2 this passed validation, and status ignored the refutation.
        CHECK(statementProblems(statementWith("refuted_by = \"\"\nrefuted_on = 2026-11-06\n")) == std::vector<P>{
            {"missing-field", 5, "`refuted_by` is empty; give it a value or leave it out."}});
        CHECK(statementProblems(statementWith("refuted_by = \"G5TZ.a\"\nrefuted_on = \"\"\n")) == std::vector<P>{
            {"missing-field", 6, "`refuted_on` is empty; give it a value or leave it out."}});
    }

    SECTION("proofs")
    {
        CHECK(proofProblems("+++\nid = \"\"\nproves = \"\"\npremises = []\nlevel = \"\"\ncreated = 2026-11-03\n+++\nBody\n") == std::vector<P>{
            {"missing-field", 2, "`id` is empty; every proof needs one."},
            {"missing-field", 3, "`proves` is empty; every proof needs one."},
            {"missing-field", 5, "`level` is empty; every proof needs one."}});
    }

    SECTION("verbose proofs")
    {
        CHECK(summary(loom::readVerboseProof("+++\nproof = \"\"\nfingerprint = \"\"\ncreated = 2026-11-05\n+++\nSteps\n", verbosePath).problems,
                      verbosePath) == std::vector<P>{
            {"missing-field", 2, "`proof` is empty; every verbose proof needs one."},
            {"missing-field", 3, "`fingerprint` is empty; every verbose proof needs one."}});
    }
}

TEST_CASE("A value of only whitespace is an empty value (A21)", "[statements][proofs]")
{
    // Specs/m1-core.md, "Front matter", A21: for required and naming fields, a value of only
    // whitespace says nothing, so it is empty.
    CHECK(statementProblems("+++\nid = \"4KX7\"\nkind = \"theorem\"\ntitle = \"   \"\ncreated = 2026-11-03\n+++\nBody\n")
          == std::vector<P>{{"missing-field", 4, "`title` is empty; every statement needs one."}});
    CHECK(statementProblems(statementWith("origin = \" \\t \"\n")) == std::vector<P>{
        {"missing-field", 5, "`origin` is empty; give it a value or leave it out."}});
    CHECK(proofProblems("+++\nid = \"4KX7.a\"\nproves = \"4KX7\"\npremises = []\nlevel = \"  \"\ncreated = 2026-11-03\n+++\nBody\n")
          == std::vector<P>{{"missing-field", 5, "`level` is empty; every proof needs one."}});
}

TEST_CASE("A value ending in a line break doesn't pass a rule", "[statements][proofs]")
{
    // Qt's `$` also matches before a final line break, so each rule must be anchored at the very end.
    CHECK(statementProblems(statementWith("origin = \"new\\n\"\n")) == std::vector<P>{
        {"bad-value", 5, "`origin` can't be `new\n`; it must be one of `new`, `spec:<spec-id>`, `import:<path>`."}});
    CHECK(summary(loom::readVerboseProof("+++\nproof = \"4KX7.a\"\nfingerprint = \"3c8e1f0a92d4\\n\"\ncreated = 2026-11-05\n+++\nSteps\n",
                                         verbosePath).problems, verbosePath) == std::vector<P>{
        {"wrong-type", 3, "`fingerprint` should be 12 characters from 0–9 and a–f, not `3c8e1f0a92d4\n`."}});
}

TEST_CASE("A date Qt can't hold is reported, and the file isn't rewritten", "[statements]")
{
    // toml++ accepts the year 0, which QDate has no room for. Read as it was, the date was
    // dropped without a word, and the next rewrite would have lost it.
    const loom::StatementReading reading = loom::readStatement(
        "+++\nid = \"4KX7\"\nkind = \"theorem\"\ntitle = \"T\"\ncreated = 0000-01-01\n+++\nBody\n", statementPath);
    CHECK(summary(reading.problems, statementPath) == std::vector<P>{
        {"bad-value", 5, "`created` can't be `0000-01-01`; it must be one of the dates from 0001-01-01 on."}});
    CHECK(reading.parsed);
    CHECK_FALSE(reading.lossless);
}

TEST_CASE("Reference fields of the wrong type are reported, and the file isn't rewritten", "[statements]")
{
    const std::pair<QByteArray, P> cases[] = {
        {"superseded_by = 7\n", {"wrong-type", 5, "`superseded_by` should be a string, not a whole number."}},
        {"refuted_by = [\"G5TZ.a\"]\nrefuted_on = 2026-11-06\n", {"wrong-type", 5, "`refuted_by` should be a string, not a list."}},
        {"refuted_by = \"G5TZ.a\"\nrefuted_on = \"2026-11-06\"\n", {"wrong-type", 6, "`refuted_on` should be a date, not a string."}},
    };
    for (const auto& [extra, problem] : cases) {
        INFO(extra.toStdString());
        const loom::StatementReading reading = loom::readStatement(statementWith(extra), statementPath);
        const std::vector<P> problems = summary(reading.problems, statementPath);
        REQUIRE_FALSE(problems.empty());
        CHECK(problems.front() == problem);
        CHECK_FALSE(reading.lossless);
    }
}

TEST_CASE("A verbose proof's unknown field and broken TOML", "[proofs]")
{
    const loom::VerboseReading unknown = loom::readVerboseProof(
        "+++\nproof = \"4KX7.a\"\nfingerprint = \"3c8e1f0a92d4\"\ncolour = \"red\"\ncreated = 2026-11-05\n+++\nSteps\n", verbosePath);
    const std::vector<P> problems = summary(unknown.problems, verbosePath);
    REQUIRE(problems.size() == 1);
    CHECK(std::get<0>(problems[0]) == "unknown-field");
    CHECK(std::get<1>(problems[0]) == 4);
    CHECK_FALSE(unknown.lossless);

    const loom::VerboseReading broken = loom::readVerboseProof("+++\nproof = \n+++\nSteps\n", verbosePath);
    CHECK_FALSE(broken.parsed);
    const std::vector<P> syntax = summary(broken.problems, verbosePath);
    REQUIRE(syntax.size() == 1);
    CHECK(std::get<0>(syntax[0]) == "toml-syntax");
    CHECK(std::get<1>(syntax[0]) == 2);
}

TEST_CASE("A statement's body keeps a lone CR, and front matter never closed is reported", "[statements]")
{
    const loom::StatementReading reading = loom::readStatement(statementWith("", "A\rB\n"), statementPath);
    CHECK(reading.problems.empty());
    CHECK(reading.document.body == "A\rB\n");
    CHECK(loom::formatStatement(reading.document).endsWith("+++\nA\rB\n"));

    const loom::StatementReading unclosed = loom::readStatement("+++\nid = \"4KX7\"\nkind = \"theorem\"\n", statementPath);
    CHECK_FALSE(unclosed.parsed);
    CHECK(summary(unclosed.problems, statementPath) == std::vector<P>{
        {"front-matter-unclosed", 1, "The front matter starting on line 1 is never closed by a `+++` line."}});
}
