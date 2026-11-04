#include "LoomCore/Edits.h"

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

namespace {

ChangeResult failure(ChangeOutcome outcome, const QString& code, const QString& message)
{
    ChangeResult result;
    result.outcome = outcome;
    result.code = code;
    result.message = message;
    return result;
}

ChangeResult refused(const QString& code, const QString& message)
{
    return failure(ChangeOutcome::Refused, code, message);
}

ChangeResult cantWork(const QString& code, const QString& message)
{
    return failure(ChangeOutcome::CantWork, code, message);
}

/// `<file>:<line>: <severity>: <message>`, as `loom validate` prints a problem.
QString problemLine(const Problem& problem)
{
    return QStringLiteral("%1:%2: %3: %4")
        .arg(problem.file).arg(problem.line)
        .arg(problem.severity == Severity::Error ? QStringLiteral("error") : QStringLiteral("warning"), problem.message);
}

/// A project opened for a change: `loom.toml` read, the write lock held, the contents read.
struct Session {
    OpenedProject project;
    ProjectContents contents;
    std::unique_ptr<QLockFile> lock;

    /// Every problem the project shows, for comparing before and after a change.
    std::vector<Problem> problems(const ProjectConfig& config, const std::vector<Problem>& tomlProblems,
                                  const ProjectContents& changed) const
    {
        std::vector<Problem> all = tomlProblems;
        const std::vector<Problem> more = checkContents(config, changed);
        all.insert(all.end(), more.begin(), more.end());
        return all;
    }
};

/// Opens a project for a change, in the order that writes nothing to a project Loom mustn't
/// touch: read `loom.toml` (stopping on a newer format), then take the lock (creating `.loom/`),
/// then read everything again, now that no other `loom` can be changing it.
/// @returns Nothing, with `result` set, if the project can't be worked on.
std::optional<Session> begin(const QString& root, const ChangeOptions& options, ChangeResult& result)
{
    const auto openChecked = [&](OpenedProject& project) {
        project = openProject(root);
        switch (project.status) {
        case OpenStatus::Opened: return true;
        case OpenStatus::NotAProject: result = cantWork(QStringLiteral("not-a-project"), project.message); break;
        case OpenStatus::Unreadable: result = cantWork(QStringLiteral("unreadable"), project.message); break;
        case OpenStatus::FormatTooNew: result = cantWork(QStringLiteral("format-too-new"), project.message); break;
        }
        return false;
    };

    Session session;
    if (!openChecked(session.project))
        return std::nullopt;

    const QString lockRelative = QStringLiteral(".loom/write.lock");
    const std::optional<QString> lockPath = resolveProjectPath(session.project.root, lockRelative);
    if (!lockPath || !QDir().mkpath(QFileInfo(*lockPath).absolutePath())) {
        result = cantWork(QStringLiteral("write-failed"), QStringLiteral("Loom couldn't create `%1`.")
            .arg(QDir::toNativeSeparators(session.project.root + u'/' + lockRelative)));
        return std::nullopt;
    }
    session.lock = std::make_unique<QLockFile>(*lockPath);
    if (!session.lock->tryLock(options.lockWaitMs)) {
        result = cantWork(QStringLiteral("locked"),
            QStringLiteral("Another loom command is changing this project. Try again when it has finished."));
        return std::nullopt;
    }

    if (!openChecked(session.project))
        return std::nullopt;
    ContentsLoading loading = loadContents(session.project.root);
    if (!loading.ok) {
        result = cantWork(QStringLiteral("unreadable"), loading.message);
        return std::nullopt;
    }
    session.contents = std::move(loading.contents);
    return session;
}

/// The errors in `after` that aren't in `before`. Problems are compared by code, file and message,
/// not line, since an edit moves lines; each problem in `before` excuses one equal one in `after`.
std::vector<Problem> addedErrors(const std::vector<Problem>& before, const std::vector<Problem>& after)
{
    QHash<QString, int> count;
    const auto key = [](const Problem& p) { return p.code + u'\x1f' + p.file + u'\x1f' + p.message; };
    for (const Problem& problem : before) {
        if (problem.severity == Severity::Error)
            ++count[key(problem)];
    }
    std::vector<Problem> added;
    for (const Problem& problem : after) {
        if (problem.severity != Severity::Error)
            continue;
        int& left = count[key(problem)];
        if (left > 0)
            --left;
        else
            added.push_back(problem);
    }
    return added;
}

/// A refusal listing the errors a change would add.
ChangeResult refusedFor(const std::vector<Problem>& problems, const QString& what)
{
    QStringList lines{QStringLiteral("Loom didn't %1, because it would add %2:")
                          .arg(what, problems.size() == 1 ? QStringLiteral("this problem") : QStringLiteral("these problems"))};
    for (const Problem& problem : problems)
        lines << problemLine(problem);
    ChangeResult result = refused(problems.front().code, lines.join(u'\n'));
    result.problems = problems;
    return result;
}

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

/// Turns IDs as typed into tags of existing statements.
/// @returns Nothing, with `result` set to the refusal, if one names nothing.
std::optional<QStringList> resolveAll(const ProjectContents& contents, const QStringList& typed, ChangeResult& result)
{
    QStringList tags;
    for (const QString& ref : typed) {
        const std::optional<QString> tag = resolveStatement(contents, ref);
        if (!tag) {
            result = refused(QStringLiteral("ref-unknown"),
                QStringLiteral("`%1` isn't a statement or paper stub in this project.").arg(ref));
            return std::nullopt;
        }
        tags << *tag;
    }
    return tags;
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

/// A refusal to rewrite a file that rewriting would damage.
ChangeResult notEditable(const QString& path, const std::vector<Problem>& problems)
{
    QStringList lines{QStringLiteral("Loom can't change %1 until these problems are fixed, since rewriting it would lose what it couldn't read:").arg(path)};
    for (const Problem& problem : problems)
        lines << problemLine(problem);
    return refused(QStringLiteral("not-editable"), lines.join(u'\n'));
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

    // Tags in use: every statement's, every alias, and every proof folder's, so a new statement
    // never adopts proofs left behind in a folder.
    QSet<QString> used;
    for (const StatementFile& file : contents.statements) {
        used.insert(file.tag);
        if (file.reading.parsed) {
            for (const QString& alias : file.reading.document.aliases)
                used.insert(alias);
        }
    }
    for (const ProofFile& proof : contents.proofs)
        used.insert(proof.folder);
    for (const VerboseFile& verbose : contents.verbose)
        used.insert(verbose.proofId.first(4));
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

} // namespace loom
