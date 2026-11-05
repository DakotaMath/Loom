#include "LoomCore/Revision.h"

#include "LoomCore/ProjectFiles.h"
#include "LoomCore/ProjectToml.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>

#include <algorithm>
#include <utility>

namespace loom {

namespace {

/// A failed reading.
RevisionTree failed(const QString& code, const QString& message)
{
    RevisionTree tree;
    tree.outcome = RevisionOutcome::CantWork;
    tree.code = code;
    tree.message = message;
    return tree;
}

/// The failure when git didn't run or didn't finish.
RevisionTree gitDidntRun(const GitRun& run, const QStringList& arguments)
{
    return failed(gitDidntRunCode(run, QStringLiteral("git-missing")), gitDidntRunMessage(run, arguments));
}

/// Git's first line of complaint, else its exit code.
QString whyGitFailed(const GitRun& run)
{
    const QString why = run.errors.trimmed().section(u'\n', 0, 0);
    return why.isEmpty() ? QStringLiteral("it exited with code %1").arg(run.exitCode) : why;
}

/// Whether a commit's file is one a build reads, apart from the preamble files: `Statements/`,
/// `Proofs/`, `Papers/<Key>/paper.toml` and `Papers/<Key>/Statements/`.
bool readByBuilds(const QString& path)
{
    if (path.startsWith(QStringLiteral("Statements/")) || path.startsWith(QStringLiteral("Proofs/")))
        return true;
    const QStringList parts = path.split(u'/');
    return parts.size() >= 3 && parts[0] == u"Papers" && (parts[2] == u"paper.toml" || (parts.size() == 4 && parts[2] == u"Statements"));
}

/// Notes that a revision was used now, for pruneRevisions().
void markUsed(const QString& root, const QString& hash)
{
    writeProjectFile(root, QStringLiteral(".loom/rev/%1/used").arg(hash),
                     QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toUtf8() + '\n');
}

} // namespace

RevisionTree readRevision(const QString& root, const QString& rev, const GitTool& git)
{
    // 1. The commit.
    const QStringList verify = {QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("--quiet"),
                                rev + QStringLiteral("^{commit}")};
    const GitRun resolved = runGit(git, verify, root);
    if (!resolved.started)
        return gitDidntRun(resolved, verify);
    if (!resolved.ok()) {
        // Like M1's reading of the last commit: `HEAD` that doesn't resolve, silently, is a
        // repository without a commit yet.
        if (rev == u"HEAD" && resolved.exitCode == 1 && resolved.errors.trimmed().isEmpty()) {
            const QStringList anyRef = {QStringLiteral("rev-parse"), QStringLiteral("--git-dir")};
            if (runGit(git, anyRef, root).ok()) {
                RevisionTree tree;
                tree.outcome = RevisionOutcome::NoCommits;
                return tree;
            }
        }
        return failed(QStringLiteral("git-failed"),
                      QStringLiteral("git can't find the commit `%1` in this repository.").arg(rev));
    }
    RevisionTree tree;
    tree.hash = resolved.output.trimmed();
    tree.folder = QStringLiteral(".loom/rev/%1/tree").arg(tree.hash);
    const QString absolute = QDir(root).filePath(tree.folder);

    // 2. Its files, unless an earlier reading left them.
    if (QFileInfo(absolute).isDir()) {
        tree.outcome = RevisionOutcome::Ready;
        tree.reused = true;
        markUsed(root, tree.hash);
        return tree;
    }
    const QStringList listTree = {QStringLiteral("ls-tree"), QStringLiteral("-r"), QStringLiteral("-z"), tree.hash};
    const GitRun listing = runGit(git, listTree, root);
    if (!listing.started)
        return gitDidntRun(listing, listTree);
    if (!listing.ok())
        return failed(QStringLiteral("git-failed"),
                      QStringLiteral("Loom couldn't list the files of %1: `git ls-tree` failed (%2).").arg(rev, whyGitFailed(listing)));

    std::vector<GitListingEntry> entries;
    for (const GitListingEntry& entry : parseGitListing(listing.outputBytes, false)) {
        // Only ordinary files: a link's target or a submodule isn't a file Loom reads.
        if ((entry.mode == u"100644" || entry.mode == u"100755") && isProjectPath(entry.path))
            entries.push_back(entry);
    }

    // `loom.toml` first, for the preamble files it names.
    GitRun catFile;
    const auto object = [&](const std::vector<QByteArray>& shas) { return readGitObjects(git, root, shas, catFile); };
    QStringList preamble;
    std::vector<GitListingEntry> wanted;
    const auto toml = std::find_if(entries.begin(), entries.end(), [](const GitListingEntry& e) { return e.path == u"loom.toml"; });
    if (toml != entries.end()) {
        const std::optional<std::vector<QByteArray>> bytes = object({toml->sha});
        if (catFile.timedOut)
            return gitDidntRun(catFile, gitCatFileBatch());
        if (!bytes)
            return failed(QStringLiteral("git-failed"), QStringLiteral("Loom couldn't read loom.toml at %1 (`git cat-file --batch` failed).").arg(rev));
        // The folder only matters for checking the preamble files exist, which isn't done here.
        preamble = readProjectToml(bytes->front(), root).config.preamble;
    }
    std::vector<QByteArray> shas;
    for (const GitListingEntry& entry : entries) {
        if (entry.path == u"loom.toml" || preamble.contains(entry.path) || readByBuilds(entry.path)) {
            wanted.push_back(entry);
            shas.push_back(entry.sha);
        }
    }
    const std::optional<std::vector<QByteArray>> contents = object(shas);
    if (catFile.timedOut)
        return gitDidntRun(catFile, gitCatFileBatch());
    if (!contents)
        return failed(QStringLiteral("git-failed"),
                      QStringLiteral("Loom couldn't read the files of %1 (`git cat-file --batch` failed).").arg(rev));

    // 3. Written beside `tree/`, then renamed, so a half-written tree is never read.
    const QString scratch = QStringLiteral(".loom/rev/%1/tree-%2").arg(tree.hash).arg(QRandomGenerator::global()->generate(), 8, 16, QChar(u'0'));
    for (std::size_t i = 0; i < wanted.size(); ++i) {
        if (const QString error = writeProjectFile(root, scratch + u'/' + wanted[i].path, (*contents)[i]); !error.isEmpty()) {
            QDir(QDir(root).filePath(scratch)).removeRecursively();
            return failed(QStringLiteral("write-failed"), QStringLiteral("Loom couldn't write the files of %1: %2").arg(rev, error));
        }
    }
    QDir().mkpath(QDir(root).filePath(scratch)); // A commit with none of Loom's files is still a tree.
    if (!QDir(root).rename(scratch, tree.folder)) {
        // Another build wrote the same tree meanwhile: theirs is as good.
        QDir(QDir(root).filePath(scratch)).removeRecursively();
        if (!QFileInfo(absolute).isDir())
            return failed(QStringLiteral("write-failed"),
                          QStringLiteral("Loom couldn't move the files of %1 into place at %2.").arg(rev, tree.folder));
    }
    tree.outcome = RevisionOutcome::Ready;
    markUsed(root, tree.hash);
    return tree;
}

QStringList pruneRevisions(const QString& root, int keep)
{
    const QDir revs(QDir(root).filePath(QStringLiteral(".loom/rev")));
    std::vector<std::pair<QDateTime, QString>> used;
    for (const QString& hash : revs.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QFileInfo mark(revs.filePath(hash + QStringLiteral("/used")));
        used.push_back({mark.exists() ? mark.lastModified() : QFileInfo(revs.filePath(hash)).lastModified(), hash});
    }
    std::sort(used.begin(), used.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    QStringList removed;
    for (std::size_t i = static_cast<std::size_t>(std::max(0, keep)); i < used.size(); ++i) {
        if (QDir(revs.filePath(used[i].second)).removeRecursively())
            removed << used[i].second;
    }
    std::sort(removed.begin(), removed.end());
    return removed;
}

QStringList cleanBuildFolders(const QString& root, const ProjectContents& contents)
{
    const QDir builds(QDir(root).filePath(QStringLiteral(".loom/build")));
    QStringList removed;
    for (const QString& name : builds.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (name == u"preview")
            continue;
        if (contents.statement(name) || contents.proof(name))
            continue;
        if (QDir(builds.filePath(name)).removeRecursively())
            removed << name;
    }
    std::sort(removed.begin(), removed.end());
    return removed;
}

} // namespace loom
