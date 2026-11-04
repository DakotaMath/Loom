#pragma once

// Paper entries and their stubs (Specs/m1-core.md, "Papers"): `Papers/<Key>/paper.toml` with its
// work, versions, readings and renumberings, and `Papers/<Key>/Statements/<tag>.toml`, a paper's
// result as a node of the project's graph. Reading them with the problems each file shows on its
// own, and writing them in canonical form.

#include "LoomCore/Problem.h"
#include "LoomCore/StatementsAndProofs.h"

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QStringList>

#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace loom {

/// The work types, in the spec's order (Specs/m1-core.md, "Papers", `[work]`).
inline const QStringList workTypes = {
    QStringLiteral("article"), QStringLiteral("book"), QStringLiteral("incollection"), QStringLiteral("inproceedings"),
    QStringLiteral("phdthesis"), QStringLiteral("mastersthesis"), QStringLiteral("techreport"), QStringLiteral("talk"),
    QStringLiteral("lecture-notes"), QStringLiteral("other")};

/// The reading levels, lowest first (Specs/m1-core.md, "Papers", `[[reading]]`).
inline const QStringList readingLevels = {QStringLiteral("skimmed"), QStringLiteral("read"), QStringLiteral("studied")};

/// One author or editor: `last` includes any particle as printed (`van der Berg`); `first` may be empty.
struct PersonName {
    QString last;
    QString first;
    bool operator==(const PersonName&) const = default;
};

/// A bibliographic value: a string, a whole number (`year`, `arxiv_version`, `page_count`), a
/// date (`posted`, `date`) or a list of names (`authors`, `editors`).
using FieldValue = std::variant<QString, qint64, QDate, std::vector<PersonName>>;

/// A bibliographic field, `{ value = …, source = "…", verified = … }` (Specs/m1-core.md, "Papers",
/// "Fields"): the value, where it came from, and the date Dakota verified it.
struct PaperField {
    FieldValue value;
    /// An `https://` URL, or a path inside the paper's `local/`.
    QString source;
    /// The date of Dakota's click; invalid means `false`. Agents always write `false`.
    QDate verified;
    /// The file line of the field.
    int line = 1;

    /// The value as text, for BibTeX and messages: strings as they are, numbers in decimal,
    /// dates as `2026-11-03`, names as `Last, First` joined by ` and ` (a name with an empty
    /// `first` as `Last`). BibTeX writes names its own way: see bibEntry(). No side effects.
    QString text() const;
    /// The value's list of names, or an empty list if it isn't one. No side effects.
    std::vector<PersonName> names() const;
};

/// A field of a version, by name.
struct NamedField {
    QString name;
    PaperField field;
};

/// One `[[versions]]` entry: an arXiv version (`arxiv-v<N>`), the published version
/// (`published`) or a book's later edition (`ed<N>`).
struct PaperVersion {
    QString id;
    QString kind;              ///< `arxiv` or `published`.
    /// Its bibliographic fields, in canonical order once written.
    std::vector<NamedField> fields;
    QString pdf;               ///< Optional: a path inside `local/`.
    QString differences;       ///< Optional: how it differs from the other versions, in our words.
    int line = 1;              ///< The line of its `[[versions]]` header.

    /// The field called `name`, or null. No side effects.
    const PaperField* field(QStringView name) const;
    /// The value of the field called `name` as text, or an empty string. No side effects.
    QString text(QStringView name) const;
    /// The arXiv version number of an arXiv version: its `arxiv_version`, else the N of its id
    /// `arxiv-v<N>`; 0 when it has neither. The one rule for which arXiv version is newest, for
    /// `cite`'s default, the key's year and the bibliography's `eprint`. No side effects.
    qint64 arxivNumber() const;
};

/// `[work]`: what is true of the paper whichever version. Absent fields are empty optionals.
struct PaperWork {
    std::optional<PaperField> type;
    std::optional<PaperField> title;
    std::optional<PaperField> authors;
    std::optional<PaperField> editors;
    int line = 1;              ///< The line of the `[work]` header.

