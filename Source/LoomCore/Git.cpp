#include "LoomCore/Git.h"

#include <QProcess>

namespace loom {

GitRun runGit(const GitTool& git, const QStringList& arguments, const QString& workingDir, int timeoutMs,
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
    if (!process.waitForFinished(timeoutMs)) {
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

} // namespace loom
