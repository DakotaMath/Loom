// Paper entries and stubs (Specs/m1-core.md, "Papers"): reading, writing in canonical form, and
// the problems each file shows on its own, with their lines.

// Shows the problem tuples in failure messages.
#define CATCH_CONFIG_ENABLE_TUPLE_STRINGMAKER

#include "LoomCore/Papers.h"

#include "PaperFixture.h"
#include "TestSupport.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <tuple>
#include <vector>

using loomtest::smithJones24;
using loomtest::stubToml;

namespace {

using P = std::tuple<QString, int, QString>; // code, line, message

const QString paperPath = QStringLiteral("Papers/SmithJones24/paper.toml");

/// Problems as (code, line, message), which a failing CHECK prints whole; the file is the same
/// for every problem of one reading, so it's left out.
std::vector<P> problemsOf(const std::vector<loom::Problem>& problems)
{
    std::vector<P> out;
    for (const loom::Problem& problem : problems)
        out.emplace_back(problem.code, problem.line, problem.message);
    return out;
}

/// A field with a source and a verified date, as TOML.
QByteArray f(const QByteArray& value)
{
    return "{ value = " + value + ", " + QByteArray(loomtest::doi) + " }";
}

/// The minimal entry of a type: `[work]` and one published version holding exactly the fields
/// `fields` (name = TOML value).
QByteArray entryOf(const QByteArray& type, const std::vector<std::pair<QByteArray, QByteArray>>& fields)
{
    QByteArray text = "key = \"Smith24\"\ncite = \"published\"\nrelevance = 2\ncreated = 2026-11-03\n\n[work]\n"
                      "type = " + f("\"" + type + "\"") + "\ntitle = " + f("\"T\"") + "\nauthors = "
                    + f("[{ last = \"Smith\", first = \"\" }]") + "\n\n[[versions]]\nid = \"published\"\nkind = \"published\"\n";
    for (const auto& [name, value] : fields)
        text += name + " = " + f(value) + "\n";
    return text;
}

} // namespace

TEST_CASE("The design's example entry reads without problems and writes back the same", "[papers]")
{
    const loom::PaperReading reading = loom::readPaper(smithJones24(), paperPath);
    CHECK(problemsOf(reading.problems).empty());
    REQUIRE(reading.parsed);
    CHECK(reading.lossless);

    const loom::Paper& p = reading.document;
    CHECK(p.key == "SmithJones24");
    CHECK(p.aliases == QStringList{"SmithJones23"});
    CHECK(p.relevance == 1);
    CHECK(p.work.typeName() == "article");
    CHECK(p.work.authors->text() == "Smith, Alice and Jones, Bob");
    REQUIRE(p.versions.size() == 2);
    CHECK(p.versions[0].text(u"posted") == "2023-01-15");
    CHECK(p.versions[0].field(u"page_count")->verified.isNull());
    CHECK(p.version("published")->text(u"year") == "2024");
    CHECK(p.readingLevel() == "studied");
    REQUIRE(p.renumberings.size() == 1);
    CHECK(p.renumberings[0].labels[0] == std::pair<QString, QString>("arxiv-v1", "(2.18)"));
    CHECK(p.renumberings[0].labels[1] == std::pair<QString, QString>("published", "(2.8)"));

    CHECK(loom::formatPaper(p) == smithJones24());
}

TEST_CASE("Every entry type reads and writes back with its fields", "[papers]")
{
    const std::vector<std::pair<QByteArray, std::vector<std::pair<QByteArray, QByteArray>>>> types = {
        {"article", {{"journal", "\"J\""}, {"volume", "\"1\""}, {"number", "\"2\""}, {"pages", "\"1--2\""}, {"year", "2024"},
                     {"doi", "\"10.1/x\""}, {"journal_abbrev", "\"J.\""}, {"month", "\"mar\""}, {"publisher", "\"P\""},
                     {"issn", "\"1234-5678\""}, {"mr", "\"MR1\""}, {"zbl", "\"1.1\""}, {"url", "\"https://x.org\""}, {"page_count", "30"}}},
        {"book", {{"publisher", "\"P\""}, {"address", "\"A\""}, {"year", "2001"}, {"edition", "\"2\""}, {"series", "\"S\""},
                  {"series_volume", "\"7\""}, {"isbn", "\"978-0\""}, {"doi", "\"10.1/b\""}}},
        {"incollection", {{"booktitle", "\"B\""}, {"publisher", "\"P\""}, {"address", "\"A\""}, {"pages", "\"3--9\""},
                          {"year", "2002"}, {"series", "\"S\""}, {"volume", "\"4\""}, {"doi", "\"10.1/c\""}}},
        {"inproceedings", {{"booktitle", "\"B\""}, {"publisher", "\"P\""}, {"address", "\"A\""}, {"pages", "\"3--9\""},
                           {"year", "2002"}, {"series", "\"S\""}, {"volume", "\"4\""}, {"doi", "\"10.1/c\""}}},
        {"phdthesis", {{"school", "\"U\""}, {"address", "\"A\""}, {"year", "1999"}}},
        {"mastersthesis", {{"school", "\"U\""}, {"address", "\"A\""}, {"year", "1999"}}},
        {"techreport", {{"institution", "\"I\""}, {"number", "\"TR-1\""}, {"year", "2003"}}},
        {"talk", {{"howpublished", "\"Seminar talk\""}, {"venue", "\"V\""}, {"date", "2009-03-02"}, {"url", "\"https://x.org\""}}},
        {"lecture-notes", {{"howpublished", "\"Notes\""}, {"venue", "\"V\""}, {"date", "2009-03-02"}, {"url", "\"https://x.org\""}}},
        {"other", {{"howpublished", "\"H\""}, {"venue", "\"V\""}, {"date", "2009-03-02"}, {"url", "\"https://x.org\""}}},
    };
    for (const auto& [type, fields] : types) {
        INFO(type.toStdString());
        const QByteArray text = entryOf(type, fields);
        const loom::PaperReading reading = loom::readPaper(text, "Papers/Smith24/paper.toml");
        CHECK(problemsOf(reading.problems).empty());
        CHECK(reading.lossless);
        CHECK(reading.document.versions.at(0).fields.size() == fields.size());
        CHECK(loom::formatPaper(reading.document) == text);
    }
}

