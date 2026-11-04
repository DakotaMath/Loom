// The fingerprint of a proof (Specs/m1-core.md, "Checks", "The fingerprint").

#include "LoomCore/Fingerprint.h"

#include "TestSupport.h"

#include <catch2/catch_test_macros.hpp>

namespace {

/// The spec's shape of example: a theorem proved from two premises, with whitespace at line ends,
/// a CRLF and blank lines at the end that the fingerprint ignores.
loom::FingerprintInput example()
{
    loom::FingerprintInput input;
    input.statementTag = "4KX7";
    input.statementBody = "Let $u$ be a weak solution with data $u_0$.  \r\nThen $|u(t)| = O(t^{-1/2})$.\n\n";
    input.premises = {{"9D3F", QByteArray("Energy estimate.\t\n")}, {"7RT2", QByteArray("Gr\xc3\xb6nwall.\n")}};
    input.proofBody = "By 7RT2 and 9D3F, with $k=0$.\n";
    return input;
}

} // namespace

TEST_CASE("The fingerprint of a fixed example: its bytes and its hash", "[fingerprint]")
{
    // Computed independently (Python's hashlib) from the spec's description.
    CHECK(loom::fingerprintBytes(example()) ==
          "loom-fingerprint-v1\n"
          "statement 4KX7\n72\nLet $u$ be a weak solution with data $u_0$.\nThen $|u(t)| = O(t^{-1/2})$.\n"
          "premise 7RT2\n10\nGr\xc3\xb6nwall.\n"
          "premise 9D3F\n16\nEnergy estimate.\n"
          "proof\n29\nBy 7RT2 and 9D3F, with $k=0$.\n");
    CHECK(loom::fingerprint(example()) == "d84640b94eb8");
}

TEST_CASE("A paper stub premise is written with no text", "[fingerprint]")
{
    loom::FingerprintInput input = example();
    input.premises[1].body.reset();
    CHECK(loom::fingerprintBytes(input).contains("premise 7RT2\n0\n\npremise 9D3F\n"));
    CHECK(loom::fingerprint(input) == "3e67474a0e36");
}

TEST_CASE("Every input the fingerprint covers changes it", "[fingerprint]")
{
    const QString original = loom::fingerprint(example());
    loom::FingerprintInput input = example();

    SECTION("the statement's body") { input.statementBody.replace("weak", "strong"); }
    SECTION("the statement's tag") { input.statementTag = "4KX8"; }
    SECTION("a premise's body") { input.premises[0].body = QByteArray("Energy estimates.\n"); }
    SECTION("a premise's tag") { input.premises[0].tag = "9D3G"; }
    SECTION("a premise added") { input.premises.push_back({"5HN8", QByteArray("Lemma.\n")}); }
    SECTION("a premise removed") { input.premises.pop_back(); }
    SECTION("the proof's body") { input.proofBody = "By 7RT2 and 9D3F, with $k=1$.\n"; }
    SECTION("whitespace inside a line") { input.proofBody = "By 7RT2 and 9D3F,  with $k=0$.\n"; }
    SECTION("a blank line between lines") { input.proofBody = "By 7RT2 and 9D3F,\n\nwith $k=0$.\n"; }
    SECTION("a premise becoming a paper stub") { input.premises[0].body.reset(); }

    CHECK(loom::fingerprint(input) != original);
}

TEST_CASE("Line-end whitespace, CRLF, final newlines and premise order don't change the fingerprint", "[fingerprint]")
{
    const QString original = loom::fingerprint(example());
    loom::FingerprintInput input = example();

    SECTION("trailing spaces and tabs") { input.proofBody = "By 7RT2 and 9D3F, with $k=0$. \t \n"; }
    SECTION("CRLF line endings")
    {
        input.premises[0].body->replace("\n", "\r\n");
        input.proofBody.replace("\n", "\r\n");
    }
    SECTION("no final newline") { input.proofBody.chop(1); }
    SECTION("more final newlines") { input.premises[0].body = QByteArray("Energy estimate.\n\n\n"); }
    SECTION("premises in the other order") { std::swap(input.premises[0], input.premises[1]); }

    CHECK(loom::fingerprint(input) == original);
}

TEST_CASE("Normalising a body for the fingerprint", "[fingerprint]")
{
    CHECK(loom::normaliseForFingerprint("a  \r\n\tb\t\n  \n\n") == "a\n\tb");
    CHECK(loom::normaliseForFingerprint("") == "");
    CHECK(loom::normaliseForFingerprint("\n\n") == "");
    CHECK(loom::normaliseForFingerprint("a\rb \r") == "a\rb \r"); // A lone CR is text, as everywhere in Loom.
}

TEST_CASE("The form of a fingerprint", "[fingerprint]")
{
    CHECK(loom::isFingerprint("3c8e1f0a92d4"));
    CHECK_FALSE(loom::isFingerprint("3C8E1F0A92D4"));
    CHECK_FALSE(loom::isFingerprint("3c8e1f0a92d"));
    CHECK_FALSE(loom::isFingerprint("3c8e1f0a92d4f"));
    CHECK_FALSE(loom::isFingerprint("3c8e1f0a92dg"));
}