    /// The type's value, or an empty string. No side effects.
    QString typeName() const;
};

/// One `[[reading]]` entry: what was read, at which level, of which version.
struct ReadingEntry {
    QString version;
    QString level;             ///< `skimmed`, `read` or `studied`.
    QString what;
    QDate date;
    QString session;           ///< Optional: `Sessions/<YYYY-MM-DD>`.
    int line = 1;              ///< The line of its `[[reading]]` header.
    /// The line of each field, for the problems found after reading (`version-unknown`,
    /// `session-missing`).
    FieldLines lines;
};

/// One `[[renumbering]]` entry: an item's label in two or more versions.
struct Renumbering {
    /// Version ID to the item's label in that version, in file order.
    std::vector<std::pair<QString, QString>> labels;
    QDate checked;             ///< Invalid means `false`.
    int line = 1;              ///< The line of its `[[renumbering]]` header.
    QList<int> labelLines;     ///< The line of each label.
};

/// A paper entry: `Papers/<Key>/paper.toml`. Fields in the spec's order; an empty string, an
/// empty list, 0 or an invalid date means the field is absent.
struct Paper {
    QString key;
    QStringList aliases;       ///< Old keys.
    QString cite;              ///< The `id` of the version the bibliography uses.
    qint64 relevance = 0;      ///< At least 1; 1 is most relevant.
    QString relevanceWhy;      ///< `relevance_why`
    QStringList tags;
    QString summary;
    QDate created;
    QDate verified;
    PaperWork work;
    std::vector<PaperVersion> versions;
    std::vector<ReadingEntry> readings;
    std::vector<Renumbering> renumberings;

    /// The version called `id`, or null. No side effects.
    const PaperVersion* version(const QString& id) const;
    /// The authors, or the editors when there are no authors (Specs/m1-core.md, "The key rule":
    /// "built from `[work]`'s authors (editors when there are none)"). No side effects.
    std::vector<PersonName> keyNames() const;
    /// The highest `level` over all readings, or an empty string if it has none (Specs/m1-core.md,
    /// "Papers", `[[reading]]`). No side effects.
    QString readingLevel() const;
};

/// A paper's result where it was found in one version: `<version-id> = { at = "…",
/// checked_against_pdf = <date or false> }`.
struct Locator {
    QString version;
    QString at;
    /// The date of Dakota's click checking it against the version's PDF; invalid means `false`.
    QDate checkedAgainstPdf;
    int line = 1;
};

/// A paper stub: `Papers/<Key>/Statements/<tag>.toml`. Fields in the spec's order.
struct Stub {
    QString id;
    QString kind;
    QString title;             ///< Optional; without one, Loom shows the gist.
    QString gist;              ///< The result in our words, never the paper's text.
    QStringList statedUsing;   ///< `stated_using`
    QStringList paperPremises; ///< `paper_premises`: stubs of the same paper. Not used for status.
    QStringList aliases;
    QString extractedFrom;     ///< `extracted_from`: a version `id` of the paper.
    QString extractedBy;       ///< `extracted_by`: `dakota` or `agent:<model>`.
    QDate created;
    /// `[locators]`, in file order.
    std::vector<Locator> locators;

    /// The locator for `version`, or null. No side effects.
    const Locator* locator(const QString& version) const;
};

using PaperReading = DocumentReading<Paper>;
using StubReading = DocumentReading<Stub>;

/// Whether a stub is **cited** rather than **extracted** (Specs/m1-core.md, "Papers", and status
/// rules 2 and 3): its locator for `extracted_from` has `checked_against_pdf` set to a date.
/// No side effects.
bool isCited(const Stub& stub);

