#pragma once

// Changing a project's statements, proofs, tags and checks: what `loom new-tag`, `new-statement`,
// `edit-statement`, `new-proof`, `edit-proof`, `add-check` and `refute` do (Specs/m1-core.md, "The `loom`
// command").
// Each one takes the write lock, reads the project, makes the change in memory, and writes only
// if the change adds no error to the project: so a refused change writes nothing.

#include "LoomCore/Checks.h"
#include "LoomCore/Problem.h"

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QRandomGenerator>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace loom {

struct ProjectContents;

/// What every change needs from its caller.
struct ChangeOptions {
    /// Today, for `created` and `verified`.
    QDate today = QDate::currentDate();
    /// Now, for a new check's `date` (Specs/m1-core.md, "Adding a check": "the current local
    /// time"); written with its offset from UTC, to the second.
    QDateTime now = QDateTime::currentDateTime();
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
    /// (no free tag), `already-there`, `not-there`, `outdates-checks` (an edit that needs Dakota's
    /// confirmation, or outdates other proofs than those he confirmed), `dakota-only` (one of
    /// Dakota's clicks without his confirmation: his check, a refutation, `failed`), `no-fingerprint`,
    /// `fingerprint-needed`, `proof-changed`.
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
    /// When done: the bytes written to `path`, so an editor knows the file it now holds without
    /// reading it again (another writer may have changed it since); empty if nothing was written.
    QByteArray written;
    /// For edits: the checked proofs whose checks the change outdates (or, when refused with
    /// `outdates-checks` or with `dakota-only` for a click it needs first, would outdate), in path
    /// order.
    QStringList outdatedChecks;
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
    /// Dakota's confirmation that the edit may outdate checks (`--outdate-checks`): the proofs
    /// he was shown and confirmed, as a refusal's `outdatedChecks` named them. Absent: not
    /// confirmed.
    std::optional<QStringList> outdateChecks;
};

/// `loom edit-statement <id>`: changes a statement's fields and body, keeping everything else
/// (the body byte for byte if it isn't replaced). A change that would outdate a checked proof's
/// checks (a proof of this statement, or one using it as a premise) is refused, naming the
/// proofs, unless `edit.outdateChecks` lists exactly the proofs it would outdate now
/// (Specs/m1-core.md, "Statements"). A different set, because the project changed after Dakota
/// was asked, is refused too (`outdates-checks`), naming both, and nothing is written.
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
    /// Dakota's confirmation that the proof may be created `failed`: the judgement is his.
    bool failedConfirmed = false;
};

/// `loom new-proof <statement-id>`: creates the statement's next proof, taking the letter after
/// the highest one in its folder (Specs/m1-core.md, "IDs", "Proof IDs"), `created` today.
/// A `failed` proof needs `request.failedConfirmed`: setting `level = "failed"` is Dakota's click
/// (Specs/m1-core.md, "Checks", verdict `fail`), and recording a dead end is that same judgement.
/// Without it the request is refused with `dakota-only`, after every other check passed.
/// @returns The result; `id` is the new proof's ID. Side effects: writes the file.
ChangeResult createProof(const QString& root, const NewProof& request, const ChangeOptions& options = {});

/// What `loom edit-proof` changes; absent or empty means unchanged. IDs are as typed.
struct ProofEdit {
    std::optional<QString> level;
    std::optional<QByteArray> body;
    QStringList addPremises;
    QStringList removePremises;
    bool verified = false;
    /// Dakota's confirmation that the edit may outdate checks (`--outdate-checks`): the proofs
    /// he was shown and confirmed. Absent: not confirmed. See StatementEdit::outdateChecks.
    std::optional<QStringList> outdateChecks;
    /// Dakota's confirmation that the proof may be marked `failed`.
    bool failedConfirmed = false;
};

/// `loom edit-proof <proof-id>`: changes a proof's level, premises and body. Changing the body,
/// premises or level of a checked proof is refused unless `edit.outdateChecks` lists exactly the
/// proofs it would outdate now, as for editStatement() (Specs/m1-core.md, "The `loom` command";
/// the level is in the fingerprint, A18).
/// Setting `level = "failed"` on a proof that isn't is Dakota's click (Specs/m1-core.md,
/// "Checks", verdict `fail`), refused with `dakota-only` without `edit.failedConfirmed`; keeping
/// `failed`, or changing it to another level, isn't. Refusals come in this order: the rules (any
/// error the edit would add), then Dakota's `failed`, then outdated checks; so marking a checked
/// proof failed needs both confirmations.
/// @param typed The proof's ID as typed.
/// @returns The result; `changed` lists the fields that changed.
/// Side effects: rewrites the file in canonical form if anything changed.
ChangeResult editProof(const QString& root, const QString& typed, const ProofEdit& edit, const ChangeOptions& options = {});