TEST_CASE("Version fields are written in the spec's order", "[papers]")
{
    const QByteArray text = entryOf("article", {{"mr", "\"MR1\""}, {"year", "2024"}, {"journal", "\"J\""}});
    const loom::PaperReading reading = loom::readPaper(text, "Papers/Smith24/paper.toml");
    const QByteArray written = loom::formatPaper(reading.document);
    CHECK(written.indexOf("journal =") < written.indexOf("year ="));
    CHECK(written.indexOf("year =") < written.indexOf("mr ="));
}

TEST_CASE("Field problems in paper.toml, with their lines", "[papers]")
{
    const QByteArray text =
        "key = \"Smith24\"\n"                                                         // 1
        "cite = \"published\"\n"                                                      // 2
        "relevance = 0\n"                                                             // 3
        "created = 2026-11-03\n"                                                      // 4
        "colour = \"red\"\n"                                                          // 5
        "\n[work]\n"                                                                  // 6, 7
        "type = { value = \"article\", source = \"https://x.org\", verified = false }\n" // 8
        "title = \"Plain\"\n"                                                         // 9
        "authors = { value = [{ last = \"Smith\" }], source = \"\", verified = true }\n" // 10
        "\n[[versions]]\n"                                                            // 11, 12
        "id = \"published\"\n"                                                        // 13
        "kind = \"published\"\n"                                                      // 14
        "journal = { value = \"J\", source = \"https://x.org\", verified = false }\n" // 15
        "volume = { value = \"1\", source = \"https://x.org\", verified = false }\n"  // 16
        "number = { value = \"2\", source = \"https://x.org\", verified = false }\n"  // 17
        "pages = { value = \"1--2\", source = \"https://x.org\", verified = false }\n" // 18
        "year = { value = \"2024\", source = \"https://x.org\", verified = false }\n" // 19
        "doi = { value = \"10.1/x\", source = \"Downloads/a.pdf\", verified = false }\n" // 20
        "school = { value = \"U\", source = \"https://x.org\", verified = false }\n"  // 21
        "jornal = { value = \"J\", source = \"https://x.org\", verified = false }\n"  // 22
        "pdf = \"published.pdf\"\n";                                                  // 23
    const loom::PaperReading reading = loom::readPaper(text, "Papers/Smith24/paper.toml");
    CHECK_FALSE(reading.lossless);
    const std::vector<P> expected = {
        {"bad-value", 3, "`relevance` can't be `0`; it must be a whole number from 1 up (1 is most relevant)."},
        {"unknown-field", 5, "`colour` isn't a field of a paper. Is it misspelt? The fields are: key, aliases, cite, relevance, "
                             "relevance_why, tags, summary, created, verified, work, versions, reading, renumbering."},
        {"field-shape", 9, "`work.title` must be `{ value = …, source = \"…\", verified = … }`."},
        {"field-source", 10, "`work.authors` has no source. Every field says where it came from."},
        {"field-verified", 10, "`work.authors`'s `verified` must be a date or `false`."},
        {"wrong-type", 19, "`published.year.value` should be a whole number, not a string."},
        {"pdf-path", 20, "`Downloads/a.pdf` must be inside this paper's `local/` folder."},
        {"field-not-for-type", 21, "`published.school` isn't a field of an article's published version."},
        {"unknown-field", 22, "`published.jornal` isn't a field of a version. Is it misspelt? The fields are: id, kind, pdf, "
                              "differences, and the bibliographic fields of its kind and the work's type."},
        {"pdf-path", 23, "`published.pdf` must be inside this paper's `local/` folder."},
        {"field-expected", 12, "Smith24's published version has no `year`; articles usually have one."},
    };
    CHECK(problemsOf(reading.problems) == expected);
}

