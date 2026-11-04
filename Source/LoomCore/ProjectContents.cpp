#include "LoomCore/ProjectContents.h"

#include "LoomCore/Cycles.h"
#include "LoomCore/Ids.h"
#include "LoomCore/PaperKeys.h"
#include "LoomCore/ProjectFiles.h"
#include "LoomCore/Status.h"

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

/// A node of the graph: a statement or a paper stub, which share one namespace of tags.
struct Node {
    QString tag;
    QString path;
    /// Its kind, if its file could be read; else empty.
    QString kind;
    bool parsed = false;
    /// The stub, if it is one.
    const StubFile* stub = nullptr;
    const FieldLines* lines = nullptr;
    const QStringList* statedUsing = nullptr;
};

/// Collects the problems that need several files, with the spec's messages.
struct ContentsChecker {
    const ProjectConfig& config;
    const ProjectContents& contents;
    std::vector<Problem> problems;
    /// Indexes, so most lookups don't scan the lists. Not all: premiseCycles() and keyOf() still
    /// look statements and papers up one by one, so checking isn't linear in the size of the
    /// project; at M1's sizes that costs nothing measurable.
    QHash<QString, Node> nodeByTag;
    QHash<QString, QString> tagByAlias;
    QSet<QString> proofIds;
    QSet<QString> declaredTags;

    /// Builds the indexes: every statement's and stub's node (statements first, so a stub
    /// reusing a statement's tag is the one reported; the first by path otherwise), their
    /// aliases, the proof IDs and `loom.toml`'s tags.
    /// @param projectConfig   The project's `loom.toml`; kept by reference.
    /// @param projectContents The files to check; kept by reference, and must outlive the checker.
    /// No side effects beyond the checker itself.
    ContentsChecker(const ProjectConfig& projectConfig, const ProjectContents& projectContents)
        : config(projectConfig), contents(projectContents)
    {
        for (const StatementFile& file : contents.statements) {
            const StatementReading& r = file.reading;
            if (!nodeByTag.contains(file.tag))
                nodeByTag.insert(file.tag, {file.tag, file.path, r.parsed ? r.document.kind : QString(), r.parsed, nullptr,
                                            &r.lines, &r.document.statedUsing});
            if (r.parsed)
                aliasesOf(file.tag, r.document.aliases);
        }
        for (const StubFile& file : contents.stubs) {
            const StubReading& r = file.reading;
            if (!nodeByTag.contains(file.tag))
                nodeByTag.insert(file.tag, {file.tag, file.path, r.parsed ? r.document.kind : QString(), r.parsed, &file,
                                            &r.lines, &r.document.statedUsing});
            if (r.parsed)
                aliasesOf(file.tag, r.document.aliases);
        }
        for (const ProofFile& file : contents.proofs)
            proofIds.insert(file.id);
        for (const ProjectTag& tag : config.tags)
            declaredTags.insert(tag.id);
    }

    /// Indexes a node's aliases; the first node to claim an alias keeps it.
    void aliasesOf(const QString& tag, const QStringList& aliases)
    {
        for (const QString& alias : aliases) {
            if (!tagByAlias.contains(alias))
                tagByAlias.insert(alias, tag);
        }
    }

    /// The statement or stub tagged `tag`, or null.
    const Node* node(const QString& tag) const
    {
        const auto it = nodeByTag.constFind(tag);
        return it == nodeByTag.constEnd() ? nullptr : &*it;
    }

    /// Records a problem.
    void add(const char* code, const QString& file, int line, const QString& message, Severity severity = Severity::Error)
    {
        problems.push_back({severity, QString::fromLatin1(code), file, line, message});
    }

    /// The key a paper is known by in messages: its key if it has one, else its folder's name.
    QString keyOf(const QString& folder) const
    {
        const PaperFile* paper = contents.paper(folder);
        return paper && paper->reading.parsed && !paper->reading.document.key.isEmpty() ? paper->reading.document.key : folder;
    }

