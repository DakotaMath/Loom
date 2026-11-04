// The bibliography (Specs/m1-core.md, "Papers", "The bibliography"): the exact entry for the
// spec's published article and arXiv-only paper, one entry per other type, escaping, and the
// warnings for unverified fields.

#include "LoomCore/Bibliography.h"

#include "PaperFixture.h"
#include "TestSupport.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

namespace {

/// A verified field, as TOML.
QByteArray f(const QByteArray& value, bool verified = true)
{
    return "{ value = " + value + ", source = \"https://example.org/s\", verified = " + (verified ? "2026-11-03" : "false") + " }";
}

/// An entry of `type` by `authors` (TOML names), its published version holding `fields`, with
/// an arXiv version if `arxiv`.
loom::Paper paperOf(const QByteArray& key, const QByteArray& type, const std::vector<std::pair<QByteArray, QByteArray>>& fields,
                    bool arxiv = false, const QByteArray& names = "authors = " + f("[{ last = \"Smith\", first = \"Alice\" }]"))
{
    QByteArray text = "key = \"" + key + "\"\ncite = \"published\"\nrelevance = 1\ncreated = 2026-11-03\n\n[work]\ntype = "
                    + f("\"" + type + "\"") + "\ntitle = " + f("\"On $L^2$ decay\"") + "\n" + names + "\n";
    if (arxiv)
        text += "\n[[versions]]\nid = \"arxiv-v1\"\nkind = \"arxiv\"\narxiv_id = " + f("\"2101.00001\"") + "\narxiv_version = "
              + f("1") + "\nposted = " + f("2021-01-04") + "\nprimary_class = " + f("\"math.AP\"") + "\npage_count = " + f("20") + "\n";
    text += "\n[[versions]]\nid = \"published\"\nkind = \"published\"\n";
    for (const auto& [name, value] : fields)
        text += name + " = " + f(value) + "\n";
    const loom::PaperReading reading = loom::readPaper(text, "Papers/" + QString::fromUtf8(key) + "/paper.toml");
    for (const loom::Problem& problem : reading.problems)
        CHECK(problem.severity == loom::Severity::Warning);
    REQUIRE(reading.parsed);
    return reading.document;
}

QString entryOf(const loom::Paper& paper)
{
    const loom::BibEntry entry = loom::bibEntry(paper);
    CHECK(entry.error.isEmpty());
    CHECK(entry.unverified.isEmpty());
    return entry.text;
}

} // namespace

TEST_CASE("BibTeX: the spec's published article, with arXiv versions", "[bib]")
{
    const loom::Paper paper = loom::readPaper(loomtest::smithJones24(), "Papers/SmithJones24/paper.toml").document;
    CHECK(entryOf(paper) ==
          "@article{SmithJones24,\n"
          "  author = {Smith, Alice and Jones, Bob},\n"
          "  title = {Energy decay for a model parabolic system},\n"
          "  journal = {Journal of Example Analysis},\n"
          "  volume = {12},\n"
          "  number = {3},\n"
          "  pages = {101--130},\n"
          "  year = {2024},\n"
          "  doi = {10.0000/jea.2024.17},\n"
          "  mrnumber = {MR0000001},\n"
          "  eprint = {2401.99999},\n"
          "  archiveprefix = {arXiv},\n"
          "  primaryclass = {math.AP},\n"
          "  note = {arXiv:2401.99999},\n"
          "}\n");
}

