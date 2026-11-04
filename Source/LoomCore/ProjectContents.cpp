#include "LoomCore/ProjectContents.h"

#include "LoomCore/Cycles.h"
#include "LoomCore/Ids.h"
#include "LoomCore/ProjectFiles.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QSet>

#include <algorithm>

namespace loom {

namespace {

/// The entries of a project folder, files and folders, hidden ones too, sorted by name.
QFileInfoList entriesOf(const QString& folder)
{
    return QDir(folder).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Name);
}

/// Collects the problems that need several files, with the spec's messages.
struct ContentsChecker {
    const ProjectConfig& config;
    const ProjectContents& contents;
    std::vector<Problem> problems;
    /// Indexes, so checking stays linear in the size of the project.
    QHash<QString, const StatementFile*> statementByTag;
    QHash<QString, QString> tagByAlias;
    QSet<QString> proofIds;

    ContentsChecker(const ProjectConfig& projectConfig, const ProjectContents& projectContents)
        : config(projectConfig), contents(projectContents)
    {
        for (const StatementFile& file : contents.statements) {
            statementByTag.insert(file.tag, &file);
            if (file.reading.parsed) {
                for (const QString& alias : file.reading.document.aliases) {
                    if (!tagByAlias.contains(alias))
                        tagByAlias.insert(alias, file.tag);
                }
            }
        }
        for (const ProofFile& file : contents.proofs)
            proofIds.insert(file.id);
    }

    const StatementFile* statement(const QString& tag) const { return statementByTag.value(tag); }

    void add(const char* code, const QString& file, int line, const QString& message)
    {
        problems.push_back({Severity::Error, QString::fromLatin1(code), file, line, message});
    }

    /// Checks a reference in front matter to a statement (Specs/m1-core.md, "IDs": "files must
    /// use tags: an alias in front matter is an error that names the tag to write instead").
    /// @returns The referenced statement, or null after reporting why there isn't one. With
    ///          `unknownCode` null, an unknown tag is left for the caller to report.
    const StatementFile* reference(const QString& ref, const QString& field, const QString& file, int line,
                                   const char* unknownCode = "ref-unknown")
    {
        if (isTag(ref)) {
            const StatementFile* target = statement(ref);
            if (!target && unknownCode)
                add(unknownCode, file, line,
                    QStringLiteral("`%1` in `%2` isn't a statement or paper stub in this project.").arg(ref, field));
            return target;
        }
        const QString tag = tagByAlias.value(ref);
        if (!tag.isEmpty()) {
            add("ref-alias", file, line, QStringLiteral("`%1` is an old name of %2. Write `%2` here.").arg(ref, tag));
            return nullptr;
        }
        QString message = QStringLiteral("`%1` isn't a valid tag: four characters from 0–9 and A–Z without I, L, O and U.").arg(ref);
        if (const std::optional<QString> normalised = normaliseTag(ref))
            message += QStringLiteral(" Did you mean `%1`?").arg(*normalised);
        add("id-syntax", file, line, message);
        return nullptr;
    }

