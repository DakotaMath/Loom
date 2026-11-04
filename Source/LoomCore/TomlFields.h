#pragma once

// Reading fields from parsed TOML and reporting what's wrong with them, with the messages of
// Specs/m1-core.md, "Validation" (`unknown-field`, `missing-field`, `wrong-type`). Shared by
// every reader of a Loom file. Internal to LoomCore: it includes toml++, which only LoomCore links.

#include "LoomCore/Problem.h"

#include <QString>

#include <toml++/toml.hpp>

#include <initializer_list>
#include <optional>
#include <string_view>
#include <vector>

namespace loom {

/// Converts toml++'s UTF-8 text to a QString.
QString fromUtf8(std::string_view text);

/// The line a value or table starts on within the TOML text; 1 if toml++ doesn't know
/// (Specs/m1-core.md, "Validation": "the line of the offending value, else of its table header, else 1").
int lineOf(const toml::source_region& region);

/// What a TOML value is, for the `wrong-type` message ("…, not {what it is}"), e.g. `a whole number`.
QString describeToml(const toml::node& node);

/// Whether a string field may be absent or empty. An empty value counts as a missing one
/// (Specs/m1-core.md, "Front matter", A2: "Empty values are missing values"), except in free text.
enum class Need {
    /// Free text: may be absent, and may be empty (a version's `differences`, an author's `first`).
    Optional,
    /// Optional, but it names something (an ID, a tag, a key, a path): absent, or not empty.
    Naming,
    /// Must be present and not empty.
    Required,
};

/// Collects the problems found in one file's TOML, with the spec's messages.
struct FieldChecker {
    /// The project-relative path every problem names, e.g. `loom.toml`.
    QString file;
    /// Added to toml++'s line numbers to make file lines: 1 for front matter, whose TOML starts
    /// on the file's second line (Specs/m1-core.md, "Front matter"), 0 for a plain TOML file.
    int lineOffset = 0;
    /// The problems so far, in the order found.
    std::vector<Problem> problems;

    /// The file line a value or table starts on.
    int line(const toml::node& node) const { return lineOf(node.source()) + lineOffset; }
    /// The file line a key is on.
    int line(const toml::key& key) const { return lineOf(key.source()) + lineOffset; }

    /// Records a problem. `line` is a file line.
    void add(const char* code, int line, const QString& message, Severity severity = Severity::Error);

    /// Reports every key of `table` that isn't in `known` as `unknown-field`.
    /// @param prefix    Put before the key in the message, e.g. `tex.`.
    /// @param thing     What the table is, e.g. `loom.toml` or `a statement`.
    /// @param fieldList The fields to list in the message, comma-separated.
    void unknownFields(const toml::table& table, std::initializer_list<std::string_view> known,
                       const QString& prefix, const QString& thing, QStringView fieldList);

    /// Reports a missing required field at `line` (a file line), e.g. "every statement needs one".
    void missingField(const QString& field, int line, const QString& thing);

    /// Reports a field that is present but empty, as `missing-field` (Specs/m1-core.md,
    /// "Front matter", A2: "`{field}` is empty"). The spec's wording, "every {thing} needs one",
    /// is for a required field; an optional one says it can be left out instead.
    /// @param line     The file line of the empty value.
    /// @param thing    What the table is, e.g. `statement`.
    /// @param required Whether every `thing` needs the field.
    void emptyField(const QString& field, int line, const QString& thing, bool required);

    /// Reports `node` if it is an empty string where `need` doesn't allow one (emptyField()).
    /// The one rule for empty values that every reader of a string field goes through.
    /// @returns True if it was reported, so the caller treats the field as having no value.
    bool reportEmpty(const QString& field, const toml::node& node, const QString& thing, Need need);

    /// Reports a value of the wrong type at the value's line.
    /// @param expected What it should be, e.g. `a date`.
    void wrongType(const QString& field, const QString& expected, const toml::node& node);

    /// Reads a string field of `table`, reporting it if it is missing, not a string, or empty
    /// when it mustn't be (reportEmpty()).
    /// @param tableLine The file line to name if the field is missing.
    /// @returns The value, or nothing if absent, wrong or empty when it mustn't be.
    std::optional<QString> stringField(const toml::table& table, std::string_view key, const QString& field,
                                       int tableLine, const QString& thing, Need need);
};

} // namespace loom
