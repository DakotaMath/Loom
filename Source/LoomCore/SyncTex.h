#pragma once

// Finding places between a build's PDF and the project's files (Specs/m2-statements-review.md,
// "SyncTeX and the open statement's page"): the page of a label from `main.aux`, and forward and
// inverse search with the `synctex` program TeX Live installs, run as a process (no SyncTeX
// library: the spec's decision *SyncTeX*).

#include <QByteArray>
#include <QProcessEnvironment>
#include <QString>

#include <functional>
#include <optional>

namespace loom {

/// A box on a page of the PDF, in PDF points (1/72 inch) from the page's top-left corner.
struct SyncTexBox {
    int page = 0;        ///< From 1.
    double x = 0;        ///< The box's left edge.
    double y = 0;        ///< The box's top edge.
    double width = 0;
    double height = 0;
};

/// A place in the project: a project-relative file with `/` and a line from 1.
struct SyncTexPlace {
    QString file;
    int line = 0;
};

/// How a `synctex` run went.
enum class SyncTexOutcome {
    Found,    ///< The place was found.
    NotFound, ///< `synctex` ran but found nothing there (or no `main.synctex.gz` yet).
    Missing,  ///< `synctex` couldn't be run (Check my setup says so), or didn't finish in time.
};

/// What to run `synctex` with.
struct SyncTexTool {
    QString program = QStringLiteral("synctex"); ///< A name looked up on PATH, or a path.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    int timeoutMs = 10'000;
};

/// The page a label is on, from a build's `main.aux`.
/// Rule: the first `\newlabel{<label>}{{<text>}{<page>}…}` line; `<text>` may hold braces (it is
/// read as a balanced group), and the page is read as a number (hyperref's extra groups after it
/// are ignored). Spec: "The open statement's page comes from main.aux (`\newlabel{<tag>}{{…}{<page>}…}`):
/// always available".
/// @param aux   The bytes of `main.aux`.
/// @param label The label: a tag or a proof ID.
/// @returns The page, from 1, or 0 if the label isn't there. No side effects.
int auxLabelPage(const QByteArray& aux, const QString& label);

/// Reads `synctex view`'s output.
/// Rule: the first result's `Page:`, `h:` (the box's left), `v:` (its baseline), `W:` and `H:`
/// (its width and height); the box's top is `v − H`. When `W` or `H` is 0 (a point, not a box),
/// `x:` and `y:` are used, with no size.
/// @returns The box, or nothing if the output has no result. No side effects.
std::optional<SyncTexBox> parseSynctexView(const QByteArray& output);

/// Reads `synctex edit`'s output and maps its file back to the project.
/// Rule: `Input:` names the file as TeX read it, absolute or relative to the build folder, with
/// `./` parts; under the build folder's `src/` it is the project file at the same path and line
/// (the copies keep lines, "The build folder"). `main.tex` is returned as it is, `main.tex` at its
/// line, for generatedPlace() to resolve. Anything else (a file of TeX's own) is not a place.
/// @param output      What `synctex edit` printed.
/// @param buildFolder The build folder, absolute.
/// @returns The place, or nothing. No side effects.
std::optional<SyncTexPlace> parseSynctexEdit(const QByteArray& output, const QString& buildFolder);

/// Where a line of `main.tex` points in the project: Loom writes `\begin{loomstatement}…` or
/// `\begin{loomproof}…` (the heading, which is where a click on it lands) right before the
/// `\input{src/<file>}` of its body, so a line at most two lines before such an `\input`, or on it,
/// belongs to `<file>`. The place is the body's first line: the copy's first line that isn't one of
/// the `%` lines standing for the front matter.
/// @param mainTex The bytes of `main.tex`.
/// @param line    Its line, from 1.
/// @param copy    Gives a copy's bytes by its build-folder path (`src/Statements/4KX7.tex`).
/// @returns The place, or nothing for a line of `main.tex` that isn't a heading. No side effects
/// beyond what `copy` does.
std::optional<SyncTexPlace> generatedPlace(const QByteArray& mainTex, int line, const std::function<QByteArray(const QString&)>& copy);

/// Forward search: where a line of a project file is in a build's PDF.
/// Runs `synctex view -i <line>:<column>:src/<file> -o main.pdf` in the build folder.
/// @param tool        Which `synctex`.
/// @param buildFolder The build folder, absolute; it holds `main.pdf` and `main.synctex.gz`.
/// @param file        The project file, project-relative with `/`.
/// @param line        Its line, from 1.
/// @param column      Its column, from 0 (`synctex` mostly ignores it).
/// @param box         Set when the outcome is Found.
/// @returns How it went. Side effects: runs `synctex`.
SyncTexOutcome synctexForward(const SyncTexTool& tool, const QString& buildFolder, const QString& file, int line, int column,
                              SyncTexBox& box);

/// Inverse search: the project place at a point of a build's PDF.
/// Runs `synctex edit -o <page>:<x>:<y>:main.pdf` in the build folder; a place in `main.tex` is
/// resolved with generatedPlace().
/// @param page  From 1.
/// @param x, y  The point, in PDF points from the page's top-left corner.
/// @param place Set when the outcome is Found.
/// @returns How it went. Side effects: runs `synctex`.
SyncTexOutcome synctexInverse(const SyncTexTool& tool, const QString& buildFolder, int page, double x, double y,
                              SyncTexPlace& place);

} // namespace loom
