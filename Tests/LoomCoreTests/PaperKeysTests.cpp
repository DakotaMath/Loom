// The key rule (Specs/m1-core.md, "Papers", "The key rule"): every row of the spec's key table,
// shared keys and their letters, and renaming.

#include "LoomCore/PaperKeys.h"

#include "PaperFixture.h"
#include "TestSupport.h"

#include <catch2/catch_test_macros.hpp>

using loomtest::paperBy;

namespace {

QString keyOf(const loom::Paper& paper)
{
    return loom::baseKey(paper).key;
}

} // namespace

TEST_CASE("Surnames in ASCII CamelCase", "[keys]")
{
    CHECK(loom::camelSurname("Smith") == "Smith");
    CHECK(loom::camelSurname("van der Berg") == "VanDerBerg");
    CHECK(loom::camelSurname("de Giorgi") == "DeGiorgi");
    CHECK(loom::camelSurname("O'Neil") == "ONeil");
    CHECK(loom::camelSurname(QString::fromUtf8("O’Neil")) == "ONeil");
    CHECK(loom::camelSurname("McKean") == "McKean");
    CHECK(loom::camelSurname("Smith-Jones") == "SmithJones");
    CHECK(loom::camelSurname(QString::fromUtf8("Erdős")) == "Erdos");
    CHECK(loom::camelSurname(QString::fromUtf8("Rényi")) == "Renyi");
    CHECK(loom::camelSurname(QString::fromUtf8("Müller")) == "Muller");
    CHECK(loom::camelSurname(QString::fromUtf8("Łojasiewicz")) == "Lojasiewicz");
    CHECK(loom::camelSurname(QString::fromUtf8("Weiß")) == "Weiss");
    CHECK(loom::camelSurname(QString::fromUtf8("Ægidius")) == "Aegidius");
    CHECK(loom::camelSurname(QString::fromUtf8("Søndergaard")) == "Sondergaard");
    CHECK(loom::camelSurname(QString::fromUtf8("Œstreicher")) == "Oestreicher");
    CHECK(loom::camelSurname(QString::fromUtf8("Đoković")) == "Dokovic");
    CHECK(loom::camelSurname(QString::fromUtf8("Þórsson")) == "Thorsson");
    CHECK(loom::camelSurname(QString::fromUtf8("Yıldırım")) == "Yildirim");
    CHECK(loom::camelSurname(QString::fromUtf8("张")).isEmpty());
}

TEST_CASE("Every row of the key table", "[keys]")
{
    CHECK(keyOf(paperBy({"Smith", "Jones"}, 2024)) == "SmithJones24");
    CHECK(keyOf(paperBy({"Smith"}, std::nullopt, QDate(2023, 1, 15))) == "Smith23");
    CHECK(keyOf(paperBy({"Smith", "Jones", "Brown"}, 2020)) == "SmithJonesBrown20");
    CHECK(keyOf(paperBy({"Smith", "Jones", "Brown", "Garcia"}, 2020)) == "SmithJoBrGa20");
    CHECK(keyOf(paperBy({"Smith", "van der Berg", "Brown", "Garcia"}, 2020)) == "SmithVaBrGa20");
    CHECK(keyOf(paperBy({QString::fromUtf8("Erdős"), QString::fromUtf8("Rényi")}, 1960)) == "ErdosRenyi60");
    CHECK(keyOf(paperBy({"van der Berg", QString::fromUtf8("Müller")}, 2019)) == "VanDerBergMuller19");
    CHECK(keyOf(paperBy({"O'Neil"}, 2021)) == "ONeil21");
    CHECK(keyOf(paperBy({"de Giorgi"}, 1957)) == "DeGiorgi57");
    CHECK(keyOf(paperBy({QString::fromUtf8("Łojasiewicz")}, 1963)) == "Lojasiewicz63");
    CHECK(keyOf(paperBy({QString::fromUtf8("Weiß")}, 2015)) == "Weiss15");
    CHECK(keyOf(paperBy({"McKean"}, 1969)) == "McKean69");
    CHECK(keyOf(paperBy({"Smith-Jones"}, 2022)) == "SmithJones22");
    // A preprint is keyed by its posting year until it is published.
    CHECK(keyOf(paperBy({"Smith", "Jones"}, std::nullopt, QDate(2023, 5, 1))) == "SmithJones23");
    CHECK(keyOf(paperBy({"Smith", "Jones"}, 2024, QDate(2023, 5, 1))) == "SmithJones24");
}

TEST_CASE("Four or more authors: one letter of a one-letter surname", "[keys]")
{
    CHECK(keyOf(paperBy({"Smith", "Ng", "O", "Li"}, 2001)) == "SmithNgOLi01");
}

