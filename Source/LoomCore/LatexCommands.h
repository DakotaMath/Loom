#pragma once

// Finding the commands Loom reads in LaTeX bodies (`\ref`, `\paperref`, `\cite`, `\nocite`,
// `\label`), shared by validation (Specs/m1-core.md, "IDs and references") and the build
// (Specs/m2-statements-review.md, "`\paperref` and `\cite`").

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <vector>

namespace loom {

/// One LaTeX command with its argument, as latexCommands() reads them.
struct LatexCommand {
    QString name;     ///< E.g. `ref`, `paperref`, `cite`, `nocite` or `label`.
    QString argument; ///< What's between the braces, trimmed.
    int line = 1;     ///< The file line.
    /// Where the text between the braces starts in its line (in UTF-16 code units of the line
    /// as QString reads it), and how long it is, untrimmed: what a rewrite replaces.
    qsizetype argumentStart = 0;
    qsizetype argumentLength = 0;
};

/// The commands M1's validation reads: `ref`, `paperref`, `cite` and `label`.
const QStringList& validationCommandNames();

/// The commands named in `names`, in a body, in order.
/// Rules (Specs/m1-core.md, "IDs and references", `checkBodies()`): a `%` not escaped by `\` (an
/// even number of backslashes before it) starts a comment, which is skipped; a command and its
/// `{…}` argument must be on one line; optional `[…]` arguments are skipped; the name must end
/// there (`\reference` isn't `\ref`, and `\cite*` isn't `\cite`).
/// @param body      The body, UTF-8 with LF line endings.
/// @param firstLine The file line the body starts on.
/// @param names     The command names to find, without `\`.
/// @returns The commands. No side effects.
std::vector<LatexCommand> latexCommands(const QByteArray& body, int firstLine,
                                        const QStringList& names = validationCommandNames());

/// Where a line's comment starts: the first `%` after an even number of backslashes.
/// @returns Its index, or the line's length if it has none. No side effects.
qsizetype commentStart(QStringView line);

} // namespace loom