TEST_CASE("Version problems in paper.toml", "[papers]")
{
    const QByteArray a = "source = \"https://arxiv.org/abs/1\", verified = false";
    const QByteArray text =
        "key = \"Smith24\"\n"                                                         // 1
        "cite = \"ed3\"\n"                                                            // 2
        "relevance = 1\n"                                                             // 3
        "created = 2026-11-03\n"                                                      // 4
        "\n[work]\n"                                                                  // 5, 6
        "type = { value = \"article\", " + a + " }\n"                                 // 7
        "title = { value = \"T\", " + a + " }\n"                                      // 8
        "authors = { value = [], " + a + " }\n"                                       // 9
        "\n[[versions]]\n"                                                            // 10, 11
        "id = \"arxiv-v2\"\n"                                                         // 12
        "kind = \"arxiv\"\n"                                                          // 13
        "arxiv_id = { value = \"2401.00001\", " + a + " }\n"                          // 14
        "arxiv_version = { value = 3, " + a + " }\n"                                  // 15
        "posted = { value = 2024-01-02, " + a + " }\n"                                // 16
        "primary_class = { value = \"math.AP\", " + a + " }\n"                        // 17
        "page_count = { value = 9, " + a + " }\n"                                     // 18
        "journal = { value = \"J\", " + a + " }\n"                                    // 19
        "\n[[versions]]\n"                                                            // 20, 21
        "id = \"arxiv-v2\"\n"                                                         // 22
        "kind = \"preprint\"\n"                                                       // 23
        "\n[[versions]]\n"                                                            // 24, 25
        "id = \"ed2\"\n"                                                              // 26
        "kind = \"published\"\n"                                                      // 27
        "\n[[reading]]\n"                                                             // 28, 29
        "version = \"arxiv-v9\"\n"                                                    // 30
        "level = \"skimmed\"\n"                                                       // 31
        "what = \"The abstract\"\n"                                                   // 32
        "date = 2026-11-03\n"                                                         // 33
        "session = \"Sessions/yesterday\"\n"                                          // 34
        "\n[[renumbering]]\n"                                                         // 35, 36
        "arxiv-v2 = \"(1.1)\"\n"                                                      // 37
        "checked = 2026-11-03\n";                                                     // 38
    const loom::PaperReading reading = loom::readPaper(text, "Papers/Smith24/paper.toml");
    const std::vector<P> expected = {
        {"cite-unknown", 2, "`cite` names `ed3`, which isn't one of this paper's versions."},
        {"authors-missing", 9, "Smith24 has no authors."},
        {"arxiv-version-mismatch", 15, "Version `arxiv-v2` has `arxiv_version = 3`; they must agree."},
        {"field-not-for-type", 19, "`arxiv-v2.journal` isn't a field of an arXiv version."},
        {"version-duplicate", 22, "Two versions are called `arxiv-v2`."},
        {"bad-value", 23, "`kind` can't be `preprint`; it must be one of `arxiv`, `published`."},
        {"version-id", 26, "`ed2` isn't a valid version id for a published version: use published."},
        {"version-unknown", 30, "`reading` names version `arxiv-v9`, which this paper doesn't have."},
        {"bad-value", 34, "`session` can't be `Sessions/yesterday`; it must be one of `Sessions/<YYYY-MM-DD>`."},
        {"bad-value", 36, "A `[[renumbering]]` entry needs the item's label in two or more versions."},
    };
    std::vector<P> withWarnings = expected;
    for (const char* name : {"journal", "volume", "number", "pages", "year", "doi"})
        withWarnings.emplace_back("field-expected", 25,
            QStringLiteral("Smith24's edition ed2 has no `%1`; articles usually have one.").arg(QLatin1StringView(name)));
    CHECK(problemsOf(reading.problems) == withWarnings);
}

TEST_CASE("A book may have editors instead of authors, and later editions", "[papers]")
{
    QByteArray text = entryOf("book", {{"publisher", "\"P\""}, {"address", "\"A\""}, {"year", "2001"}, {"edition", "\"1\""},
                                       {"series", "\"S\""}, {"series_volume", "\"7\""}, {"isbn", "\"978-0\""}, {"doi", "\"10.1/b\""}});
    text.replace("authors = ", "editors = ");
    text += "\n[[versions]]\nid = \"ed2\"\nkind = \"published\"\nyear = " + f("2010") + "\n";
    const loom::PaperReading reading = loom::readPaper(text, "Papers/Smith24/paper.toml");
    const std::vector<P> expected = {
        {"field-expected", 23, "Smith24's edition ed2 has no `publisher`; books usually have one."},
        {"field-expected", 23, "Smith24's edition ed2 has no `address`; books usually have one."},
        {"field-expected", 23, "Smith24's edition ed2 has no `edition`; books usually have one."},
        {"field-expected", 23, "Smith24's edition ed2 has no `series`; books usually have one."},
        {"field-expected", 23, "Smith24's edition ed2 has no `series_volume`; books usually have one."},
        {"field-expected", 23, "Smith24's edition ed2 has no `isbn`; books usually have one."},
        {"field-expected", 23, "Smith24's edition ed2 has no `doi`; books usually have one."},
    };
    CHECK(problemsOf(reading.problems) == expected);
    CHECK(reading.document.keyNames() == std::vector<loom::PersonName>{{"Smith", ""}});

    QByteArray noOne = text;
    noOne.replace("editors = ", "colour = ");
    const loom::PaperReading none = loom::readPaper(noOne, "Papers/Smith24/paper.toml");
    CHECK(std::count_if(none.problems.begin(), none.problems.end(), [](const loom::Problem& p) {
        return p.code == "authors-missing" && p.message == "Smith24 has no authors (and no editors).";
    }) == 1);
}

TEST_CASE("Required fields of paper.toml, and a draft that leaves out key, cite and created", "[papers]")
{
    const QByteArray text = "relevance = 1\n\n[work]\ntype = " + f("\"article\"") + "\ntitle = " + f("\"T\"") + "\n";
    const std::vector<P> expected = {
        {"missing-field", 1, "`key` is missing; every paper needs one."},
        {"missing-field", 1, "`cite` is missing; every paper needs one."},
        {"missing-field", 1, "`created` is missing; every paper needs one."},
        {"missing-field", 1, "`versions` is missing; every paper needs one."},
        {"authors-missing", 3, "Smith24 has no authors."},
    };
    CHECK(problemsOf(loom::readPaper(text, "Papers/Smith24/paper.toml").problems) == expected);

    // A draft is named by its file, whatever folder it is in.
    const std::vector<P> draft = {
        {"missing-field", 1, "`versions` is missing; every paper needs one."},
        {"authors-missing", 3, "paper.toml has no authors."},
    };
    CHECK(problemsOf(loom::readPaper(text, "Papers/Smith24/paper.toml", false).problems) == draft);
}