TEST_CASE("The key's year: published, else the lowest arXiv version, else a date", "[keys]")
{
    loom::Paper paper = paperBy({"Smith"}, std::nullopt, QDate(2023, 1, 15));
    loom::PaperVersion v3;
    v3.id = "arxiv-v3";
    v3.kind = "arxiv";
    v3.fields.push_back({"arxiv_version", loomtest::field(qint64(3))});
    v3.fields.push_back({"posted", loomtest::field(QDate(2021, 6, 1))});
    paper.versions.insert(paper.versions.begin(), v3);
    CHECK(loom::keyYear(paper) == 2023); // v1 is the lowest version, though v3 comes first.

    loom::Paper talk = paperBy({"Smith"}, std::nullopt);
    talk.work.type = loomtest::field(QStringLiteral("talk"));
    loom::PaperVersion published;
    published.id = "published";
    published.kind = "published";
    published.fields.push_back({"date", loomtest::field(QDate(2009, 3, 2))});
    talk.versions.push_back(published);
    CHECK(keyOf(talk) == "Smith09");
}

TEST_CASE("A book with editors and no authors is keyed by its editors", "[keys]")
{
    loom::Paper book = paperBy({}, 2010);
    book.work.authors.reset();
    book.work.editors = loomtest::field(std::vector<loom::PersonName>{{"Brown", "C"}, {"Garcia", "D"}});
    CHECK(keyOf(book) == "BrownGarcia10");
}

TEST_CASE("No key without Latin letters or a year", "[keys]")
{
    const loom::KeyBase chinese = loom::baseKey(paperBy({QString::fromUtf8("张")}, 2020));
    CHECK(chinese.key.isEmpty());
    CHECK(chinese.problem == QString::fromUtf8("Can't make a key from the surname `张`: write it in Latin letters, as the paper prints it."));

    const loom::KeyBase noYear = loom::baseKey(paperBy({"Smith"}, std::nullopt));
    CHECK(noYear.key.isEmpty());
    CHECK(noYear.problem == "Can't make a key for this paper: it has no year. Add a published year, an arXiv posting date, or a date.");
}

TEST_CASE("Shared keys: both get letters in posting order the first time", "[keys]")
{
    const loom::Paper earlier = paperBy({"Smith", "Jones"}, 2024, QDate(2023, 5, 1));
    const loom::Paper later = paperBy({"Smith", "Jones"}, 2024, QDate(2024, 1, 10));

    // The later paper arrives second: the existing one, posted earlier, gets `a`.
    loom::KeyAssignment second = loom::assignKey(later, {{"SmithJones24", {}, &earlier}});
    CHECK(second.key == "SmithJones24b");
    REQUIRE(second.renames.size() == 1);
    CHECK(second.renames[0] == std::pair<QString, QString>("SmithJones24", "SmithJones24a"));

    // The earlier paper arrives second: it takes `a`, and the existing one becomes `b`.
    loom::KeyAssignment first = loom::assignKey(earlier, {{"SmithJones24", {}, &later}});
    CHECK(first.key == "SmithJones24a");
    REQUIRE(first.renames.size() == 1);
    CHECK(first.renames[0] == std::pair<QString, QString>("SmithJones24", "SmithJones24b"));

    // Smith-Jones and Smith & Jones share a base too.
    CHECK(loom::assignKey(paperBy({"Smith-Jones"}, 2022), {{"SmithJones22", {}, nullptr}}).key == "SmithJones22b");
}

TEST_CASE("Shared keys: posting order goes by title when the dates agree", "[keys]")
{
    const loom::Paper alpha = paperBy({"Smith"}, 2024, std::nullopt, "Alpha");
    const loom::Paper beta = paperBy({"Smith"}, 2024, std::nullopt, "Beta");
    CHECK(loom::postedBefore(alpha, beta));
    CHECK_FALSE(loom::postedBefore(beta, alpha));
    CHECK(loom::assignKey(alpha, {{"Smith24", {}, &beta}}).key == "Smith24a");
}

