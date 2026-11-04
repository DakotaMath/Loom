#pragma once

// Changing a project's statements, proofs and tags: what `loom new-tag`, `new-statement`,
// `edit-statement`, `new-proof` and `edit-proof` do (Specs/m1-core.md, "The `loom` command").
// Each one takes the write lock, reads the project, makes the change in memory, and writes only
// if the change adds no error to the project: so a refused change writes nothing.

#include "LoomCore/Problem.h"

#include <QByteArray>
#include <QDate>
#include <QRandomGenerator>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace loom {

/// What every change needs from its caller.
struct ChangeOptions {
    /// Today, for `created` and `verified`.
    QDate today = QDate::currentDate();
    /// Where new tags come from (Specs/m1-core.md, "IDs": `QRandomGenerator::system()`); tests
    /// pass a seeded generator.
    QRandomGenerator* random = QRandomGenerator::system();
    /// How long to wait for `.loom/write.lock` (Specs/m1-core.md, "Front matter": 10 seconds).
    int lockWaitMs = 10000;
};

/// How a change went; each maps to one of `loom`'s exit codes.
enum class ChangeOutcome {
    Done,     ///< Exit 0.
    Refused,  ///< Exit 1: a rule refused it, and nothing was written.
    CantWork, ///< Exit 3: no project, a newer format, the lock, or a file Loom can't read or write.
};

/// The result of a change.
struct ChangeResult {
    ChangeOutcome outcome = ChangeOutcome::Refused;
    /// A stable code for `--json` when not done: a problem code from the spec (the first of
    /// `problems`), or one of `not-a-project`, `unreadable`, `format-too-new`, `locked`,
    /// `write-failed`, `ref-unknown` (an ID typed that names nothing), `body-empty`,
    /// `not-editable` (the file has problems that rewriting it would lose), `no-tag`
    /// (no free tag), `already-there`, `not-there`.
    QString code;
    /// What went wrong, in plain English, ready to print; empty when done.
    QString message;
    /// When refused because the change would add errors: those errors.
    std::vector<Problem> problems;
    /// The new or changed statement's tag, proof's ID or tag's ID.
    QString id;
    /// The file written (or that would have been), project-relative.
    QString path;
    /// For edits: the fields that changed, in the spec's order (`body` for the body).
    QStringList changed;
};

/// `loom new-tag <tag> --kind … --title …`: declares a tag in `loom.toml`.
/// The `[[tags]]` table is added at the end of the file, so the rest of it (comments included)
/// stays exactly as it was.
/// @returns The result; `id` is the tag. Side effects: appends to `loom.toml`.
ChangeResult addTag(const QString& root, const QString& tag, const QString& kind, const QString& title,
                    const ChangeOptions& options = {});

/// What `loom new-statement` asks for. IDs are as a person typed them.
struct NewStatement {
    QString kind;
    QString title;
    /// The body as given; see bodyFromText().
    QByteArray body;
    QStringList tags;
    QStringList statedUsing;
    QString origin;
};

/// `loom new-statement`: creates `Statements/<new tag>.tex`, `created` today.
/// @returns The result; `id` is the new tag. Side effects: writes the file.
ChangeResult createStatement(const QString& root, const NewStatement& request, const ChangeOptions& options = {});

/// What `loom edit-statement` changes; absent or empty means unchanged. IDs are as typed.
struct StatementEdit {
    std::optional<QString> title;
    std::optional<QString> kind;
    std::optional<QByteArray> body;
    QStringList addTags;
    QStringList removeTags;
    QStringList addStatedUsing;
    QStringList removeStatedUsing;
    std::optional<QString> supersededBy;
    /// Sets `verified` to today.
    bool verified = false;
};

/// `loom edit-statement <id>`: changes a statement's fields and body, keeping everything else
/// (the body byte for byte if it isn't replaced). Refusing an edit that would outdate checks
/// comes with #6.
/// @param typed The statement's ID as typed.
/// @returns The result; `changed` lists the fields that changed (empty: nothing to write).
/// Side effects: rewrites the file in canonical form if anything changed.
ChangeResult editStatement(const QString& root, const QString& typed, const StatementEdit& edit,
                           const ChangeOptions& options = {});

/// What `loom new-proof` asks for. IDs are as typed.
struct NewProof {
    QString statement;
    QString level;
    QByteArray body;
    QStringList premises;
};

/// `loom new-proof <statement-id>`: creates the statement's next proof, taking the letter after
/// the highest one in its folder (Specs/m1-core.md, "IDs", "Proof IDs"), `created` today.
/// @returns The result; `id` is the new proof's ID. Side effects: writes the file.
ChangeResult createProof(const QString& root, const NewProof& request, const ChangeOptions& options = {});

/// What `loom edit-proof` changes; absent or empty means unchanged. IDs are as typed.
struct ProofEdit {
    std::optional<QString> level;
    std::optional<QByteArray> body;
    QStringList addPremises;
    QStringList removePremises;
    bool verified = false;
};

/// `loom edit-proof <proof-id>`: changes a proof's level, premises and body.
/// @param typed The proof's ID as typed.
/// @returns The result; `changed` lists the fields that changed.
/// Side effects: rewrites the file in canonical form if anything changed.
ChangeResult editProof(const QString& root, const QString& typed, const ProofEdit& edit, const ChangeOptions& options = {});

} // namespace loom