    void statements()
    {
        QSet<QString> declaredTags;
        for (const ProjectTag& tag : config.tags)
            declaredTags.insert(tag.id);

        QHash<QString, QString> pathOfId;
        QHash<QString, QString> tagOfAlias;
        for (const StatementFile& file : contents.statements) {
            const StatementReading& r = file.reading;
            if (!r.parsed)
                continue;
            const Statement& s = r.document;

            if (isTag(s.id)) {
                if (pathOfId.contains(s.id))
                    add("id-duplicate", file.path, r.lines.of(QStringLiteral("id")),
                        QStringLiteral("The tag `%1` is used by both %2 and %3.").arg(s.id, pathOfId.value(s.id), file.path));
                else
                    pathOfId.insert(s.id, file.path);
            }

            for (qsizetype i = 0; i < s.tags.size(); ++i) {
                if (!declaredTags.contains(s.tags[i]))
                    add("tag-unknown", file.path, r.lines.of(QStringLiteral("tags"), i),
                        QStringLiteral("`%1` isn't one of the project's tags. Add it with `loom new-tag`, or fix the spelling.").arg(s.tags[i]));
            }

            QSet<QString> ownAliases;
            for (qsizetype i = 0; i < s.aliases.size(); ++i) {
                const QString& alias = s.aliases[i];
                if (ownAliases.contains(alias) || !aliasProblem(alias).isEmpty())
                    continue; // Reported with the file: a repeat, or not an alias at all.
                ownAliases.insert(alias);
                if (tagOfAlias.contains(alias))
                    add("alias-duplicate", file.path, r.lines.of(QStringLiteral("aliases"), i),
                        QStringLiteral("`%1` is an alias of both %2 and %3.").arg(alias, tagOfAlias.value(alias), file.tag));
                else
                    tagOfAlias.insert(alias, file.tag);
            }

            const QString statedUsing = QStringLiteral("stated_using");
            for (qsizetype i = 0; i < s.statedUsing.size(); ++i) {
                const int line = r.lines.of(statedUsing, i);
                if (s.statedUsing[i] == file.tag)
                    add("stated-using-self", file.path, line, QStringLiteral("%1 can't be stated using itself.").arg(file.tag));
                else
                    reference(s.statedUsing[i], statedUsing, file.path, line);
            }

            if (!s.supersededBy.isEmpty()) {
                const int line = r.lines.of(QStringLiteral("superseded_by"));
                if (s.supersededBy == file.tag)
                    add("superseded-invalid", file.path, line,
                        QStringLiteral("%1 is superseded by %2, which is itself.").arg(file.tag, s.supersededBy));
                else if (!reference(s.supersededBy, QStringLiteral("superseded_by"), file.path, line, nullptr) && isTag(s.supersededBy))
                    add("superseded-invalid", file.path, line,
                        QStringLiteral("%1 is superseded by %2, which doesn't exist.").arg(file.tag, s.supersededBy));
            }

            const bool refuted = !s.refutedBy.isEmpty() || s.refutedOn.isValid();
            if (!s.refutedBy.isEmpty() && !proofIds.contains(s.refutedBy))
                add("refuted-unknown", file.path, r.lines.of(QStringLiteral("refuted_by")),
                    QStringLiteral("%1 is refuted by `%2`, which isn't a proof in this project.").arg(file.tag, s.refutedBy));
            if (refuted && statementKinds.contains(s.kind) && !isProvableKind(s.kind))
                add("refuted-no-status", file.path, r.lines.of(s.refutedBy.isEmpty() ? QStringLiteral("refuted_on") : QStringLiteral("refuted_by")),
                    QStringLiteral("%1 is a %2; only lemmas, propositions, theorems, corollaries and conjectures can be refuted.").arg(file.tag, s.kind));
        }
    }

    /// `stated_using` and `superseded_by` mustn't go round in a circle.
    void circles()
    {
        QMap<QString, QStringList> statedUsing;
        QMap<QString, QStringList> supersededBy;
        for (const StatementFile& file : contents.statements) {
            if (!file.reading.parsed)
                continue;
            const Statement& s = file.reading.document;
            for (const QString& ref : s.statedUsing) {
                if (statement(ref))
                    statedUsing[file.tag] << ref;
            }
            if (statement(s.supersededBy))
                supersededBy[file.tag] << s.supersededBy;
        }

        for (const QStringList& cycle : findCycles(statedUsing)) {
            const StatementFile* first = statement(cycle[0]);
            const FieldLines& lines = first->reading.lines;
            const qsizetype element = first->reading.document.statedUsing.indexOf(cycle[1]);
            add("stated-using-cycle", first->path, lines.of(QStringLiteral("stated_using"), element),
                QStringLiteral("`stated_using` goes round in a circle: %1.").arg(cycle.join(u" → ")));
        }
        for (const QStringList& cycle : findCycles(supersededBy)) {
            const StatementFile* first = statement(cycle[0]);
            add("superseded-invalid", first->path, first->reading.lines.of(QStringLiteral("superseded_by")),
                QStringLiteral("%1 is superseded by %2, which leads round in a circle: %3.")
                    .arg(cycle[0], cycle[1], cycle.join(u" → ")));
        }
    }

