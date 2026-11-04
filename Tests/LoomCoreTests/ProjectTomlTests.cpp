// Reading, checking and writing loom.toml (Specs/m1-core.md, "`loom.toml`" and "Validation",
// the project's problems).

#include "LoomCore/ProjectToml.h"
#include "LoomCore/TomlWriter.h"

#include "TestSupport.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTimeZone>

#include <catch2/catch_test_macros.hpp>

#include <tuple>
#include <vector>

using loomtest::writeFile;

namespace {

/// The spec's example `loom.toml`, byte for byte.
constexpr auto specExample = R"(format = 1
name = "Energy decay"
idea = """
Find the sharp decay rate for weak solutions of the model system with L^2 data.
"""
created = 2026-11-03

[tex]
preamble = ["TeX/preamble.tex", "TeX/macros.tex"]

[[tags]]
id = "model-system"
kind = "equation"
title = "The model system"

[[tags]]
id = "energy-estimates"
kind = "technique"
title = "Energy estimates"
)";

/// A project folder with the preamble files the example lists.
struct TempProject {
    QTemporaryDir temp;
    QString root;

    /// Writes the two preamble files, so a reading of the example has no `preamble-missing`.
    /// Side effects: creates the temporary folder and the files; fails the test if it can't.
    TempProject()
    {
        REQUIRE(temp.isValid());
        root = temp.path();
        writeFile(root + "/TeX/preamble.tex", "% preamble\n");
        writeFile(root + "/TeX/macros.tex", "% macros\n");
    }

    /// Reads `text` as this project's loom.toml.
    loom::ProjectTomlReading read(const QByteArray& text) const { return loom::readProjectToml(text, root); }
};

/// Each problem as (code, line, message), for comparing whole lists.
std::vector<std::tuple<QString, int, QString>> summary(const std::vector<loom::Problem>& problems)
{
    std::vector<std::tuple<QString, int, QString>> out;
    for (const loom::Problem& problem : problems) {
        CHECK(problem.file == "loom.toml");
        out.emplace_back(problem.code, problem.line, problem.message);
    }
    return out;
}

/// The one problem in `problems`, as (code, line, message); fails the test if there isn't exactly one.
std::tuple<QString, int, QString> only(const std::vector<loom::Problem>& problems)
{
    REQUIRE(problems.size() == 1);
    CHECK(problems[0].severity == loom::Severity::Error);
    return summary(problems).front();
}

/// The one problem found by a reading.
std::tuple<QString, int, QString> only(const loom::ProjectTomlReading& reading)
{
    return only(reading.problems);
}

/// A valid loom.toml with `extra` added at the end of the top-level fields.
QByteArray withTopLevel(const char* extra)
{
    return QByteArray("format = 1\nname = \"Demo\"\ncreated = 2026-11-03\n") + extra
         + "\n[tex]\npreamble = [\"TeX/preamble.tex\"]\n";
}

/// A valid loom.toml whose preamble list is `preamble`.
QByteArray withPreamble(const char* preamble)
{
    return QByteArray("format = 1\nname = \"Demo\"\ncreated = 2026-11-03\n\n[tex]\npreamble = ") + preamble + "\n";
}

/// A valid loom.toml followed by `tags`.
QByteArray withTags(const char* tags)
{
    return withPreamble("[\"TeX/preamble.tex\"]") + tags;
}

} // namespace

TEST_CASE("The spec's example loom.toml reads without problems", "[loom.toml]")
{
    const TempProject project;
    const loom::ProjectTomlReading reading = project.read(specExample);

    CHECK(reading.problems.empty());
    CHECK_FALSE(reading.formatTooNew);
    const loom::ProjectConfig& config = reading.config;
    CHECK(config.format == 1);
    CHECK(config.name == "Energy decay");
    CHECK(config.idea == "Find the sharp decay rate for weak solutions of the model system with L^2 data.\n");
    CHECK(config.created == QDate(2026, 11, 3));
    CHECK(config.preamble == QStringList{"TeX/preamble.tex", "TeX/macros.tex"});
    REQUIRE(config.tags.size() == 2);
    CHECK(config.tags[1].id == "energy-estimates");
    CHECK(config.tags[1].kind == "technique");
    CHECK(config.tags[1].title == "Energy estimates");
}

