#pragma once

// A proof's checks in its project: its current fingerprint, which of its checks count and
// whether it is checked (Specs/m1-core.md, "Checks"), and which proofs a change would outdate.

#include "LoomCore/Checks.h"
#include "LoomCore/ProjectContents.h"

#include <QString>
#include <QStringList>

#include <optional>

namespace loom {

/// A proof's current fingerprint (Specs/m1-core.md, "The fingerprint"), from the bodies of the
/// proof, the statement it proves and each premise statement, as they are in `contents`; a
/// premise that is a paper stub counts with no body.
/// @param id      The proof's ID.
/// @param problem Set, when there is no fingerprint, to why: in plain English, ready to print.
/// @returns The fingerprint, or nothing if the proof, the statement it proves or a premise isn't
///          in `contents` with front matter Loom could read. No side effects.
std::optional<QString> proofFingerprint(const ProjectContents& contents, const QString& id, QString* problem = nullptr);

/// What a proof's checks say now.
struct ProofChecking {
    /// The proof's current fingerprint; empty if it can't be computed, and then no check matches.
    QString fingerprint;
    /// When `fingerprint` is empty, why: proofFingerprint()'s `problem`, ready to print (e.g. a
    /// premise whose front matter Loom can't read). Empty when the fingerprint was computed.
    QString fingerprintProblem;
    /// Its check history, in file order; empty if it has none.
    std::vector<Check> checks;
    /// Which checks count, whether the proof is checked and whether it supports its statement.
    CheckStanding standing;
};

/// What a proof's checks say now (Specs/m1-core.md, "Checks", rules 4–7).
/// @param id The proof's ID.
/// @returns The fingerprint (or why there is none), the history and its standing. No side effects.
ProofChecking proofChecking(const ProjectContents& contents, const QString& id);

/// The proofs a change would outdate (Specs/m1-core.md, "Statements": editing text that is in a
/// checked proof's fingerprint needs Dakota's click).
/// Rule: a proof is outdated when it is checked in `before` and its fingerprint in `after`
/// differs (or can't be computed). Only checked proofs count, as the spec says; a proof with an
/// agent's pass that Dakota hasn't confirmed yet edits freely.
/// @param before The project as it is.
/// @param after  The project with the change made.
/// @returns The IDs of the outdated proofs, in path order. No side effects.
QStringList outdatedChecks(const ProjectContents& before, const ProjectContents& after);

} // namespace loom
