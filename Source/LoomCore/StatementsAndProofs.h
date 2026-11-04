#pragma once

// Statement, proof and verbose proof files (Specs/m1-core.md, "Statements" and "Proofs"):
// their fields, reading them with the problems each file shows on its own, and writing them in
// canonical form around a body kept byte for byte.

#include "LoomCore/Problem.h"

#include <QByteArray>
#include <QDate>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <vector>

namespace loom {

/// The statement kinds, in the spec's order.
inline const QStringList statementKinds = {
    QStringLiteral("definition"), QStringLiteral("lemma"), QStringLiteral("proposition"),
    QStringLiteral("theorem"), QStringLiteral("corollary"), QStringLiteral("conjecture"),
    QStringLiteral("question"), QStringLiteral("remark"), QStringLiteral("example")};

/// The proof levels, in the spec's order.
inline const QStringList proofLevels = {
    QStringLiteral("sketch"), QStringLiteral("heuristic"), QStringLiteral("complete"), QStringLiteral("failed")};

/// Whether a kind has a status, proofs, and can be a premise or refuted (Specs/m1-core.md,
/// "Statements": the provable kinds are lemma, proposition, theorem, corollary and conjecture).
/// No side effects.
bool isProvableKind(const QString& kind);

/// A statement: `Statements/<id>.tex`. Fields in the spec's order; an empty string, an empty
/// list or an invalid date means the field is absent.
struct Statement {
    QString id;
    QString kind;
    QString title;
    QStringList tags;
    QStringList statedUsing; ///< `stated_using`
    QStringList aliases;
    QString origin;          ///< Absent means `new`.
    QString supersededBy;    ///< `superseded_by`
    QString refutedBy;       ///< `refuted_by`
    QDate refutedOn;         ///< `refuted_on`
    QDate created;
    QDate verified;
    /// The LaTeX body, UTF-8, with LF line endings.
    QByteArray body;
};

/// A proof: `Proofs/<statement-id>/<proof-id>.tex`.
struct Proof {
    QString id;
    QString proves;
    QStringList premises;
    QString level;
    QDate created;
    QDate verified;
    QByteArray body;
};

/// A verbose proof: `Proofs/<statement-id>/<proof-id>.verbose.tex` (Specs/m1-core.md, D8).
struct VerboseProof {
    QString proof;
    QString fingerprint;
    QDate created;
    QByteArray body;
};

/// The file lines of a document's fields, for problems found after reading.
struct FieldLines {
    QHash<QString, int> fields;            ///< Field name to the line of its value.
    QHash<QString, QList<int>> elements;   ///< List field name to the line of each element.

    /// The line of a field's value, or of one element of a list field; 1 if unknown.
    int of(const QString& field, qsizetype element = -1) const;
};

/// What reading one document gave.
template <typename Document>
struct DocumentReading {
    /// The fields that could be read; a field with the wrong type is left absent.
    Document document;
    /// True if the front matter was found and parsed as TOML, so `document` means something.
    bool parsed = false;
    /// True if `parsed` and every field and list element was kept: rewriting the file in
    /// canonical form loses nothing but comments. Loom edits only such files.
    bool lossless = false;
    /// Where each field is.
    FieldLines lines;
    /// The file line the body starts on; 0 if there's no front matter, so no body.
    int bodyLine = 0;
    /// The problems this file shows on its own: syntax, `unknown-field`, `missing-field`,
    /// `wrong-type`, `bad-value`, `id-syntax` and `id-mismatch` (or `proof-id-mismatch`),
    /// `alias-invalid`, `refuted-incomplete`, `premise-self`, `premise-duplicate`, `body-empty`.
    /// Problems that need the rest of the project are found by checkContents().
    std::vector<Problem> problems;
};

using StatementReading = DocumentReading<Statement>;
using ProofReading = DocumentReading<Proof>;
using VerboseReading = DocumentReading<VerboseProof>;

/// Reads a statement file.
/// @param bytes The file's content.
/// @param path  Its project-relative path, `Statements/<name>.tex`; `id` must equal `<name>`.
/// @returns The fields and problems. No side effects.
StatementReading readStatement(const QByteArray& bytes, const QString& path);

/// Reads a proof file.
/// @param bytes The file's content.
/// @param path  Its project-relative path, `Proofs/<folder>/<name>.tex`; `id` must equal `<name>`.
/// @returns The fields and problems. No side effects.
ProofReading readProof(const QByteArray& bytes, const QString& path);

/// Reads a verbose proof file.
/// @param bytes The file's content.
/// @param path  Its project-relative path, `Proofs/<folder>/<proof-id>.verbose.tex`; `proof` must
///              equal `<proof-id>`.
/// @returns The fields and problems. No side effects.
VerboseReading readVerboseProof(const QByteArray& bytes, const QString& path);

/// Writes a statement in canonical form (Specs/m1-core.md, "Front matter"): the fields in the
/// order of the spec's table, one per line; absent fields left out; then the body untouched.
/// @returns The file's bytes. No side effects.
QByteArray formatStatement(const Statement& statement);

/// Writes a proof in canonical form; `premises` is always written, `[]` when empty.
/// @returns The file's bytes. No side effects.
QByteArray formatProof(const Proof& proof);

/// Writes a verbose proof in canonical form.
/// @returns The file's bytes. No side effects.
QByteArray formatVerboseProof(const VerboseProof& verbose);

/// Prepares body text given to Loom (`--body`, `--body-file`): a byte-order mark dropped, CRLF
/// made LF, and a final newline added if there is none (Specs/m1-core.md, "Front matter").
/// @returns The body bytes. No side effects.
QByteArray bodyFromText(const QByteArray& text);

} // namespace loom
