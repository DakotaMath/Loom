#include "LoomCore/PaperEdits.h"

#include "LoomCore/Bibliography.h"
#include "LoomCore/EditSession.h"
#include "LoomCore/Ids.h"
#include "LoomCore/PaperKeys.h"
#include "LoomCore/ProjectFiles.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace loom {

using namespace edits;

namespace {

/// `Papers/<folder>/paper.toml`. No side effects.
QString paperPath(const QString& folder)
{
    return QStringLiteral("Papers/%1/paper.toml").arg(folder);
}

/// The other papers of the project, as the key rule sees them.
std::vector<KeyedPaper> othersThan(const ProjectContents& contents, const QString& folder)
{
    std::vector<KeyedPaper> others;
    for (const PaperFile& file : contents.papers) {
        if (file.folder != folder && file.reading.parsed)
            others.push_back({file.reading.document.key, file.reading.document.aliases, &file.reading.document});
    }
    return others;
}

/// `cite`'s default (Specs/m1-core.md, "Papers": "`published` if there is a published version,
/// else the newest arXiv version").
QString defaultCite(const Paper& paper)
{
    if (paper.version(QStringLiteral("published")))
        return QStringLiteral("published");
    const PaperVersion* newest = nullptr;
    for (const PaperVersion& v : paper.versions) {
        if (v.kind == u"arxiv" && (!newest || v.arxivNumber() > newest->arxivNumber()))
            newest = &v;
    }
    if (newest)
        return newest->id;
    return paper.versions.empty() ? QString() : paper.versions.front().id;
}

/// A paper rename to carry out: the model is changed first, the disk after the checks pass.
struct Rename {
    QString from;       ///< The folder's name now.
    QString fromKey;    ///< The key now, for the result.
    QString to;
    QByteArray bytes;   ///< The new paper.toml.
};

/// Renames a paper in the model: its folder, key and aliases, and its stubs' paths (Specs/m1-core.md,
/// "The key rule", step 5: "the folder is renamed, `key` changes, the old key is added to `aliases`";
/// unless only its case changes, since keys compare ignoring case, A3).
/// @returns The rename to carry out, or nothing after setting `result` (the paper can't be rewritten).
std::optional<Rename> renameInModel(const QString& root, ProjectContents& model, const QString& from, const QString& to,
                                    ChangeResult& result)
{
    const auto file = std::find_if(model.papers.begin(), model.papers.end(), [&](const PaperFile& p) { return p.folder == from; });
    if (file == model.papers.end() || !file->reading.lossless) {
        result = notEditable(paperPath(from), file == model.papers.end() ? std::vector<Problem>() : file->reading.problems);
        return std::nullopt;
    }
    Paper paper = file->reading.document;
    Rename rename{from, paper.key, to, {}};
    // Keys compare ignoring case (A3), so a rename that only changes the key's case keeps no old
    // key: `smithjones24` would be SmithJones24's own key, not an old one. For the same reason
    // an old key equal to the new key in any case goes.
    const bool sameKey = paper.key.compare(to, Qt::CaseInsensitive) == 0;
    if (!paper.key.isEmpty() && !sameKey && !paper.aliases.contains(paper.key, Qt::CaseInsensitive))
        paper.aliases << paper.key;
    paper.aliases.removeIf([&](const QString& alias) { return alias.compare(to, Qt::CaseInsensitive) == 0; });
    paper.key = to;
    rename.bytes = formatPaper(paper);
    file->folder = to;
    file->path = paperPath(to);
    file->reading = readPaper(rename.bytes, file->path);

    const QString oldPrefix = QStringLiteral("Papers/%1/").arg(from);
    const QString newPrefix = QStringLiteral("Papers/%1/").arg(to);
    for (StubFile& stub : model.stubs) {
        if (stub.paperFolder != from)
            continue;
        const std::optional<QByteArray> bytes = readProjectFile(root, stub.path);
        if (!bytes) {
            result = cantWork(QStringLiteral("unreadable"), QStringLiteral("Loom couldn't read %1.").arg(stub.path));
            return std::nullopt;
        }
        stub.paperFolder = to;
        stub.path = newPrefix + stub.path.mid(oldPrefix.size());
        stub.reading = readStub(*bytes, stub.path);
    }
    return rename;
}

/// The problems as they will read after the renames: files under a renamed folder move with it.
std::vector<Problem> movedBy(std::vector<Problem> problems, const std::vector<Rename>& renames)
{
    for (Problem& problem : problems) {
        for (const Rename& rename : renames) {
            const QString oldPrefix = QStringLiteral("Papers/%1").arg(rename.from);
            if (problem.file == oldPrefix || problem.file.startsWith(oldPrefix + u'/'))
                problem.file = QStringLiteral("Papers/%1").arg(rename.to) + problem.file.mid(oldPrefix.size());
        }
    }
    return problems;
}

/// A rename done on disk, with what undoing it needs.
struct DoneRename {
    QString fromPath; ///< The folder's absolute path before.
    QString toPath;   ///< Its absolute path now.
    QString to;       ///< Its name now.
    /// Its paper.toml before Loom rewrote it; nothing while it hasn't been rewritten.
    std::optional<QByteArray> before;
};

/// Undoes renames done on disk, the last first: each paper.toml put back as it was, then its
/// folder renamed back. A rename that can't be undone doesn't stop the others.
/// @returns What couldn't be undone, one sentence each; empty if everything was.
/// Side effects: writes paper.toml files and renames paper folders.
QStringList undo(const QString& root, const std::vector<DoneRename>& done)
{
    QStringList failed;
    for (auto it = done.rbegin(); it != done.rend(); ++it) {
        if (it->before) {
            if (const QString error = writeProjectFile(root, paperPath(it->to), *it->before); !error.isEmpty())
                failed << error;
        }
        if (!QDir().rename(it->toPath, it->fromPath))
            failed << QStringLiteral("Loom couldn't rename `%1` back to `%2`; rename it by hand.")
                          .arg(QDir::toNativeSeparators(it->toPath), QDir::toNativeSeparators(it->fromPath));
    }
    return failed;
}

/// Carries out renames on disk, in order: each folder renamed, then its paper.toml rewritten.
/// @param done Each rename done so far, so that the caller can undo them all if this or a later
///             step fails (the key rule's posting order lives in which paper got which letter).
/// @returns An empty string, or what went wrong; `done` then holds what was done before it.
/// Side effects: renames paper folders and rewrites their paper.toml.
QString carryOut(const QString& root, const std::vector<Rename>& renames, std::vector<DoneRename>& done)
{
    for (const Rename& rename : renames) {
        const QString fromRelative = QStringLiteral("Papers/%1").arg(rename.from);
        const QString toRelative = QStringLiteral("Papers/%1").arg(rename.to);
        const std::optional<QString> fromPath = resolveProjectPath(root, fromRelative);
        const std::optional<QString> toPath = resolveProjectPath(root, toRelative);
        if (!fromPath || !toPath)
            return QStringLiteral("`%1` must be a path inside the project, written with `/`, without `..`.")
                .arg(fromPath ? toRelative : fromRelative);
        // A rename that only changes the case of the name finds itself there on Windows.
        if (QFileInfo::exists(*toPath) && rename.from.compare(rename.to, Qt::CaseInsensitive) != 0)
            return QStringLiteral("Loom couldn't rename `%1` to `%2`: `%2` already exists.").arg(fromRelative, toRelative);
        if (!QDir().rename(*fromPath, *toPath))
            return QStringLiteral("Loom couldn't rename `%1` to `%2`. Is a file in it open in another program?")
                .arg(QDir::toNativeSeparators(*fromPath), QDir::toNativeSeparators(*toPath));
        done.push_back({*fromPath, *toPath, rename.to, std::nullopt});
        const std::optional<QByteArray> before = readProjectFile(root, paperPath(rename.to));
        if (!before)
            return QStringLiteral("Loom couldn't read `%1`.").arg(QDir::toNativeSeparators(*toPath + QStringLiteral("/paper.toml")));
        // A failed write leaves the old file in place (QSaveFile), so only the folder needs undoing.
        if (const QString error = writeProjectFile(root, paperPath(rename.to), rename.bytes); !error.isEmpty())
            return error;
        done.back().before = *before;
    }
    return {};
}

/// Writes a multi-file paper change if it adds no error: renames, then `extra` (path → bytes).
/// If any write fails, every rename already done is undone, the last first, so a retry letters
/// the papers as the first try would have.
/// @returns `result` marked done, or why nothing was written (refused) or what failed (can't work).
/// Side effects: renames paper folders, rewrites their paper.toml, writes `extra`.
ChangeResult commitPapers(Session& session, const ProjectContents& changed, const std::vector<Rename>& renames,
                          const std::vector<std::pair<QString, QByteArray>>& extra, const QString& what, ChangeResult result)
{
    const std::vector<Problem> before = movedBy(session.problems(), renames);
    const std::vector<Problem> added = addedErrors(before, session.problems(session.project.config, session.project.problems, changed));
    if (!added.empty()) {
        ChangeResult refusal = refusedFor(added, what);
        refusal.id = result.id;
        refusal.path = result.path;
        return refusal;
    }
    const QString& root = session.project.root;
    std::vector<DoneRename> done;
    QString error = carryOut(root, renames, done);
    for (auto it = extra.begin(); error.isEmpty() && it != extra.end(); ++it) {
        error = writeProjectFile(root, it->first, it->second);
        // Its folder, if the write made it and left it empty, would block a retry (`paper-exists`).
        if (!error.isEmpty())
            QDir().rmdir(QFileInfo(root + u'/' + it->first).absolutePath());
    }
    if (error.isEmpty()) {
        result.outcome = ChangeOutcome::Done;
        return result;
    }
    if (const QStringList notUndone = undo(root, done); !notUndone.isEmpty())
        error += QStringLiteral(" Loom couldn't undo all the renames it had done: ") + notUndone.join(u' ');
    else if (!done.empty())
        error += QStringLiteral(" Loom undid the renames it had done, so the papers are as they were.");
    return cantWork(QStringLiteral("write-failed"), error);
}

/// The entry in `Papers/` whose name is `name` ignoring case, whatever it is (a folder, a file;
/// whether or not its paper.toml parses). Windows ignores case in file names, so `MacDonald20`
/// and `Macdonald20` are one folder there, and Loom treats them as one everywhere (A3).
/// @returns The entry's name as it is on disk, or nothing. Reads the folder.
std::optional<QString> entryInPapers(const QString& root, const QString& name)
{
    const QStringList entries = QDir(root + QStringLiteral("/Papers"))
                                    .entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        if (entry.compare(name, Qt::CaseInsensitive) == 0)
            return entry;
    }
    return std::nullopt;
}