TEST_CASE("loom.toml is written in the layout of the spec's example", "[loom.toml]")
{
    const TempProject project;
    const loom::ProjectConfig config = project.read(specExample).config;
    CHECK(loom::formatProjectToml(config) == QByteArray(specExample));
}

TEST_CASE("Writing then reading loom.toml gives back the same fields", "[loom.toml]")
{
    const TempProject project;
    loom::ProjectConfig config;
    config.name = QString::fromUtf8("Décroissance \"rapide\" \\ C:\\path\tand a tab");
    config.idea = QString::fromUtf8("Line one with \"\"\" inside.\r\nLine two: ∫ |u|² ≤ C.\n");
    config.created = QDate(2026, 11, 3);
    config.preamble = {"TeX/preamble.tex"};
    config.tags = {{"a1", "other", "A \"quoted\" title"}};

    const QByteArray text = loom::formatProjectToml(config);
    CHECK_FALSE(text.contains('\r'));
    const loom::ProjectTomlReading reading = project.read(text);

    CHECK(reading.problems.empty());
    CHECK(reading.config.name == config.name);
    CHECK(reading.config.idea == QString::fromUtf8("Line one with \"\"\" inside.\nLine two: ∫ |u|² ≤ C.\n"));
    REQUIRE(reading.config.tags.size() == 1);
    CHECK(reading.config.tags[0].title == "A \"quoted\" title");
}

TEST_CASE("No idea, no idea field", "[loom.toml]")
{
    loom::ProjectConfig config;
    config.name = "Demo";
    config.created = QDate(2026, 11, 3);
    config.preamble = {"TeX/preamble.tex", "TeX/macros.tex"};
    CHECK(loom::formatProjectToml(config)
          == "format = 1\nname = \"Demo\"\ncreated = 2026-11-03\n\n[tex]\npreamble = [\"TeX/preamble.tex\", \"TeX/macros.tex\"]\n");
}

TEST_CASE("loom.toml problems: syntax and fields", "[loom.toml]")
{
    const TempProject project;
    using P = std::tuple<QString, int, QString>;

    SECTION("not TOML")
    {
        const auto [code, line, message] = only(project.read("format = 1\nname = \n"));
        CHECK(code == "toml-syntax");
        CHECK(line == 2);
        CHECK(message.startsWith("This isn't valid TOML: "));
        CHECK(message.endsWith("."));
        CHECK_FALSE(message.endsWith(".."));
    }

    SECTION("an unknown top-level field")
    {
        CHECK(only(project.read(withTopLevel("colour = \"blue\""))) ==
              P{"unknown-field", 4, "`colour` isn't a field of loom.toml. Is it misspelt? The fields are: format, name, idea, created, tex.preamble, tags."});
    }

    SECTION("an unknown field in [tex]")
    {
        CHECK(only(project.read(withPreamble("[\"TeX/preamble.tex\"]\nengine = \"lualatex\""))) ==
              P{"unknown-field", 7, "`tex.engine` isn't a field of loom.toml. Is it misspelt? The fields are: format, name, idea, created, tex.preamble, tags."});
    }

    SECTION("every required field missing")
    {
        CHECK(summary(project.read("idea = \"Something\"\n").problems) == std::vector<P>{
            {"missing-field", 1, "`format` is missing; every project needs one."},
            {"missing-field", 1, "`name` is missing; every project needs one."},
            {"missing-field", 1, "`created` is missing; every project needs one."},
            {"missing-field", 1, "`tex.preamble` is missing; every project needs one."},
        });
    }

    SECTION("[tex] without preamble")
    {
        CHECK(only(project.read("format = 1\nname = \"Demo\"\ncreated = 2026-11-03\n\n[tex]\n")) ==
              P{"missing-field", 5, "`tex.preamble` is missing; every project needs one."});
    }

    SECTION("wrong types")
    {
        CHECK(summary(project.read("format = \"1\"\nname = \"\"\ncreated = \"2026-11-03\"\nidea = 3\n\n[tex]\npreamble = \"TeX/preamble.tex\"\n").problems) ==
              std::vector<P>{
                  {"wrong-type", 1, "`format` should be a whole number, not a string."},
                  {"missing-field", 2, "`name` is empty; every project needs one."},
                  {"wrong-type", 3, "`created` should be a date, not a string."},
                  {"wrong-type", 4, "`idea` should be a string, not a whole number."},
                  {"wrong-type", 7, "`tex.preamble` should be a list of paths, not a string."},
              });
        CHECK(only(project.read(withPreamble("[\"TeX/preamble.tex\", 2]"))) ==
              P{"wrong-type", 6, "`tex.preamble` should be a list of paths, not a list holding a whole number."});
    }

    SECTION("a date and time is not a date")
    {
        CHECK(only(project.read(withTopLevel("").replace("created = 2026-11-03", "created = 2026-11-03T10:00:00-06:00"))) ==
              P{"wrong-type", 3, "`created` should be a date, not a date and time."});
    }

    SECTION("a date Qt can't hold, the year 0")
    {
        // toml++ reads it, QDate has no year 0: it must be reported, not dropped without a word.
        CHECK(only(project.read(withTopLevel("").replace("created = 2026-11-03", "created = 0000-11-03"))) ==
              P{"bad-value", 3, "`created` can't be `0000-11-03`; it must be one of the dates from 0001-01-01 on."});
    }

    SECTION("[tex] that isn't a table")
    {
        CHECK(only(project.read("format = 1\nname = \"Demo\"\ncreated = 2026-11-03\ntex = \"x\"\n")) ==
              P{"wrong-type", 4, "`tex` should be a table, not a string."});
    }
}

