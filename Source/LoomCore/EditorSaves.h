#pragma once

// Saving what the Statements tab's editor holds (Specs/m2-statements-review.md, "The Statements
// tab", "Saving" and "Verbose versions"): a statement's or proof's whole file text, checked against
// the file the editor loaded, under the rules of editStatement() and editProof(); and the body of a
// verbose version. Like every change, each takes the write lock and writes nothing it refuses.

#include "LoomCore/Edits.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

namespace loom {

/// What an editor calls `base`: the SHA-256 of a file's bytes as it loaded them, in lowercase hex
/// (Specs/m2-statements-review.md, "Saving"). No side effects.
QString fileBase(const QByteArray& bytes);

/// Dakota's clicks a save carries, each given only after his confirmation dialog
/// (Specs/m2-statements-review.md, "Dakota's clicks").
struct SaveConfirmations {
    /// The proofs whose checks he confirmed outdating, exactly as a refusal's `outdatedChecks`
    /// named them; absent: not confirmed. See StatementEdit::outdateChecks.
    std::optional<QStringList> outdateChecks;
    /// He confirmed marking the proof `failed`.
    bool failedConfirmed = false;
    /// He confirmed marking the statement false (setting `refuted_by` and `refuted_on`).
    bool refutationConfirmed = false;
};

/// Saves a statement file from the editor's whole text (Specs/m2-statements-review.md, "Saving").
/// Rules, in order, each a refusal that writes nothing:
/// 1. `file-changed` if the file on disk isn't `base` any more: its bytes hash to something else,
///    or it is gone while `base` isn't empty, or it exists while `base` is empty (`base` empty
///    means a file that doesn't exist yet, as for *Restore it with my text*).
/// 2. `id-changed` if the text's `id` isn't the tag its file name gives: a tag is permanent.
/// 3. Any validation error the text would add to the project (M1's rule for every change, E6).
/// 4. `dakota-only` if it sets `refuted_by` or `refuted_on` to something new without
///    `confirmations.refutationConfirmed` (clearing them, to undo a refutation, needs no click).
/// 5. `outdates-checks` unless `confirmations.outdateChecks` lists exactly the checked proofs it
///    would outdate now (M1, A16).
/// What is written: the front matter in canonical form and the body as typed (CRLF made LF), if
/// Loom could read every field of the text; otherwise, the text as typed, which rule 3 has already
/// checked adds no error (so a file that was already broken can be fixed a step at a time).
/// @param root  The project folder.
/// @param path  The file, project-relative: `Statements/<tag>.tex`.
/// @param text  The editor's whole file, front matter included.
/// @param base  fileBase() of the file when the editor loaded it; empty for a new file.
/// @returns The result; `changed` lists the fields that changed (`body` for the body), empty with
///          `Done` when the text is the file already. Side effects: writes the file.
ChangeResult saveStatementText(const QString& root, const QString& path, const QByteArray& text, const QString& base,
                               const SaveConfirmations& confirmations = {}, const ChangeOptions& options = {});

/// Saves a proof file from the editor's whole text, as saveStatementText() does a statement's.
/// Its rule 2 refuses an `id` that isn't the one its file name gives and a `proves` that isn't its
/// folder's tag (`id-changed`); its rule 4 refuses setting `level = "failed"` on a proof that
/// wasn't, without `confirmations.failedConfirmed` (M1: `failed` is Dakota's judgement). A refusal
/// for rule 4 carries in `outdatedChecks` the checks the save would outdate too, so one dialog can
/// ask for both clicks.
/// @param path The file, project-relative: `Proofs/<tag>/<tag>.<letters>.tex`.
/// @returns The result. Side effects: writes the file.
ChangeResult saveProofText(const QString& root, const QString& path, const QByteArray& text, const QString& base,
                           const SaveConfirmations& confirmations = {}, const ChangeOptions& options = {});

/// Saves the body of a proof's verbose version, `<proof-id>.verbose.tex`, beside the proof
/// (Specs/m2-statements-review.md, "Verbose versions").
/// Rules: `ref-unknown` if the proof doesn't exist; `file-changed` as for saveStatementText();
/// an existing verbose file Loom can't fully read is `not-editable`. If there is none (and `base` is
/// empty), it is created with `proof`, the proof's current `fingerprint` and `created` today
/// (`no-fingerprint` if the fingerprint can't be computed). Saving an existing one changes only
/// its body: its `fingerprint` stays, so a stale version stays stale until markVerboseUpToDate().
/// Then any error it would add refuses it. The verbose version is in no fingerprint, so no check
/// is outdated.
/// @param proofId The proof's ID, exactly as files write it (`4KX7.a`).
/// @param body    The editor's text, written as typed (CRLF made LF).
/// @returns The result; `path` is the verbose file. Side effects: writes it.
ChangeResult saveVerboseBody(const QString& root, const QString& proofId, const QByteArray& body, const QString& base,
                             const ChangeOptions& options = {});

/// *Mark up to date*: sets a verbose version's `fingerprint` to its proof's current one, Dakota's
/// click saying the verbose text now matches the proof (Specs/m2-statements-review.md, "Verbose
/// versions"). Refuses: `ref-unknown`, `not-there` (no verbose version), `file-changed`,
/// `not-editable`, `no-fingerprint`. Up to date already is `Done` with nothing changed.
/// @param base fileBase() of the verbose file as the editor shows it.
/// @returns The result; `changed` is `fingerprint` when it changed. Side effects: writes the file.
ChangeResult markVerboseUpToDate(const QString& root, const QString& proofId, const QString& base,
                                 const ChangeOptions& options = {});

} // namespace loom
