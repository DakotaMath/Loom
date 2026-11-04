#pragma once

// The `+++` TOML front matter every statement, proof and verbose proof starts with
// (Specs/m1-core.md, "Front matter"): finding it, and putting a file back together around a body
// kept byte for byte.

#include "LoomCore/Problem.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

#include <optional>
#include <vector>

namespace loom {

/// A file split into its front matter and its body.
struct FrontMatterSplit {
    /// True if the file is UTF-8 and has front matter between two `+++` lines; then `toml` and
    /// `body` are meaningful.
    bool found = false;
    /// The TOML between the `+++` lines, with LF line endings. Its first line is the file's line 2.
    QByteArray toml;
    /// Everything after the closing `+++` line's line ending, byte for byte apart from CRLF
    /// becoming LF.
    QByteArray body;
    /// The file line the body starts on (the line after the closing `+++`).
    int bodyLine = 0;
    /// `not-utf8`, `bom` (a warning), `front-matter-missing` or `front-matter-unclosed`.
    std::vector<Problem> problems;
};

/// Finds the front matter (Specs/m1-core.md, "Front matter").
/// Rules: the file must be UTF-8; a byte-order mark is skipped, with a warning; every CRLF counts
/// as LF; the first line must be exactly `+++`; the front matter ends at the next line that is
/// exactly `+++`, so a `+++` line further down (in a verbatim block, say) is body text.
/// @param bytes The file's content.
/// @param path  The file's project-relative path, for the problems.
/// @returns The parts and any problems. No side effects.
FrontMatterSplit splitFrontMatter(const QByteArray& bytes, const QString& path);

/// Puts a file together: `+++`, the TOML, `+++`, then the body as is.
/// @param toml The front matter in canonical form, each line ending in LF.
/// @param body The body bytes, written untouched.
/// @returns The file's bytes. No side effects.
QByteArray joinFrontMatter(const QByteArray& toml, const QByteArray& body);

/// Where UTF-8 text first goes wrong: a byte that can't start or continue a character, an
/// overlong form, a surrogate, a code point above U+10FFFF, or a character cut off at the end.
/// @returns The offset of the first bad byte, or nothing if `bytes` is valid UTF-8. No side effects.
std::optional<qsizetype> firstInvalidUtf8(QByteArrayView bytes);

/// Turns every CRLF into LF; a lone CR stays. No side effects.
QByteArray crlfToLf(QByteArray bytes);

/// Whether a body counts as empty: nothing in it but whitespace (Specs/m1-core.md, "Statements").
/// No side effects.
bool isBlankBody(const QByteArray& body);

} // namespace loom