TEST_CASE("BibTeX: the spec's arXiv-only paper carries the cited version", "[bib]")
{
    const QByteArray text =
        "key = \"Garcia25\"\ncite = \"arxiv-v2\"\nrelevance = 2\ncreated = 2026-11-03\n\n[work]\n"
        "type = " + f("\"article\"") + "\ntitle = " + f("\"A counterexample to energy decay in two dimensions\"") + "\n"
        "authors = " + f("[{ last = \"García\", first = \"Ana\" }]") + "\n"
        "\n[[versions]]\nid = \"arxiv-v1\"\nkind = \"arxiv\"\narxiv_id = " + f("\"2503.00001\"") + "\narxiv_version = " + f("1")
        + "\nposted = " + f("2024-12-30") + "\nprimary_class = " + f("\"math.AP\"") + "\npage_count = " + f("11") + "\n"
        "\n[[versions]]\nid = \"arxiv-v2\"\nkind = \"arxiv\"\narxiv_id = " + f("\"2503.00001\"") + "\narxiv_version = " + f("2")
        + "\nposted = " + f("2025-06-10") + "\nprimary_class = " + f("\"math.AP\"") + "\npage_count = " + f("12") + "\n";
    const loom::PaperReading reading = loom::readPaper(text, "Papers/Garcia25/paper.toml");
    REQUIRE(reading.problems.empty());
    CHECK(entryOf(reading.document) ==
          QString::fromUtf8("@misc{Garcia25,\n"
                            "  author = {García, Ana},\n"
                            "  title = {A counterexample to energy decay in two dimensions},\n"
                            "  year = {2025},\n"
                            "  eprint = {2503.00001v2},\n"
                            "  archiveprefix = {arXiv},\n"
                            "  primaryclass = {math.AP},\n"
                            "  note = {arXiv:2503.00001v2},\n"
                            "}\n"));
}

TEST_CASE("BibTeX: an article's optional fields follow doi", "[bib]")
{
    const loom::Paper paper = paperOf("Smith24", "article",
        {{"journal", "\"J. Ex.\""}, {"volume", "\"1\""}, {"number", "\"2\""}, {"pages", "\"3--4\""}, {"year", "2024"},
         {"doi", "\"10.1/a\""}, {"journal_abbrev", "\"JE\""}, {"month", "\"mar\""}, {"publisher", "\"Pub\""},
         {"issn", "\"1234-5678\""}, {"mr", "\"MR2\""}, {"zbl", "\"1234.5\""}, {"url", "\"https://example.org/a\""}});
    CHECK(entryOf(paper) ==
          "@article{Smith24,\n  author = {Smith, Alice},\n  title = {On $L^2$ decay},\n  journal = {J. Ex.},\n  volume = {1},\n"
          "  number = {2},\n  pages = {3--4},\n  year = {2024},\n  doi = {10.1/a},\n  month = {mar},\n  publisher = {Pub},\n"
          "  issn = {1234-5678},\n  mrnumber = {MR2},\n  zbl = {1234.5},\n  url = {https://example.org/a},\n}\n");
}