/// The refusal for something already in `Papers/` where a paper would go (A3: never replaced).
/// @param taken The entry's name as it is on disk. No side effects.
ChangeResult paperExists(const QString& taken)
{
    return refused(QStringLiteral("paper-exists"),
        QStringLiteral("`Papers/%1/` already exists. Fix or remove what's there first; `loom validate` shows its problems.").arg(taken));
}

/// Checks, before anything is renamed, that nothing is in `Papers/` where a rename goes, compared
/// ignoring case (A3). A folder an earlier rename moves away doesn't count, nor the paper's own
/// folder when only the case of its name changes. Otherwise the rename would fail part way,
/// after the renames before it, which then have to be undone.
/// @returns The refusal (`paper-exists`), or nothing if every target is free. Reads `Papers/`.
std::optional<ChangeResult> renameTargetTaken(const QString& root, const std::vector<Rename>& renames)
{
    const QStringList entries = QDir(root + QStringLiteral("/Papers"))
                                    .entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (std::size_t i = 0; i < renames.size(); ++i) {
        for (const QString& entry : entries) {
            if (entry.compare(renames[i].to, Qt::CaseInsensitive) != 0)
                continue;
            const bool movedAway = std::any_of(renames.begin(), renames.begin() + std::ptrdiff_t(i) + 1,
                                               [&](const Rename& earlier) { return earlier.from == entry; });
            if (!movedAway)
                return paperExists(entry);
        }
    }
    return std::nullopt;
}