TEST_CASE("Empty values in paper.toml are missing values", "[papers]")
{
    // Specs/m1-core.md, "Front matter", A2. `differences` is free text, and a name's `first` may
    // be empty, so neither is reported.
    const QByteArray x = "source = \"https://x.org\", verified = false";
    const QByteArray text =
        "key = \"\"\n"                                                                 // 1
        "cite = \"\"\n"                                                                // 2
        "relevance = 1\n"                                                              // 3
        "created = 2026-11-03\n"                                                       // 4
        "\n[work]\n"                                                                   // 5, 6
        "type = { value = \"\", " + x + " }\n"                                         // 7
        "title = { value = \"\", " + x + " }\n"                                        // 8
        "authors = { value = [{ last = \"Smith\", first = \"\" }, { last = \"\", first = \"B\" }], " + x + " }\n" // 9
        "\n[[versions]]\n"                                                             // 10, 11
        "id = \"\"\n"                                                                  // 12
        "kind = \"\"\n"                                                                // 13
        "pdf = \"\"\n"                                                                 // 14
        "differences = \"\"\n"                                                         // 15
        "\n[[reading]]\n"                                                              // 16, 17
        "version = \"\"\n"                                                             // 18
        "level = \"\"\n"                                                               // 19
        "what = \"\"\n"                                                                // 20
        "date = 2026-11-03\n"                                                          // 21
        "session = \"\"\n";                                                            // 22
    const std::vector<P> expected = {
        {"missing-field", 1, "`key` is empty; every paper needs one."},
        {"missing-field", 2, "`cite` is empty; every paper needs one."},
        {"missing-field", 7, "`work.type` is empty; every paper needs one."},
        {"missing-field", 8, "`work.title` is empty; every paper needs one."},
        {"missing-field", 9, "`work.authors.last` is empty; every name needs one."},
        {"missing-field", 12, "`id` is empty; every version needs one."},
        {"missing-field", 13, "`kind` is empty; every version needs one."},
        {"missing-field", 14, "`pdf` is empty; give it a value or leave it out."},
        {"missing-field", 18, "`version` is empty; every reading needs one."},
        {"missing-field", 19, "`level` is empty; every reading needs one."},
        {"missing-field", 20, "`what` is empty; every reading needs one."},
        {"missing-field", 22, "`session` is empty; give it a value or leave it out."},
    };
    CHECK(problemsOf(loom::readPaper(text, "Papers/Smith24/paper.toml").problems) == expected);

    // A draft may leave out `key` and `cite`, but not give them empty.
    std::vector<P> draft = expected;
    std::get<2>(draft[0]) = "`key` is empty; give it a value or leave it out.";
    std::get<2>(draft[1]) = "`cite` is empty; give it a value or leave it out.";
    CHECK(problemsOf(loom::readPaper(text, "Papers/Smith24/paper.toml", false).problems) == draft);
}

TEST_CASE("A version id or session ending in a line break doesn't pass its rule", "[papers]")
{
    // Qt's `$` also matches before a final line break, so each rule must be anchored at the very end.
    const auto has = [](const loom::PaperReading& reading, const P& problem) {
        const std::vector<P> problems = problemsOf(reading.problems);
        return std::find(problems.begin(), problems.end(), problem) != problems.end();
    };

    QByteArray session = smithJones24();
    REQUIRE(session.contains("session = \"Sessions/2026-10-18\""));
    session.replace("session = \"Sessions/2026-10-18\"", "session = \"Sessions/2026-10-18\\n\"");
    CHECK(problemsOf(loom::readPaper(session, paperPath).problems) == std::vector<P>{
        {"bad-value", 44, "`session` can't be `Sessions/2026-10-18\n`; it must be one of `Sessions/<YYYY-MM-DD>`."}});

    QByteArray arxiv = smithJones24();
    REQUIRE(arxiv.contains("id = \"arxiv-v1\""));
    arxiv.replace("id = \"arxiv-v1\"", "id = \"arxiv-v1\\n\"");
    CHECK(has(loom::readPaper(arxiv, paperPath),
              P{"version-id", 17, "`arxiv-v1\n` isn't a valid version id for an arxiv version: use arxiv-v<N>."}));

    const QByteArray edition = entryOf("book", {{"year", "2001"}}) + "\n[[versions]]\nid = \"ed2\\n\"\nkind = \"published\"\n";
    CHECK(has(loom::readPaper(edition, "Papers/Smith24/paper.toml"),
              P{"version-id", 17, "`ed2\n` isn't a valid version id for a published version: use published or ed<N>."}));
}

TEST_CASE("paper.toml that isn't TOML or UTF-8", "[papers]")
{
    const loom::PaperReading broken = loom::readPaper("key = \n", paperPath);
    REQUIRE(broken.problems.size() == 1);
    CHECK(broken.problems[0].code == "toml-syntax");
    CHECK(broken.problems[0].line == 1);
    CHECK(std::get<0>(problemsOf(loom::readPaper("key = \"\xff\"\n", paperPath).problems).at(0)) == "not-utf8");
    CHECK_FALSE(loom::readPaper("key = \n", paperPath).parsed);
}

