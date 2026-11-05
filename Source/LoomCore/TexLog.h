#pragma once

// Reading a LaTeX log for errors and warnings, and mapping each back to the project file and line
// it came from (Specs/m2-statements-review.md, "Problems from a build").

#include "LoomCore/BuildDocument.h"
#include "LoomCore/Problem.h"

#include <QByteArray>
#include <QString>

#include <vector>

namespace loom {

/// One error or warning as the log gives it, before mapping.
struct TexLogEntry {
    Severity severity = Severity::Error;
    /// `tex-error`, `tex-ref-undefined`, `tex-cite-undefined` or `tex-warning`.
    QString code;
    /// The file as TeX named it, without a leading `./`, with `/`: `src/Statements/4KX7.tex`,
    /// `main.tex`, or a file of TeX's own (`c:/texlive/…/amsmath.sty`). Empty if unknown.
    QString file;
    /// The line in `file`; 0 if the log doesn't say.
    int line = 0;
    /// The nearest file of the build (`main.tex` or under `src/`) open when the entry was written,
    /// counting `file` itself: where to point when `file` is one of TeX's own. Empty if none.
    QString buildFile;
    /// The message, cleaned: LaTeX's words, without `!`, `LaTeX Error:`, the context lines or the
    /// ` on input line n.` ending. For `tex-ref-undefined` and `tex-cite-undefined`, the label or key.
    QString message;
};

/// Reads a log written by pdfLaTeX with `-file-line-error` and lines that don't wrap.
///
/// Rules:
/// - **Errors** are `<file>:<line>: <message>` lines (and `! <message>` lines, with the line from
///   the `l.<n>` context line after them). The message is that line, joined with its indented
///   continuation lines; `LaTeX Error: ` is dropped; for *Undefined control sequence* the
///   control sequence at the end of the first context line is named: *"Undefined control
///   sequence `\fracc`."* The help text and context lines after an error are skipped.
/// - **Warnings** are `LaTeX Warning:`, `LaTeX Font Warning:`, `Package <name> Warning:` and
///   `Class <name> Warning:` lines, joined with their continuation lines (those starting
///   `(<name>)`); the line is the `on input line <n>` at the end, and the file is the one open at
///   that point. *Reference … undefined* is `tex-ref-undefined`, *Citation … undefined*
///   `tex-cite-undefined`, any other `tex-warning`. Left out: the end-of-run summaries (*There
///   were undefined references*, *Label(s) may have changed*, …), *Some font shapes were not
///   available*, *Empty `thebibliography' environment* (which follows from an undefined
///   citation), and hyperref's *Suppressing link with empty target* (Loom's own outside
///   references, which are deliberately not links).
/// - **The open file** is tracked through the log's `(<file>` … `)` nesting; overfull and underfull
///   box reports (which aren't reported) and the lines after errors are skipped, since they quote
///   the document's text, whose parentheses needn't balance.
///
/// @param log The log's bytes; read as UTF-8, with invalid bytes replaced.
/// @returns The entries, in log order. No side effects.
std::vector<TexLogEntry> parseTexLog(const QByteArray& log);

/// Maps log entries to problems of the project (Specs/m2-statements-review.md, "Problems from a
/// build").
/// Rules: `src/<path>` is `<path>` at the same line. A line of `main.tex` belongs to the part Loom
/// wrote around a statement, stub or proof (BuildDocument::mainLines): an error there is
/// `tex-error-generated` (*"LaTeX stopped in the part Loom writes around {id}: …"*), and a warning
/// keeps its code; both point at that file's line 1. A file of TeX's own is mapped through the
/// build file that was open (`buildFile`), at its line 1 (TeX doesn't say which of its lines);
/// failing that, the target's file, line 1. When the log has errors, `tex-ref-undefined` and
/// `tex-cite-undefined` are left out: latexmk stops after a pass with errors, before the pass
/// that resolves references, so they would be noise. The result is sorted with sortProblems() and
/// has no duplicates (LaTeX repeats warnings).
/// @param entries  From parseTexLog().
/// @param document The build the log is of.
/// @returns The problems. No side effects.
std::vector<Problem> mapTexLog(const std::vector<TexLogEntry>& entries, const BuildDocument& document);

/// Sorts problems with sortProblems() and removes exact duplicates (same severity, code, file,
/// line and message), keeping the first.
/// @param problems Sorted and de-duplicated in place. No other side effects.
void sortUniqueProblems(std::vector<Problem>& problems);

} // namespace loom
