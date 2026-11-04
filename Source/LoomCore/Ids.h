#pragma once

// Tags, proof IDs and aliases (Specs/m1-core.md, "IDs").

#include <QRandomGenerator>
#include <QString>

#include <functional>
#include <optional>

namespace loom {

/// Crockford's base 32: digits, then capitals without I, L, O and U.
inline constexpr char tagAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

/// How many draws generateTag() makes before giving up (Specs/m1-core.md, "IDs").
inline constexpr int tagDraws = 100;

/// Whether `text` is a tag as files must write it: exactly four characters from tagAlphabet.
/// No side effects.
bool isTag(const QString& text);

/// Reads a tag typed on the command line, as Crockford intends (Specs/m1-core.md, "IDs": "lower
/// case is accepted, `O`/`o` is read as `0`, and `I`, `i`, `L`, `l` as `1`").
/// @param typed What the person typed, e.g. `4kx7` or `1O2l`.
/// @returns The tag in capitals (`4KX7`, `1021`), or nothing if it isn't one even read that way.
///          No side effects.
std::optional<QString> normaliseTag(const QString& typed);

/// Draws a new tag (Specs/m1-core.md, "IDs", "Generation"): each character uniformly from
/// tagAlphabet; drawn again if the tag is all digits or `isUsed` says it is taken, at most
/// tagDraws times.
/// @param isUsed Whether a tag is already used in the project as a tag or an alias.
/// @param random The source: `*QRandomGenerator::system()` in Loom, a seeded generator in tests.
/// @returns The tag, or nothing after tagDraws failed draws. Side effects: advances `random`.
std::optional<QString> generateTag(const std::function<bool(const QString&)>& isUsed, QRandomGenerator& random);

/// The letters of the n-th proof of a statement, counting in bijective base 26: 1 is `a`, 26 is
/// `z`, 27 is `aa`, 28 is `ab` (Specs/m1-core.md, "IDs", "Proof IDs").
/// @param number At least 1.
/// @returns The lowercase letters. No side effects.
QString proofLetters(qint64 number);

/// The inverse of proofLetters().
/// @param letters Lowercase letters, e.g. `aa`.
/// @returns The number, or 0 if `letters` is empty, has anything but `a`–`z`, or is too long to
///          count. No side effects.
qint64 proofNumber(const QString& letters);

/// The parts of a proof ID, `<tag>.<letters>`.
struct ProofIdParts {
    QString tag;     ///< The statement's tag, e.g. `4KX7`.
    QString letters; ///< The proof's letters, e.g. `a`.
};

/// Splits a proof ID as files write it.
/// @param text E.g. `4KX7.a`.
/// @returns The parts, or nothing unless `text` is a tag, a `.`, and one or more lowercase
///          letters. No side effects.
std::optional<ProofIdParts> splitProofId(const QString& text);

/// Reads a proof ID typed on the command line: the tag part as normaliseTag() reads it, the
/// letters in either case.
/// @returns The proof ID as files write it (`4KX7.a`), or nothing. No side effects.
std::optional<QString> normaliseProofId(const QString& typed);

/// Why a string can't be an alias, if it can't (Specs/m1-core.md, "IDs", "Aliases": "1–100
/// characters with no whitespace and none of `{}[],"`, isn't shaped like a tag"). "Shaped like a
/// tag" means normaliseTag() reads it as one, so an alias can never be mistaken for a tag typed
/// on the command line.
/// @returns An empty string if `alias` is fine, else the reason as the `alias-invalid` message
///          words it (e.g. `it is shaped like a tag`). Uniqueness is checked across the project
///          elsewhere. No side effects.
QString aliasProblem(const QString& alias);

} // namespace loom