TEST_CASE("BibTeX: one entry per other type", "[bib]")
{
    SECTION("book, with an arXiv version")
    {
        const loom::Paper paper = paperOf("Smith01", "book",
            {{"publisher", "\"Pub\""}, {"address", "\"City\""}, {"year", "2001"}, {"edition", "\"2\""}, {"series", "\"Lecture Series\""},
             {"series_volume", "\"7\""}, {"isbn", "\"978-0-00\""}, {"doi", "\"10.1/b\""}, {"mr", "\"MR3\""}},
            true);
        CHECK(entryOf(paper) ==
              "@book{Smith01,\n  author = {Smith, Alice},\n  title = {On $L^2$ decay},\n  edition = {2},\n  series = {Lecture Series},\n"
              "  volume = {7},\n  publisher = {Pub},\n  address = {City},\n  year = {2001},\n  isbn = {978-0-00},\n  doi = {10.1/b},\n"
              "  mrnumber = {MR3},\n  eprint = {2101.00001},\n  archiveprefix = {arXiv},\n  primaryclass = {math.AP},\n"
              "  note = {arXiv:2101.00001},\n}\n");
    }
    SECTION("book with editors and no authors")
    {
        const loom::Paper paper = paperOf("BrownGarcia10", "book", {{"publisher", "\"Pub\""}, {"year", "2010"}}, false,
            "editors = " + f("[{ last = \"Brown\", first = \"C\" }, { last = \"Garcia\", first = \"D\" }]"));
        CHECK(entryOf(paper) ==
              "@book{BrownGarcia10,\n  editor = {Brown, C and Garcia, D},\n  title = {On $L^2$ decay},\n  publisher = {Pub},\n"
              "  year = {2010},\n}\n");
    }
    SECTION("incollection and inproceedings")
    {
        for (const QByteArray& type : {QByteArray("incollection"), QByteArray("inproceedings")}) {
            const QByteArray names = "authors = " + f("[{ last = \"Smith\", first = \"Alice\" }]") + "\neditors = "
                                   + f("[{ last = \"Brown\", first = \"C\" }]");
            const loom::Paper paper = paperOf("Smith02", type,
                {{"booktitle", "\"Proceedings of Examples\""}, {"publisher", "\"Pub\""}, {"address", "\"City\""}, {"pages", "\"5--9\""},
                 {"year", "2002"}, {"series", "\"S\""}, {"volume", "\"4\""}, {"doi", "\"10.1/c\""}, {"zbl", "\"1.2\""}},
                false, names);
            CHECK(entryOf(paper) ==
                  "@" + QString::fromUtf8(type) + "{Smith02,\n  author = {Smith, Alice},\n  title = {On $L^2$ decay},\n"
                  "  booktitle = {Proceedings of Examples},\n  editor = {Brown, C},\n  series = {S},\n  volume = {4},\n"
                  "  publisher = {Pub},\n  address = {City},\n  pages = {5--9},\n  year = {2002},\n  doi = {10.1/c},\n"
                  "  zbl = {1.2},\n}\n");
        }
    }
    SECTION("phdthesis and mastersthesis")
    {
        for (const QByteArray& type : {QByteArray("phdthesis"), QByteArray("mastersthesis")}) {
            const loom::Paper paper = paperOf("Smith99", type,
                {{"school", "\"Example University\""}, {"address", "\"City\""}, {"year", "1999"}, {"url", "\"https://example.org/t\""}});
            CHECK(entryOf(paper) ==
                  "@" + QString::fromUtf8(type) + "{Smith99,\n  author = {Smith, Alice},\n  title = {On $L^2$ decay},\n"
                  "  school = {Example University},\n  address = {City},\n  year = {1999},\n  url = {https://example.org/t},\n}\n");
        }
    }
    SECTION("techreport")
    {
        const loom::Paper paper = paperOf("Smith03", "techreport",
            {{"institution", "\"Example Institute\""}, {"number", "\"TR-12\""}, {"year", "2003"}});
        CHECK(entryOf(paper) ==
              "@techreport{Smith03,\n  author = {Smith, Alice},\n  title = {On $L^2$ decay},\n  institution = {Example Institute},\n"
              "  number = {TR-12},\n  year = {2003},\n}\n");
    }
    SECTION("talk, lecture-notes and other")
    {
        for (const QByteArray& type : {QByteArray("talk"), QByteArray("lecture-notes"), QByteArray("other")}) {
            const loom::Paper paper = paperOf("Smith09", type,
                {{"howpublished", "\"Seminar talk\""}, {"venue", "\"Example Seminar\""}, {"date", "2009-03-02"},
                 {"url", "\"https://example.org/v\""}});
            CHECK(entryOf(paper) ==
                  "@misc{Smith09,\n  author = {Smith, Alice},\n  title = {On $L^2$ decay},\n  howpublished = {Seminar talk},\n"
                  "  note = {Example Seminar, 2009-03-02},\n  year = {2009},\n  url = {https://example.org/v},\n}\n");
        }
    }
}

TEST_CASE("BibTeX: escaping &, % and # outside math", "[bib]")
{
    CHECK(loom::escapeBibValue("Analysis & PDE, 100% #1") == "Analysis \\& PDE, 100\\% \\#1");
    CHECK(loom::escapeBibValue("Already \\& escaped") == "Already \\& escaped");
    CHECK(loom::escapeBibValue("Sets $A \\cap B & C$ and & D") == "Sets $A \\cap B & C$ and \\& D");

    const loom::Paper paper = paperOf("Smith24", "article", {{"journal", "\"Analysis & PDE\""}, {"year", "2024"}});
    CHECK(entryOf(paper).contains("  journal = {Analysis \\& PDE},\n"));
}