TEST_CASE("loom.toml problems: format", "[loom.toml]")
{
    const TempProject project;

    SECTION("a newer format stops the reading, whatever else is wrong")
    {
        const loom::ProjectTomlReading reading = project.read("format = 2\nunknown = true\n");
        CHECK(reading.formatTooNew);
        CHECK(only(reading.problems) ==
              std::tuple<QString, int, QString>{"format-too-new", 1, "This project was made by a newer Loom (format 2). Update Loom to open it."});
    }

    SECTION("format 0 isn't a format")
    {
        const loom::ProjectTomlReading reading = project.read(withTopLevel("").replace("format = 1", "format = 0"));
        CHECK_FALSE(reading.formatTooNew);
        CHECK(only(reading.problems) == std::tuple<QString, int, QString>{"bad-value", 1, "`format` can't be `0`; it must be one of `1`."});
    }
}

TEST_CASE("loom.toml problems: the preamble", "[loom.toml]")
{
    const TempProject project;
    using P = std::tuple<QString, int, QString>;

    CHECK(only(project.read(withPreamble("[\"TeX/missing.tex\"]"))) ==
          P{"preamble-missing", 6, "The preamble file `TeX/missing.tex` listed in loom.toml doesn't exist."});
    CHECK(only(project.read(withPreamble("[\"../outside.tex\"]"))) ==
          P{"bad-path", 6, "`../outside.tex` must be a path inside the project, written with `/`, without `..`."});
    CHECK(only(project.read(withPreamble("[\"TeX\\\\preamble.tex\"]"))) ==
          P{"bad-path", 6, "`TeX\\preamble.tex` must be a path inside the project, written with `/`, without `..`."});
    CHECK(only(project.read(withPreamble("[\"TeX/style.sty\"]"))) ==
          P{"bad-value", 6, "`tex.preamble` can't be `TeX/style.sty`; it must be one of the project's `.tex` files."});
    CHECK(only(project.read(withPreamble("[\"TeX\"]"))) ==
          P{"bad-value", 6, "`tex.preamble` can't be `TeX`; it must be one of the project's `.tex` files."});

    // A folder named like a .tex file isn't a preamble file.
    writeFile(project.root + "/Folder.tex/inner.tex", "x");
    CHECK(only(project.read(withPreamble("[\"Folder.tex\"]"))) ==
          P{"preamble-missing", 6, "The preamble file `Folder.tex` listed in loom.toml doesn't exist."});

    // An empty list is allowed: the project just has no preamble files.
    CHECK(project.read(withPreamble("[]")).problems.empty());
}

