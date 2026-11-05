#include "LoomCore/Build.h"

#include "LoomCore/ProcessTree.h"
#include "LoomCore/ProjectContents.h"
#include "LoomCore/ProjectFiles.h"
#include "LoomCore/ProjectToml.h"
#include "LoomCore/Revision.h"
#include "LoomCore/TexLog.h"
#include "LoomCore/TomlWriter.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>

#include <toml++/toml.hpp>

#include <algorithm>
#include <string_view>
#include <utility>

namespace loom {

namespace {

/// `latexmk`'s arguments (Specs/m2-statements-review.md, "Running `latexmk`").
const QStringList& latexmkArguments()
{
    static const QStringList arguments = {QStringLiteral("-norc"), QStringLiteral("-pdf"), QStringLiteral("-interaction=nonstopmode"),
                                          QStringLiteral("-file-line-error"), QStringLiteral("-synctex=1"), QStringLiteral("main.tex")};
    return arguments;
}

/// A result that stops before LaTeX.
BuildResult stopped(BuildResult result, BuildOutcome outcome, const QString& code, const QString& message)
{
    result.outcome = outcome;
    result.code = code;
    result.message = message;
    return result;
}

/// What `loom-build.toml` said about the last build of a folder.
struct LastBuild {
    QString inputs;
    QString result;
};

/// Reads `loom-build.toml`; anything unreadable is an empty record, so the build simply runs.
LastBuild readLastBuild(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray bytes = file.readAll();
    LastBuild last;
    try {
        const toml::table table = toml::parse(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
        last.inputs = QString::fromStdString(table["inputs"].value_or(std::string()));
        last.result = QString::fromStdString(table["result"].value_or(std::string()));
    } catch (const toml::parse_error&) {
        return {};
    }
    return last;
}

/// `loom-build.toml` (Specs/m2-statements-review.md, "The build folder"): what was built.
QByteArray formatLastBuild(const BuildResult& result, const QString& inputs, const QString& outcome)
{
    QString text = QStringLiteral("# Written by Loom: the last build in this folder. Don't edit.\n");
    text += QStringLiteral("target = %1\n").arg(tomlBasicString(result.target));
    text += QStringLiteral("mode = %1\n").arg(tomlBasicString(buildModeName(result.mode)));
    text += QStringLiteral("verbose = %1\n").arg(result.verbose ? QStringLiteral("true") : QStringLiteral("false"));
    if (!result.rev.isEmpty())
        text += QStringLiteral("rev = %1\n").arg(tomlBasicString(result.rev));
    text += QStringLiteral("inputs = %1\n").arg(tomlBasicString(inputs));
    text += QStringLiteral("built = %1\n").arg(tomlOffsetDateTime(QDateTime::currentDateTime()));
    text += QStringLiteral("result = %1\n").arg(tomlBasicString(outcome));
    text += QStringLiteral("seconds = %1\n").arg(QString::number(result.seconds, 'f', 1));
    return text.toUtf8();
}

/// The inputs' hash: SHA-256 over `loom.sty`, every file written, and the options.
QString inputsHash(const BuildDocument& doc, const QByteArray& style)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const auto part = [&](const QByteArray& label, const QByteArray& bytes) {
        hash.addData(label + '\n' + QByteArray::number(bytes.size()) + '\n');
        hash.addData(bytes);
        hash.addData("\n");
    };
    part("loom-build-inputs-v1", buildModeName(doc.mode).toUtf8() + (doc.verbose ? " verbose" : ""));
    part("loom.sty", style);
    for (const BuildFile& file : doc.files)
        part(file.path.toUtf8(), file.bytes);
    return QString::fromLatin1(hash.result().toHex());
}

/// Reads a whole file, or nothing.
QByteArray readAll(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

int BuildResult::errors() const
{
    return static_cast<int>(std::count_if(problems.begin(), problems.end(), [](const Problem& p) { return p.severity == Severity::Error; }));
}

QString buildFolder(const QString& target, BuildMode mode, bool verbose, const QString& revHash)
{
    const QString base = revHash.isEmpty() ? QStringLiteral(".loom/build") : QStringLiteral(".loom/rev/%1/build").arg(revHash);
    return QStringLiteral("%1/%2/%3%4").arg(base, target, buildModeName(mode), verbose ? QStringLiteral("-verbose") : QString());
}

QProcessEnvironment latexmkEnvironment(QProcessEnvironment environment)
{
    // Scoped to pdfLaTeX (kpathsea reads `<variable>.<program>` first): BibTeX reads the same
    // variables and refuses a `max_print_line` above its buffer size ("3 is a bad bad").
    environment.insert(QStringLiteral("max_print_line.pdflatex"), QStringLiteral("100000"));
    environment.insert(QStringLiteral("error_line.pdflatex"), QStringLiteral("254"));
    environment.insert(QStringLiteral("half_error_line.pdflatex"), QStringLiteral("238"));
    return environment;
}

BuildResult buildTarget(const BuildRequest& request)
{
    QElapsedTimer clock;
    clock.start();
    BuildResult result;
    result.mode = request.mode;
    result.verbose = request.verbose;
    const auto done = [&](BuildResult r) {
        r.seconds = static_cast<double>(clock.elapsed()) / 1000.0;
        return r;
    };

    // --- The project, or the commit -----------------------------------------------------------------
    const OpenedProject project = openProject(request.root);
    switch (project.status) {
    case OpenStatus::Opened: break;
    case OpenStatus::NotAProject: return done(stopped(result, BuildOutcome::CantWork, QStringLiteral("not-a-project"), project.message));
    case OpenStatus::Unreadable: return done(stopped(result, BuildOutcome::CantWork, QStringLiteral("unreadable"), project.message));
    case OpenStatus::FormatTooNew: return done(stopped(result, BuildOutcome::CantWork, QStringLiteral("format-too-new"), project.message));
    }
    const QString root = project.root;

    BuildSource source;
    source.root = root;
    source.config = project.config;
    if (!request.rev.isEmpty()) {
        const RevisionTree tree = readRevision(root, request.rev, request.git);
        if (tree.outcome == RevisionOutcome::NoCommits)
            return done(stopped(result, BuildOutcome::CantWork, QStringLiteral("no-commits"),
                                QStringLiteral("The repository has no commits yet, so there is no `%1` to build.").arg(request.rev)));
        if (tree.outcome == RevisionOutcome::CantWork)
            return done(stopped(result, BuildOutcome::CantWork, tree.code, tree.message));
        result.rev = tree.hash;
        source.root = QDir(root).filePath(tree.folder);
        const OpenedProject atCommit = openProject(source.root);
        if (atCommit.status != OpenStatus::Opened)
            return done(stopped(result, BuildOutcome::CantWork,
                                atCommit.status == OpenStatus::FormatTooNew ? QStringLiteral("format-too-new") : QStringLiteral("not-a-project"),
                                QStringLiteral("Loom can't read the project at `%1`: %2").arg(request.rev, atCommit.message)));
        source.config = atCommit.config;
    }
    ContentsLoading loading = loadContents(source.root);
    if (!loading.ok)
        return done(stopped(result, BuildOutcome::CantWork, QStringLiteral("unreadable"), loading.message));
    source.contents = std::move(loading.contents);
    source.graph = buildGraph(source.contents);

    // --- What to build -------------------------------------------------------------------------------
    const BuildDocument doc = composeBuild(source, request.target, request.mode, request.verbose);
    result.target = doc.target;
    if (!doc.refusal.isEmpty()) {
        QString message = doc.refusalMessage;
        if (doc.refusal == u"ref-unknown" && !result.rev.isEmpty())
            message = QStringLiteral("`%1` isn't a statement or proof at `%2` (%3).").arg(request.target, request.rev, result.rev.left(7));
        return done(stopped(result, BuildOutcome::Refused, doc.refusal, message));
    }
    if (doc.unreadable()) {
        result.outcome = BuildOutcome::Failed;
        result.problems = doc.problems;
        return done(result);
    }

    // --- The folder, one build at a time ------------------------------------------------------------
    result.folder = buildFolder(doc.target, request.mode, request.verbose, result.rev);
    const QDir folder(QDir(root).filePath(result.folder));
    if (!QDir().mkpath(folder.path()))
        return done(stopped(result, BuildOutcome::CantWork, QStringLiteral("write-failed"),
                            QStringLiteral("Loom couldn't make the build folder %1.").arg(result.folder)));
    QLockFile lock(folder.filePath(QStringLiteral("build.lock")));
    lock.setStaleLockTime(0); // A long build isn't stale; a crashed one is (QLockFile checks the process).
    QElapsedTimer waiting;
    waiting.start();
    while (!lock.tryLock(200)) {
        if (request.stop && request.stop->load())
            return done(stopped(result, BuildOutcome::Stopped, {}, {}));
        if (waiting.elapsed() >= request.lockWaitMs)
            return done(stopped(result, BuildOutcome::CantWork, QStringLiteral("locked"),
                                QStringLiteral("Another build of %1 in this mode is still running; try again when it ends.").arg(doc.target)));
    }

    const auto relative = [&](const QString& name) { return result.folder + u'/' + name; };
    const QString pdfPath = folder.filePath(QStringLiteral("main.pdf"));
    const QString lastGoodPath = folder.filePath(QStringLiteral("last-good.pdf"));
    const QString recordPath = folder.filePath(QStringLiteral("loom-build.toml"));
    const QByteArray style = loomStyle();
    const QString inputs = inputsHash(doc, style);
    result.command = QStringLiteral("latexmk ") + latexmkArguments().join(u' ');
    const auto finish = [&](BuildResult r, const QString& outcome) {
        r.seconds = static_cast<double>(clock.elapsed()) / 1000.0;
        if (QFileInfo::exists(lastGoodPath))
            r.lastGoodPdf = relative(QStringLiteral("last-good.pdf"));
        writeProjectFile(root, relative(QStringLiteral("loom-build.toml")), formatLastBuild(r, inputs, outcome));
        return r;
    };

    // --- Unchanged since a good build: its result ----------------------------------------------------
    const LastBuild last = readLastBuild(recordPath);
    if (last.inputs == inputs && last.result == u"succeeded" && QFileInfo::exists(pdfPath)) {
        result.reused = true;
        result.log = readAll(folder.filePath(QStringLiteral("main.log")));
        result.problems = doc.problems;
        for (const Problem& p : mapTexLog(parseTexLog(result.log), doc))
            result.problems.push_back(p);
        sortUniqueProblems(result.problems);
        result.outcome = BuildOutcome::Succeeded;
        result.pdf = relative(QStringLiteral("main.pdf"));
        return finish(result, QStringLiteral("succeeded"));
    }

    // --- The files ---------------------------------------------------------------------------------
    QDir(folder.filePath(QStringLiteral("src"))).removeRecursively();
    QFile::remove(folder.filePath(QStringLiteral("refs.bib")));
    std::vector<BuildFile> files = doc.files;
    files.push_back({QStringLiteral("loom.sty"), style});
    for (const BuildFile& file : files) {
        if (const QString error = writeProjectFile(root, relative(file.path), file.bytes); !error.isEmpty())
            return finish(stopped(result, BuildOutcome::CantWork, QStringLiteral("write-failed"), error), QStringLiteral("couldnt-run"));
    }
    // A PDF there now is from an earlier build; one there after is this build's. Removing it can
    // fail (another viewer holds it open on Windows); then its time tells.
    QFile::remove(pdfPath);
    const QDateTime pdfBefore = QFileInfo(pdfPath).lastModified();

    // --- latexmk -----------------------------------------------------------------------------------
    ProcessRequest run;
    run.program = request.latexmk;
    run.arguments = latexmkArguments();
    run.workingDir = folder.path();
    run.environment = latexmkEnvironment(request.environment);
    run.timeoutMs = request.timeoutMs;
    run.stop = request.stop;
    const ProcessRun ran = runProcessTree(run);
    result.output = ran.output;
    switch (ran.end) {
    case ProcessEnd::FailedToStart:
        return finish(stopped(result, BuildOutcome::CantWork, QStringLiteral("tex-missing"),
                              QStringLiteral("Loom couldn't run `latexmk`. Install TeX Live, then use Check my setup.")),
                      QStringLiteral("couldnt-run"));
    case ProcessEnd::Stopped:
        return finish(stopped(result, BuildOutcome::Stopped, {}, {}), QStringLiteral("couldnt-run"));
    case ProcessEnd::TimedOut: {
        const QString message = QStringLiteral("The build didn't finish in %1 seconds, so Loom stopped it. Look in Output for where it "
                                               "got stuck, or raise the compile timeout in Settings.")
                                    .arg(request.timeoutMs / 1000);
        result.problems = doc.problems;
        result.problems.push_back({Severity::Error, QStringLiteral("tex-timeout"), doc.targetFile, 1, message});
        sortUniqueProblems(result.problems);
        result.log = readAll(folder.filePath(QStringLiteral("main.log")));
        return finish(stopped(result, BuildOutcome::CantWork, QStringLiteral("tex-timeout"), message), QStringLiteral("couldnt-run"));
    }
    case ProcessEnd::Finished:
        break;
    }

    // --- The result --------------------------------------------------------------------------------
    result.log = readAll(folder.filePath(QStringLiteral("main.log")));
    result.problems = doc.problems;
    for (const Problem& p : mapTexLog(parseTexLog(result.log), doc))
        result.problems.push_back(p);
    const QFileInfo pdf(pdfPath);
    const bool madePdf = pdf.exists() && (!pdfBefore.isValid() || pdf.lastModified() != pdfBefore);
    if (madePdf)
        result.pdf = relative(QStringLiteral("main.pdf"));
    if (result.errors() == 0 && !madePdf)
        result.problems.push_back({Severity::Error, QStringLiteral("tex-no-pdf"), doc.targetFile, 1,
                                   QStringLiteral("latexmk finished without an error Loom could read, but made no PDF. The full log "
                                                  "is in Output.")});
    sortUniqueProblems(result.problems);
    if (result.errors() > 0) {
        result.outcome = BuildOutcome::Failed;
        return finish(result, QStringLiteral("failed"));
    }
    QFile::remove(lastGoodPath);
    QFile::copy(pdfPath, lastGoodPath);
    result.outcome = BuildOutcome::Succeeded;
    return finish(result, QStringLiteral("succeeded"));
}

} // namespace loom