TEST_CASE("BibTeX: a value with unbalanced braces skips its entry", "[bib]")
{
    CHECK(loom::bracesBalanced("The {Navier--Stokes} equations"));
    CHECK(loom::bracesBalanced("A \\{ literal brace"));
    CHECK_FALSE(loom::bracesBalanced("Open {brace"));
    CHECK_FALSE(loom::bracesBalanced("Close} {first"));

    const loom::Paper broken = paperOf("Smith24", "article", {{"journal", "\"J. {Ex.\""}, {"year", "2024"}});
    const loom::BibEntry entry = loom::bibEntry(broken);
    CHECK(entry.text.isEmpty());
    CHECK(entry.error == "Smith24's `journal` has unbalanced braces, so its BibTeX entry was left out of refs.bib.");

    const loom::Bibliography bib = loom::formatBibliography({broken, paperOf("Jones20", "article", {{"year", "2020"}})});
    CHECK(bib.text.startsWith("@article{Jones20,"));
    REQUIRE(bib.problems.size() == 1);
    CHECK(bib.problems[0].severity == loom::Severity::Error);
    CHECK(bib.problems[0].file == "Papers/Smith24/paper.toml");
}

TEST_CASE("BibTeX: entries in key order, and a warning naming each unverified field used", "[bib]")
{
    QByteArray text = loomtest::smithJones24();
    text.replace("journal = { value = \"Journal of Example Analysis\", source = \"https://doi.org/10.0000/jea.2024.17\", verified = 2026-11-03 }",
                 "journal = { value = \"Journal of Example Analysis\", source = \"https://doi.org/10.0000/jea.2024.17\", verified = false }");
    text.replace("volume = { value = \"12\", source = \"https://doi.org/10.0000/jea.2024.17\", verified = 2026-11-03 }",
                 "volume = { value = \"12\", source = \"https://doi.org/10.0000/jea.2024.17\", verified = false }");
    const loom::Paper smith = loom::readPaper(text, "Papers/SmithJones24/paper.toml").document;
    const loom::Paper jones = paperOf("Jones20", "article", {{"journal", "\"J\""}, {"year", "2020"}});
    const loom::Paper adams = paperOf("Adams19", "article", {{"journal", "\"J\""}, {"year", "2019"}});

    const loom::Bibliography bib = loom::formatBibliography({smith, jones, adams});
    const QString written = QString::fromUtf8(bib.text);
    CHECK(written.indexOf("@article{Adams19,") == 0);
    CHECK(written.indexOf("}\n\n@article{Jones20,") > 0);
    CHECK(written.indexOf("}\n\n@article{SmithJones24,") > written.indexOf("@article{Jones20,"));
    CHECK(written.endsWith("}\n"));
    REQUIRE(bib.problems.size() == 1);
    CHECK(bib.problems[0].severity == loom::Severity::Warning);
    CHECK(bib.problems[0].message == "refs.bib uses unverified fields: SmithJones24 journal, volume");
}

TEST_CASE("BibTeX: unverified fields of another version are named with the version", "[bib]")
{
    QByteArray text = loomtest::smithJones24();
    text.replace("arxiv_id = { value = \"2401.99999\", source = \"https://arxiv.org/abs/2401.99999v1\", verified = 2026-11-03 }",
                 "arxiv_id = { value = \"2401.99999\", source = \"https://arxiv.org/abs/2401.99999v1\", verified = false }");
    const loom::BibEntry entry = loom::bibEntry(loom::readPaper(text, "Papers/SmithJones24/paper.toml").document);
    CHECK(entry.unverified == QStringList{"arxiv-v1.arxiv_id"});
}