TEST_CASE("A stub reads without problems and writes back the same", "[papers][stubs]")
{
    const QByteArray text =
        stubToml("7RT2", "theorem", true, "stated_using = [\"2M9Q\"]\npaper_premises = [\"3WQ6\"]\naliases = [\"thm:smith-1.2\"]\n")
        + "published = { at = \"Theorem 1.2\", checked_against_pdf = false }\n";
    const loom::StubReading reading = loom::readStub(text, "Papers/SmithJones24/Statements/7RT2.toml");
    CHECK(problemsOf(reading.problems).empty());
    CHECK(reading.lossless);
    const loom::Stub& s = reading.document;
    CHECK(s.paperPremises == QStringList{"3WQ6"});
    REQUIRE(s.locators.size() == 2);
    CHECK(s.locators[0].version == "arxiv-v1");
    CHECK(s.locators[0].checkedAgainstPdf == QDate(2026, 10, 18));
    CHECK(s.locators[1].version == "published");
    CHECK(loom::isCited(s));
    CHECK(loom::formatStub(s) == text);
}

TEST_CASE("A stub is cited only once its extracted_from locator is checked against the PDF", "[papers][stubs]")
{
    loom::Stub stub = loom::readStub(stubToml("3WQ6", "lemma", false), "Papers/SmithJones24/Statements/3WQ6.toml").document;
    CHECK_FALSE(loom::isCited(stub));
    // A checked locator in another version doesn't make it cited.
    stub.locators.push_back({"published", "Lemma 3.1", QDate(2026, 11, 1), 1});
    CHECK_FALSE(loom::isCited(stub));
    stub.locators[0].checkedAgainstPdf = QDate(2026, 11, 2);
    CHECK(loom::isCited(stub));
}

TEST_CASE("Stub problems, with their lines", "[papers][stubs]")
{
    const QByteArray text =
        "id = \"7RT3\"\n"                                          // 1
        "kind = \"lema\"\n"                                        // 2
        "gist = \"  \"\n"                                          // 3
        "aliases = [\"7RT2\"]\n"                                   // 4
        "extracted_from = \"published\"\n"                         // 5
        "extracted_by = \"claude\"\n"                              // 6
        "created = 2026-10-18\n"                                   // 7
        "\n[locators]\n"                                           // 8, 9
        "arxiv-v1 = { at = \"Theorem 1.2\", checked_against_pdf = true }\n"; // 10
    const loom::StubReading reading = loom::readStub(text, "Papers/SmithJones24/Statements/7RT2.toml");
    const std::vector<P> expected = {
        {"id-mismatch", 1, "The id is `7RT3` but the file is named `7RT2`; they must match."},
        {"bad-value", 2, "`kind` can't be `lema`; it must be one of `definition`, `lemma`, `proposition`, `theorem`, "
                         "`corollary`, `conjecture`, `question`, `remark`, `example`."},
        {"stub-gist-empty", 3, "Stub 7RT3 has no gist."},
        {"alias-invalid", 4, "`7RT2` can't be an alias: it is shaped like a tag."},
        {"stub-locator-missing", 5, "Stub 7RT3 was extracted from `published` but has no locator for it."},
        {"bad-value", 6, "`extracted_by` can't be `claude`; it must be one of `dakota`, `agent:<model>`."},
        {"wrong-type", 10, "`locators.arxiv-v1.checked_against_pdf` should be a date or `false`, not true or false."},
    };
    CHECK(problemsOf(reading.problems) == expected);
    CHECK_FALSE(reading.lossless);
}

TEST_CASE("Empty values in a stub are missing values", "[papers][stubs]")
{
    // Specs/m1-core.md, "Front matter", A2. An empty `gist` keeps its own code, `stub-gist-empty`.
    const QByteArray text =
        "id = \"\"\n"                                                // 1
        "kind = \"\"\n"                                              // 2
        "gist = \"G\"\n"                                             // 3
        "extracted_from = \"\"\n"                                    // 4
        "extracted_by = \"\"\n"                                      // 5
        "created = 2026-10-18\n"                                     // 6
        "\n[locators]\n"                                             // 7, 8
        "arxiv-v1 = { at = \"\", checked_against_pdf = false }\n";   // 9
    CHECK(problemsOf(loom::readStub(text, "Papers/SmithJones24/Statements/7RT2.toml").problems) == std::vector<P>{
        {"missing-field", 1, "`id` is empty; every stub needs one."},
        {"missing-field", 2, "`kind` is empty; every stub needs one."},
        {"missing-field", 4, "`extracted_from` is empty; every stub needs one."},
        {"missing-field", 5, "`extracted_by` is empty; every stub needs one."},
        {"missing-field", 9, "`locators.arxiv-v1.at` is empty; every locator needs one."},
    });
}