/// Reads `paper.toml`.
/// Rules (Specs/m1-core.md, "Papers"): unknown fields are errors at every level; every
/// bibliographic field is `{ value, source, verified }` (`field-shape`, `field-source`,
/// `field-verified`) with a value of its field's type; version IDs follow their kind
/// (`version-id`, `version-duplicate`, `arxiv-version-mismatch`); each version holds only the
/// fields of its kind and the work's type (`field-not-for-type`) and lacks none it expects
/// (`field-expected`, a warning); a string field's value isn't empty (`missing-field`, A2); an
/// arXiv version's `arxiv_id` is an identifier without `vN` and its `primary_class` a category
/// (`bad-value`, A4); `cite`, readings and renumberings name existing versions (`cite-unknown`,
/// `version-unknown`); `pdf` and local sources are inside `local/` (`pdf-path`), other sources
/// are `https://` links (`field-source`); a date is one QDate can hold (`bad-value`); there are
/// authors, or for a book editors (`authors-missing`); no old key is the key, or another old key,
/// in another case (`alias-duplicate`, A3), and none is listed twice (`bad-value`).
/// Problems that need the rest of the project (the folder name, other papers' keys, tags,
/// sessions on disk) are found by checkContents().
/// @param bytes       The file's content.
/// @param path        Its project-relative path, `Papers/<Key>/paper.toml`; for a draft, its path
///                    as typed.
/// @param keyRequired False for a draft given to `loom new-paper`, which may leave `key` out.
/// @returns The fields and problems; `document.key` is used in messages, or when the key is
///          missing the folder's name (a draft's file name, since its folder says nothing about
///          the paper). No side effects.
PaperReading readPaper(const QByteArray& bytes, const QString& path, bool keyRequired = true);

/// Reads a stub.
/// Rules: the fields of the spec's table; `id` a tag equal to the file name; `kind` a statement
/// kind; `gist` not empty (`stub-gist-empty`); `extracted_by` `dakota` or `agent:<model>`;
/// `[locators]` holding one `{ at, checked_against_pdf }` per version and one for
/// `extracted_from` (`stub-locator-missing`); dates QDate can hold (`bad-value`, which makes the
/// reading lossy). Whether the versions exist is checked by
/// checkContents(), which knows the paper.
/// @param bytes The file's content.
/// @param path  Its project-relative path, `Papers/<Key>/Statements/<name>.toml`.
/// @returns The fields and problems. No side effects.
StubReading readStub(const QByteArray& bytes, const QString& path);

/// The canonical order of a version's fields (Specs/m1-core.md, "Papers", the tables of
/// `[[versions]]`): for an arXiv version `arxiv_id`, `arxiv_version`, `posted`, `primary_class`,
/// `page_count`; for a published version the expected fields of the work's type, then its
/// optional ones; then `mr`, `zbl`, `url` and `page_count`, each once.
/// @param kind     `arxiv` or `published`.
/// @param workType The work's type; unknown types allow only the fields every version may have.
/// @returns The field names. No side effects.
QStringList versionFieldOrder(const QString& kind, const QString& workType);

/// The fields a version is expected to have: a missing one is a `field-expected` warning.
/// @returns The field names, in canonical order. No side effects.
QStringList expectedVersionFields(const QString& kind, const QString& workType);

/// Writes `paper.toml` in canonical form, in the layout of the design's example: the top-level
/// fields in the spec's order, then `[work]`, one `[[versions]]` table per version (`id`, `kind`,
/// its fields in versionFieldOrder(), `pdf`, `differences`), `[[reading]]` and `[[renumbering]]`
/// tables. Optional fields are left out when empty; strings with line breaks are written as
/// multi-line strings, others as basic strings; a field is `{ value = …, source = "…",
/// verified = <date or false> }` on one line.
/// @returns The file's bytes, UTF-8 with LF line endings. No side effects.
QByteArray formatPaper(const Paper& paper);

/// Writes a stub in canonical form: the fields in the spec's order, then `[locators]` with one
/// line per locator, in the order of `stub.locators`.
/// @returns The file's bytes. No side effects.
QByteArray formatStub(const Stub& stub);

} // namespace loom
