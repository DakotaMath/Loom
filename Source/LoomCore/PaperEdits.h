#pragma once

// Changing a project's papers: what `loom new-paper`, `new-stub`, `rekey` and `build-bib` do
// (Specs/m1-core.md, "Papers" and "The `loom` command"). Each takes the write lock and reads the
// project. Like the changes in Edits.h, `new-paper`, `new-stub` and `rekey` make the change in
// memory and write only if it adds no error to the project. `build-bib` writes a generated file
// that isn't part of the project's model, so it has no such check; it refuses instead to write
// over any of the project's own files.

#include "LoomCore/Edits.h"
#include "LoomCore/Problem.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <vector>

namespace loom {

/// A paper renamed: its folder and `key` changed, the old key kept in `aliases`.
struct PaperRenamed {
    QString from;
    QString to;
};

/// The result of a change that keys papers.
struct PaperChange {
    /// How it went; `id` is the paper's key, `path` its `paper.toml`.
    ChangeResult change;
    /// Every paper renamed, in the order done: for `rekey`, the paper itself last; for
    /// `new-paper`, an existing paper that now shares its key and was lettered.
    std::vector<PaperRenamed> renamed;
};

/// `loom new-paper <draft.toml>`: creates `Papers/<Key>/paper.toml` from a draft (Specs/m1-core.md,
/// "The `loom` command"). The draft is a `paper.toml` that may leave out `key`, `cite` and
/// `created`. Loom computes the key by the key rule (a `key` in the draft must be that key),
/// defaults `cite` (`published` if there is a published version, else the newest arXiv version),
/// sets `created` to today if absent, and writes the entry in canonical form. A paper that now
/// shares the key is renamed, both getting letters (the key rule, step 4).
/// Refuses: a draft with errors of its own; a paper with no key by the rule; a draft key that
/// isn't the rule's; `paper-exists` when anything named `Papers/<Key>` is already there,
/// compared ignoring case (A3: it is never replaced); anything that would add an error to the
/// project. If a write fails part way, the renames already done are undone.
/// @param draft     The draft's bytes.
/// @param draftName The draft's name, for its problems (e.g. its path as typed).
/// @returns The result and the renames. Side effects: renames paper folders; writes the entry.
PaperChange createPaper(const QString& root, const QByteArray& draft, const QString& draftName,
                        const ChangeOptions& options = {});

/// What `loom new-stub` asks for. IDs and the key are as a person typed them.
struct NewStub {
    QString paper;             ///< The paper's key or old key.
    QString kind;
    QString gist;
    QString version;           ///< `extracted_from`, and the version of the first locator.
    QString at;                ///< Where in that version, e.g. `Theorem 1.2, p. 4`.
    QString by;                ///< `extracted_by`: `dakota` or `agent:<model>`.
    QString title;
    QStringList statedUsing;
    QStringList paperPremises;
};

/// `loom new-stub <key>`: creates `Papers/<Key>/Statements/<new tag>.toml`, `created` today, its
/// locator for `version` with `checked_against_pdf = false` (setting it is Dakota's click).
/// @returns The result; `id` is the new tag. Side effects: writes the file.
ChangeResult createStub(const QString& root, const NewStub& request, const ChangeOptions& options = {});

/// `loom rekey <key>`: renames a paper to the key the rule gives (Specs/m1-core.md, "The key
/// rule", step 5): the folder is renamed, `key` changes, and the old key is added to `aliases`.
/// `\cite{…}` in bodies isn't rewritten (D9): old keys keep working through the alias. A key that
/// already fits the rule (its base, or its base and a letter) is left alone. If a write fails
/// part way, the renames already done are undone.
/// @param typed The paper's key or old key.
/// @returns The result; `renamed` is empty when the key was already right. Side effects: renames
///          paper folders and rewrites their `paper.toml`.
PaperChange rekeyPaper(const QString& root, const QString& typed, const ChangeOptions& options = {});

/// The result of buildBibliography().
struct BibliographyBuilt {
    /// How it went; `path` is the file written.
    ChangeResult change;
    /// Entries skipped (errors) and unverified fields used (warnings).
    std::vector<Problem> problems;
};

/// `loom build-bib [--out <path>]`: writes the bibliography (Specs/m1-core.md, "The bibliography")
/// of every paper Loom can read. The file is written even when some entries are skipped.
/// Refuses (`bib-out-project-file`) an `out` that is one of the project's own files or in a
/// folder Loom reads every file of: `loom.toml`, `CLAUDE.md`, `.gitignore`, `.gitattributes`,
/// the preamble files, `.loom/write.lock`, and anything in `Statements/`, `Proofs/`, `Papers/`
/// or `.git/`, compared ignoring case.
/// @param out The project-relative path to write; empty means `.loom/build/refs.bib`.
/// @returns The result and the problems. Side effects: writes the file.
BibliographyBuilt buildBibliography(const QString& root, const QString& out = {}, const ChangeOptions& options = {});

} // namespace loom
