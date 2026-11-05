#include "LoomCore/Git.h"

#include <QProcess>

#include <algorithm>

namespace loom {

GitRun runGit(const GitTool& git, const QStringList& arguments, const QString& workingDir, std::optional<int> timeoutMs,
              const QByteArray& input)
{
    QProcess process;
    process.setProgram(git.program);
    process.setArguments(arguments);
    process.setWorkingDirectory(workingDir);
    process.setProcessEnvironment(git.environment);
    process.start();

    GitRun run;
    if (!process.waitForStarted())
        return run;
    // QProcess buffers the input and writes it while waiting below, so a large input can't
    // block against git's output filling its pipe.
    if (!input.isEmpty())
        process.write(input);
    process.closeWriteChannel();
    if (!process.waitForFinished(timeoutMs.value_or(git.timeoutMs))) {
        // Still running means it ran out of time; otherwise waiting failed for another reason.
        run.timedOut = process.state() != QProcess::NotRunning;
        process.kill();
        process.waitForFinished();
        return run;
    }
    run.started = process.exitStatus() == QProcess::NormalExit;
    run.exitCode = process.exitCode();
    run.outputBytes = process.readAllStandardOutput();
    run.output = QString::fromUtf8(run.outputBytes);
    run.errors = QString::fromUtf8(process.readAllStandardError());
    return run;
}

QString gitCommand(const QStringList& arguments)
{
    QString command = QStringLiteral("git");
    for (QString argument : arguments) {
        const bool quoted = argument.isEmpty() || argument.contains(u'"')
                         || std::any_of(argument.cbegin(), argument.cend(), [](QChar c) { return c.isSpace(); });
        if (quoted) {
            argument.replace(QStringLiteral("\""), QStringLiteral("\\\""));
            argument = u'"' + argument + u'"';
        }
        command += u' ' + argument;
    }
    return command;
}

QString gitDidntRunMessage(const GitRun& run, const QStringList& arguments)
{
    if (run.timedOut)
        return QStringLiteral("`%1` didn't finish in time, so Loom stopped it. Git may be waiting for something, such as "
                              "another git's lock or a slow network drive; try again.")
            .arg(gitCommand(arguments));
    return QStringLiteral("Loom needs git, and `git` didn't run. Install Git for Windows and try again.");
}

QString gitDidntRunCode(const GitRun& run, const QString& missingCode)
{
    return run.timedOut ? QStringLiteral("git-timeout") : missingCode;
}

std::vector<GitListingEntry> parseGitListing(const QByteArray& output, bool index)
{
    std::vector<GitListingEntry> entries;
    for (const QByteArray& record : output.split('\0')) {
        const qsizetype tab = record.indexOf('\t');
        if (tab < 0)
            continue;
        const QList<QByteArray> fields = record.first(tab).split(' ');
        if (fields.size() != 3 || (index && fields[2] != "0"))
            continue;
        entries.push_back({QString::fromLatin1(fields[0]), index ? fields[1] : fields[2], QString::fromUtf8(record.sliced(tab + 1))});
    }
    return entries;
}

const QStringList& gitCatFileBatch()
{
    static const QStringList arguments = {QStringLiteral("cat-file"), QStringLiteral("--batch")};
    return arguments;
}

std::optional<std::vector<QByteArray>> readGitObjects(const GitTool& git, const QString& root,
                                                      const std::vector<QByteArray>& shas, GitRun& run)
{
    if (shas.empty())
        return std::vector<QByteArray>();
    QByteArray input;
    for (const QByteArray& sha : shas)
        input += sha + '\n';
    run = runGit(git, gitCatFileBatch(), root, std::nullopt, input);
    if (!run.ok())
        return std::nullopt;
    // Each object is `<sha> <type> <size>\n<contents>\n`.
    std::vector<QByteArray> objects;
    const QByteArray& out = run.outputBytes;
    qsizetype at = 0;
    for (std::size_t i = 0; i < shas.size(); ++i) {
        const qsizetype headerEnd = out.indexOf('\n', at);
        if (headerEnd < 0)
            return std::nullopt;
        const QList<QByteArray> header = out.sliced(at, headerEnd - at).split(' ');
        bool sized = false;
        const qsizetype size = header.size() == 3 ? header[2].toLongLong(&sized) : 0;
        if (!sized || headerEnd + 1 + size > out.size())
            return std::nullopt;
        objects.push_back(out.sliced(headerEnd + 1, size));
        at = headerEnd + 1 + size + 1;
    }
    return objects;
}

} // namespace loom
