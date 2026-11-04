#pragma once

// Loom's own TOML writer, value by value. toml++ only parses: its formatter writes keys in
// alphabetical order (Wiki/gotchas/toml/tomlplusplus-sorts-keys.md), and Loom writes fields in
// the order its specs list them.

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>

namespace loom {

/// Writes a TOML basic string (Specs/m1-core.md, "Front matter", canonical form: "strings as basic
/// strings, escaping `"`, `\` and control characters, so no string ever spans lines").
/// @param text The string's value.
/// @returns The TOML text, quotes included, e.g. `"Energy \"decay\""`. No side effects.
QString tomlBasicString(const QString& text);

/// Writes a TOML multi-line basic string, as `loom.toml` writes `idea` (Specs/m1-core.md,
/// "`loom.toml`"): `"""`, a line break, the text with a final line break added if it has none,
/// then `"""`. CRLF becomes LF. `"`, `\` and control characters other than tab and LF are escaped,
/// so the text can't close the string early.
/// @param text The string's value.
/// @returns The TOML text, from the opening to the closing `"""`. No side effects.
QString tomlMultilineString(const QString& text);

/// Writes a TOML multi-line basic string that reads back as exactly `text`, as check histories
/// write a `changes` message with line breaks (Specs/m1-core.md, "Checks"): `"""`, a line break
/// (which TOML drops), the text, then `"""`. Unlike tomlMultilineString() nothing is added or
/// changed: a CR is escaped rather than dropped, and no final line break is added, so a value
/// read from a file is written back unchanged.
/// @param text The string's value.
/// @returns The TOML text, from the opening to the closing `"""`. No side effects.
QString tomlMultilineStringAsIs(const QString& text);

/// Writes a TOML offset date-time to the second, as check histories date their entries
/// (Specs/m1-core.md, D1): `2026-11-04T15:32:10-06:00`, or `…Z` when the offset is zero.
/// @param dateTime A valid date-time; its offset from UTC is written, its milliseconds aren't.
/// @returns The TOML text. No side effects.
QString tomlOffsetDateTime(const QDateTime& dateTime);

/// Writes a TOML key: bare if it is only ASCII letters, digits, `_` and `-` (TOML 1.0, "Keys"),
/// else as a basic string. So a version ID such as `arxiv-v1` is written as it reads.
/// @returns The TOML text. No side effects.
QString tomlKey(const QString& key);

/// Writes a TOML local date, e.g. `2026-11-03`.
/// @param date A valid date.
/// @returns The TOML text. No side effects.
QString tomlDate(const QDate& date);

/// Writes an inline TOML array of basic strings, e.g. `["TeX/preamble.tex", "TeX/macros.tex"]`.
/// @param items The strings, in order.
/// @returns The TOML text; `[]` for no items. No side effects.
QString tomlStringArray(const QStringList& items);

} // namespace loom
