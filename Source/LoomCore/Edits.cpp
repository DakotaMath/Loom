#include "LoomCore/Edits.h"

#include "LoomCore/EditSession.h"

#include "LoomCore/FrontMatter.h"
#include "LoomCore/Ids.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProofChecks.h"
#include "LoomCore/ProjectFiles.h"
#include "LoomCore/ProjectToml.h"
#include "LoomCore/TomlWriter.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QLockFile>
#include <QSet>
#include <QTimeZone>

#include <algorithm>
#include <memory>

namespace loom {

using namespace edits;

namespace {

/// "A, B and C".
QString listed(const QStringList& items)
{
    if (items.size() < 2)
        return items.join(QString());
    return items.first(items.size() - 1).join(u", ") + QStringLiteral(" and ") + items.last();
}

/// Writes `bytes` to `path` if, with the file's contents replaced by `changed`, the project has no
/// error it didn't have before, and the change outdates no checked proof's checks unless
/// `outdateChecks` (Specs/m1-core.md, "Statements": editing text in a checked proof's fingerprint
/// needs Dakota's confirmation, and the refusal names the proofs).
/// @param what For the refusal: `create the statement`, …
ChangeResult commit(Session& session, const QString& path, const QByteArray& bytes, const ProjectContents& changed,
                    const QString& what, ChangeResult result, bool outdateChecks = false)
{
    const ProjectConfig& config = session.project.config;
    const std::vector<Problem> added = addedErrors(session.problems(config, session.project.problems, session.contents),
                                                   session.problems(config, session.project.problems, changed));
    result.path = path;
    if (!added.empty()) {
        ChangeResult refusal = refusedFor(added, what);
        refusal.id = result.id;
        refusal.path = path;
        return refusal;
    }
    result.outdatedChecks = outdatedChecks(session.contents, changed);
    if (!result.outdatedChecks.isEmpty() && !outdateChecks) {
        const bool one = result.outdatedChecks.size() == 1;
        result.outcome = ChangeOutcome::Refused;
        result.code = QStringLiteral("outdates-checks");
        result.message = QStringLiteral("Loom didn't %1, because it would outdate the checks of %2: %3 checked, and the "
                                        "text %4 read would change. Only Dakota can outdate checks (`--outdate-checks`, "
                                        "at a terminal).")
                             .arg(what, listed(result.outdatedChecks), one ? QStringLiteral("it is") : QStringLiteral("they are"),
                                  one ? QStringLiteral("its checks") : QStringLiteral("their checks"));
        return result;
    }
    const QString error = writeProjectFile(session.project.root, path, bytes);
    if (!error.isEmpty())
        return cantWork(QStringLiteral("write-failed"), error);
    result.outcome = ChangeOutcome::Done;
    return result;
}

/// Where an ID typed by a person sits in a list of tags: as written, as a forgiving tag, or as
/// an alias of a listed tag. So a reference that names nothing can still be removed.
/// @returns The index, or -1.
qsizetype findTyped(const ProjectContents& contents, const QStringList& list, const QString& typed)
{
    if (const qsizetype exact = list.indexOf(typed); exact >= 0)
        return exact;
    if (const std::optional<QString> tag = normaliseTag(typed))
        return list.indexOf(*tag);
    if (const std::optional<QString> tag = resolveStatement(contents, typed))
        return list.indexOf(*tag);
    return -1;
}

/// Adds typed IDs to a list of tags, refusing one already there.
bool addRefs(const ProjectContents& contents, QStringList& list, const QStringList& typed, const QString& owner,
             const QString& field, ChangeResult& result)
{
    const std::optional<QStringList> tags = resolveAll(contents, typed, result);
    if (!tags)
        return false;
    for (const QString& tag : *tags) {
        if (list.contains(tag)) {
            result = refused(QStringLiteral("already-there"), QStringLiteral("`%1` of %2 already lists %3.").arg(field, owner, tag));
            return false;
        }
        list << tag;
    }
    return true;
}

/// Removes typed IDs from a list of tags, refusing one that isn't there.
bool removeRefs(const ProjectContents& contents, QStringList& list, const QStringList& typed, const QString& owner,
                const QString& field, ChangeResult& result)
{
    for (const QString& ref : typed) {
        const qsizetype index = findTyped(contents, list, ref);
        if (index < 0) {
            result = refused(QStringLiteral("not-there"), QStringLiteral("`%1` of %2 doesn't list `%3`.").arg(field, owner, ref));
            return false;
        }
        list.removeAt(index);
    }
    return true;
}

/// The highest proof number used in a statement's proof folder, by any proof, verbose version or
/// checks file, so a deleted proof's letter isn't reused.
qint64 highestProofNumber(const ProjectContents& contents, const QString& tag)
{
    qint64 highest = 0;
    const auto consider = [&](const QString& id) {
        if (const std::optional<ProofIdParts> parts = splitProofId(id); parts && parts->tag == tag)
            highest = std::max(highest, proofNumber(parts->letters));
    };
    for (const ProofFile& proof : contents.proofs)
        consider(proof.id);
    for (const VerboseFile& verbose : contents.verbose)
        consider(verbose.proofId);
    for (const ChecksFile& checks : contents.checks)
        consider(checks.proofId);
    return highest;
}

} // namespace

ChangeResult addTag(const QString& root, const QString& tag, const QString& kind, const QString& title,
                    const ChangeOptions& options)
{
    ChangeResult result;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return result;

    const QString tomlPath = QStringLiteral("loom.toml");
    const std::optional<QByteArray> bytes = readProjectFile(session->project.root, tomlPath);
    if (!bytes)
        return cantWork(QStringLiteral("unreadable"), QStringLiteral("Loom couldn't read loom.toml."));

    // Appending keeps the rest of the file, comments included, exactly as it was.
    QByteArray changed = *bytes;
    if (!changed.isEmpty() && !changed.endsWith('\n'))
        changed += '\n';
    changed += QStringLiteral("\n[[tags]]\nid = %1\nkind = %2\ntitle = %3\n")
                   .arg(tomlBasicString(tag), tomlBasicString(kind), tomlBasicString(title)).toUtf8();

    const ProjectTomlReading reading = readProjectToml(changed, session->project.root);
    const bool declared = std::any_of(reading.config.tags.begin(), reading.config.tags.end(),
                                      [&](const ProjectTag& t) { return t.id == tag; });
    const std::vector<Problem> added = addedErrors(
        session->problems(session->project.config, session->project.problems, session->contents),
        session->problems(reading.config, reading.problems, session->contents));
    if (!added.empty()) {
        result = refusedFor(added, QStringLiteral("add the tag"));
        result.id = tag;
        return result;
    }
    if (!declared || reading.formatTooNew) {
        std::vector<Problem> problems = session->project.problems;
        QStringList lines{QStringLiteral("Loom didn't add the tag: loom.toml has problems to fix first:")};
        for (const Problem& problem : problems)
            lines << problemLine(problem);
        return refused(QStringLiteral("not-editable"), lines.join(u'\n'));
    }

    const QString error = writeProjectFile(session->project.root, tomlPath, changed);
    if (!error.isEmpty())
        return cantWork(QStringLiteral("write-failed"), error);
    result.outcome = ChangeOutcome::Done;
    result.id = tag;
    result.path = tomlPath;
    return result;
}

ChangeResult createStatement(const QString& root, const NewStatement& request, const ChangeOptions& options)
{
    ChangeResult result;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return result;
    const ProjectContents& contents = session->contents;

    if (isBlankBody(request.body))
        return refused(QStringLiteral("body-empty"), QStringLiteral("A statement needs its text: the body can't be empty."));
    const std::optional<QStringList> statedUsing = resolveAll(contents, request.statedUsing, result);
    if (!statedUsing)
        return result;

    const QSet<QString> used = tagsInUse(contents);
    const std::optional<QString> tag = generateTag([&](const QString& t) { return used.contains(t); }, *options.random);
    if (!tag)
        return refused(QStringLiteral("no-tag"),
            QStringLiteral("Loom couldn't find a free tag after 100 tries; the project may be nearly full."));

    Statement statement;
    statement.id = *tag;
    statement.kind = request.kind;
    statement.title = request.title;
    statement.tags = request.tags;
    statement.statedUsing = *statedUsing;
    statement.origin = request.origin;
    statement.created = options.today;
    statement.body = bodyFromText(request.body);

    const QString path = QStringLiteral("Statements/%1.tex").arg(*tag);
    const QByteArray bytes = formatStatement(statement);
    ProjectContents changed = contents;
    changed.statements.push_back({path, *tag, readStatement(bytes, path)});

    result.id = *tag;
    return commit(*session, path, bytes, changed, QStringLiteral("create the statement"), result);
}

ChangeResult editStatement(const QString& root, const QString& typed, const StatementEdit& edit, const ChangeOptions& options)
{
    ChangeResult result;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return result;
    const ProjectContents& contents = session->contents;

    const std::optional<QString> tag = resolveStatement(contents, typed);
    if (!tag)
        return refused(QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a statement in this project.").arg(typed));
    if (const StubFile* stub = contents.stub(*tag); stub && !contents.statement(*tag))
        return refused(QStringLiteral("ref-unknown"),
            QStringLiteral("%1 is a result from %2, not a statement of this project: its stub is `%3`.")
                .arg(*tag, stub->paperFolder, stub->path));
    const StatementFile& file = *contents.statement(*tag);
    if (!file.reading.lossless)
        return notEditable(file.path, file.reading.problems);
    if (edit.body && isBlankBody(*edit.body))
        return refused(QStringLiteral("body-empty"), QStringLiteral("A statement needs its text: the body can't be empty."));

    const Statement& before = file.reading.document;
    Statement s = before;
    if (edit.kind)
        s.kind = *edit.kind;
    if (edit.title)
        s.title = *edit.title;
    const QString tagsField = QStringLiteral("tags");
    for (const QString& add : edit.addTags) {
        if (s.tags.contains(add))
            return refused(QStringLiteral("already-there"), QStringLiteral("`tags` of %1 already lists `%2`.").arg(*tag, add));
        s.tags << add;
    }
    for (const QString& remove : edit.removeTags) {
        if (!s.tags.removeOne(remove))
            return refused(QStringLiteral("not-there"), QStringLiteral("`tags` of %1 doesn't list `%2`.").arg(*tag, remove));
    }
    const QString statedUsing = QStringLiteral("stated_using");
    if (!addRefs(contents, s.statedUsing, edit.addStatedUsing, *tag, statedUsing, result)
        || !removeRefs(contents, s.statedUsing, edit.removeStatedUsing, *tag, statedUsing, result))
        return result;
    if (edit.supersededBy) {
        const std::optional<QStringList> target = resolveAll(contents, {*edit.supersededBy}, result);
        if (!target)
            return result;
        s.supersededBy = target->front();
    }
    if (edit.verified)
        s.verified = options.today;
    if (edit.body)
        s.body = bodyFromText(*edit.body);

    // In the spec's field order, then the body.
    const std::pair<const char*, bool> fields[] = {
        {"kind", s.kind != before.kind},
        {"title", s.title != before.title},
        {"tags", s.tags != before.tags},
        {"stated_using", s.statedUsing != before.statedUsing},
        {"superseded_by", s.supersededBy != before.supersededBy},
        {"verified", s.verified != before.verified},
        {"body", s.body != before.body},
    };
    result.id = *tag;
    for (const auto& [name, changedField] : fields) {
        if (changedField)
            result.changed << QString::fromLatin1(name);
    }
    if (result.changed.isEmpty()) {
        result.outcome = ChangeOutcome::Done;
        result.path = file.path;
        return result;
    }

    const QByteArray bytes = formatStatement(s);
    ProjectContents changed = contents;
    for (StatementFile& entry : changed.statements) {
        if (entry.path == file.path)
            entry.reading = readStatement(bytes, file.path);
    }
    return commit(*session, file.path, bytes, changed, QStringLiteral("change %1").arg(*tag), result, edit.outdateChecks);
}

ChangeResult createProof(const QString& root, const NewProof& request, const ChangeOptions& options)
{
    ChangeResult result;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return result;
    const ProjectContents& contents = session->contents;

    const std::optional<QString> tag = resolveStatement(contents, request.statement);
    if (!tag)
        return refused(QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a statement in this project.").arg(request.statement));
    if (isBlankBody(request.body))
        return refused(QStringLiteral("body-empty"), QStringLiteral("A proof needs its text: the body can't be empty."));
    const std::optional<QStringList> premises = resolveAll(contents, request.premises, result);
    if (!premises)
        return result;

    Proof proof;
    proof.id = *tag + u'.' + proofLetters(highestProofNumber(contents, *tag) + 1);
    proof.proves = *tag;
    proof.premises = *premises;
    proof.level = request.level;
    proof.created = options.today;
    proof.body = bodyFromText(request.body);

    const QString path = QStringLiteral("Proofs/%1/%2.tex").arg(*tag, proof.id);
    const QByteArray bytes = formatProof(proof);
    ProjectContents changed = contents;
    changed.proofs.push_back({path, *tag, proof.id, readProof(bytes, path)});

    result.id = proof.id;
    return commit(*session, path, bytes, changed, QStringLiteral("create the proof"), result);
}

ChangeResult editProof(const QString& root, const QString& typed, const ProofEdit& edit, const ChangeOptions& options)
{
    ChangeResult result;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return result;
    const ProjectContents& contents = session->contents;

    const std::optional<QString> id = resolveProof(contents, typed);
    if (!id)
        return refused(QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a proof in this project.").arg(typed));
    const ProofFile& file = *contents.proof(*id);
    if (!file.reading.lossless)
        return notEditable(file.path, file.reading.problems);
    if (edit.body && isBlankBody(*edit.body))
        return refused(QStringLiteral("body-empty"), QStringLiteral("A proof needs its text: the body can't be empty."));

    const Proof& before = file.reading.document;
    Proof p = before;
    const QString premises = QStringLiteral("premises");
    if (!addRefs(contents, p.premises, edit.addPremises, *id, premises, result)
        || !removeRefs(contents, p.premises, edit.removePremises, *id, premises, result))
        return result;
    if (edit.level)
        p.level = *edit.level;
    if (edit.verified)
        p.verified = options.today;
    if (edit.body)
        p.body = bodyFromText(*edit.body);

    const std::pair<const char*, bool> fields[] = {
        {"premises", p.premises != before.premises},
        {"level", p.level != before.level},
        {"verified", p.verified != before.verified},
        {"body", p.body != before.body},
    };
    result.id = *id;
    for (const auto& [name, changedField] : fields) {
        if (changedField)
            result.changed << QString::fromLatin1(name);
    }
    if (result.changed.isEmpty()) {
        result.outcome = ChangeOutcome::Done;
        result.path = file.path;
        return result;
    }

    const QByteArray bytes = formatProof(p);
    ProjectContents changed = contents;
    for (ProofFile& entry : changed.proofs) {
        if (entry.path == file.path)
            entry.reading = readProof(bytes, file.path);
    }
    return commit(*session, file.path, bytes, changed, QStringLiteral("change %1").arg(*id), result, edit.outdateChecks);
}

CheckAdded addCheck(const QString& root, const QString& typed, const NewCheck& request, bool dakotaConfirmed,
                    const ChangeOptions& options)
{
    CheckAdded added;
    ChangeResult& result = added.change;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return added;
    const ProjectContents& contents = session->contents;

    const std::optional<QString> id = resolveProof(contents, typed);
    if (!id) {
        result = refused(QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a proof in this project.").arg(typed));
        return added;
    }
    const auto refuse = [&](const char* code, const QString& message) {
        result = refused(QString::fromLatin1(code), message);
        result.id = *id;
        return added;
    };
    Check check;
    check.by = request.by;
    if (check.byDakota() && !dakotaConfirmed)
        return refuse("dakota-only", QStringLiteral("Only Dakota can add Dakota's checks, and only at a terminal."));

    // Loom computes the fingerprint itself, never on the caller's word (Specs/m1-core.md, "Adding a check").
    QString why;
    const std::optional<QString> current = proofFingerprint(contents, *id, &why);
    if (!current)
        return refuse("no-fingerprint", why);
    if (check.byAgent() && request.fingerprint.isEmpty())
        return refuse("fingerprint-needed",
            QStringLiteral("An agent's check needs `--fingerprint`: what `loom fingerprint %1` printed before you read the proof.").arg(*id));
    if (!request.fingerprint.isEmpty() && request.fingerprint != *current)
        return refuse("proof-changed",
            QStringLiteral("%1 changed after you read it (you read %2, it is now %3). Read it again before checking it.")
                .arg(*id, request.fingerprint, *current));

    const QDateTime& now = options.now;
    check.date = QDateTime(now.date(), QTime(now.time().hour(), now.time().minute(), now.time().second()),
                           QTimeZone::fromSecondsAheadOfUtc(now.offsetFromUtc()));
    check.fingerprint = *current;
    check.verdict = request.verdict;
    QString changes = request.changes;
    changes.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    if (changes.trimmed().isEmpty())
        changes.clear();
    else if (changes.contains(u'\n') && !changes.endsWith(u'\n'))
        changes += u'\n'; // So the closing `"""` of the multi-line string sits on its own line.
    check.changes = changes;
    check.record = request.record;
    check.note = request.note;

    // Appended as text: the entries already there stay byte for byte (rule 1, append-only).
    const QString path = QStringLiteral("%1/%2.checks.toml").arg(QFileInfo(contents.proof(*id)->path).path(), *id);
    QByteArray bytes;
    if (const ChecksFile* existing = contents.checksOf(*id)) {
        if (!existing->reading.parsed) {
            result = notEditable(existing->path, existing->reading.problems);
            result.id = *id;
            return added;
        }
        const std::optional<QByteArray> before = readProjectFile(session->project.root, path);
        if (!before) {
            result = cantWork(QStringLiteral("unreadable"), QStringLiteral("Loom couldn't read %1.").arg(path));
            return added;
        }
        bytes = *before;
        if (!bytes.isEmpty() && !bytes.endsWith('\n'))
            bytes += '\n';
        if (!bytes.trimmed().isEmpty())
            bytes += '\n';
    }
    bytes += formatCheck(check);

    const ChecksReading reading = readChecks(bytes, path);
    ProjectContents changed = contents;
    const auto entry = std::find_if(changed.checks.begin(), changed.checks.end(), [&](const ChecksFile& c) { return c.path == path; });
    if (entry != changed.checks.end())
        entry->reading = reading;
    else
        changed.checks.push_back({path, *id, reading});

    result.id = *id;
    result = commit(*session, path, bytes, changed, QStringLiteral("add the check"), result);
    if (result.outcome == ChangeOutcome::Done) {
        added.check = check;
        added.index = static_cast<qsizetype>(reading.checks.size()) - 1;
        added.standing = standingOf(reading.checks, *current);
    }
    return added;
}

ChangeResult refuteStatement(const QString& root, const QString& typed, const QString& typedProof, bool dakotaConfirmed,
                             const ChangeOptions& options)
{
    ChangeResult result;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return result;
    const ProjectContents& contents = session->contents;

    if (!dakotaConfirmed)
        return refused(QStringLiteral("dakota-only"), QStringLiteral("Only Dakota can refute a statement, and only at a terminal."));
    const std::optional<QString> tag = resolveStatement(contents, typed);
    if (!tag)
        return refused(QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a statement in this project.").arg(typed));
    if (const StubFile* stub = contents.stub(*tag); stub && !contents.statement(*tag))
        return refused(QStringLiteral("ref-unknown"),
            QStringLiteral("%1 is a result from %2, cited from the paper; only statements of this project can be refuted.")
                .arg(*tag, stub->paperFolder));
    const std::optional<QString> proof = resolveProof(contents, typedProof);
    if (!proof)
        return refused(QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a proof in this project.").arg(typedProof));
    const StatementFile& file = *contents.statement(*tag);
    if (!file.reading.lossless)
        return notEditable(file.path, file.reading.problems);

    Statement s = file.reading.document;
    if (!s.refutedBy.isEmpty() || s.refutedOn.isValid())
        return refused(QStringLiteral("already-there"),
            QStringLiteral("%1 is already refuted by %2, accepted %3.")
                .arg(*tag, s.refutedBy.isEmpty() ? QStringLiteral("(no proof)") : s.refutedBy,
                     s.refutedOn.isValid() ? s.refutedOn.toString(Qt::ISODate) : QStringLiteral("(no date)")));
    s.refutedBy = *proof;
    s.refutedOn = options.today;

    const QByteArray bytes = formatStatement(s);
    ProjectContents changed = contents;
    for (StatementFile& entry : changed.statements) {
        if (entry.path == file.path)
            entry.reading = readStatement(bytes, file.path);
    }
    result.id = *tag;
    result.changed = QStringList{QStringLiteral("refuted_by"), QStringLiteral("refuted_on")};
    return commit(*session, file.path, bytes, changed, QStringLiteral("refute %1").arg(*tag), result);
}

} // namespace loom
