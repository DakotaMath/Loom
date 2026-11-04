#pragma once

// Reading a Loom file's fields one by one, recording where each is and whether anything had to
// be dropped. Shared by the readers of statements, proofs, stubs and paper entries. Internal to
// LoomCore: it includes toml++, which only LoomCore links.

#include "LoomCore/StatementsAndProofs.h"
#include "LoomCore/TomlFields.h"

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QStringList>

#include <initializer_list>
#include <optional>
#include <string_view>

namespace loom {

/// `a`, `b` → "`a`, `b`", for `bad-value`'s list of allowed values. No side effects.
QString quotedList(const QStringList& values);

/// Parses a plain TOML file (not front matter): it must be UTF-8 (`not-utf8`) and TOML 1.0
/// (`toml-syntax`).
/// @param bytes   The file's content.
/// @param checker Where the problems go; its `file` names the file.
/// @returns The table, or nothing after recording the problem. No other side effects.
std::optional<toml::table> parsePlainToml(const QByteArray& bytes, FieldChecker& checker);

/// Reads one table's fields, recording their lines and whether anything was dropped.
struct FieldReader {
    FieldChecker checker;
    FieldLines lines;
    /// Set when a field or element is dropped (unknown, or of the wrong type).
    bool lost = false;
    /// What the document is, for `missing-field`: `statement`, `proof`, `stub`, …
    QString thing;

    /// @param path       The file's project-relative path, for the problems.
    /// @param what       What the document is, e.g. `statement`.
    /// @param lineOffset Added to toml++'s lines: 1 for front matter, 0 for a plain TOML file.
    FieldReader(const QString& path, QString what, int lineOffset);

    /// Reports every key of `table` that isn't in `known` as `unknown-field`.
    void unknownFields(const toml::table& table, std::initializer_list<std::string_view> known, QStringView fieldList);

    /// Reads a string field. A missing required field is reported at `missingLine` (by default
    /// line 1: the opening `+++` of front matter, or the top of a plain file). An empty string
    /// where `need` doesn't allow one is `missing-field` (FieldChecker::reportEmpty()).
    /// @returns The value; nothing if absent, not a string (which sets `lost`), or empty where
    ///          it mustn't be (which doesn't: an empty value loses nothing when left out).
    std::optional<QString> string(const toml::table& table, const char* key, Need need, int missingLine = 1);

    /// Reads a list of strings, reporting elements that aren't strings (and dropping them).
    /// @param expected What the list should be, e.g. `a list of tags`.
    std::optional<QStringList> list(const toml::table& table, const char* key, const QString& expected, bool required);

    /// Reads a date field (a local date, `2026-11-03`). An empty string is `missing-field`, as
    /// for a string field that names something. A date QDate can't hold (the year 0) is
    /// `bad-value` and sets `lost`, so the file isn't rewritten without it.
    /// @returns The date; an invalid QDate if absent, empty, not a date, or the year 0.
    QDate date(const toml::table& table, const char* key, bool required, int missingLine = 1);

    /// Reads a whole-number field.
    std::optional<qint64> integer(const toml::table& table, const char* key, bool required, int missingLine = 1);

    /// Reports a value that isn't one of `allowed`.
    void badValue(const QString& field, const QString& value, const QString& allowed);

    /// Reports each entry a list holds more than once. The spec gives no code for this outside
    /// `premises`; it is `bad-value`, at the repeat's line.
    void repeats(const QString& field, const QStringList& values);

    /// Reports a tag that isn't written as files must write it (`id-syntax`), suggesting the
    /// tag it would be on the command line.
    void tagSyntax(const QString& value, int line);

    /// Reports each alias that can't be one (`alias-invalid`).
    void aliases(const QStringList& aliases);
};

} // namespace loom
