#pragma once

// Paper entries for the tests of papers, stubs, keys and the bibliography: the design's example
// entry in canonical form, and helpers that build entries in memory or as TOML.

#include "LoomCore/Papers.h"

#include <QByteArray>
#include <QDate>
#include <QString>

#include <optional>
#include <utility>
#include <vector>

namespace loomtest {

/// A field, verified on 2026-11-03 unless `verified` is false.
inline loom::PaperField field(loom::FieldValue value, bool verified = true)
{
    return {std::move(value), QStringLiteral("https://example.org/source"), verified ? QDate(2026, 11, 3) : QDate(), 1};
}

/// A paper in memory: an article by `surnames` (first names `A`, `B`, …), with a published
/// version of `year` if given and an arXiv version `arxiv-v1` posted on `posted` if given.
inline loom::Paper paperBy(const QStringList& surnames, std::optional<int> year, std::optional<QDate> posted = std::nullopt,
                           const QString& title = QStringLiteral("A title"))
{
    loom::Paper paper;
    std::vector<loom::PersonName> names;
    for (qsizetype i = 0; i < surnames.size(); ++i)
        names.push_back({surnames[i], QString(QChar(char16_t(u'A' + i)))});
    paper.work.type = field(QStringLiteral("article"));
    paper.work.title = field(title);
    paper.work.authors = field(names);
    if (posted) {
        loom::PaperVersion arxiv;
        arxiv.id = QStringLiteral("arxiv-v1");
        arxiv.kind = QStringLiteral("arxiv");
        arxiv.fields.push_back({QStringLiteral("arxiv_version"), field(qint64(1))});
        arxiv.fields.push_back({QStringLiteral("posted"), field(*posted)});
        paper.versions.push_back(arxiv);
    }
    if (year) {
        loom::PaperVersion published;
        published.id = QStringLiteral("published");
        published.kind = QStringLiteral("published");
        published.fields.push_back({QStringLiteral("year"), field(qint64(*year))});
        paper.versions.push_back(published);
    }
    return paper;
}

/// A source and date for TOML fields: `source = "https://doi.org/10.0000/jea.2024.17", verified = 2026-11-03`.
inline constexpr char doi[] = "source = \"https://doi.org/10.0000/jea.2024.17\", verified = 2026-11-03";

/// The design's example paper entry (Specs/loom.md, "Papers"), in canonical form, as
/// formatPaper() writes it. Its tags are the ProjectFixture's.
inline QByteArray smithJones24()
{
    const QByteArray d = doi;
    const QByteArray a = "source = \"https://arxiv.org/abs/2401.99999v1\", verified = 2026-11-03";
    return "key = \"SmithJones24\"\n"
           "aliases = [\"SmithJones23\"]\n"
           "cite = \"published\"\n"
           "relevance = 1\n"
           "relevance_why = \"Proves the estimate the project wants to extend\"\n"
           "tags = [\"model-system\"]\n"
           "summary = \"Proves energy decay for weak solutions of the model system with L^2 data.\"\n"
           "created = 2026-10-18\n"
           "verified = 2026-11-03\n"
           "\n[work]\n"
           "type = { value = \"article\", " + d + " }\n"
           "title = { value = \"Energy decay for a model parabolic system\", " + d + " }\n"
           "authors = { value = [{ last = \"Smith\", first = \"Alice\" }, { last = \"Jones\", first = \"Bob\" }], " + d + " }\n"
           "\n[[versions]]\n"
           "id = \"arxiv-v1\"\n"
           "kind = \"arxiv\"\n"
           "arxiv_id = { value = \"2401.99999\", " + a + " }\n"
           "arxiv_version = { value = 1, " + a + " }\n"
           "posted = { value = 2023-01-15, " + a + " }\n"
           "primary_class = { value = \"math.AP\", " + a + " }\n"
           "page_count = { value = 18, source = \"local/arxiv-v1.pdf\", verified = false }\n"
           "pdf = \"local/arxiv-v1.pdf\"\n"
           "\n[[versions]]\n"
           "id = \"published\"\n"
           "kind = \"published\"\n"
           "journal = { value = \"Journal of Example Analysis\", " + d + " }\n"
           "volume = { value = \"12\", " + d + " }\n"
           "number = { value = \"3\", " + d + " }\n"
           "pages = { value = \"101--130\", " + d + " }\n"
           "year = { value = 2024, " + d + " }\n"
           "doi = { value = \"10.0000/jea.2024.17\", " + d + " }\n"
           "mr = { value = \"MR0000001\", " + d + " }\n"
           "pdf = \"local/published.pdf\"\n"
           "differences = \"Equations renumbered; adds Remark 2.5\"\n"
           "\n[[reading]]\n"
           "version = \"arxiv-v1\"\n"
           "level = \"studied\"\n"
           "what = \"§§1–3, every proof line by line\"\n"
           "date = 2026-10-18\n"
           "session = \"Sessions/2026-10-18\"\n"
           "\n[[renumbering]]\n"
           "arxiv-v1 = \"(2.18)\"\n"
           "published = \"(2.8)\"\n"
           "checked = false\n";
}

/// A stub of SmithJones24, as formatStub() writes it, extracted from `arxiv-v1`.
/// @param checked Whether its `arxiv-v1` locator is checked against the PDF.
/// @param extra   Lines added after `gist`.
inline QByteArray stubToml(const QString& tag, const QByteArray& kind, bool checked, const QByteArray& extra = {})
{
    return "id = \"" + tag.toUtf8() + "\"\nkind = \"" + kind + "\"\ngist = \"Weak solutions lose energy\"\n" + extra
         + "extracted_from = \"arxiv-v1\"\nextracted_by = \"agent:claude-opus-5-5\"\ncreated = 2026-10-18\n\n[locators]\n"
           "arxiv-v1 = { at = \"Theorem 1.2, p. 4\", checked_against_pdf = " + (checked ? "2026-10-18" : "false") + " }\n";
}

} // namespace loomtest