    void proofs()
    {
        for (const ProofFile& file : contents.proofs) {
            const ProofReading& r = file.reading;
            if (!r.parsed)
                continue;
            const Proof& p = r.document;
            const StatementFile* proved = statement(file.folder);
            const int provesLine = r.lines.of(QStringLiteral("proves"));

            if (!proved)
                add("proof-folder-unknown", file.path, 1,
                    QStringLiteral("`Proofs/%1/` is for proofs of %1, but there is no statement %1.").arg(file.folder));
            if (!p.proves.isEmpty()) {
                if (isTag(p.proves)) {
                    if (p.proves != file.folder)
                        add("proves-mismatch", file.path, provesLine,
                            QStringLiteral("%1 says it proves %2, but it's filed under %3.").arg(file.id, p.proves, file.folder));
                } else {
                    reference(p.proves, QStringLiteral("proves"), file.path, provesLine);
                }
            }
            if (proved && proved->reading.parsed) {
                const QString& kind = proved->reading.document.kind;
                if (statementKinds.contains(kind) && !isProvableKind(kind))
                    add("proof-of-no-status", file.path, provesLine,
                        QStringLiteral("%1 is a %2; only lemmas, propositions, theorems, corollaries and conjectures have proofs.")
                            .arg(file.folder, kind));
            }

            QSet<QString> seen;
            for (qsizetype i = 0; i < p.premises.size(); ++i) {
                const QString& premise = p.premises[i];
                if (premise == p.proves || seen.contains(premise))
                    continue; // premise-self or premise-duplicate, reported with the file.
                seen.insert(premise);
                const int line = r.lines.of(QStringLiteral("premises"), i);
                const StatementFile* target = reference(premise, QStringLiteral("premises"), file.path, line);
                if (target && target->reading.parsed) {
                    const QString& kind = target->reading.document.kind;
                    if (statementKinds.contains(kind) && !isProvableKind(kind))
                        add("premise-no-status", file.path, line,
                            QStringLiteral("%1 is a %2, so it can't be a premise. Definitions belong in the statement's `stated_using`.")
                                .arg(premise, kind));
                }
            }
        }

        for (const VerboseFile& file : contents.verbose) {
            if (!proofIds.contains(file.proofId))
                add("verbose-orphan", file.path, 1,
                    QStringLiteral("`%1` is the verbose version of %2, which doesn't exist.").arg(file.path, file.proofId));
        }
    }
};

/// The project-relative path `folder/name`.
QString join(const QString& folder, const QString& name)
{
    return folder + u'/' + name;
}

/// Adds a `stray-file` problem.
void stray(ProjectContents& contents, const QString& path, const QString& folder, const QString& holds)
{
    contents.listingProblems.push_back({Severity::Error, QStringLiteral("stray-file"), path, 1,
        QStringLiteral("`%1` doesn't belong in %2: it holds only %3.").arg(path, folder, holds)});
}

/// Reads one project file for loadContents().
/// @returns The bytes, or nothing after recording a `bad-path` problem or failing the loading.
std::optional<QByteArray> readFile(const QString& root, const QString& path, ContentsLoading& loading)
{
    if (const std::optional<QByteArray> bytes = readProjectFile(root, path))
        return bytes;
    if (!resolveProjectPath(root, path)) {
        loading.contents.listingProblems.push_back({Severity::Error, QStringLiteral("bad-path"), path, 1,
            QStringLiteral("`%1` must be a path inside the project, written with `/`, without `..`.").arg(path)});
    } else if (loading.ok) {
        loading.ok = false;
        loading.message = QStringLiteral("Loom couldn't read `%1`.").arg(QDir::toNativeSeparators(join(root, path)));
    }
    return std::nullopt;
}

/// The `<letters>` of `<tag>.<letters><suffix>`, or an empty string if `name` isn't of that form.
QString lettersOf(const QString& name, const QString& tag, QStringView suffix)
{
    if (!name.endsWith(suffix))
        return {};
    const std::optional<ProofIdParts> parts = splitProofId(name.chopped(suffix.size()));
    return parts && parts->tag == tag ? parts->letters : QString();
}

} // namespace

const StatementFile* ProjectContents::statement(const QString& tag) const
{
    const auto it = std::find_if(statements.begin(), statements.end(), [&](const StatementFile& s) { return s.tag == tag; });
    return it == statements.end() ? nullptr : &*it;
}