/// What `loom add-check` asks for.
struct NewCheck {
    QString by;          ///< `dakota` or `agent:<model>`.
    QString verdict;     ///< `pass`, `needs-changes` or `fail`.
    QString changes;     ///< What needs to change; required on Dakota's `needs-changes`.
    /// The fingerprint of the version the checker read (`loom fingerprint` before reading).
    /// Agents must give it; if given, it must be the proof's fingerprint now.
    QString fingerprint;
    QString record;      ///< Required for agents.
    QString note;
};

/// The result of addCheck().
struct CheckAdded {
    /// How it went; `id` is the proof's ID and `path` its check history.
    ChangeResult change;
    /// The entry appended.
    Check check;
    /// The entry's index in the history (from 0).
    qsizetype index = 0;
    /// What the proof's checks say now, with the entry.
    CheckStanding standing;
};

/// `loom add-check <proof-id>`: appends a check to the proof's history (Specs/m1-core.md,
/// "Adding a check"). Loom computes the fingerprint itself; the date is `options.now`. The entry
/// is appended as text, so the entries before it stay byte for byte as they were: nothing ever
/// edits, reorders or deletes an entry.
/// Refuses: a check by Dakota without `dakotaConfirmed`; an agent's check without a fingerprint;
/// a fingerprint that isn't the proof's now ("changed after you read it"); and anything that
/// would add an error to the history (`check-by`, `check-verdict`, `check-changes-missing`,
/// `check-record-missing`, …).
/// @param typed           The proof's ID as typed.
/// @param dakotaConfirmed True only when Dakota confirmed at a terminal; stands for his click.
/// @returns The result. Side effects: writes `Proofs/<tag>/<proof-id>.checks.toml`.
CheckAdded addCheck(const QString& root, const QString& typed, const NewCheck& request, bool dakotaConfirmed,
                    const ChangeOptions& options = {});

/// `loom refute <id> --by <proof-id>`: marks a statement false (Specs/m1-core.md, "Statements",
/// D4): sets `refuted_by` to the proof and `refuted_on` to today, the date of Dakota's click.
/// Refuses: without `dakotaConfirmed`; a stub (it is cited from its paper, not stated here); a
/// statement already refuted; an ID or proof that names nothing; one of the statement's own
/// proofs (`refuted-self`: a refutation proves another statement); and anything that adds an error
/// (`refuted-no-status` for a definition, …). Neither field is in a fingerprint, so no check is
/// outdated. A stub's refusal names its paper by key.
/// @param typed           The statement's ID as typed.
/// @param typedProof      The refuting proof's ID as typed.
/// @param dakotaConfirmed True only when Dakota confirmed at a terminal; stands for his click.
/// @returns The result; `id` is the statement's tag, `changed` is `refuted_by, refuted_on`.
/// Side effects: rewrites the statement's file in canonical form.
ChangeResult refuteStatement(const QString& root, const QString& typed, const QString& typedProof, bool dakotaConfirmed,
                             const ChangeOptions& options = {});

/// The refusals of refuteStatement() that need only the project's contents: the statement and
/// the proof name something, the statement isn't a stub or already refuted, the proof isn't one
/// of its own (`refuted-self`), and its file can be rewritten. `loom refute` runs this before
/// asking Dakota, so he is never asked about a refutation that can't happen; refuteStatement()
/// runs it again under the write lock. It doesn't compute the errors the change would add.
/// @param contents   The project, as read without the lock.
/// @param typed      The statement's ID as typed.
/// @param typedProof The refuting proof's ID as typed.
/// @returns `Done` with `id` (the statement's tag) and `path` (its file) if nothing here refuses
///          it, else the refusal, as refuteStatement() would give it. No side effects.
ChangeResult checkRefutation(const ProjectContents& contents, const QString& typed, const QString& typedProof);

} // namespace loom