TEST_CASE("loom.toml problems: tags", "[loom.toml]")
{
    const TempProject project;
    using P = std::tuple<QString, int, QString>;

    SECTION("tag syntax")
    {
        CHECK(only(project.read(withTags("\n[[tags]]\nid = \"Energy--estimates\"\nkind = \"technique\"\ntitle = \"Energy\"\n"))) ==
              P{"tag-syntax", 9, "`Energy--estimates` isn't a valid tag name: use lowercase letters, digits and single hyphens, like `energy-estimates`."});
    }

    SECTION("a tag declared twice is reported at the second")
    {
        const loom::ProjectTomlReading reading = project.read(withTags(
            "\n[[tags]]\nid = \"energy\"\nkind = \"technique\"\ntitle = \"Energy\"\n"
            "\n[[tags]]\nid = \"energy\"\nkind = \"other\"\ntitle = \"Energy again\"\n"));
        CHECK(only(reading.problems) == P{"tag-duplicate", 14, "The tag `energy` is declared twice in loom.toml."});
        CHECK(reading.config.tags.size() == 1);
    }

    SECTION("a bad kind")
    {
        CHECK(only(project.read(withTags("\n[[tags]]\nid = \"energy\"\nkind = \"lemma\"\ntitle = \"Energy\"\n"))) ==
              P{"bad-value", 10, "`kind` can't be `lemma`; it must be one of `equation`, `technique`, `other`."});
    }

    SECTION("missing and unknown fields in a tag")
    {
        CHECK(summary(project.read(withTags("\n[[tags]]\nid = \"energy\"\ncolour = \"red\"\n")).problems) == std::vector<P>{
            {"missing-field", 8, "`kind` is missing; every tag needs one."},
            {"missing-field", 8, "`title` is missing; every tag needs one."},
            {"unknown-field", 10, "`colour` isn't a field of a tag. Is it misspelt? The fields are: id, kind, title."},
        });
    }

    SECTION("tags that aren't tables")
    {
        CHECK(only(project.read(withTopLevel("tags = [\"energy\"]"))) ==
              P{"wrong-type", 4, "`tags` should be a list of tables, not a list holding a string."});
        CHECK(only(project.read(withTopLevel("tags = 3"))) ==
              P{"wrong-type", 4, "`tags` should be a list of tables, not a whole number."});
    }

    SECTION("empty values are missing values")
    {
        // Specs/m1-core.md, "Front matter", A2.
        const loom::ProjectTomlReading reading = project.read(withTags("\n[[tags]]\nid = \"\"\nkind = \"\"\ntitle = \"\"\n"));
        CHECK(summary(reading.problems) == std::vector<P>{
            {"missing-field", 9, "`id` is empty; every tag needs one."},
            {"missing-field", 10, "`kind` is empty; every tag needs one."},
            {"missing-field", 11, "`title` is empty; every tag needs one."},
        });
        CHECK(reading.config.tags.empty());
        CHECK(only(project.read(withTopLevel("").replace("created = 2026-11-03", "created = \"\""))) ==
              P{"missing-field", 3, "`created` is empty; every project needs one."});
    }

    SECTION("a tag ending in a line break")
    {
        // Qt's `$` also matches before a final line break, so the rule must be anchored at the very end.
        CHECK(only(project.read(withTags("\n[[tags]]\nid = \"energy\\n\"\nkind = \"technique\"\ntitle = \"Energy\"\n"))) ==
              P{"tag-syntax", 9, "`energy\n` isn't a valid tag name: use lowercase letters, digits and single hyphens, like `energy-estimates`."});
    }
}

TEST_CASE("loom.toml that isn't UTF-8", "[loom.toml]")
{
    const TempProject project;
    CHECK(only(project.read("format = 1\nname = \"\xff\"\n")) ==
          std::tuple<QString, int, QString>{"not-utf8", 1, "This file isn't valid UTF-8 (the first bad byte is at offset 19). Save it as UTF-8."});
}

