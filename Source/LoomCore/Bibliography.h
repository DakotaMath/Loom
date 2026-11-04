#pragma once

// The bibliography, `refs.bib`, generated from the paper entries (Specs/m1-core.md, "Papers",
// "The bibliography"): one BibTeX entry per paper, for the version in `cite`.

#include "LoomCore/Papers.h"
#include "LoomCore/Problem.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <vector>

namespace loom {

/// One paper's BibTeX entry.
struct BibEntry {
    /// The entry, from `@type{Key,` to the closing `}` and its line break; empty when skipped.
    QString text;
    /// The paper.toml fields the entry used whose `verified` is `false`, in the order used: the
    /// cited version's and `[work]`'s by name (`journal`, `title`), another version's as
    /// `<version>.<name>` (`arxiv-v2.arxiv_id`).
    QStringList unverified;
    /// When the entry is skipped (a value with unbalanced braces, or `cite` naming no version), why.
    QString error;
};

/// Writes one paper's entry (Specs/m1-core.md, "The bibliography").
/// Rules: the entry type and fields come from the work's type and the cited version's kind, in
/// the spec's order, two-space indented, `field = {value},`, and only when they have a value.
/// Authors and editors are `Last, First` joined by ` and `. A published version adds the newest
/// arXiv version's identifier without its version as `eprint`, with `archiveprefix`,
/// `primaryclass` and `note = {arXiv:…}`; an arXiv-only paper is `@misc` with the identifier
/// with its version. In values, `&`, `%` and `#` that aren't already escaped and aren't inside
/// `$…$` are escaped with `\`; a value with unbalanced braces skips the entry.
/// @returns The entry, its unverified fields, or why it was skipped. No side effects.
BibEntry bibEntry(const Paper& paper);

/// The bibliography and what writing it found.
struct Bibliography {
    /// `refs.bib`: the entries in key order, a blank line between them, UTF-8.
    QByteArray text;
    /// An error per skipped entry and a warning per paper with unverified fields (*"refs.bib uses
    /// unverified fields: SmithJones24 journal, volume"*), each on the paper's `paper.toml`.
    std::vector<Problem> problems;
};

/// Writes the bibliography of a project's papers.
/// @param papers The papers, each read without losing a field; in any order.
/// @returns The text and its problems, sorted with sortProblems(). No side effects.
Bibliography formatBibliography(const std::vector<Paper>& papers);

/// Escapes a BibTeX value: `&`, `%` and `#` that aren't already escaped and aren't inside `$…$`
/// get a `\`. No side effects.
QString escapeBibValue(const QString& value);

/// Whether a value's braces are balanced: no `}` before its `{`, and none left open; `\{` and
/// `\}` don't count. No side effects.
bool bracesBalanced(const QString& value);

} // namespace loom
