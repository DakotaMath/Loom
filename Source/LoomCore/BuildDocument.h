#pragma once

// What a build compiles (Specs/m2-statements-review.md, "Building LaTeX"): which statements and
// proofs each mode contains and in what order, the wrapper document `main.tex`, the
// line-preserving copies of the bodies, `\paperref` and `\cite`, and `refs.bib`. Nothing here
// runs TeX or writes a file: Build.h does that with what composeBuild() returns.

#include "LoomCore/Problem.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProjectToml.h"
#include "LoomCore/Status.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace loom {

/// What a build shows besides its target (Specs/m2-statements-review.md, "What each mode contains").
enum class BuildMode {
    Alone,    ///< The target only.
    Premises, ///< Plus what the target statement is stated using and its proofs' direct premises.
    Chain,    ///< Plus everything reachable through premises and `stated_using`, with deciding proofs.
};

/// `alone`, `premises` or `chain`, as `loom build --mode` and build folders name the mode.
/// No side effects.
QString buildModeName(BuildMode mode);

/// Reads a mode's name.
/// @returns The mode, or nothing for a name that isn't one. No side effects.
std::optional<BuildMode> buildModeFromName(const QString& name);

/// A project as a build reads it: the working tree, or a commit's tree (Revision.h).
struct BuildSource {
    QString root;              ///< The folder the files are read from.
    ProjectConfig config;      ///< Its `loom.toml`.
    ProjectContents contents;  ///< Its statements, proofs and papers.
    ProjectGraph graph;        ///< buildGraph(contents): statuses and deciding proofs.
};

/// One file of the build folder, by its path in that folder (`main.tex`, `src/Statements/4KX7.tex`).
struct BuildFile {
    QString path;
    QByteArray bytes;
};

/// What a line of `main.tex` belongs to: the statement, stub or proof whose part Loom wrote
/// around it, for errors there (`tex-error-generated`).
struct GeneratedPart {
    QString id;   ///< E.g. `4KX7`, `4KX7.a` or `7RT2`.
    QString file; ///< Its project file, e.g. `Statements/4KX7.tex`.
};

/// The result of composeBuild().
struct BuildDocument {
    /// Empty when the target is a statement or proof; else why there is nothing to build:
    /// `build-stub` (a paper stub) or `ref-unknown` (nothing in the project has that ID).
    QString refusal;
    /// The refusal in plain English.
    QString refusalMessage;

    QString target;      ///< The target's tag or proof ID.
    QString targetFile;  ///< Its project file.
    QString statement;   ///< The tag of the target's statement (the target itself, or what a proof proves).
    BuildMode mode = BuildMode::Alone;
    bool verbose = false;
    /// A statement's preview (composePreview()), not a build of a target.
    bool preview = false;

    /// `main.tex`, the copies under `src/` and `refs.bib`, in that order. Empty when refused or
    /// when a file can't be read (`build-unreadable`). `loom.sty` isn't here: see loomStyle().
    std::vector<BuildFile> files;
    /// What each line of `main.tex` belongs to; line n is element n − 1.
    std::vector<GeneratedPart> mainLines;
    /// The statements and stubs in the build, in the order `main.tex` shows them.
    QStringList statements;
    /// The proofs it shows, in order (each as its verbose version when `verbose` and it has one).
    QStringList proofs;
    /// What composing found: `build-unreadable` (errors: then there are no files), and the
    /// warnings `build-cycle`, `verbose-stale`, `paperref-no-locator`, `paperref-unchecked`,
    /// `paperref-unknown`, and refs.bib's (`bib-entry-skipped` and unverified fields).
    std::vector<Problem> problems;

    /// Whether a file the build needs couldn't be read, so it stops before TeX. No side effects.
    bool unreadable() const;
};