TEST_CASE("A value of only whitespace in paper.toml or a stub is an empty value (A21)", "[papers][stubs]")
{
    // Specs/m1-core.md, "Front matter", A21. Free text (`summary`, a name's `first`) is unaffected.
    QByteArray paper = smithJones24();
    REQUIRE(paper.contains("doi = { value = \"10.0000/jea.2024.17\""));
    paper.replace("doi = { value = \"10.0000/jea.2024.17\"", "doi = { value = \"  \"");
    const qsizetype title = paper.indexOf("title = { value = \"");
    REQUIRE(title >= 0);
    const qsizetype titleEnd = paper.indexOf('"', title + 19);
    paper.replace(title + 19, titleEnd - title - 19, "   ");
    const loom::PaperReading reading = loom::readPaper(paper, paperPath);
    std::vector<P> problems = problemsOf(reading.problems);
    CHECK(std::count(problems.begin(), problems.end(), P{"missing-field", 34, "`published.doi` is empty; give it a value or leave it out."}) == 1);
    CHECK(std::count_if(problems.begin(), problems.end(), [](const P& p) {
              return std::get<2>(p) == "`work.title` is empty; every paper needs one.";
          }) == 1);
    CHECK(std::none_of(problems.begin(), problems.end(), [](const P& p) { return std::get<0>(p) == "wrong-type"; }));

    QByteArray stub = stubToml("7RT2", "theorem", false);
    const qsizetype at = stub.indexOf("at = \"");
    REQUIRE(at >= 0);
    stub.replace(at + 6, stub.indexOf('"', at + 6) - at - 6, "  ");
    const std::vector<P> stubProblems = problemsOf(loom::readStub(stub, "Papers/SmithJones24/Statements/7RT2.toml").problems);
    REQUIRE(stubProblems.size() == 1);
    CHECK(std::get<0>(stubProblems[0]) == "missing-field");
    CHECK(std::get<2>(stubProblems[0]).endsWith(".at` is empty; every locator needs one."));
}

TEST_CASE("A stub's extracted_by can't end in a line break", "[papers][stubs]")
{
    QByteArray text = stubToml("7RT2", "theorem", false);
    REQUIRE(text.contains("extracted_by = \"agent:claude-opus-5-5\""));
    text.replace("extracted_by = \"agent:claude-opus-5-5\"", "extracted_by = \"agent:claude-opus-5-5\\n\"");
    CHECK(problemsOf(loom::readStub(text, "Papers/SmithJones24/Statements/7RT2.toml").problems) == std::vector<P>{
        {"bad-value", 5, "`extracted_by` can't be `agent:claude-opus-5-5\n`; it must be one of `dakota`, `agent:<model>`."}});
}

TEST_CASE("A stub needs its locators and its required fields", "[papers][stubs]")
{
    const std::vector<P> expected = {
        {"missing-field", 1, "`id` is missing; every stub needs one."},
        {"missing-field", 1, "`kind` is missing; every stub needs one."},
        {"missing-field", 1, "`extracted_from` is missing; every stub needs one."},
        {"missing-field", 1, "`extracted_by` is missing; every stub needs one."},
        {"missing-field", 1, "`created` is missing; every stub needs one."},
        {"stub-gist-empty", 1, "Stub 7RT2 has no gist."},
        {"missing-field", 1, "`locators` is missing; every stub needs one."},
    };
    CHECK(problemsOf(loom::readStub("title = \"T\"\n", "Papers/SmithJones24/Statements/7RT2.toml").problems) == expected);
}

TEST_CASE("An old key can't be the paper's own key, or another old key, in another case (A3)", "[papers]")
{
    // Keys compare ignoring case everywhere (A3), so `smithjones24` is SmithJones24's key, not an
    // old one, and `smithjones23` is the old key `SmithJones23` again. An exact repeat in
    // `aliases` keeps its own `bad-value`.
    QByteArray text = smithJones24();
    REQUIRE(text.contains("aliases = [\"SmithJones23\"]\n"));
    text.replace("aliases = [\"SmithJones23\"]\n",
                 "aliases = [\"SmithJones23\", \"smithjones24\", \"smithjones23\", \"SmithJones24\", \"SmithJones23\"]\n");
    CHECK(problemsOf(loom::readPaper(text, paperPath).problems) == std::vector<P>{
        {"alias-duplicate", 2, "`smithjones24` is the key of SmithJones24 itself (keys compare ignoring case, A3), so it "
                               "can't be one of its old keys."},
        {"alias-duplicate", 2, "`smithjones23` and `SmithJones23` are one old key of SmithJones24 (keys compare ignoring "
                               "case, A3); keep one."},
        {"alias-duplicate", 2, "`SmithJones24` is the key of SmithJones24 itself (keys compare ignoring case, A3), so it "
                               "can't be one of its old keys."},
        {"bad-value", 2, "`aliases` can't list `SmithJones23` twice."},
    });
}

TEST_CASE("arxiv_id is an arXiv identifier without its version (A4)", "[papers]")
{
    // Specs/m1-core.md, "Papers", `[[versions]]`, A4: `YYMM.NNNN` from April 2007 to December
    // 2014, `YYMM.NNNNN` from January 2015, or old-style `archive/YYMMNNN` with an optional
    // subject class; never with `vN`.
    const auto withId = [](const QString& id) {
        QByteArray text = smithJones24();
        REQUIRE(text.contains("arxiv_id = { value = \"2401.99999\""));
        text.replace("arxiv_id = { value = \"2401.99999\"", "arxiv_id = { value = \"" + id.toUtf8() + "\"");
        return loom::readPaper(text, paperPath);
    };
    for (const char* good : {"2401.99999", "0704.0001", "1412.9999", "1501.00001", "math/0601001", "math.AP/0601001",
                             "hep-th/9901001"}) {
        INFO(good);
        CHECK(problemsOf(withId(QString::fromLatin1(good)).problems).empty());
    }
    for (const char* bad : {"2401.99999v2", "arXiv:2401.99999", "0703.0001", "1501.0001", "1412.00001", "2413.00001",
                            "2401.9999", "math/0601001v1", "Math/0601001", "math/060100", "math/0613001", "2401.99999\\n"}) {
        const QString value = QString::fromLatin1(bad).replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        INFO(bad);
        CHECK(problemsOf(withId(QString::fromLatin1(bad)).problems) == std::vector<P>{
            {"bad-value", 19, "`arxiv-v1.arxiv_id` can't be `" + value + "`; it must be an arXiv identifier such as "
                              "`2503.01234` or `math/0601001`, without `vN`."}});
    }
}