TEST_CASE("Shared keys: a third paper takes the next letter, and lettered keys never change", "[keys]")
{
    // The third paper is the earliest of all, but the existing letters stay.
    const loom::Paper third = paperBy({"Smith", "Jones"}, 2024, QDate(2022, 1, 1));
    const loom::KeyAssignment assigned =
        loom::assignKey(third, {{"SmithJones24a", {}, nullptr}, {"SmithJones24b", {}, nullptr}});
    CHECK(assigned.key == "SmithJones24c");
    CHECK(assigned.renames.empty());

    // An old key counts as used: the next letter is after the highest anyone has had.
    CHECK(loom::assignKey(third, {{"SmithJones24a", {"SmithJones24d"}, nullptr}}).key == "SmithJones24e");

    // An unlettered paper beside lettered ones is renamed to the next letter first.
    const loom::KeyAssignment fourth =
        loom::assignKey(third, {{"SmithJones24", {}, nullptr}, {"SmithJones24a", {}, nullptr}});
    CHECK(fourth.key == "SmithJones24c");
    REQUIRE(fourth.renames.size() == 1);
    CHECK(fourth.renames[0] == std::pair<QString, QString>("SmithJones24", "SmithJones24b"));
}

TEST_CASE("Shared keys: no key once every letter is taken", "[keys]")
{
    const loom::Paper paper = paperBy({"Smith"}, 2024);
    const loom::KeyAssignment full = loom::assignKey(paper, {{"Smith24z", {}, nullptr}});
    CHECK(full.key.isEmpty());
    CHECK(full.renames.empty());
    CHECK(full.problem == "Can't make a key for this paper: every letter after `Smith24` is taken.");
}

TEST_CASE("A key that is only another paper's old key is not taken", "[keys]")
{
    const loom::Paper paper = paperBy({"Smith", "Jones"}, 2023);
    const loom::KeyAssignment assigned = loom::assignKey(paper, {{"SmithJones24", {"SmithJones23"}, nullptr}});
    CHECK(assigned.key == "SmithJones23a");
    CHECK(assigned.renames.empty());
}

TEST_CASE("Which keys fit a base", "[keys]")
{
    CHECK(loom::keyFitsBase("SmithJones24", "SmithJones24"));
    CHECK(loom::keyFitsBase("SmithJones24b", "SmithJones24"));
    CHECK_FALSE(loom::keyFitsBase("SmithJones24B", "SmithJones24"));
    CHECK_FALSE(loom::keyFitsBase("SmithJones24ab", "SmithJones24"));
    CHECK_FALSE(loom::keyFitsBase("SmithJones23", "SmithJones24"));
}

TEST_CASE("Keys that differ only in case are the same key (A3)", "[keys]")
{
    // A key is a folder name, and Windows folder names ignore case.
    const loom::Paper earlier = paperBy({"MacDonald"}, 2020, QDate(2019, 5, 1));
    const loom::Paper later = paperBy({"Macdonald"}, 2020, QDate(2020, 2, 1));
    const loom::KeyAssignment assigned = loom::assignKey(later, {{"MacDonald20", {}, &earlier}});
    CHECK(assigned.key == "Macdonald20b");
    REQUIRE(assigned.renames.size() == 1);
    // The paper renamed keeps its own spelling of the base.
    CHECK(assigned.renames[0] == std::pair<QString, QString>("MacDonald20", "MacDonald20a"));

    // Lettered keys and old keys count whatever their case.
    CHECK(loom::assignKey(later, {{"MacDonald20a", {}, nullptr}}).key == "Macdonald20b");
    CHECK(loom::assignKey(later, {{"Smith20", {"MACDONALD20C"}, nullptr}}).key == "Macdonald20d");
    // A key only another paper's old key is, ignoring case, is not taken.
    const loom::KeyAssignment old = loom::assignKey(later, {{"Smith21", {"macdonald20"}, nullptr}});
    CHECK(old.key == "Macdonald20a");
    CHECK(old.renames.empty());
}

TEST_CASE("An unlettered key beside a lettered one of its base doesn't follow the rule (step 4)", "[keys]")
{
    // Once a base is shared, every paper with it carries a letter.
    CHECK(loom::keyFollowsRule("SmithJones24", "SmithJones24", {}));
    CHECK(loom::keyFollowsRule("SmithJones24", "SmithJones24", {{"Smith23", {}, nullptr}}));
    CHECK_FALSE(loom::keyFollowsRule("SmithJones24", "SmithJones24", {{"SmithJones24a", {}, nullptr}}));
    CHECK_FALSE(loom::keyFollowsRule("SmithJones24", "SmithJones24", {{"Smith23", {"smithjones24c"}, nullptr}}));
    CHECK(loom::keyFollowsRule("SmithJones24b", "SmithJones24", {{"SmithJones24a", {}, nullptr}}));
    CHECK_FALSE(loom::keyFollowsRule("SmithJones23", "SmithJones24", {}));

    // The key it should have is the next letter, as assignKey() gives it.
    const loom::Paper paper = paperBy({"Smith", "Jones"}, 2024);
    CHECK(loom::assignKey(paper, {{"SmithJones24a", {}, nullptr}}).key == "SmithJones24b");
}
