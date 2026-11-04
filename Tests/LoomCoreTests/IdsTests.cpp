// Tags, proof IDs and aliases (Specs/m1-core.md, "IDs").

#include "LoomCore/Ids.h"

#include "TestSupport.h"

#include <QSet>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Tags are four characters of Crockford's base 32", "[ids]")
{
    CHECK(QByteArray(loom::tagAlphabet) == "0123456789ABCDEFGHJKMNPQRSTVWXYZ");
    CHECK(loom::isTag("4KX7"));
    CHECK(loom::isTag("2024")); // Never generated, but a tag.
    CHECK_FALSE(loom::isTag("4KX"));
    CHECK_FALSE(loom::isTag("4KX7A"));
    CHECK_FALSE(loom::isTag("4kx7"));
    for (const char* bad : {"I000", "L000", "O000", "U000", "4K-7"})
        CHECK_FALSE(loom::isTag(QString::fromLatin1(bad)));
}

TEST_CASE("Tags typed on the command line are read as Crockford intends", "[ids]")
{
    CHECK(loom::normaliseTag("4kx7") == "4KX7");
    CHECK(loom::normaliseTag("1O2l") == "1021");
    CHECK(loom::normaliseTag("iIlL") == "1111");
    CHECK(loom::normaliseTag("oOo0") == "0000");
    CHECK_FALSE(loom::normaliseTag("u000"));
    CHECK_FALSE(loom::normaliseTag("4kx"));
    CHECK_FALSE(loom::normaliseTag(QString::fromUtf8("4kxé")));
}

TEST_CASE("Generated tags", "[ids]")
{
    QRandomGenerator random(20261103);
    const auto never = [](const QString&) { return false; };

    SECTION("use the whole alphabet, four characters, never all digits")
    {
        QSet<QChar> seen;
        for (int i = 0; i < 2000; ++i) {
            const std::optional<QString> tag = loom::generateTag(never, random);
            REQUIRE(tag);
            CHECK(loom::isTag(*tag));
            bool allDigits = true;
            for (const QChar c : *tag) {
                seen.insert(c);
                allDigits = allDigits && c.isDigit();
            }
            CHECK_FALSE(allDigits);
        }
        CHECK(seen.size() == 32);
    }

    SECTION("never collide with a tag or alias in use")
    {
        QRandomGenerator copy = random;
        const QString first = *loom::generateTag(never, copy);
        const std::optional<QString> tag = loom::generateTag([&](const QString& t) { return t == first; }, random);
        REQUIRE(tag);
        CHECK(*tag != first);
    }

    SECTION("give up after 100 draws")
    {
        int asked = 0;
        CHECK_FALSE(loom::generateTag([&](const QString&) { ++asked; return true; }, random));
        CHECK(asked <= 100);
        CHECK(asked > 90); // Only all-digit draws (about 1 in 105) skip the question.
    }
}

TEST_CASE("Proof letters count in bijective base 26", "[ids]")
{
    const std::pair<qint64, const char*> cases[] = {
        {1, "a"}, {2, "b"}, {26, "z"}, {27, "aa"}, {28, "ab"}, {52, "az"}, {53, "ba"}, {702, "zz"}, {703, "aaa"}};
    for (const auto& [number, letters] : cases) {
        CHECK(loom::proofLetters(number) == QLatin1StringView(letters));
        CHECK(loom::proofNumber(QString::fromLatin1(letters)) == number);
    }
    CHECK(loom::proofNumber("") == 0);
    CHECK(loom::proofNumber("A") == 0);
    CHECK(loom::proofNumber("a1") == 0);
}

TEST_CASE("Proof IDs", "[ids]")
{
    const std::optional<loom::ProofIdParts> parts = loom::splitProofId("4KX7.ab");
    REQUIRE(parts);
    CHECK(parts->tag == "4KX7");
    CHECK(parts->letters == "ab");
    for (const char* bad : {"4KX7", "4KX7.", "4KX7.A", "4kx7.a", "4KX7.a.b", "4KX.a", ".a"})
        CHECK_FALSE(loom::splitProofId(QString::fromLatin1(bad)));

    CHECK(loom::normaliseProofId("4kx7.A") == "4KX7.a");
    CHECK(loom::normaliseProofId("4KXl.ab") == "4KX1.ab");
    CHECK_FALSE(loom::normaliseProofId("4KX7"));
    CHECK_FALSE(loom::normaliseProofId("4KX7.a1"));
}

TEST_CASE("Aliases", "[ids]")
{
    CHECK(loom::aliasProblem("thm:energy-decay").isEmpty());
    CHECK(loom::aliasProblem("lem:energy-decay").isEmpty());
    CHECK(loom::aliasProblem(QString::fromUtf8("Satz-Ü")).isEmpty());
    CHECK(loom::aliasProblem(QString(100, u'a')).isEmpty());
    CHECK(loom::aliasProblem("") == "it is empty");
    CHECK(loom::aliasProblem("two words") == "it contains whitespace or one of {}[],\"");
    for (const char* bad : {"a{b", "a}b", "a[b", "a]b", "a,b", "a\"b", "a\tb"})
        CHECK(loom::aliasProblem(QString::fromLatin1(bad)) == "it contains whitespace or one of {}[],\"");
    CHECK(loom::aliasProblem("4KX7") == "it is shaped like a tag");
    CHECK(loom::aliasProblem("abcd") == "it is shaped like a tag");
    CHECK(loom::aliasProblem("lOil") == "it is shaped like a tag");
    CHECK(loom::aliasProblem(QString(101, u'a')) == "it's over 100 characters");
}

TEST_CASE("A proof letter run too long to count is no number, not an overflow", "[ids]")
{
    // 26^13 still fits in 64 bits; a fourteenth letter doesn't.
    CHECK(loom::proofNumber(QString(13, u'a')) > 0);
    CHECK(loom::proofNumber(QString(14, u'z')) == 0);
    CHECK(loom::proofNumber(QString(40, u'a')) == 0);
}

TEST_CASE("An alias's length counts characters, not UTF-16 units", "[ids]")
{
    // U+1D53C is two UTF-16 units; 100 of them are still 100 characters.
    const QString doubleStruck = QString::fromUtf8("\xF0\x9D\x94\xBC");
    CHECK(loom::aliasProblem(doubleStruck.repeated(100)).isEmpty());
    CHECK(loom::aliasProblem(doubleStruck.repeated(101)) == "it's over 100 characters");
}