/// Composes a build (Specs/m2-statements-review.md, "Building LaTeX").
///
/// Rules:
/// - **The target** is a proof ID (as resolveProof() reads it) or a statement's tag or alias (as
///   resolveStatement() reads it). A paper stub is refused (`build-stub`); anything else
///   (`ref-unknown`).
/// - **Proofs shown:** for a statement, all its proofs in letter order with failed ones last; for
///   a proof, that proof. With `verbose`, each shown proof that has a verbose version shows it
///   instead; a stale one (its `fingerprint` isn't the proof's) says so and warns `verbose-stale`.
/// - **Modes:** *alone* is the target statement with the proofs shown. *premises* adds before it
///   a *Stated using* group (the statement's `stated_using`) and a *Premises* group (every premise
///   of the proofs shown), as statements without proofs. *chain* adds *The chain* instead:
///   everything reachable from the proofs shown and the statement's `stated_using`, transitively,
///   through `stated_using` and premises; each statement comes with its deciding proof (M1,
///   *Tie-breaks*) and only that proof's premises are followed; paper stubs come as stubs, and
///   their `paper_premises` aren't followed. A statement appears once, in the first group that
///   has it; the target is never in another group.
/// - **Order within a group:** dependency order (a statement after everything in its group that it
///   is stated using or proved from by a proof shown), ties by tag; each set of statements that
///   go round in a circle is placed as a whole, in tag order, and warns `build-cycle`.
/// - **Copies:** each statement, proof and verbose body is copied to `src/<project path>` with every
///   front-matter line (both `+++` lines included) replaced by a line holding only `%`, so line n
///   of the copy is line n of the file; in the copies, `\cite` and `\nocite` keys that are a
///   paper's old key, or a key or old key in another case, become its current key (E3). Preamble
///   files (`tex.preamble`) are copied whole.
/// - **`main.tex`:** the document class (`article`, 11 pt), the preambles, `\usepackage{loom}`,
///   a `\loompaperref` line for each name of each stub a body `\paperref`s (with the locator of
///   its paper's `cite` version), a `\loomoutside` line for each tag, label-safe alias and proof
///   ID of the project that isn't in the build, then the header, the groups (with a
///   `\loomsection` heading when there is more than one), and the bibliography when any body
///   has `\cite`, `\nocite` or a `\paperref` of a stub.
/// - **`refs.bib`:** M1's bibliography of the papers the build cites (every paper for
///   `\nocite{*}`); its `bib-entry-skipped` problems are warnings here, since LaTeX still builds.
/// - **`\paperref`** warns `paperref-no-locator` (the `cite` version has no locator for the stub),
///   `paperref-unchecked` (the locator isn't checked against the PDF) or `paperref-unknown` (it
///   doesn't name a stub), at the line of the `\paperref`.
/// - A statement, proof or verbose version in the build whose front matter Loom can't read is
///   `build-unreadable`, and the build stops there: no files.
///
/// @param source   The project.
/// @param typedId  The target, as a person typed it.
/// @param mode     What to show besides the target.
/// @param verbose  Whether proofs show their verbose versions.
/// @returns The document. Reads the preamble files under `source.root`; writes nothing.
BuildDocument composeBuild(const BuildSource& source, const QString& typedId, BuildMode mode, bool verbose);

/// Composes a statement's preview, the picture a hover over its ID shows (Specs/m2-statements-review.md,
/// "IDs in the editor", "Previews": "the statement built alone (without proofs)").
///
/// Rules: as composeBuild() in mode *alone*, but without the statement's proofs, without the
/// `\loomheader` line, with `\pagestyle{empty}` (no page number), and with every `\loomoutside`
/// title empty: LaTeX doesn't print them, and leaving them out keeps a preview's inputs, and so
/// its cache, unchanged when another statement is renamed. A stub is refused (`build-stub`: its
/// preview is its gist, as text); anything but a statement's exact tag is `ref-unknown`.
/// @param source The project.
/// @param tag    The statement's tag.
/// @returns The document, with `preview` set. Reads the preamble files; writes nothing.
BuildDocument composePreview(const BuildSource& source, const QString& tag);

/// A body's line-preserving copy (Specs/m2-statements-review.md, "The build folder"): `bodyLine −
/// 1` lines holding only `%` (the front matter's lines), then the body.
/// @param body     The body, as its reading gives it (UTF-8, LF line endings).
/// @param bodyLine The file line the body starts on.
/// @returns The copy. No side effects.
QByteArray lineCopy(const QByteArray& body, int bodyLine);

/// Escapes text for LaTeX's text mode, for the parts of `main.tex` that come from fields that
/// aren't LaTeX (a stub's gist and locators): `\ { } % # & _ ^ ~ $ < > |` are escaped, except that
/// a `$…$` span is kept as math, as paper titles allow (Specs/m1-core.md, "Papers"); control
/// characters become spaces.
/// @param text The text.
/// @param keepMath Whether `$…$` spans are kept as math.
/// @returns The LaTeX. No side effects.
QString escapeLatexText(const QString& text, bool keepMath = true);

/// `loom.sty`, the package every build loads (Source/LoomCore/TeX/loom.sty, compiled in).
/// @returns Its bytes. No side effects.
QByteArray loomStyle();

} // namespace loom
