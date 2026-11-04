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

    const std::vector<P> draft = {
        {"missing-field", 1, "`versions` is missing; every paper needs one."},
        {"authors-missing", 3, "Smith24 has no authors."},
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
        {"missing-field", 9, "`locators.arxiv-v1.at` is empty; every locator says where the result is."},
    });
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