TEST_CASE("primary_class is an arXiv category", "[papers]")
{
    const auto withClass = [](const QString& category) {
        QByteArray text = smithJones24();
        REQUIRE(text.contains("primary_class = { value = \"math.AP\""));
        text.replace("primary_class = { value = \"math.AP\"", "primary_class = { value = \"" + category.toUtf8() + "\"");
        return loom::readPaper(text, paperPath);
    };
    for (const char* good : {"math.AP", "math", "hep-th", "cs.LG", "cond-mat.mes-hall", "physics.flu-dyn"}) {
        INFO(good);
        CHECK(problemsOf(withClass(QString::fromLatin1(good)).problems).empty());
    }
    for (const char* bad : {"math AP", "MATH.AP", "math.", "math.A", "math.APX", ".AP", "math.AP\\n"}) {
        const QString value = QString::fromLatin1(bad).replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        INFO(bad);
        CHECK(problemsOf(withClass(QString::fromLatin1(bad)).problems) == std::vector<P>{
            {"bad-value", 22, "`arxiv-v1.primary_class` can't be `" + value + "`; it must be an arXiv category such as "
                              "`math.AP` or `hep-th`."}});
    }
}

TEST_CASE("A source that is a link but not https:// says so", "[papers]")
{
    QByteArray text = smithJones24();
    const QByteArray journal = "journal = { value = \"Journal of Example Analysis\", source = \"https://doi.org/10.0000/jea.2024.17\"";
    REQUIRE(text.contains(journal));
    text.replace(journal, "journal = { value = \"Journal of Example Analysis\", source = \"http://www.ams.org/mathscinet\"");
    CHECK(problemsOf(loom::readPaper(text, paperPath).problems) == std::vector<P>{
        {"field-source", 29, "`published.journal`'s source `http://www.ams.org/mathscinet` must be an `https://` link or a "
                             "path inside this paper's `local/` folder."}});
}

TEST_CASE("Empty bibliographic values are missing values, and keep their source", "[papers]")
{
    // Specs/m1-core.md, "Front matter", A2: an empty value is a missing one.
    QByteArray text = smithJones24();
    REQUIRE(text.contains("doi = { value = \"10.0000/jea.2024.17\""));
    text.replace("doi = { value = \"10.0000/jea.2024.17\"", "doi = { value = \"\"");
    text.replace("mr = { value = \"MR0000001\"", "mr = { value = \"\"");
    text.replace("primary_class = { value = \"math.AP\"", "primary_class = { value = \"\"");
    const loom::PaperReading reading = loom::readPaper(text, paperPath);
    CHECK(problemsOf(reading.problems) == std::vector<P>{
        {"missing-field", 22, "`arxiv-v1.primary_class` is empty; give it a value or leave it out."},
        {"missing-field", 34, "`published.doi` is empty; give it a value or leave it out."},
        {"missing-field", 35, "`published.mr` is empty; give it a value or leave it out."},
    });
    CHECK(reading.lossless);
    CHECK(loom::formatPaper(reading.document) == text);
}

TEST_CASE("A date Qt can't hold, the year 0, is reported and the entry isn't rewritten", "[papers]")
{
    // toml++ reads the year 0 and QDate has none, so without a word the date would be lost on a rewrite.
    QByteArray text = smithJones24();
    text.replace("posted = { value = 2023-01-15", "posted = { value = 0000-01-15");
    text.replace("journal = { value = \"Journal of Example Analysis\", source = \"https://doi.org/10.0000/jea.2024.17\", verified = 2026-11-03 }",
                 "journal = { value = \"Journal of Example Analysis\", source = \"https://doi.org/10.0000/jea.2024.17\", verified = 0000-01-01 }");
    text.replace("checked = false", "checked = 0000-01-02");
    const loom::PaperReading reading = loom::readPaper(text, paperPath);
    CHECK(problemsOf(reading.problems) == std::vector<P>{
        {"bad-value", 21, "`arxiv-v1.posted.value` can't be `0000-01-15`; it must be one of the dates from 0001-01-01 on."},
        {"bad-value", 29, "`published.journal.verified` can't be `0000-01-01`; it must be one of the dates from 0001-01-01 on."},
        {"bad-value", 49, "`renumbering.checked` can't be `0000-01-02`; it must be one of the dates from 0001-01-01 on."},
        {"field-expected", 16, "SmithJones24's arXiv version arxiv-v1 has no `posted`; arXiv versions usually have one."},
    });
    CHECK_FALSE(reading.lossless);

    QByteArray stub = stubToml("7RT2", "theorem", true);
    stub.replace("checked_against_pdf = 2026-10-18", "checked_against_pdf = 0000-10-18");
    const loom::StubReading stubReading = loom::readStub(stub, "Papers/SmithJones24/Statements/7RT2.toml");
    CHECK(problemsOf(stubReading.problems) == std::vector<P>{
        {"bad-value", 9, "`locators.arxiv-v1.checked_against_pdf` can't be `0000-10-18`; it must be one of the dates from "
                         "0001-01-01 on."}});
    CHECK_FALSE(stubReading.lossless);
}

