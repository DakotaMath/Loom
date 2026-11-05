#include "LoomCore/EditorSaves.h"

#include "LoomCore/EditSession.h"
#include "LoomCore/FrontMatter.h"
#include "LoomCore/Ids.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProjectFiles.h"
#include "LoomCore/ProofChecks.h"
#include "LoomCore/StatementsAndProofs.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

namespace loom {

using namespace edits;

namespace {

/// The file on disk, as a save finds it under the lock.
struct OnDisk {
    bool exists = false;
    QByteArray bytes;
};

/// Reads the file and checks it is still what the editor loaded (Specs/m2-statements-review.md,
/// "Saving", step 1).
/// Rule: an empty `base` means the editor has no file (a new verbose version, or *Restore it with
/// my text* after a deletion), so the file must not exist; otherwise it must exist and its bytes
/// must hash to `base`.
/// @returns A refusal or failure, or nothing if the save may go on. Reads the file.
std::optional<ChangeResult> checkBase(const QString& root, const QString& path, const QString& base, OnDisk& disk)
{
    const std::optional<QString> absolute = resolveProjectPath(root, path);
    if (!absolute)
        return refused(QStringLiteral("bad-path"), QStringLiteral("`%1` isn't a path inside the project.").arg(path));
    disk.exists = QFileInfo::exists(*absolute);
    if (disk.exists) {
        const std::optional<QByteArray> bytes = readProjectFile(root, path);
        if (!bytes)
            return cantWork(QStringLiteral("unreadable"), QStringLiteral("Loom couldn't read %1.").arg(path));
        disk.bytes = *bytes;
    }
    QString why;
    if (base.isEmpty() && disk.exists)
        why = QStringLiteral("%1 was created on disk while you were editing, so Loom didn't save over it.").arg(path);
    else if (!base.isEmpty() && !disk.exists)
        why = QStringLiteral("%1 was deleted on disk while you were editing, so Loom didn't save it.").arg(path);
    else if (!base.isEmpty() && fileBase(disk.bytes) != base)
        why = QStringLiteral("%1 changed on disk while you were editing it, so Loom didn't save over it.").arg(path);
    if (why.isEmpty())
        return std::nullopt;
    ChangeResult result = refused(QStringLiteral("file-changed"), why);
    result.path = path;
    return result;
}

/// A refusal with its file.
ChangeResult refusedAt(const QString& path, const QString& code, const QString& message)
{
    ChangeResult result = refused(code, message);
    result.path = path;
    return result;
}

/// What a save writes: canonical front matter around the body as typed when every field was read
/// (formatted by `format`), else the text as typed, with CRLF made LF either way.
template <typename Reading, typename Format>
QByteArray bytesToWrite(const QByteArray& typed, const Reading& reading, Format format)
{
    return reading.lossless ? format(reading.document) : crlfToLf(typed);
}

/// Replaces or adds a file's entry in a list kept sorted by path.
template <typename Entry>
void putEntry(std::vector<Entry>& list, Entry entry)
{
    const auto at = std::find_if(list.begin(), list.end(), [&](const Entry& e) { return e.path == entry.path; });
    if (at != list.end()) {
        *at = std::move(entry);
        return;
    }
    const auto before = std::find_if(list.begin(), list.end(), [&](const Entry& e) { return e.path > entry.path; });
    list.insert(before, std::move(entry));
}

/// Adds `name` to `changed` when `differs`, keeping the spec's field order by the order of calls.
void note(QStringList& changed, const char* name, bool differs)
{
    if (differs)
        changed << QString::fromLatin1(name);
}

} // namespace

QString fileBase(const QByteArray& bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

ChangeResult saveStatementText(const QString& root, const QString& path, const QByteArray& text, const QString& base,
                               const SaveConfirmations& confirmations, const ChangeOptions& options)
{
    static const QRegularExpression statementPath(QStringLiteral("^Statements/([^/]+)\\.tex$"));
    const QRegularExpressionMatch match = statementPath.match(path);
    if (!match.hasMatch() || !isProjectPath(path))
        return refusedAt(path, QStringLiteral("bad-path"), QStringLiteral("`%1` isn't a statement's file.").arg(path));
    const QString tag = match.captured(1);

    ChangeResult result;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return result;
    OnDisk disk;
    if (std::optional<ChangeResult> refusal = checkBase(session->project.root, path, base, disk))
        return *refusal;
    result.id = tag;
    result.path = path;
    if (disk.exists && disk.bytes == text) {
        result.outcome = ChangeOutcome::Done;
        return result;
    }

    const StatementReading typed = readStatement(text, path);
    // Rule 2: a tag is permanent. A text whose front matter doesn't parse has no `id` to compare;
    // it is refused by the errors it adds instead.
    if (typed.parsed && !typed.document.id.isEmpty() && typed.document.id != tag)
        return refusedAt(path, QStringLiteral("id-changed"),
            QStringLiteral("A statement's tag is permanent; Loom can't change %1 to %2.").arg(tag, typed.document.id));
    const QByteArray bytes = bytesToWrite(text, typed, formatStatement);
    if (disk.exists && bytes == disk.bytes) {
        result.outcome = ChangeOutcome::Done;
        return result;
    }
    StatementReading reading = readStatement(bytes, path);

    const StatementFile* before = session->contents.statement(tag);
    const Statement old = before && before->reading.parsed ? before->reading.document : Statement{};
    const Statement& now = reading.document;
    note(result.changed, "kind", now.kind != old.kind);
    note(result.changed, "title", now.title != old.title);
    note(result.changed, "tags", now.tags != old.tags);
    note(result.changed, "stated_using", now.statedUsing != old.statedUsing);
    note(result.changed, "aliases", now.aliases != old.aliases);
    note(result.changed, "origin", now.origin != old.origin);
    note(result.changed, "superseded_by", now.supersededBy != old.supersededBy);
    note(result.changed, "refuted_by", now.refutedBy != old.refutedBy);
    note(result.changed, "refuted_on", now.refutedOn != old.refutedOn);
    note(result.changed, "created", now.created != old.created);
    note(result.changed, "verified", now.verified != old.verified);
    note(result.changed, "body", now.body != old.body);

    // Rule 4 (Specs/m2-statements-review.md, "Saving", step 2): setting `refuted_by` or
    // `refuted_on` marks the statement false, which is Dakota's click (M1, D4). Clearing them is an
    // ordinary edit ("Undoing a refutation … is an edit in raw mode, saved under the usual rules").
    const bool refutes = (!now.refutedBy.isEmpty() && now.refutedBy != old.refutedBy)
                         || (now.refutedOn.isValid() && now.refutedOn != old.refutedOn);

    ProjectContents changed = session->contents;
    putEntry(changed.statements, StatementFile{path, tag, std::move(reading)});
    return commit(*session, path, bytes, changed, QStringLiteral("save %1").arg(tag), result, confirmations.outdateChecks,
                  refutes && !confirmations.refutationConfirmed ? QStringLiteral("mark a statement false") : QString());
}

ChangeResult saveProofText(const QString& root, const QString& path, const QByteArray& text, const QString& base,
                           const SaveConfirmations& confirmations, const ChangeOptions& options)
{
    static const QRegularExpression proofPath(QStringLiteral("^Proofs/([^/]+)/([^/]+)\\.tex$"));
    const QRegularExpressionMatch match = proofPath.match(path);
    const std::optional<ProofIdParts> parts = match.hasMatch() ? splitProofId(match.captured(2)) : std::nullopt;
    if (!parts || parts->tag != match.captured(1) || !isProjectPath(path))
        return refusedAt(path, QStringLiteral("bad-path"), QStringLiteral("`%1` isn't a proof's file.").arg(path));
    const QString folder = match.captured(1);
    const QString id = match.captured(2);

    ChangeResult result;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return result;
    OnDisk disk;
    if (std::optional<ChangeResult> refusal = checkBase(session->project.root, path, base, disk))
        return *refusal;
    result.id = id;
    result.path = path;
    if (disk.exists && disk.bytes == text) {
        result.outcome = ChangeOutcome::Done;
        return result;
    }

    const ProofReading typed = readProof(text, path);
    if (typed.parsed && !typed.document.id.isEmpty() && typed.document.id != id)
        return refusedAt(path, QStringLiteral("id-changed"),
            QStringLiteral("A proof's ID is permanent; Loom can't change %1 to %2.").arg(id, typed.document.id));
    if (typed.parsed && !typed.document.proves.isEmpty() && typed.document.proves != folder)
        return refusedAt(path, QStringLiteral("id-changed"),
            QStringLiteral("%1 proves %2, and which statement a proof proves is permanent; Loom can't change it to %3.")
                .arg(id, folder, typed.document.proves));
    const QByteArray bytes = bytesToWrite(text, typed, formatProof);
    if (disk.exists && bytes == disk.bytes) {
        result.outcome = ChangeOutcome::Done;
        return result;
    }
    ProofReading reading = readProof(bytes, path);

    const ProofFile* before = session->contents.proof(id);
    const Proof old = before && before->reading.parsed ? before->reading.document : Proof{};
    const Proof& now = reading.document;
    note(result.changed, "premises", now.premises != old.premises);
    note(result.changed, "level", now.level != old.level);
    note(result.changed, "created", now.created != old.created);
    note(result.changed, "verified", now.verified != old.verified);
    note(result.changed, "body", now.body != old.body);

    // Rule 4, as editProof(): setting `failed` is Dakota's click (Specs/m1-core.md, "Checks",
    // verdict `fail`); keeping it, or leaving it for another level, isn't.
    const bool fails = now.level == u"failed" && old.level != u"failed";

    ProjectContents changed = session->contents;
    putEntry(changed.proofs, ProofFile{path, folder, id, std::move(reading)});
    return commit(*session, path, bytes, changed, QStringLiteral("save %1").arg(id), result, confirmations.outdateChecks,
                  fails && !confirmations.failedConfirmed ? QStringLiteral("mark a proof failed") : QString());
}

ChangeResult saveVerboseBody(const QString& root, const QString& proofId, const QByteArray& body, const QString& base,
                             const ChangeOptions& options)
{
    ChangeResult result;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return result;
    const ProjectContents& contents = session->contents;
    const ProofFile* proof = contents.proof(proofId);
    if (!proof)
        return refused(QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a proof in this project.").arg(proofId));
    const QString path = QStringLiteral("%1/%2.verbose.tex").arg(QFileInfo(proof->path).path(), proofId);
    OnDisk disk;
    if (std::optional<ChangeResult> refusal = checkBase(session->project.root, path, base, disk))
        return *refusal;
    result.id = proofId;
    result.path = path;

    const auto existing = std::find_if(contents.verbose.begin(), contents.verbose.end(),
                                       [&](const VerboseFile& v) { return v.path == path; });
    VerboseProof verbose;
    if (disk.exists && existing != contents.verbose.end()) {
        if (!existing->reading.lossless)
            return notEditable(path, existing->reading.problems);
        // Only the body changes: the fingerprint says which version of the proof it was written
        // from, and only *Mark up to date* moves it (Specs/m2-statements-review.md, "Verbose versions").
        verbose = existing->reading.document;
    } else {
        QString why;
        const std::optional<QString> fingerprint = proofFingerprint(contents, proofId, &why);
        if (!fingerprint)
            return refusedAt(path, QStringLiteral("no-fingerprint"), why);
        verbose.proof = proofId;
        verbose.fingerprint = *fingerprint;
        verbose.created = options.today;
        result.changed << QStringLiteral("proof") << QStringLiteral("fingerprint") << QStringLiteral("created");
    }
    const QByteArray typed = crlfToLf(body);
    if (verbose.body == typed && disk.exists) {
        result.outcome = ChangeOutcome::Done;
        return result;
    }
    verbose.body = typed;
    result.changed << QStringLiteral("body");

    const QByteArray bytes = formatVerboseProof(verbose);
    ProjectContents changed = contents;
    putEntry(changed.verbose, VerboseFile{path, proofId, readVerboseProof(bytes, path)});
    return commit(*session, path, bytes, changed, QStringLiteral("save the verbose version of %1").arg(proofId), result);
}

ChangeResult markVerboseUpToDate(const QString& root, const QString& proofId, const QString& base, const ChangeOptions& options)
{
    ChangeResult result;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return result;
    const ProjectContents& contents = session->contents;
    const ProofFile* proof = contents.proof(proofId);
    if (!proof)
        return refused(QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a proof in this project.").arg(proofId));
    const QString path = QStringLiteral("%1/%2.verbose.tex").arg(QFileInfo(proof->path).path(), proofId);
    const auto existing = std::find_if(contents.verbose.begin(), contents.verbose.end(),
                                       [&](const VerboseFile& v) { return v.path == path; });
    if (existing == contents.verbose.end())
        return refusedAt(path, QStringLiteral("not-there"), QStringLiteral("%1 has no verbose version.").arg(proofId));
    OnDisk disk;
    if (std::optional<ChangeResult> refusal = checkBase(session->project.root, path, base, disk))
        return *refusal;
    if (!existing->reading.lossless)
        return notEditable(path, existing->reading.problems);
    QString why;
    const std::optional<QString> fingerprint = proofFingerprint(contents, proofId, &why);
    if (!fingerprint)
        return refusedAt(path, QStringLiteral("no-fingerprint"), why);

    result.id = proofId;
    result.path = path;
    VerboseProof verbose = existing->reading.document;
    if (verbose.fingerprint == *fingerprint) {
        result.outcome = ChangeOutcome::Done;
        return result;
    }
    verbose.fingerprint = *fingerprint;
    result.changed << QStringLiteral("fingerprint");
    const QByteArray bytes = formatVerboseProof(verbose);
    ProjectContents changed = contents;
    putEntry(changed.verbose, VerboseFile{path, proofId, readVerboseProof(bytes, path)});
    return commit(*session, path, bytes, changed, QStringLiteral("mark the verbose version of %1 up to date").arg(proofId), result);
}

} // namespace loom