    /// Checks a reference in front matter to a statement or stub (Specs/m1-core.md, "IDs": "files
    /// must use tags: an alias in front matter is an error that names the tag to write instead").
    /// @returns The referenced node, or null after reporting why there isn't one. With
    ///          `unknownCode` null, an unknown tag is left for the caller to report.
    const Node* reference(const QString& ref, const QString& field, const QString& file, int line,
                          const char* unknownCode = "ref-unknown")
    {
        if (isTag(ref)) {
            const Node* target = node(ref);
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

    /// Checks `stated_using` of a statement or stub.
    void statedUsing(const QString& tag, const QString& path, const QStringList& refs, const FieldLines& lines)
    {
        const QString field = QStringLiteral("stated_using");
        for (qsizetype i = 0; i < refs.size(); ++i) {
            const int line = lines.of(field, i);
            if (refs[i] == tag)
                add("stated-using-self", path, line, QStringLiteral("%1 can't be stated using itself.").arg(tag));
            else
                reference(refs[i], field, path, line);
        }
    }

    /// Tags are used once across statements and stubs, and so are aliases.
    void identities()
    {
        QHash<QString, QString> pathOfId;
        QHash<QString, QString> tagOfAlias;
        const auto visit = [&](const QString& path, const QString& tag, const QString& id, const QStringList& aliases,
                               const FieldLines& lines) {
            if (isTag(id)) {
                if (pathOfId.contains(id))
                    add("id-duplicate", path, lines.of(QStringLiteral("id")),
                        QStringLiteral("The tag `%1` is used by both %2 and %3.").arg(id, pathOfId.value(id), path));
                else
                    pathOfId.insert(id, path);
            }
            QSet<QString> own;
            for (qsizetype i = 0; i < aliases.size(); ++i) {
                const QString& alias = aliases[i];
                if (own.contains(alias) || !aliasProblem(alias).isEmpty())
                    continue; // Reported with the file: a repeat, or not an alias at all.
                own.insert(alias);
                if (tagOfAlias.contains(alias))
                    add("alias-duplicate", path, lines.of(QStringLiteral("aliases"), i),
                        QStringLiteral("`%1` is an alias of both %2 and %3.").arg(alias, tagOfAlias.value(alias), tag));
                else
                    tagOfAlias.insert(alias, tag);
            }
        };
        for (const StatementFile& file : contents.statements) {
            if (file.reading.parsed)
                visit(file.path, file.tag, file.reading.document.id, file.reading.document.aliases, file.reading.lines);
        }
        for (const StubFile& file : contents.stubs) {
            if (file.reading.parsed)
                visit(file.path, file.tag, file.reading.document.id, file.reading.document.aliases, file.reading.lines);
        }
    }

    /// Checks each statement's tags, `stated_using`, `superseded_by` and refutation.
    void statements()
    {
        for (const StatementFile& file : contents.statements) {
            const StatementReading& r = file.reading;
            if (!r.parsed)
                continue;
            const Statement& s = r.document;

            for (qsizetype i = 0; i < s.tags.size(); ++i) {
                if (!declaredTags.contains(s.tags[i]))
                    add("tag-unknown", file.path, r.lines.of(QStringLiteral("tags"), i),
                        QStringLiteral("`%1` isn't one of the project's tags. Add it with `loom new-tag`, or fix the spelling.").arg(s.tags[i]));
            }

            statedUsing(file.tag, file.path, s.statedUsing, r.lines);

            // `superseded_by` is "the tag of another statement" (Specs/m1-core.md, "Statements"),
            // so a paper stub, which shares the tags, doesn't qualify (open question S6).
            if (!s.supersededBy.isEmpty()) {
                const int line = r.lines.of(QStringLiteral("superseded_by"));
                if (s.supersededBy == file.tag) {
                    add("superseded-invalid", file.path, line,
                        QStringLiteral("%1 is superseded by %2, which is itself.").arg(file.tag, s.supersededBy));
                } else if (const Node* target = reference(s.supersededBy, QStringLiteral("superseded_by"), file.path, line, nullptr)) {
                    if (target->stub)
                        add("superseded-invalid", file.path, line,
                            QStringLiteral("%1 is superseded by %2, which is a result from %3, not a statement of this project.")
                                .arg(file.tag, s.supersededBy, keyOf(target->stub->paperFolder)));
                } else if (isTag(s.supersededBy)) {
                    add("superseded-invalid", file.path, line,
                        QStringLiteral("%1 is superseded by %2, which doesn't exist.").arg(file.tag, s.supersededBy));
                }
            }

            const bool refuted = !s.refutedBy.isEmpty() || s.refutedOn.isValid();
            if (!s.refutedBy.isEmpty() && !proofIds.contains(s.refutedBy)) {
                add("refuted-unknown", file.path, r.lines.of(QStringLiteral("refuted_by")),
                    QStringLiteral("%1 is refuted by `%2`, which isn't a proof in this project.").arg(file.tag, s.refutedBy));
            } else if (const std::optional<ProofIdParts> by = splitProofId(s.refutedBy); by && by->tag == file.tag) {
                // A proof of the statement itself argues for it, so it can't refute it (open
                // question S26): a refutation is a proof of another statement, such as its negation.
                add("refuted-self", file.path, r.lines.of(QStringLiteral("refuted_by")),
                    QStringLiteral("%1 is refuted by `%2`, which is one of its own proofs. A statement is refuted by a proof of "
                                   "another statement, such as its negation.").arg(file.tag, s.refutedBy));
            }
            if (refuted && statementKinds.contains(s.kind) && !isProvableKind(s.kind))
                add("refuted-no-status", file.path, r.lines.of(s.refutedBy.isEmpty() ? QStringLiteral("refuted_on") : QStringLiteral("refuted_by")),
                    QStringLiteral("%1 is a %2; only lemmas, propositions, theorems, corollaries and conjectures can be refuted.").arg(file.tag, s.kind));
        }
    }

    /// Checks each stub's `stated_using`, `paper_premises` and versions against its paper.
    void stubs()
    {
        for (const StubFile& file : contents.stubs) {
            const StubReading& r = file.reading;
            if (!r.parsed)
                continue;
            const Stub& s = r.document;
            statedUsing(file.tag, file.path, s.statedUsing, r.lines);

            // The paper's own proof structure: stubs of the same paper only.
            const QString premisesField = QStringLiteral("paper_premises");
            for (qsizetype i = 0; i < s.paperPremises.size(); ++i) {
                const QString& ref = s.paperPremises[i];
                const int line = r.lines.of(premisesField, i);
                const Node* target = ref == file.tag ? nullptr : reference(ref, premisesField, file.path, line, nullptr);
                if ((ref == file.tag || isTag(ref)) && (!target || !target->stub || target->stub->paperFolder != file.paperFolder))
                    add("stub-paper-premise", file.path, line,
                        QStringLiteral("`%1` in `paper_premises` isn't a result of the same paper.").arg(ref));
            }

            const PaperFile* paper = contents.paper(file.paperFolder);
            if (!paper || !paper->reading.parsed)
                continue;
            const Paper& p = paper->reading.document;
            if (!s.extractedFrom.isEmpty() && !p.version(s.extractedFrom))
                add("version-unknown", file.path, r.lines.of(QStringLiteral("extracted_from")),
                    QStringLiteral("`extracted_from` names version `%1`, which this paper doesn't have.").arg(s.extractedFrom));
            for (const Locator& locator : s.locators) {
                if (!p.version(locator.version))
                    add("version-unknown", file.path, locator.line,
                        QStringLiteral("`locators` names version `%1`, which this paper doesn't have.").arg(locator.version));
            }
        }
    }

    /// Checks each paper's folder, keys, tags and sessions.
    void papers()
    {
        // Every key and old key, case-folded, to the papers using it, for `paper-key-duplicate`:
        // keys compare ignoring case (A3), since a key is a folder name and Windows folder names
        // ignore case.
        QHash<QString, QString> folderOfName;
        std::vector<KeyedPaper> keyed;
        for (const PaperFile& file : contents.papers) {
            if (file.reading.parsed)
                keyed.push_back({file.reading.document.key, file.reading.document.aliases, &file.reading.document});
        }

        for (const PaperFile& file : contents.papers) {
            const PaperReading& r = file.reading;
            if (!r.parsed)
                continue;
            const Paper& p = r.document;
            const int keyLine = r.lines.of(QStringLiteral("key"));

            if (!p.key.isEmpty() && p.key != file.folder)
                add("paper-key-mismatch", file.path, keyLine,
                    QStringLiteral("The key is `%1` but the folder is `%2`; they must match.").arg(p.key, file.folder));

            std::vector<std::pair<QString, int>> names;
            if (!p.key.isEmpty())
                names.emplace_back(p.key, keyLine);
            for (qsizetype i = 0; i < p.aliases.size(); ++i)
                names.emplace_back(p.aliases[i], r.lines.of(QStringLiteral("aliases"), i));
            QSet<QString> own;
            for (const auto& [name, line] : names) {
                const QString folded = name.toCaseFolded();
                if (own.contains(folded))
                    continue; // A repeat within the paper, reported with the file.
                own.insert(folded);
                if (folderOfName.contains(folded))
                    add("paper-key-duplicate", file.path, line,
                        QStringLiteral("`%1` is the key or an old key of more than one paper.").arg(name));
                else
                    folderOfName.insert(folded, file.folder);
            }

            if (!p.key.isEmpty()) {
                std::vector<KeyedPaper> others;
                for (const KeyedPaper& other : keyed) {
                    if (other.paper != &p)
                        others.push_back(other);
                }
                const KeyBase base = baseKey(p);
                if (base.key.isEmpty()) {
                    // With no authors or editors, `authors-missing` already says so (or the list
                    // couldn't be read, which is reported too). Without a year or Latin letters,
                    // the key rule's own message (Specs/m1-core.md, "The key rule", steps 1 and 3).
                    if (!p.keyNames().empty())
                        add("paper-key-rule", file.path, keyLine, base.problem, Severity::Warning);
                } else if (!keyFollowsRule(p.key, base.key, others)) {
                    const KeyAssignment expected = assignKey(p, others);
                    add("paper-key-rule", file.path, keyLine,
                        QStringLiteral("By the key rule this paper's key is `%1`, not `%2`. Run `loom rekey %2`.")
                            .arg(expected.key.isEmpty() ? base.key : expected.key, p.key),
                        Severity::Warning);
                }
            }

            for (qsizetype i = 0; i < p.tags.size(); ++i) {
                if (!declaredTags.contains(p.tags[i]))
                    add("tag-unknown", file.path, r.lines.of(QStringLiteral("tags"), i),
                        QStringLiteral("`%1` isn't one of the project's tags. Add it with `loom new-tag`, or fix the spelling.").arg(p.tags[i]));
            }

            for (const ReadingEntry& e : p.readings) {
                if (!e.session.isEmpty() && !file.sessions.contains(e.session))
                    add("session-missing", file.path, e.lines.of(QStringLiteral("session")),
                        QStringLiteral("Reading of %1 links `%2`, which doesn't exist.").arg(e.date.toString(Qt::ISODate), e.session),
                        Severity::Warning);
            }
        }
    }

    /// `stated_using`, `superseded_by` and premises mustn't go round in a circle.
    void circles()
    {
        // Premises (Specs/m1-core.md, "Cycles"): reported on the first proof, in path order, of
        // the cycle's first statement that lists the next one.
        for (const QStringList& cycle : premiseCycles(contents)) {
            for (const ProofFile& file : contents.proofs) {
                if (file.folder != cycle[0] || !file.reading.parsed)
                    continue;
                const qsizetype element = file.reading.document.premises.indexOf(cycle[1]);
                if (element < 0)
                    continue;
                add("premise-cycle", file.path, file.reading.lines.of(QStringLiteral("premises"), element),
                    QStringLiteral("Premises go round in a circle: %1. A statement can't help prove itself.").arg(cycle.join(u" → ")));
                break;
            }
        }

        QMap<QString, QStringList> statedUsing;
        QMap<QString, QStringList> supersededBy;
        for (const Node& n : nodeByTag) {
            if (!n.parsed)
                continue;
            for (const QString& ref : *n.statedUsing) {
                if (node(ref))
                    statedUsing[n.tag] << ref;
            }
        }
        for (const StatementFile& file : contents.statements) {
            if (file.reading.parsed && node(file.reading.document.supersededBy))
                supersededBy[file.tag] << file.reading.document.supersededBy;
        }

        for (const QStringList& cycle : findCycles(statedUsing)) {
            const Node* first = node(cycle[0]);
            const qsizetype element = first->statedUsing->indexOf(cycle[1]);
            add("stated-using-cycle", first->path, first->lines->of(QStringLiteral("stated_using"), element),
                QStringLiteral("`stated_using` goes round in a circle: %1.").arg(cycle.join(u" → ")));
        }
        for (const QStringList& cycle : findCycles(supersededBy)) {
            const Node* first = node(cycle[0]);
            add("superseded-invalid", first->path, first->lines->of(QStringLiteral("superseded_by")),
                QStringLiteral("%1 is superseded by %2, which leads round in a circle: %3.")
                    .arg(cycle[0], cycle[1], cycle.join(u" → ")));
        }
    }

    /// Checks each proof's folder, `proves` and premises, and the orphans beside them.
    void proofs()
    {
        for (const ProofFile& file : contents.proofs) {
            const ProofReading& r = file.reading;
            if (!r.parsed)
                continue;
            const Proof& p = r.document;
            const Node* proved = node(file.folder);
            const int provesLine = r.lines.of(QStringLiteral("proves"));

            if (!proved)
                add("proof-folder-unknown", file.path, 1,
                    QStringLiteral("`Proofs/%1/` is for proofs of %1, but there is no statement %1.").arg(file.folder));
            else if (proved->stub)
                add("proof-of-stub", file.path, provesLine,
                    QStringLiteral("%1 is a result from %2; it's cited from the paper, not proved here.")
                        .arg(file.folder, keyOf(proved->stub->paperFolder)));
            if (!p.proves.isEmpty()) {
                if (isTag(p.proves)) {
                    if (p.proves != file.folder)
                        add("proves-mismatch", file.path, provesLine,
                            QStringLiteral("%1 says it proves %2, but it's filed under %3.").arg(file.id, p.proves, file.folder));
                } else {
                    reference(p.proves, QStringLiteral("proves"), file.path, provesLine);
                }
            }
            if (proved && proved->parsed && !proved->stub) {
                const QString& kind = proved->kind;
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
                const Node* target = reference(premise, QStringLiteral("premises"), file.path, line);
                if (target && target->parsed) {
                    const QString& kind = target->kind;
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
        for (const ChecksFile& file : contents.checks) {
            if (!proofIds.contains(file.proofId))
                add("checks-orphan", file.path, 1,
                    QStringLiteral("`%1` holds checks of %2, which doesn't exist.").arg(file.path, file.proofId));
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

/// Reads `Papers/`: each paper's folder, its `paper.toml` and its stubs.
void loadPapers(const QString& root, ContentsLoading& loading)
{
    ProjectContents& contents = loading.contents;
    const QString papersHold = QStringLiteral("a folder for each paper, named by its key");
    for (const QFileInfo& folder : entriesOf(join(root, QStringLiteral("Papers")))) {
        const QString key = folder.fileName();
        const QString folderPath = join(QStringLiteral("Papers"), key);
        if (!folder.isDir()) {
            stray(contents, folderPath, QStringLiteral("Papers/"), papersHold);
            continue;
        }
        const QString holds = QStringLiteral("paper.toml, Statements/, notes.md, Sessions/ and local/");
        PaperFile paper{join(folderPath, QStringLiteral("paper.toml")), key, {}, {}};
        // Whether paper.toml is there, and whether Loom read it: one refused (`bad-path`) is
        // there, so it isn't also missing.
        bool hasPaper = false;
        bool sawPaper = false;
        for (const QFileInfo& entry : entriesOf(folder.filePath())) {
            const QString name = entry.fileName();
            const QString path = join(folderPath, name);
            if (name == u"paper.toml" && entry.isFile()) {
                sawPaper = true;
                if (const std::optional<QByteArray> bytes = readFile(root, path, loading)) {
                    paper.reading = readPaper(*bytes, path);
                    hasPaper = true;
                }
            } else if (name == u"Statements" && entry.isDir()) {
                const QString stubsHold = QStringLiteral("the paper's stubs, each as `<tag>.toml`");
                for (const QFileInfo& stubEntry : entriesOf(entry.filePath())) {
                    const QString stubName = stubEntry.fileName();
                    const QString stubPath = join(path, stubName);
                    const QString tag = stubName.chopped(std::min<qsizetype>(5, stubName.size()));
                    if (!stubEntry.isFile() || !stubName.endsWith(u".toml") || !isTag(tag)) {
                        stray(contents, stubPath, path + u'/', stubsHold);
                        continue;
                    }
                    if (const std::optional<QByteArray> bytes = readFile(root, stubPath, loading))
                        contents.stubs.push_back({stubPath, key, tag, readStub(*bytes, stubPath)});
                }
            } else if (name == u"Sessions" && entry.isDir()) {
                // Reading sessions are M6's; M1 only needs to know which exist.
                for (const QFileInfo& session : entriesOf(entry.filePath())) {
                    if (session.isDir())
                        paper.sessions << QStringLiteral("Sessions/") + session.fileName();
                }
            } else if (!((name == u"notes.md" && entry.isFile()) || (name == u"local" && entry.isDir()))) {
                stray(contents, path, folderPath + u'/', holds);
            }
        }
        if (hasPaper)
            contents.papers.push_back(std::move(paper));
        else if (!sawPaper)
            contents.listingProblems.push_back({Severity::Error, QStringLiteral("missing-field"), folderPath, 1,
                QStringLiteral("`paper.toml` is missing; every paper folder needs one.")});
    }
}

} // namespace

const StatementFile* ProjectContents::statement(const QString& tag) const
{
    const auto it = std::find_if(statements.begin(), statements.end(), [&](const StatementFile& s) { return s.tag == tag; });
    return it == statements.end() ? nullptr : &*it;
}

const StubFile* ProjectContents::stub(const QString& tag) const
{
    const auto it = std::find_if(stubs.begin(), stubs.end(), [&](const StubFile& s) { return s.tag == tag; });
    return it == stubs.end() ? nullptr : &*it;
}

const PaperFile* ProjectContents::paper(const QString& folder) const
{
    const auto it = std::find_if(papers.begin(), papers.end(), [&](const PaperFile& p) { return p.folder == folder; });
    return it == papers.end() ? nullptr : &*it;
}

const PaperFile* ProjectContents::paperWithKey(const QString& key) const
{
    for (const Qt::CaseSensitivity cs : {Qt::CaseSensitive, Qt::CaseInsensitive}) {
        for (const PaperFile& file : papers) {
            if (file.folder.compare(key, cs) == 0)
                return &file;
        }
        for (const PaperFile& file : papers) {
            if (file.reading.parsed
                && (file.reading.document.key.compare(key, cs) == 0 || file.reading.document.aliases.contains(key, cs)))
                return &file;
        }
    }
    return nullptr;
}

const ProofFile* ProjectContents::proof(const QString& id) const
{
    const auto it = std::find_if(proofs.begin(), proofs.end(), [&](const ProofFile& p) { return p.id == id; });
    return it == proofs.end() ? nullptr : &*it;
}

const ChecksFile* ProjectContents::checksOf(const QString& id) const
{
    const auto it = std::find_if(checks.begin(), checks.end(), [&](const ChecksFile& c) { return c.proofId == id; });
    return it == checks.end() ? nullptr : &*it;
}

QString ProjectContents::tagWithAlias(const QString& alias) const
{
    for (const StatementFile& file : statements) {
        if (file.reading.parsed && file.reading.document.aliases.contains(alias))
            return file.tag;
    }
    for (const StubFile& file : stubs) {
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
        contents.proofFolders << tag;
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
                if (const std::optional<QByteArray> bytes = readFile(root, path, loading))
                    contents.checks.push_back({path, name.chopped(12), readChecks(*bytes, path)});
            } else if (!lettersOf(name, tag, u".tex").isEmpty()) {
                if (const std::optional<QByteArray> bytes = readFile(root, path, loading))
                    contents.proofs.push_back({path, tag, name.chopped(4), readProof(*bytes, path)});
            } else {
                stray(contents, path, folderPath + u'/', holds);
            }
        }
    }

    loadPapers(root, loading);
    return loading;
}

std::vector<Problem> checkContents(const ProjectConfig& config, const ProjectContents& contents)
{
    ContentsChecker checker(config, contents);
    checker.problems = contents.listingProblems;
    const auto own = [&](const std::vector<Problem>& problems) {
        checker.problems.insert(checker.problems.end(), problems.begin(), problems.end());
    };
    for (const StatementFile& file : contents.statements)
        own(file.reading.problems);
    for (const ProofFile& file : contents.proofs)
        own(file.reading.problems);
    for (const VerboseFile& file : contents.verbose)
        own(file.reading.problems);
    for (const ChecksFile& file : contents.checks)
        own(file.reading.problems);
    for (const PaperFile& file : contents.papers)
        own(file.reading.problems);
    for (const StubFile& file : contents.stubs)
        own(file.reading.problems);
    checker.identities();
    checker.statements();
    checker.stubs();
    checker.papers();
    checker.circles();
    checker.proofs();
    sortProblems(checker.problems);
    return checker.problems;
}

QSet<QString> tagsInUse(const ProjectContents& contents)
{
    QSet<QString> used;
    for (const StatementFile& file : contents.statements) {
        used.insert(file.tag);
        if (file.reading.parsed) {
            for (const QString& alias : file.reading.document.aliases)
                used.insert(alias);
        }
    }
    for (const StubFile& file : contents.stubs) {
        used.insert(file.tag);
        if (file.reading.parsed) {
            for (const QString& alias : file.reading.document.aliases)
                used.insert(alias);
        }
    }
    // Every proof folder, whatever it holds: a folder with only a check history would otherwise
    // hand its history to a new statement. The files are counted too, for contents an edit
    // changed in memory, which has no listing of folders.
    for (const QString& folder : contents.proofFolders)
        used.insert(folder);
    for (const ProofFile& proof : contents.proofs)
        used.insert(proof.folder);
    for (const VerboseFile& verbose : contents.verbose)
        used.insert(verbose.proofId.first(4));
    for (const ChecksFile& checks : contents.checks)
        used.insert(checks.proofId.first(4));
    return used;
}

std::optional<QString> resolveStatement(const ProjectContents& contents, const QString& typed)
{
    if (const std::optional<QString> tag = normaliseTag(typed)) {
        if (contents.statement(*tag) || contents.stub(*tag))
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
