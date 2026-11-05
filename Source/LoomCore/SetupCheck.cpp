#include "LoomCore/SetupCheck.h"

#include "LoomCore/Git.h"
#include "LoomCore/NewProject.h"
#include "LoomCore/ProcessTree.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace loom {

namespace {

/// The oldest git Loom accepts (Specs/m2-statements-review.md, "Settings": "2.40 or newer").
constexpr int minimumGitMajor = 2;
constexpr int minimumGitMinor = 40;

/// The first non-empty line of a program's output, trimmed. No side effects.
QString firstLine(const QString& output)
{
    for (const QString& line : output.split(u'\n')) {
        if (const QString trimmed = line.trimmed(); !trimmed.isEmpty())
            return trimmed;
    }
    return {};
}

/// An item that is found.
SetupItem found(const char* key, const QString& name, const QString& version, const QString& path)
{
    return {QString::fromLatin1(key), name, SetupState::Found, version, path, {}};
}

/// An item that isn't found, or doesn't work.
SetupItem notFound(const char* key, const QString& name, SetupState state, const QString& message, const QString& path = {})
{
    return {QString::fromLatin1(key), name, state, {}, path, message};
}

/// Whether a run counts as the tool working: it ran, and (unless `anyExit`) exited 0.
bool worked(const ProcessRun& run, bool anyExit)
{
    return run.end == ProcessEnd::Finished && (anyExit || run.exitCode == 0);
}

/// Why a found program didn't work, for a cross's message after the spec's own words.
QString whyItFailed(const ProcessRun& run, int timeoutMs)
{
    switch (run.end) {
    case ProcessEnd::FailedToStart:
        return QStringLiteral("It was found but didn't start.");
    case ProcessEnd::TimedOut:
        return QStringLiteral("It didn't finish in %1 seconds.").arg(timeoutMs / 1000);
    case ProcessEnd::Stopped:
        return {};
    case ProcessEnd::Finished:
        break;
    }
    const QString said = firstLine(QString::fromUtf8(run.output));
    return said.isEmpty() ? QStringLiteral("It exited with code %1.").arg(run.exitCode)
                          : QStringLiteral("It exited with code %1: %2").arg(run.exitCode).arg(said);
}

/// The lines to show for a failed test build (Specs/m2-statements-review.md, "Settings": "The
/// first lines of the log"): from the log's first error (a line starting `!`), else its first
/// lines, else what `latexmk` printed; at most `count` lines.
QString logExcerpt(const QByteArray& log, const QByteArray& output, int count = 6)
{
    QStringList lines = QString::fromUtf8(log.isEmpty() ? output : log).split(u'\n');
    for (QString& line : lines)
        line = line.trimmed();
    lines.removeAll(QString());
    qsizetype start = 0;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (lines[i].startsWith(u'!')) {
            start = i;
            break;
        }
    }
    return lines.mid(start, count).join(u'\n');
}