/// Whether `build-bib` must refuse to write to `path` (a project path): one of the project's own
/// files, or a file in a folder Loom reads every file of. Those are `loom.toml`, the files
/// `new-project` writes, the preamble files, the write lock, and anything in `Statements/`,
/// `Proofs/`, `Papers/` (each file there is read, or is a `stray-file`) or `.git/`. Compared
/// ignoring case, as Windows does. No side effects.
bool isProjectOwnFile(const ProjectConfig& config, const QString& path)
{
    QStringList files = {QStringLiteral("loom.toml"), QStringLiteral("CLAUDE.md"), QStringLiteral(".gitignore"),
                         QStringLiteral(".gitattributes"), QStringLiteral(".loom/write.lock")};
    files += config.preamble;
    for (const QString& file : files) {
        if (path.compare(file, Qt::CaseInsensitive) == 0)
            return true;
    }
    for (const QString& folder : {QStringLiteral("Statements/"), QStringLiteral("Proofs/"), QStringLiteral("Papers/"), QStringLiteral(".git/")}) {
        if (path.startsWith(folder, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

/// Takes Dakota's clicks out of a draft (open spec question S16, its obvious reading): setting
/// `verified` or `checked` to a date is his click (Specs/m1-core.md, "Papers", "Fields": "agents
/// write `false`"), and `new-paper` isn't one, so each such date becomes `false`, and the
/// top-level `verified`, which can't be `false`, is left out.
/// @returns The fields changed, in file order (`verified`, `work.title`, `arxiv-v1.posted`,
///          `renumbering.checked`). Side effects: changes `paper`.
QStringList withoutClicks(Paper& paper)
{
    QStringList changed;
    if (paper.verified.isValid()) {
        paper.verified = QDate();
        changed << QStringLiteral("verified");
    }
    const auto unclick = [&](std::optional<PaperField>& field, const QString& name) {
        if (field && field->verified.isValid()) {
            field->verified = QDate();
            changed << name;
        }
    };
    unclick(paper.work.type, QStringLiteral("work.type"));
    unclick(paper.work.title, QStringLiteral("work.title"));
    unclick(paper.work.authors, QStringLiteral("work.authors"));
    unclick(paper.work.editors, QStringLiteral("work.editors"));
    for (PaperVersion& version : paper.versions) {
        for (NamedField& field : version.fields) {
            if (field.field.verified.isValid()) {
                field.field.verified = QDate();
                changed << version.id + u'.' + field.name;
            }
        }
    }
    for (Renumbering& renumbering : paper.renumberings) {
        if (renumbering.checked.isValid()) {
            renumbering.checked = QDate();
            if (!changed.contains(QStringLiteral("renumbering.checked")))
                changed << QStringLiteral("renumbering.checked");
        }
    }
    return changed;
}

/// Plans the renames a key assignment needs, applying them to `model`.
/// @returns The renames, or nothing after setting `result`.
std::optional<std::vector<Rename>> planRenames(const QString& root, ProjectContents& model,
                                               const std::vector<std::pair<QString, QString>>& keyRenames, ChangeResult& result)
{
    std::vector<Rename> renames;
    for (const auto& [fromKey, to] : keyRenames) {
        const PaperFile* file = model.paperWithKey(fromKey);
        if (!file) {
            result = refused(QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a paper in this project.").arg(fromKey));
            return std::nullopt;
        }
        const std::optional<Rename> rename = renameInModel(root, model, file->folder, to, result);
        if (!rename)
            return std::nullopt;
        renames.push_back(*rename);
    }
    return renames;
}

} // namespace

PaperChange createPaper(const QString& root, const QByteArray& draft, const QString& draftName, const ChangeOptions& options)
{
    PaperChange out;
    ChangeResult& result = out.change;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return out;
    const ProjectContents& contents = session->contents;

    PaperReading reading = readPaper(draft, draftName, false);
    std::vector<Problem> errors;
    for (const Problem& problem : reading.problems) {
        if (problem.severity == Severity::Error)
            errors.push_back(problem);
    }
    if (!reading.parsed || !reading.lossless || !errors.empty()) {
        QStringList lines{QStringLiteral("Loom didn't create the paper: the draft has problems to fix first:")};
        for (const Problem& problem : errors.empty() ? reading.problems : errors)
            lines << problemLine(problem);
        result = refused(errors.empty() ? QStringLiteral("not-editable") : errors.front().code, lines.join(u'\n'));
        result.problems = errors;
        return out;
    }

    Paper paper = reading.document;
    if (paper.cite.isEmpty())
        paper.cite = defaultCite(paper);
    if (!paper.created.isValid())
        paper.created = options.today;

    const KeyAssignment assigned = assignKey(paper, othersThan(contents, QString()));
    if (assigned.key.isEmpty()) {
        result = refused(QStringLiteral("paper-key-rule"), assigned.problem);
        return out;
    }
    if (!paper.key.isEmpty() && paper.key != assigned.key) {
        result = refused(QStringLiteral("paper-key-rule"),
            QStringLiteral("The draft's key is `%1`, but by the key rule it is `%2`. Leave `key` out, and Loom fills it in.")
                .arg(paper.key, assigned.key));
        return out;
    }
    paper.key = assigned.key;

    // A3: never replace what is in `Papers/<Key>/`. The key rule sees only entries that parse,
    // under their `key`, so an entry that doesn't parse, or whose key disagrees with its folder,
    // or a folder differing only in case, would otherwise be written over.
    if (const std::optional<QString> taken = entryInPapers(session->project.root, paper.key)) {
        result = paperExists(*taken);
        return out;
    }
    result.id = paper.key;
    result.path = paperPath(paper.key);

    ProjectContents changed = contents;
    const std::optional<std::vector<Rename>> renames = planRenames(session->project.root, changed, assigned.renames, result);
    if (!renames)
        return out;
    if (std::optional<ChangeResult> taken = renameTargetTaken(session->project.root, *renames)) {
        result = std::move(*taken);
        return out;
    }
    const QStringList unclicked = withoutClicks(paper);
    const QByteArray bytes = formatPaper(paper);
    PaperFile file{result.path, paper.key, readPaper(bytes, result.path), {}};
    changed.papers.push_back(std::move(file));

    result = commitPapers(*session, changed, *renames, {{result.path, bytes}}, QStringLiteral("create the paper"), result);
    if (result.outcome == ChangeOutcome::Done) {
        for (const Rename& rename : *renames)
            out.renamed.push_back({rename.fromKey, rename.to});
        out.unclicked = unclicked;
    }
    return out;
}

ChangeResult createStub(const QString& root, const NewStub& request, const ChangeOptions& options)
{
    ChangeResult result;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return result;
    const ProjectContents& contents = session->contents;

    const PaperFile* paper = contents.paperWithKey(request.paper);
    if (!paper)
        return refused(QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a paper in this project.").arg(request.paper));
    if (request.gist.trimmed().isEmpty())
        return refused(QStringLiteral("stub-gist-empty"), QStringLiteral("A stub needs its gist: the result in our words."));
    const std::optional<QStringList> statedUsing = resolveAll(contents, request.statedUsing, result);
    if (!statedUsing)
        return result;
    const std::optional<QStringList> paperPremises = resolveAll(contents, request.paperPremises, result);
    if (!paperPremises)
        return result;

    const QSet<QString> used = tagsInUse(contents);
    const std::optional<QString> tag = generateTag([&](const QString& t) { return used.contains(t); }, *options.random);
    if (!tag)
        return refused(QStringLiteral("no-tag"),
            QStringLiteral("Loom couldn't find a free tag after 100 tries; the project may be nearly full."));

    Stub stub;
    stub.id = *tag;
    stub.kind = request.kind;
    stub.title = request.title;
    stub.gist = request.gist;
    stub.statedUsing = *statedUsing;
    stub.paperPremises = *paperPremises;
    stub.extractedFrom = request.version;
    stub.extractedBy = request.by;
    stub.created = options.today;
    stub.locators.push_back({request.version, request.at, QDate(), 1});

    const QString path = QStringLiteral("Papers/%1/Statements/%2.toml").arg(paper->folder, *tag);
    const QByteArray bytes = formatStub(stub);
    ProjectContents changed = contents;
    changed.stubs.push_back({path, paper->folder, *tag, readStub(bytes, path)});

    result.id = *tag;
    result.path = path;
    return commitPapers(*session, changed, {}, {{path, bytes}}, QStringLiteral("create the stub"), result);
}

PaperChange rekeyPaper(const QString& root, const QString& typed, const ChangeOptions& options)
{
    PaperChange out;
    ChangeResult& result = out.change;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return out;
    const ProjectContents& contents = session->contents;

    const PaperFile* file = contents.paperWithKey(typed);
    if (!file) {
        result = refused(QStringLiteral("ref-unknown"), QStringLiteral("`%1` isn't a paper in this project.").arg(typed));
        return out;
    }
    if (!file->reading.lossless) {
        result = notEditable(file->path, file->reading.problems);
        return out;
    }
    const Paper& paper = file->reading.document;
    const KeyBase base = baseKey(paper);
    if (base.key.isEmpty()) {
        result = refused(QStringLiteral("paper-key-rule"), base.problem);
        return out;
    }
    result.id = paper.key;
    result.path = file->path;
    const std::vector<KeyedPaper> others = othersThan(contents, file->folder);
    const bool follows = keyFollowsRule(paper.key, base.key, others);
    if (follows && paper.key == file->folder) {
        result.outcome = ChangeOutcome::Done;
        return out;
    }

    // A key that follows the rule but disagrees with its folder: the folder is renamed to match
    // the key. Otherwise the key the rule gives, among the other papers (an unlettered key beside
    // a lettered one of its base takes the next letter, step 4).
    const KeyAssignment assigned = follows ? KeyAssignment{paper.key, {}, {}} : assignKey(paper, others);
    if (assigned.key.isEmpty()) {
        result = refused(QStringLiteral("paper-key-rule"), assigned.problem);
        return out;
    }
    ProjectContents changed = contents;
    std::optional<std::vector<Rename>> renames = planRenames(session->project.root, changed, assigned.renames, result);
    if (!renames)
        return out;
    const std::optional<Rename> own = renameInModel(session->project.root, changed, file->folder, assigned.key, result);
    if (!own)
        return out;
    renames->push_back(*own);
    if (std::optional<ChangeResult> taken = renameTargetTaken(session->project.root, *renames)) {
        result = std::move(*taken);
        return out;
    }

    result.id = assigned.key;
    result.path = paperPath(assigned.key);
    result = commitPapers(*session, changed, *renames, {}, QStringLiteral("rename the paper"), result);
    if (result.outcome == ChangeOutcome::Done) {
        for (const Rename& rename : *renames)
            out.renamed.push_back({rename.fromKey, rename.to});
    }
    return out;
}

BibliographyBuilt buildBibliography(const QString& root, const QString& out, const ChangeOptions& options)
{
    BibliographyBuilt built;
    ChangeResult& result = built.change;
    std::optional<Session> session = begin(root, options, result);
    if (!session)
        return built;

    const QString path = out.isEmpty() ? QStringLiteral(".loom/build/refs.bib") : out;
    if (!isProjectPath(path) || !resolveProjectPath(session->project.root, path)) {
        result = cantWork(QStringLiteral("bad-path"),
            QStringLiteral("`%1` must be a path inside the project, written with `/`, without `..`.").arg(path));
        return built;
    }
    // The bibliography is generated, outside the model, so it can't go through the "adds no
    // error" check; it must never replace a file of the project instead.
    if (isProjectOwnFile(session->project.config, path)) {
        result = refused(QStringLiteral("bib-out-project-file"),
            QStringLiteral("`%1` is one of the project's own files, or in a folder Loom keeps them in, so Loom won't write "
                           "the bibliography there. Give `--out` a path of its own, such as `TeX/refs.bib`.").arg(path));
        return built;
    }

    std::vector<BibSource> papers;
    for (const PaperFile& file : session->contents.papers) {
        if (!file.reading.parsed || file.reading.document.key.isEmpty()) {
            built.problems.push_back({Severity::Error, QStringLiteral("bib-entry-skipped"), file.path, 1,
                QStringLiteral("Loom can't read %1 (run `loom validate`), so its BibTeX entry was left out of refs.bib.").arg(file.path)});
            continue;
        }
        // An entry with errors of its own is skipped, as one with unbalanced braces is: a value
        // Loom has reported as wrong (an `arxiv_id` with `vN`, say) would go into refs.bib as is.
        const bool hasErrors = std::any_of(file.reading.problems.begin(), file.reading.problems.end(),
                                           [](const Problem& problem) { return problem.severity == Severity::Error; });
        if (hasErrors) {
            built.problems.push_back({Severity::Error, QStringLiteral("bib-entry-skipped"), file.path, 1,
                QStringLiteral("%1's paper.toml has errors (`loom validate` lists them), so its BibTeX entry was left out "
                               "of refs.bib.").arg(file.reading.document.key)});
            continue;
        }
        papers.push_back({file.path, file.reading.document});
    }
    Bibliography bib = formatBibliography(papers);
    built.problems.insert(built.problems.end(), bib.problems.begin(), bib.problems.end());
    sortProblems(built.problems);

    if (const QString error = writeProjectFile(session->project.root, path, bib.text); !error.isEmpty()) {
        result = cantWork(QStringLiteral("write-failed"), error);
        return built;
    }
    result.outcome = ChangeOutcome::Done;
    result.path = path;
    return built;
}

} // namespace loom
