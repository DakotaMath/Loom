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

/// The arXiv version number of a version: its `arxiv_version`, else the N of `arxiv-v<N>`.
qint64 arxivNumber(const PaperVersion& version)
{
    if (const PaperField* field = version.field(u"arxiv_version"); field && std::holds_alternative<qint64>(field->value))
        return std::get<qint64>(field->value);
    return version.id.mid(7).toLongLong();
}

/// `cite`'s default (Specs/m1-core.md, "Papers": "`published` if there is a published version,
/// else the newest arXiv version").
QString defaultCite(const Paper& paper)
{
    if (paper.version(QStringLiteral("published")))
        return QStringLiteral("published");
    const PaperVersion* newest = nullptr;
    for (const PaperVersion& v : paper.versions) {
        if (v.kind == u"arxiv" && (!newest || arxivNumber(v) > arxivNumber(*newest)))
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
/// "The key rule", step 5: "the folder is renamed, `key` changes, the old key is added to `aliases`").
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
    if (!paper.key.isEmpty() && !paper.aliases.contains(paper.key))
        paper.aliases << paper.key;
    paper.aliases.removeAll(to);
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

/// Carries out renames on disk: each folder renamed, then its paper.toml rewritten.
/// @returns An empty string, or what went wrong (after undoing the rename it was doing).
QString carryOut(const QString& root, const std::vector<Rename>& renames)
{
    for (const Rename& rename : renames) {
        const QString fromRelative = QStringLiteral("Papers/%1").arg(rename.from);
        const QString toRelative = QStringLiteral("Papers/%1").arg(rename.to);
        const std::optional<QString> fromPath = resolveProjectPath(root, fromRelative);
        const std::optional<QString> toPath = resolveProjectPath(root, toRelative);
        if (!fromPath || !toPath)
            return QStringLiteral("`%1` must be a path inside the project, written with `/`, without `..`.")
                .arg(fromPath ? toRelative : fromRelative);
        if (QFileInfo::exists(*toPath))
            return QStringLiteral("Loom couldn't rename `%1` to `%2`: `%2` already exists.").arg(fromRelative, toRelative);
        if (!QDir().rename(*fromPath, *toPath))
            return QStringLiteral("Loom couldn't rename `%1` to `%2`. Is a file in it open in another program?")
                .arg(QDir::toNativeSeparators(*fromPath), QDir::toNativeSeparators(*toPath));
        const QString error = writeProjectFile(root, paperPath(rename.to), rename.bytes);
        if (!error.isEmpty()) {
            QDir().rename(*toPath, *fromPath);
            return error;
        }
    }
    return {};
}

/// Writes a multi-file paper change if it adds no error: renames, then `extra` (path → bytes).
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
    if (const QString error = carryOut(session.project.root, renames); !error.isEmpty())
        return cantWork(QStringLiteral("write-failed"), error);
    for (const auto& [path, bytes] : extra) {
        if (const QString error = writeProjectFile(session.project.root, path, bytes); !error.isEmpty())
            return cantWork(QStringLiteral("write-failed"), error);
    }
    result.outcome = ChangeOutcome::Done;
    return result;
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
    result.id = paper.key;
    result.path = paperPath(paper.key);

    ProjectContents changed = contents;
    const std::optional<std::vector<Rename>> renames = planRenames(session->project.root, changed, assigned.renames, result);
    if (!renames)
        return out;
    const QByteArray bytes = formatPaper(paper);
    PaperFile file{result.path, paper.key, readPaper(bytes, result.path), {}};
    changed.papers.push_back(std::move(file));

    result = commitPapers(*session, changed, *renames, {{result.path, bytes}}, QStringLiteral("create the paper"), result);
    if (result.outcome == ChangeOutcome::Done) {
        for (const Rename& rename : *renames)
            out.renamed.push_back({rename.fromKey, rename.to});
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
    if (keyFitsBase(paper.key, base.key) && paper.key == file->folder) {
        result.outcome = ChangeOutcome::Done;
        return out;
    }

    // A key that fits the rule but disagrees with its folder is put back with its folder's name.
    const KeyAssignment assigned = keyFitsBase(paper.key, base.key) ? KeyAssignment{paper.key, {}, {}}
                                                                    : assignKey(paper, othersThan(contents, file->folder));
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

    std::vector<Paper> papers;
    for (const PaperFile& file : session->contents.papers) {
        if (file.reading.parsed && !file.reading.document.key.isEmpty()) {
            papers.push_back(file.reading.document);
            continue;
        }
        built.problems.push_back({Severity::Error, QStringLiteral("bib-entry-skipped"), file.path, 1,
            QStringLiteral("Loom can't read %1 (run `loom validate`), so its BibTeX entry was left out of refs.bib.").arg(file.path)});
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
