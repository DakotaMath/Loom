// Finding front matter and keeping the body byte for byte (Specs/m1-core.md, "Front matter").

#include "LoomCore/FrontMatter.h"

#include "TestSupport.h"

#include <catch2/catch_test_macros.hpp>

namespace {

const QString path = QStringLiteral("Statements/4KX7.tex");

} // namespace

TEST_CASE("Front matter is found between the first two +++ lines", "[front-matter]")
{
    const loom::FrontMatterSplit split = loom::splitFrontMatter("+++\nid = \"4KX7\"\n+++\nThe body.\n", path);
    CHECK(split.found);
    CHECK(split.problems.empty());
    CHECK(split.toml == "id = \"4KX7\"\n");
    CHECK(split.body == "The body.\n");
    CHECK(split.bodyLine == 4);
}

TEST_CASE("A +++ line in the body is body text", "[front-matter]")
{
    const QByteArray body = "\\begin{verbatim}\n+++\nnot front matter\n+++\n\\end{verbatim}\n";
    const loom::FrontMatterSplit split = loom::splitFrontMatter("+++\nid = \"4KX7\"\n+++\n" + body, path);
    CHECK(split.found);
    CHECK(split.body == body);
}

TEST_CASE("CRLF counts as LF, in the front matter and the body", "[front-matter]")
{
    const loom::FrontMatterSplit split = loom::splitFrontMatter("+++\r\nid = \"4KX7\"\r\n+++\r\nLine one.\r\nLine two.\r\n", path);
    CHECK(split.found);
    CHECK(split.toml == "id = \"4KX7\"\n");
    CHECK(split.body == "Line one.\nLine two.\n");
}

TEST_CASE("A lone CR and a missing final newline are kept", "[front-matter]")
{
    const loom::FrontMatterSplit split = loom::splitFrontMatter("+++\nid = \"4KX7\"\n+++\nA\rB", path);
    CHECK(split.body == "A\rB");
}

TEST_CASE("Unicode in the body is kept byte for byte", "[front-matter]")
{
    const QByteArray body = QString::fromUtf8("Für alle $u_0 ∈ L^2(ℝ^n)$: 𝔼|u|² ≤ C. 能量\n").toUtf8();
    CHECK(loom::splitFrontMatter("+++\nid = \"4KX7\"\n+++\n" + body, path).body == body);
}

TEST_CASE("A byte-order mark is skipped with a warning", "[front-matter]")
{
    const loom::FrontMatterSplit split = loom::splitFrontMatter("\xEF\xBB\xBF+++\nid = \"4KX7\"\n+++\nBody\n", path);
    CHECK(split.found);
    REQUIRE(split.problems.size() == 1);
    CHECK(split.problems[0].code == "bom");
    CHECK(split.problems[0].severity == loom::Severity::Warning);
    CHECK(split.problems[0].message == "This file starts with a byte-order mark. Loom will drop it the next time it saves the file.");
    CHECK(loom::joinFrontMatter(split.toml, split.body) == "+++\nid = \"4KX7\"\n+++\nBody\n");
}

TEST_CASE("The first line must be exactly +++", "[front-matter]")
{
    for (const QByteArray text : {QByteArray(""), QByteArray("id = \"4KX7\"\n"), QByteArray("++++\n+++\n"),
                                  QByteArray(" +++\n+++\n"), QByteArray("+++ \n+++\n")}) {
        const loom::FrontMatterSplit split = loom::splitFrontMatter(text, path);
        CHECK_FALSE(split.found);
        REQUIRE(split.problems.size() == 1);
        CHECK(split.problems[0].code == "front-matter-missing");
        CHECK(split.problems[0].line == 1);
        CHECK(split.problems[0].message == "The first line should be `+++`: Loom files start with front matter between two `+++` lines.");
    }
}

TEST_CASE("Front matter that is never closed", "[front-matter]")
{
    for (const QByteArray text : {QByteArray("+++"), QByteArray("+++\n"), QByteArray("+++\nid = \"4KX7\"\n++ +\nBody\n")}) {
        const loom::FrontMatterSplit split = loom::splitFrontMatter(text, path);
        CHECK_FALSE(split.found);
        REQUIRE(split.problems.size() == 1);
        CHECK(split.problems[0].code == "front-matter-unclosed");
        CHECK(split.problems[0].message == "The front matter starting on line 1 is never closed by a `+++` line.");
    }
}

TEST_CASE("A closing +++ at the very end leaves an empty body", "[front-matter]")
{
    const loom::FrontMatterSplit split = loom::splitFrontMatter("+++\nid = \"4KX7\"\n+++", path);
    CHECK(split.found);
    CHECK(split.body.isEmpty());
    CHECK(split.bodyLine == 4);
}

TEST_CASE("A file that isn't UTF-8 names the first bad byte", "[front-matter]")
{
    const loom::FrontMatterSplit split = loom::splitFrontMatter("+++\nid = \"4KX7\"\n+++\nCaf\xE9\n", path);
    CHECK_FALSE(split.found);
    REQUIRE(split.problems.size() == 1);
    CHECK(split.problems[0].code == "not-utf8");
    CHECK(split.problems[0].line == 4);
    CHECK(split.problems[0].message == "This file isn't valid UTF-8 (the first bad byte is at offset 23). Save it as UTF-8.");
}

TEST_CASE("UTF-8 validation", "[front-matter]")
{
    CHECK_FALSE(loom::firstInvalidUtf8("plain ASCII"));
    CHECK_FALSE(loom::firstInvalidUtf8("\xC3\xA9 \xE2\x88\x88 \xF0\x9D\x94\xBC")); // é ∈ 𝔼
    CHECK(loom::firstInvalidUtf8("ab\x80") == 2);         // a continuation byte alone
    CHECK(loom::firstInvalidUtf8("\xC0\x80") == 0);       // overlong
    CHECK(loom::firstInvalidUtf8("\xE0\x80\x80") == 0);   // overlong
    CHECK(loom::firstInvalidUtf8("\xED\xA0\x80") == 0);   // a surrogate
    CHECK(loom::firstInvalidUtf8("\xF4\x90\x80\x80") == 0); // above U+10FFFF
    CHECK(loom::firstInvalidUtf8("\xF5\x80\x80\x80") == 0);
    CHECK(loom::firstInvalidUtf8("x\xE2\x82") == 3);      // cut off at the end
    CHECK(loom::firstInvalidUtf8("\xE2\x28\xA1") == 0);   // a bad second byte
    CHECK(loom::firstInvalidUtf8("\xF0\x9D\x28\xBC") == 2); // a bad third byte
}

TEST_CASE("Blank bodies", "[front-matter]")
{
    CHECK(loom::isBlankBody(""));
    CHECK(loom::isBlankBody(" \n\t\n"));
    CHECK_FALSE(loom::isBlankBody("x"));
}
