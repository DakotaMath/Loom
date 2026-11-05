#pragma once

// Check histories (Specs/m1-core.md, "Checks"): `Proofs/<tag>/<proof-id>.checks.toml`, reading
// and writing it, which checks count and whether a proof is checked, and merging two versions.

#include "LoomCore/Problem.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace loom {

/// The verdicts, the same three for agents and Dakota (Specs/m1-core.md, D10).
inline const QStringList checkVerdicts = {QStringLiteral("pass"), QStringLiteral("needs-changes"), QStringLiteral("fail")};

/// One entry of a check history. Fields in the spec's order; an empty string or an invalid
/// date-time means the field is absent.
struct Check {
    QString by;          ///< `dakota` or `agent:<model>`.
    QDateTime date;      ///< An offset date-time to the second (Specs/m1-core.md, D1).
    QString fingerprint; ///< The proof's fingerprint when the check was made.
    QString verdict;     ///< `pass`, `needs-changes` or `fail`.
    QString changes;     ///< What needs to change; only on `needs-changes`.
    QString record;      ///< Project-relative path to the job record or review.
    QString note;        ///< Project-relative path to a note.

    /// Whether this is Dakota's check.
    bool byDakota() const { return by == u"dakota"; }
    /// Whether this is an agent's check (`by` starts `agent:`).
    bool byAgent() const { return by.startsWith(u"agent:"); }
};

/// Whether two entries are the same entry: every field equal, the date's offset included
/// (Specs/m1-core.md, "Merging two check lists": "entries are equal when all their fields are
/// equal"). No side effects.
bool sameCheck(const Check& a, const Check& b);

/// Whether `by` is `dakota` or `agent:<model>` with `<model>` matching `[a-z0-9][a-z0-9.-]*`
/// (Specs/m1-core.md, "Checks", the `by` field). No side effects.
bool isCheckAuthor(const QString& by);

/// Where one entry is in its file.
struct CheckLines {
    int header = 1;              ///< The line of its `[[checks]]`.
    QHash<QString, int> fields;  ///< Field name to the line of its value.

    /// The line of a field's value, else the header's.
    int of(const QString& field) const { return fields.value(field, header); }
};

/// What reading a check history gave.
struct ChecksReading {
    /// The entries, in file order. An entry that isn't a table is left out (and `lossless` is false).
    std::vector<Check> checks;
    /// Where each of `checks` is.
    std::vector<CheckLines> lines;
    /// True if the file is UTF-8 and parsed as TOML, so `checks` means something.
    bool parsed = false;
    /// True if `parsed` and every entry and field was kept: writing `checks` back in canonical
    /// form loses nothing but comments.
    bool lossless = false;
    /// The problems this file shows on its own (Specs/m1-core.md, "Validation", "Checks"):
    /// `not-utf8`, `toml-syntax`, `unknown-field`, `missing-field`, `wrong-type`, `bad-path`,
    /// `check-by`, `check-verdict`, `check-changes-missing`, `check-changes-unexpected`,
    /// `check-fingerprint`, `check-record-missing`, `check-order`, `check-duplicate`.
    /// `check-record-not-found` (the disk) and `check-changed` (git's `HEAD`) are `loom validate`'s.
    std::vector<Problem> problems;
};

/// Reads a check history: plain TOML, one array of tables `checks` and nothing else. A file
/// without `checks` (empty, as git gives a merge driver for a file both sides created) is an
/// empty history.
/// @param bytes The file's content.
/// @param path  Its project-relative path, for the problems.
/// @returns The entries and problems. No side effects.
ChecksReading readChecks(const QByteArray& bytes, const QString& path);

/// Writes one entry in canonical form: `[[checks]]`, then the fields in the spec's order, one per
/// line, absent ones left out; `changes` as a multi-line string when it has a line break.
/// @returns The TOML text, ending in a line break. No side effects.
QByteArray formatCheck(const Check& check);

/// Writes a whole check history in canonical form: each entry by formatCheck(), a blank line
/// between entries.
/// @returns The file's bytes; empty for no entries. No side effects.
QByteArray formatChecks(const std::vector<Check>& checks);

/// Which checks of a proof count, and what they mean (Specs/m1-core.md, "Checks", rules 4–7).
struct CheckStanding {
    /// The index in the history of the counting agent check: the newest matching check by any agent.
    std::optional<qsizetype> countingAgent;
    /// The index of the counting Dakota check: the newest matching check by Dakota.
    std::optional<qsizetype> countingDakota;
    /// Rule 6: both counting checks are `pass`, and Dakota's is newer than the agent's.
    bool checked = false;
    /// Rule 7: false when either counting check is `fail`; the proof then doesn't support its statement.
    bool supports = true;
};

/// Works out which checks count (Specs/m1-core.md, "Checks").
/// Rules: a check *matches* when its fingerprint is `currentFingerprint` (rule 4); the counting
/// agent check and the counting Dakota check are the newest matching check of each (rule 5),
/// where newer means a later date, or for equal dates, later in the file (rule 2); the proof is
/// checked when both are `pass` and Dakota's is newer (rule 6); it doesn't support its statement
/// when either is `fail` (rule 7). A check with no valid date (missing, unreadable, or the year
/// 0) never counts, since it has no place in the date order (A22).
/// @param checks             The history, in file order.
/// @param currentFingerprint The proof's fingerprint now; empty if it can't be computed, and
///                           then nothing matches.
/// @returns The counting checks and what they mean. No side effects.
CheckStanding standingOf(const std::vector<Check>& checks, const QString& currentFingerprint);

/// The result of mergeChecks().
struct ChecksMerge {
    /// False if one of the three couldn't be read in full; then git's conflict is left for a person.
    bool ok = false;
    /// The merged history in canonical form, for the "ours" file.
    QByteArray merged;
    /// True if an entry of the base was missing from ours or theirs and was put back.
    bool restored = false;
    /// Why it isn't ok, in plain English.
    QString message;
};

/// Merges two versions of a check history, as the `loom-checks` git merge driver does
/// (Specs/m1-core.md, "Merging two check lists").
/// Rules: every entry of ours, plus every entry of theirs not already there, sorted stably by
/// date, so for equal dates ours' come first; an entry of the base missing from ours or theirs
/// was deleted, which is never allowed, so it is put back.
/// @param base, ours, theirs The three versions' bytes, as git gives them.
/// @returns The merge. No side effects.
ChecksMerge mergeChecks(const QByteArray& base, const QByteArray& ours, const QByteArray& theirs);

} // namespace loom