/// Runs the checks; see checkSetup(). `report` is called with each item.
void runChecks(const SetupRequest& request, const std::function<void(const SetupItem&)>& report)
{
    const auto stopped = [&] { return request.stop && request.stop->load(); };
    QTemporaryDir temp;
    const QString workDir = temp.isValid() ? temp.path() : QDir::tempPath();

    const auto run = [&](const QString& program, const QStringList& arguments, int timeoutMs, const QString& in) {
        ProcessRequest process;
        process.program = program;
        process.arguments = arguments;
        process.workingDir = in;
        process.environment = request.environment;
        process.timeoutMs = timeoutMs;
        process.stop = request.stop;
        return runProcessTree(process);
    };

    // git, 2.40 or newer.
    const QString gitName = QStringLiteral("git");
    const QString git = findProgram(request.git, request.environment);
    bool gitWorks = false;
    if (git.isEmpty()) {
        report(notFound("git", gitName, SetupState::Missing, QStringLiteral("Install Git for Windows.")));
    } else {
        const ProcessRun version = run(git, {QStringLiteral("--version")}, request.toolTimeoutMs, workDir);
        if (stopped())
            return;
        const QString line = firstLine(QString::fromUtf8(version.output));
        static const QRegularExpression number(QStringLiteral("(\\d+)\\.(\\d+)"));
        const QRegularExpressionMatch match = number.match(line);
        if (!worked(version, false) || !match.hasMatch()) {
            report(notFound("git", gitName, SetupState::Missing,
                          QStringLiteral("Install Git for Windows. %1").arg(whyItFailed(version, request.toolTimeoutMs)).trimmed(), git));
        } else if (const int major = match.captured(1).toInt(), minor = match.captured(2).toInt();
                   major < minimumGitMajor || (major == minimumGitMajor && minor < minimumGitMinor)) {
            report(notFound("git", gitName, SetupState::Missing,
                          QStringLiteral("This is %1; Loom needs git %2.%3 or newer. Install Git for Windows.")
                              .arg(line).arg(minimumGitMajor).arg(minimumGitMinor), git));
        } else {
            gitWorks = true;
            report(found("git", gitName, line, git));
        }
    }
    if (stopped())
        return;

    // git's identity, as a new repository would see it: pointing GIT_DIR at a .git folder that
    // doesn't exist makes git read the global and system config only (as createProject() does).
    const QString identityName = QStringLiteral("git identity");
    if (!gitWorks) {
        report(notFound("git-identity", identityName, SetupState::Skipped, QStringLiteral("Needs git.")));
    } else {
        GitTool tool;
        tool.program = git;
        tool.environment = request.environment;
        tool.environment.insert(QStringLiteral("GIT_DIR"), workDir + QStringLiteral("/no-repository/.git"));
        tool.timeoutMs = request.toolTimeoutMs;
        QStringList values;
        for (const char* key : {"user.name", "user.email"}) {
            const GitRun get = runGit(tool, {QStringLiteral("config"), QStringLiteral("--get"), QString::fromLatin1(key)}, workDir);
            if (get.ok() && !get.output.trimmed().isEmpty())
                values << get.output.trimmed();
        }
        if (stopped())
            return;
        if (values.size() == 2)
            report(found("git-identity", identityName, QStringLiteral("%1 <%2>").arg(values[0], values[1]), {}));
        else
            report(notFound("git-identity", identityName, SetupState::Missing, gitIdentityMissingMessage()));
    }
    if (stopped())
        return;

    // Git LFS.
    const QString lfsName = QStringLiteral("Git LFS");
    if (!gitWorks) {
        report(notFound("git-lfs", lfsName, SetupState::Skipped, QStringLiteral("Needs git.")));
    } else {
        const ProcessRun lfs = run(git, {QStringLiteral("lfs"), QStringLiteral("version")}, request.toolTimeoutMs, workDir);
        if (stopped())
            return;
        if (worked(lfs, false))
            report(found("git-lfs", lfsName, firstLine(QString::fromUtf8(lfs.output)), git));
        else
            report(notFound("git-lfs", lfsName, SetupState::Missing, gitLfsMissingMessage()));
    }
    if (stopped())
        return;

    // A tool checked by one command: found and working is a tick, else `missing`'s words (and
    // why it failed, when it was found).
    const auto tool = [&](const char* key, const QString& name, const QString& program, const QStringList& arguments,
                          SetupState whenMissing, const QString& missing, bool anyExit) {
        const QString path = findProgram(program, request.environment);
        if (path.isEmpty()) {
            report(notFound(key, name, whenMissing, missing));
            return false;
        }
        const ProcessRun ran = run(path, arguments, request.toolTimeoutMs, workDir);
        if (stopped())
            return false;
        if (!worked(ran, anyExit)) {
            report(notFound(key, name, whenMissing, (missing + u' ' + whyItFailed(ran, request.toolTimeoutMs)).trimmed(), path));
            return false;
        }
        report(found(key, name, firstLine(QString::fromUtf8(ran.output)), path));
        return true;
    };

    tool("loom", QStringLiteral("loom on PATH"), request.loom, {QStringLiteral("--version")}, SetupState::Missing,
         QStringLiteral("The commit guard runs `loom` from PATH. Add Loom's folder to PATH."), false);
    if (stopped())
        return;
    const bool latexmk = tool("latexmk", QStringLiteral("latexmk"), request.latexmk, {QStringLiteral("-v")}, SetupState::Missing,
         QStringLiteral("Install TeX Live (it includes latexmk and Perl). With MiKTeX, also install Perl."), false);
    if (stopped())
        return;
    const bool pdflatex = tool("pdflatex", QStringLiteral("pdfLaTeX"), request.pdflatex, {QStringLiteral("--version")},
         SetupState::Missing, QStringLiteral("Install TeX Live."), false);
    if (stopped())
        return;
    tool("synctex", QStringLiteral("synctex"), request.synctex, {QStringLiteral("help")}, SetupState::Warning,
         QStringLiteral("Without synctex, clicking in the PDF can't find the source; everything else works."), true);
    if (stopped())
        return;

    // A test build of a two-line document.
    const QString buildName = QStringLiteral("A test build");
    if (!latexmk || !pdflatex) {
        report(notFound("test-build", buildName, SetupState::Skipped, QStringLiteral("Needs latexmk and pdfLaTeX.")));
        return;
    }
    const QString buildDir = workDir + QStringLiteral("/test-build");
    QFile document(buildDir + QStringLiteral("/main.tex"));
    if (!QDir().mkpath(buildDir) || !document.open(QIODevice::WriteOnly)) {
        report(notFound("test-build", buildName, SetupState::Missing, QStringLiteral("Loom couldn't write a test document in the temporary folder.")));
        return;
    }
    document.write("\\documentclass{article}\n\\begin{document}Loom's test build.\\end{document}\n");
    document.close();
    QElapsedTimer timer;
    timer.start();
    const ProcessRun build = run(findProgram(request.latexmk, request.environment),
                                 {QStringLiteral("-norc"), QStringLiteral("-pdf"), QStringLiteral("-interaction=nonstopmode"),
                                  QStringLiteral("main.tex")},
                                 request.compileTimeoutMs, buildDir);
    if (stopped())
        return;
    QFile logFile(buildDir + QStringLiteral("/main.log"));
    const QByteArray log = logFile.open(QIODevice::ReadOnly) ? logFile.readAll() : QByteArray();
    if (build.end == ProcessEnd::TimedOut) {
        report(notFound("test-build", buildName, SetupState::Missing,
                      QStringLiteral("It didn't finish within the compile timeout (%1 seconds).\n%2")
                          .arg(request.compileTimeoutMs / 1000).arg(logExcerpt(log, build.output)).trimmed()));
    } else if (!worked(build, false) || !QFileInfo::exists(buildDir + QStringLiteral("/main.pdf"))) {
        report(notFound("test-build", buildName, SetupState::Missing, logExcerpt(log, build.output)));
    } else {
        report(found("test-build", buildName, QStringLiteral("Built in %1 s").arg(timer.elapsed() / 1000.0, 0, 'f', 1), {}));
    }
}

} // namespace

QString findProgram(const QString& program, const QProcessEnvironment& environment)
{
    if (program.isEmpty())
        return {};
    if (program.contains(u'/') || program.contains(u'\\')) {
        QStringList candidates = {program};
#ifdef Q_OS_WIN
        if (!program.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
            candidates << program + QStringLiteral(".exe");
#endif
        for (const QString& candidate : candidates) {
            const QFileInfo info(candidate);
            if (info.isFile() && info.isExecutable())
                return QDir::cleanPath(info.absoluteFilePath());
        }
        return {};
    }
    const QStringList path = environment.value(QStringLiteral("PATH")).split(QDir::listSeparator(), Qt::SkipEmptyParts);
    if (path.isEmpty())
        return {};
    const QString found = QStandardPaths::findExecutable(program, path);
    return found.isEmpty() ? QString() : QDir::cleanPath(QDir::fromNativeSeparators(found));
}

std::vector<SetupItem> checkSetup(const SetupRequest& request, const std::function<void(const SetupItem&)>& onItem)
{
    std::vector<SetupItem> items;
    runChecks(request, [&](const SetupItem& item) {
        items.push_back(item);
        if (onItem)
            onItem(item);
    });
    return items;
}

} // namespace loom