TEST_CASE("A CRLF paper.toml or stub reads as an LF one", "[papers]")
{
    QByteArray text = smithJones24();
    text.replace("\n", "\r\n");
    const loom::PaperReading reading = loom::readPaper(text, paperPath);
    CHECK(problemsOf(reading.problems).empty());
    CHECK(reading.lossless);
    CHECK(loom::formatPaper(reading.document) == smithJones24());

    QByteArray stub = stubToml("7RT2", "theorem", true);
    const QByteArray lf = stub;
    stub.replace("\n", "\r\n");
    const loom::StubReading stubReading = loom::readStub(stub, "Papers/SmithJones24/Statements/7RT2.toml");
    CHECK(problemsOf(stubReading.problems).empty());
    CHECK(loom::formatStub(stubReading.document) == lf);
}

TEST_CASE("Non-ASCII names and a multi-line summary write back as they were read", "[papers]")
{
    QByteArray text = smithJones24();
    text.replace("{ last = \"Jones\", first = \"Bob\" }", QString::fromUtf8("{ last = \"Ó Súilleabháin\", first = \"Siobhán\" }").toUtf8());
    text.replace("summary = \"Proves energy decay for weak solutions of the model system with L^2 data.\"\n",
                 "summary = \"\"\"\nProves energy decay.\nAlso: $\\\\|u\\\\|_2 \\\\le C$.\"\"\"\n");
    const loom::PaperReading reading = loom::readPaper(text, paperPath);
    CHECK(problemsOf(reading.problems).empty());
    CHECK(reading.document.summary == "Proves energy decay.\nAlso: $\\|u\\|_2 \\le C$.");
    CHECK(reading.document.work.authors->names()[1].last == QString::fromUtf8("Ó Súilleabháin"));
    CHECK(loom::readPaper(loom::formatPaper(reading.document), paperPath).document.summary == reading.document.summary);
    CHECK(loom::formatPaper(loom::readPaper(loom::formatPaper(reading.document), paperPath).document)
          == loom::formatPaper(reading.document));
    CHECK(loom::formatPaper(reading.document).contains(QString::fromUtf8("last = \"Ó Súilleabháin\", first = \"Siobhán\"").toUtf8()));
}

TEST_CASE("Tables that aren't tables, and names that aren't names", "[papers]")
{
    // Each is reported, and the entry isn't rewritten.
    const QByteArray names = "{ last = \"Smith\", first = \"Alice\" }, { last = \"Jones\", first = \"Bob\" }";
    const std::pair<QByteArray, QByteArray> cases[] = {
        {names, "\"Smith\", { last = \"Jones\", first = \"Bob\" }"},
        {names, "{ last = \"Smith\", first = \"Alice\", middle = \"B\" }, { last = \"Jones\", first = \"Bob\" }"},
    };
    for (const auto& [from, to] : cases) {
        QByteArray text = smithJones24();
        REQUIRE(text.contains(from));
        text.replace(from, to);
        INFO(text.toStdString());
        const loom::PaperReading reading = loom::readPaper(text, paperPath);
        CHECK_FALSE(reading.problems.empty());
        CHECK_FALSE(reading.lossless);
    }

    const loom::PaperReading flat = loom::readPaper(
        "key = \"Smith24\"\ncite = \"published\"\nrelevance = 1\ncreated = 2026-10-18\nwork = \"article\"\nversions = 3\n", paperPath);
    CHECK(problemsOf(flat.problems) == std::vector<P>{
        {"cite-unknown", 2, "`cite` names `published`, which isn't one of this paper's versions."},
        {"wrong-type", 5, "`work` should be a table, `[work]`, not a string."},
        {"wrong-type", 6, "`versions` should be a list of versions, each `[[versions]]`, not a whole number."},
    });
    CHECK_FALSE(flat.lossless);
}

TEST_CASE("A renumbering needs checked, and labels that are strings", "[papers]")
{
    QByteArray text = smithJones24();
    REQUIRE(text.contains("published = \"(2.8)\"\nchecked = false\n"));
    QByteArray unchecked = text;
    unchecked.replace("published = \"(2.8)\"\nchecked = false\n", "published = \"(2.8)\"\n");
    const std::vector<P> missing = problemsOf(loom::readPaper(unchecked, paperPath).problems);
    REQUIRE(missing.size() == 1);
    CHECK(std::get<0>(missing[0]) == "missing-field");

    QByteArray number = text;
    number.replace("published = \"(2.8)\"", "published = 28");
    const loom::PaperReading reading = loom::readPaper(number, paperPath);
    // The label that isn't a string is dropped, which leaves the entry with one label.
    CHECK(problemsOf(reading.problems) == std::vector<P>{
        {"bad-value", 46, "A `[[renumbering]]` entry needs the item's label in two or more versions."},
        {"wrong-type", 48, "`renumbering.published` should be a string: the item's label in that version, not a whole number."},
    });
    CHECK_FALSE(reading.lossless);
}

TEST_CASE("A draft without a key is named by its file in messages", "[papers]")
{
    // `loom new-paper Downloads/draft.toml`: the draft's folder says nothing about the paper.
    const QByteArray text = "relevance = 1\n\n[work]\ntype = " + f("\"article\"") + "\ntitle = " + f("\"T\"") + "\n";
    const std::vector<P> expected = {
        {"missing-field", 1, "`versions` is missing; every paper needs one."},
        {"authors-missing", 3, "draft.toml has no authors."},
    };
    CHECK(problemsOf(loom::readPaper(text, "C:/Users/someone/Downloads/draft.toml", false).problems) == expected);
}
