#include "LoomCore/Git.h"

#include <QProcess>

namespace loom {

GitRun runGit(const GitTool& git, const QStringList& arguments, const QString& workingDir, int timeoutMs)
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
    process.closeWriteChannel();
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished();
        return run;
    }
    run.started = process.exitStatus() == QProcess::NormalExit;
    run.exitCode = process.exitCode();
    run.output = QString::fromUtf8(process.readAllStandardOutput());
    run.errors = QString::fromUtf8(process.readAllStandardError());
    return run;
}

} // namespace loom