TEST_CASE("Opening a project", "[loom.toml]")
{
    const TempProject project;

    SECTION("a folder without loom.toml isn't a project")
    {
        const loom::OpenedProject opened = loom::openProject(project.root);
        CHECK(opened.status == loom::OpenStatus::NotAProject);
        CHECK(opened.message.endsWith("` isn't a Loom project: it has no loom.toml."));
    }

    SECTION("a good project opens with its fields")
    {
        writeFile(project.root + "/loom.toml", specExample);
        const loom::OpenedProject opened = loom::openProject(project.root);
        CHECK(opened.status == loom::OpenStatus::Opened);
        CHECK(opened.problems.empty());
        CHECK(opened.config.name == "Energy decay");
        CHECK(opened.root == project.root);
    }

    SECTION("problems are reported, and the project still opens")
    {
        writeFile(project.root + "/loom.toml", withTopLevel("nmae = \"Typo\""));
        const loom::OpenedProject opened = loom::openProject(project.root);
        CHECK(opened.status == loom::OpenStatus::Opened);
        REQUIRE(opened.problems.size() == 1);
        CHECK(opened.problems[0].code == "unknown-field");
    }

    SECTION("a newer format stops")
    {
        writeFile(project.root + "/loom.toml", "format = 7\n");
        const loom::OpenedProject opened = loom::openProject(project.root);
        CHECK(opened.status == loom::OpenStatus::FormatTooNew);
        CHECK(opened.message == "This project was made by a newer Loom (format 7). Update Loom to open it.");
    }

    SECTION("a loom.toml that is a folder can't be read")
    {
        REQUIRE(QDir().mkpath(project.root + "/loom.toml"));
        const loom::OpenedProject opened = loom::openProject(project.root);
        CHECK(opened.status == loom::OpenStatus::Unreadable);
        CHECK_FALSE(opened.message.isEmpty());
    }
}

TEST_CASE("A CRLF loom.toml reads as an LF one", "[loom.toml]")
{
    const TempProject project;
    QByteArray text = specExample;
    text.replace("\n", "\r\n");
    const loom::ProjectTomlReading reading = project.read(text);
    CHECK(reading.problems.empty());
    CHECK(reading.config.idea == "Find the sharp decay rate for weak solutions of the model system with L^2 data.\n");
    REQUIRE(reading.config.tags.size() == 2);
    CHECK(reading.config.tags[1].title == "Energy estimates");
    // And a problem in it is on the line a person counts.
    text.replace("kind = \"technique\"", "kind = \"lemma\"");
    CHECK(only(project.read(text)) ==
          std::tuple<QString, int, QString>{"bad-value", 18, "`kind` can't be `lemma`; it must be one of `equation`, `technique`, `other`."});
}

TEST_CASE("A preamble file through a link that leaves the project is a bad path", "[loom.toml][paths]")
{
    QTemporaryDir temp;
    REQUIRE(temp.isValid());
    const QString root = temp.path() + "/Project";
    writeFile(temp.path() + "/Outside/preamble.tex", "% outside\n");
    REQUIRE(QDir().mkpath(root));
    if (!loomtest::makeDirectoryLink(temp.path() + "/Outside", root + "/TeX"))
        SKIP("This system can make neither a symbolic link nor a junction.");
    CHECK(only(loom::readProjectToml(withPreamble("[\"TeX/preamble.tex\"]"), root)) ==
          std::tuple<QString, int, QString>{"bad-path", 6,
              "`TeX/preamble.tex` must be a path inside the project, written with `/`, without `..`."});
}

TEST_CASE("The TOML writer escapes every control character and writes any offset", "[toml]")
{
    // Specs/m1-core.md, "Front matter", canonical form: "escaping `\"`, `\\` and control characters".
    CHECK(loom::tomlBasicString(QString::fromUtf8("a\x01" "b\x1F" "c\x7F" "d\b" "e\f" "f\tg")) ==
          "\"a\\u0001b\\u001Fc\\u007Fd\\be\\ff\\tg\"");
    // D1: the date's own offset, to the second; `Z` for UTC.
    const QDate day(2026, 11, 4);
    const QTime time(15, 32, 10, 250);
    CHECK(loom::tomlOffsetDateTime(QDateTime(day, time, QTimeZone::fromSecondsAheadOfUtc(0))) == "2026-11-04T15:32:10Z");
    CHECK(loom::tomlOffsetDateTime(QDateTime(day, time, QTimeZone::fromSecondsAheadOfUtc(19800))) == "2026-11-04T15:32:10+05:30");
    CHECK(loom::tomlOffsetDateTime(QDateTime(day, time, QTimeZone::fromSecondsAheadOfUtc(-21600))) == "2026-11-04T15:32:10-06:00");
    // A key with anything but ASCII letters, digits, `_` and `-` is quoted.
    CHECK(loom::tomlKey("arxiv-v1") == "arxiv-v1");
    CHECK(loom::tomlKey("ed 2") == "\"ed 2\"");
    CHECK(loom::tomlKey(QString::fromUtf8("é")) == QString::fromUtf8("\"é\""));
}