const ProofFile* ProjectContents::proof(const QString& id) const
{
    const auto it = std::find_if(proofs.begin(), proofs.end(), [&](const ProofFile& p) { return p.id == id; });
    return it == proofs.end() ? nullptr : &*it;
}

QString ProjectContents::tagWithAlias(const QString& alias) const
{
    for (const StatementFile& file : statements) {
        if (file.reading.parsed && file.reading.document.aliases.contains(alias))
            return file.tag;
    }
    return {};
}

ContentsLoading loadContents(const QString& root)
{
    ContentsLoading loading;
    loading.ok = true;
    ProjectContents& contents = loading.contents;

    const QString statementsHold = QStringLiteral("statements, each as `<tag>.tex`");
    for (const QFileInfo& entry : entriesOf(join(root, QStringLiteral("Statements")))) {
        const QString path = join(QStringLiteral("Statements"), entry.fileName());
        const QString tag = entry.fileName().chopped(std::min<qsizetype>(4, entry.fileName().size()));
        if (!entry.isFile() || !entry.fileName().endsWith(u".tex") || !isTag(tag)) {
            stray(contents, path, QStringLiteral("Statements/"), statementsHold);
            continue;
        }
        if (const std::optional<QByteArray> bytes = readFile(root, path, loading))
            contents.statements.push_back({path, tag, readStatement(*bytes, path)});
    }

    const QString proofsHold = QStringLiteral("a folder for each statement's proofs, named by its tag");
    for (const QFileInfo& folder : entriesOf(join(root, QStringLiteral("Proofs")))) {
        const QString tag = folder.fileName();
        const QString folderPath = join(QStringLiteral("Proofs"), tag);
        if (!folder.isDir() || !isTag(tag)) {
            stray(contents, folderPath, QStringLiteral("Proofs/"), proofsHold);
            continue;
        }
        const QString holds = QStringLiteral("the proofs of %1, their verbose versions and their checks").arg(tag);
        for (const QFileInfo& entry : entriesOf(folder.filePath())) {
            const QString name = entry.fileName();
            const QString path = join(folderPath, name);
            if (!entry.isFile()) {
                stray(contents, path, folderPath + u'/', holds);
            } else if (!lettersOf(name, tag, u".verbose.tex").isEmpty()) {
                if (const std::optional<QByteArray> bytes = readFile(root, path, loading))
                    contents.verbose.push_back({path, name.chopped(12), readVerboseProof(*bytes, path)});
            } else if (!lettersOf(name, tag, u".checks.toml").isEmpty()) {
                contents.checksFiles << path;
            } else if (!lettersOf(name, tag, u".tex").isEmpty()) {
                if (const std::optional<QByteArray> bytes = readFile(root, path, loading))
                    contents.proofs.push_back({path, tag, name.chopped(4), readProof(*bytes, path)});
            } else {
                stray(contents, path, folderPath + u'/', holds);
            }
        }
    }
    return loading;
}

std::vector<Problem> checkContents(const ProjectConfig& config, const ProjectContents& contents)
{
    ContentsChecker checker(config, contents);
    checker.problems = contents.listingProblems;
    for (const StatementFile& file : contents.statements)
        checker.problems.insert(checker.problems.end(), file.reading.problems.begin(), file.reading.problems.end());
    for (const ProofFile& file : contents.proofs)
        checker.problems.insert(checker.problems.end(), file.reading.problems.begin(), file.reading.problems.end());
    for (const VerboseFile& file : contents.verbose)
        checker.problems.insert(checker.problems.end(), file.reading.problems.begin(), file.reading.problems.end());
    checker.statements();
    checker.circles();
    checker.proofs();
    sortProblems(checker.problems);
    return checker.problems;
}

std::optional<QString> resolveStatement(const ProjectContents& contents, const QString& typed)
{
    if (const std::optional<QString> tag = normaliseTag(typed)) {
        if (contents.statement(*tag))
            return tag;
        return std::nullopt;
    }
    const QString tag = contents.tagWithAlias(typed);
    if (!tag.isEmpty())
        return tag;
    return std::nullopt;
}

std::optional<QString> resolveProof(const ProjectContents& contents, const QString& typed)
{
    const std::optional<QString> id = normaliseProofId(typed);
    if (id && contents.proof(*id))
        return id;
    return std::nullopt;
}

} // namespace loom
