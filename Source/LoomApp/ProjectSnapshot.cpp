#include "LoomApp/ProjectSnapshot.h"

#include "LoomCore/ProjectFiles.h"
#include "LoomCore/Validation.h"

#include <QDir>
#include <QFileInfo>

namespace loom {

namespace {

/// Whether a folder has `loom.toml` (a file, or a link to one).
bool hasProjectToml(const QString& folder)
{
    const QFileInfo toml(folder + QStringLiteral("/loom.toml"));
    return toml.exists() || toml.isSymLink();
}

/// The message for a `loom.toml` that doesn't parse (Specs/m2-statements-review.md, "Opening a
/// folder"): *"Loom can't read loom.toml: {parser message} (line {n}). Fix it in a text editor,
/// then open the project again."*
QString unreadableToml(const Problem& problem)
{
    QString parser = problem.message;
    const QString tomlPrefix = QStringLiteral("This isn't valid TOML: ");
    if (problem.code == u"not-utf8")
        parser = QStringLiteral("it isn't valid UTF-8");
    else if (parser.startsWith(tomlPrefix))
        parser = parser.mid(tomlPrefix.size());
    while (parser.endsWith(u'.'))
        parser.chop(1);
    return QStringLiteral("Loom can't read loom.toml: %1 (line %2). Fix it in a text editor, then open the project again.")
        .arg(parser)
        .arg(problem.line);
}

/// The project's name, or its folder's name if `loom.toml` doesn't give one.
QString nameOf(const OpenedProject& project)
{
    return project.config.name.isEmpty() ? QFileInfo(project.root).fileName() : project.config.name;
}

} // namespace

FolderInspection inspectFolder(const QString& path)
{
    FolderInspection result;
    result.folder = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QFileInfo info(result.folder);
    if (!info.isDir()) {
        result.kind = FolderKind::Missing;
        result.message = QStringLiteral("This folder doesn't exist: %1").arg(QDir::toNativeSeparators(result.folder));
        return result;
    }

    if (hasProjectToml(result.folder)) {
        const OpenedProject project = openProject(result.folder);
        result.projectRoot = project.root;
        result.projectName = nameOf(project);
        switch (project.status) {
        case OpenStatus::FormatTooNew:
            result.kind = FolderKind::FormatTooNew;
            result.message = project.message;
            return result;
        case OpenStatus::Unreadable:
        case OpenStatus::NotAProject:
            result.kind = FolderKind::TomlUnreadable;
            // The file exists but can't be read (permissions, a link outside the project).
            result.message = QStringLiteral("Loom can't read loom.toml: the file couldn't be opened. "
                                            "Check it in Explorer, then open the project again.");
            return result;
        case OpenStatus::Opened:
            break;
        }
        for (const Problem& problem : project.problems) {
            if (problem.code == u"toml-syntax" || problem.code == u"not-utf8") {
                result.kind = FolderKind::TomlUnreadable;
                result.message = unreadableToml(problem);
                return result;
            }
        }
        result.kind = FolderKind::Project;
        return result;
    }

    for (QDir above(result.folder); above.cdUp();) {
        if (hasProjectToml(above.path())) {
            const OpenedProject project = openProject(above.path());
            result.kind = FolderKind::InsideProject;
            result.projectRoot = project.root;
            result.projectName = nameOf(project);
            result.message = QStringLiteral("This folder is inside the project %1 at %2.")
                                 .arg(result.projectName, QDir::toNativeSeparators(project.root));
            return result;
        }
    }

    if (QDir(result.folder).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
        result.kind = FolderKind::Empty;
        result.message = QStringLiteral("This folder is empty.");
        return result;
    }
    result.kind = FolderKind::NotAProject;
    result.message = QStringLiteral("This folder isn't a Loom project: it has no loom.toml. Loom creates projects only in new or empty folders.");
    return result;
}

ProjectSnapshot readSnapshot(const QString& root, const GitTool& git)
{
    ProjectSnapshot snapshot;
    snapshot.root = QDir::cleanPath(QFileInfo(root).absoluteFilePath());

    const OpenedProject project = openProject(snapshot.root);
    snapshot.config = project.config;
    if (project.status != OpenStatus::Opened)
        snapshot.unreadable = project.message;

    if (ContentsLoading loading = loadContents(snapshot.root); loading.ok) {
        snapshot.contents = std::move(loading.contents);
        snapshot.graph = buildGraph(snapshot.contents);
        QStringList paths;
        for (const StatementFile& file : snapshot.contents.statements)
            paths << file.path;
        for (const ProofFile& file : snapshot.contents.proofs)
            paths << file.path;
        for (const VerboseFile& file : snapshot.contents.verbose)
            paths << file.path;
        for (const StubFile& file : snapshot.contents.stubs)
            paths << file.path;
        for (const QString& path : paths) {
            if (const std::optional<QByteArray> bytes = readProjectFile(snapshot.root, path)) {
                snapshot.files.insert(path, *bytes);
                snapshot.modified.insert(path, QFileInfo(snapshot.root + u'/' + path).lastModified());
            }
        }
    } else if (snapshot.unreadable.isEmpty()) {
        snapshot.unreadable = loading.message;
    }

    ValidationRequest request;
    request.root = snapshot.root;
    request.git = git;
    request.commitOptional = true;
    const ValidationResult validation = validateProject(request);
    if (validation.outcome == ValidationOutcome::Checked) {
        snapshot.problems = validation.problems;
        snapshot.errors = validation.errors();
        snapshot.warnings = validation.warnings();
    } else if (snapshot.unreadable.isEmpty()) {
        snapshot.unreadable = validation.message;
    }

    // In English, whatever the user's locale, so "not a git repository" can be recognised.
    GitTool english = git;
    english.environment.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    const QStringList gitDir = {QStringLiteral("rev-parse"), QStringLiteral("--git-dir")};
    const GitRun repository = runGit(english, gitDir, snapshot.root);
    if (!repository.started) {
        snapshot.gitProblem = repository.timedOut ? gitDidntRunMessage(repository, gitDir)
                                                  : QStringLiteral("Loom can't find git; Check my setup says what to install.");
    } else if (!repository.ok()) {
        snapshot.gitProblem = repository.errors.contains(u"not a git repository")
            ? QStringLiteral("This project isn't in a git repository")
            : QStringLiteral("Git can't use this project's repository: %1").arg(repository.errors.trimmed().section(u'\n', 0, 0));
    } else {
        const GitRun branch = runGit(git, {QStringLiteral("symbolic-ref"), QStringLiteral("--short"), QStringLiteral("-q"), QStringLiteral("HEAD")},
                                     snapshot.root);
        if (branch.ok())
            snapshot.branch = branch.output.trimmed();
    }
    return snapshot;
}

} // namespace loom
