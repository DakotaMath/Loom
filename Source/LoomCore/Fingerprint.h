#pragma once

// The fingerprint of a proof: a short hash of exactly the text a check read (Specs/m1-core.md,
// "Checks", "The fingerprint"). A check counts only while the proof's fingerprint is the one it
// recorded.

#include <QByteArray>
#include <QString>

#include <optional>
#include <vector>

namespace loom {

/// One premise of a proof, as the fingerprint sees it.
struct FingerprintPremise {
    /// The premise's tag.
    QString tag;
    /// The premise statement's body, or nothing for a paper stub, whose content is the paper
    /// (Specs/m1-core.md, "The fingerprint": a stub is written `premise <tag>\n0\n\n`).
    std::optional<QByteArray> body;
};

/// Everything a proof's fingerprint covers (Specs/m1-core.md, D3): the bodies of the proof, the
/// statement it proves and each premise. Titles, kinds, tags, `stated_using`, `level`, dates,
/// the order of `premises` and the verbose version are deliberately not here.
struct FingerprintInput {
    QString statementTag;
    QByteArray statementBody;
    std::vector<FingerprintPremise> premises;
    QByteArray proofBody;
};

/// Normalises a body for the fingerprint (Specs/m1-core.md, "The fingerprint", step 1).
/// Rule: CRLF becomes LF; spaces and tabs at the end of every line are removed; newlines at the
/// very end are removed. Nothing else changes, so an editor trimming whitespace doesn't outdate a
/// check, and any other edit does.
/// @returns The normalised bytes. No side effects.
QByteArray normaliseForFingerprint(const QByteArray& body);

/// The byte string the fingerprint hashes (Specs/m1-core.md, "The fingerprint", step 2):
/// `loom-fingerprint-v1\n`, then a `statement <tag>` section, one `premise <tag>` section per
/// premise in byte order of the tags, and a `proof` section. Each section is its label line, the
/// length of the normalised text in bytes as a decimal line, the text, and `\n`.
/// @returns The bytes. No side effects.
QByteArray fingerprintBytes(const FingerprintInput& input);

/// A proof's fingerprint (Specs/m1-core.md, "The fingerprint", step 3, and D2): the first 12
/// lowercase hex characters of the SHA-256 of fingerprintBytes().
/// @returns E.g. `3c8e1f0a92d4`. No side effects.
QString fingerprint(const FingerprintInput& input);

/// Whether text has the form of a fingerprint: 12 characters from `0`–`9` and `a`–`f`
/// (Specs/m1-core.md, "Checks", the `fingerprint` field). No side effects.
bool isFingerprint(const QString& text);

} // namespace loom
