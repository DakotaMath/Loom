#include "LoomCore/ProcessTree.h"

#include <QElapsedTimer>
#include <QProcess>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#else
#include <csignal>
#include <sys/types.h>
#endif

namespace loom {

namespace {

#ifdef Q_OS_WIN
/// A job object whose processes all end when it is closed: what lets Loom stop latexmk together
/// with the pdflatex it started.
class ProcessGroup {
public:
    ProcessGroup() : m_job(CreateJobObjectW(nullptr, nullptr))
    {
        if (!m_job)
            return;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(m_job, JobObjectExtendedLimitInformation, &limits, sizeof limits);
    }
    ~ProcessGroup()
    {
        if (m_job)
            CloseHandle(m_job); // Ends whatever is still in it.
    }
    ProcessGroup(const ProcessGroup&) = delete;
    ProcessGroup& operator=(const ProcessGroup&) = delete;

    /// Makes the process start suspended, so it can join the job before it runs.
    void prepare(QProcess& process)
    {
        process.setCreateProcessArgumentsModifier(
            [](QProcess::CreateProcessArguments* arguments) { arguments->flags |= CREATE_SUSPENDED; });
    }

    /// Puts the started (suspended) process in the job, then lets it run. QProcess doesn't give
    /// the thread's handle, so the process's threads are found by a snapshot; a suspended new
    /// process has exactly one.
    void adopt(qint64 pid)
    {
        if (HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid))) {
            if (m_job)
                AssignProcessToJobObject(m_job, process);
            CloseHandle(process);
        }
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return;
        THREADENTRY32 entry{};
        entry.dwSize = sizeof entry;
        for (BOOL more = Thread32First(snapshot, &entry); more; more = Thread32Next(snapshot, &entry)) {
            if (entry.th32OwnerProcessID != static_cast<DWORD>(pid))
                continue;
            if (HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID)) {
                ResumeThread(thread);
                CloseHandle(thread);
            }
        }
        CloseHandle(snapshot);
    }

    /// Ends every process in the job.
    void stop(QProcess& process)
    {
        if (m_job)
            TerminateJobObject(m_job, 1);
        else
            process.kill();
    }

private:
    HANDLE m_job;
};
#else
/// A session of the program's own, so its process group can be killed whole.
class ProcessGroup {
public:
    /// Starts the program as the leader of a new session (and so of a new process group).
    void prepare(QProcess& process)
    {
        process.setUnixProcessParameters(QProcess::UnixProcessFlag::CreateNewSession);
    }
    /// Remembers the group to stop.
    void adopt(qint64 pid) { m_group = static_cast<pid_t>(pid); }
    /// Kills every process in the group.
    void stop(QProcess& process)
    {
        if (m_group > 0)
            ::kill(-m_group, SIGKILL);
        else
            process.kill();
    }

private:
    pid_t m_group = 0;
};
#endif

} // namespace

ProcessRun runProcessTree(const ProcessRequest& request)
{
    ProcessRun run;
    ProcessGroup group;
    QProcess process;
    process.setProgram(request.program);
    process.setArguments(request.arguments);
    process.setWorkingDirectory(request.workingDir);
    process.setProcessEnvironment(request.environment);
    process.setProcessChannelMode(QProcess::MergedChannels);
    group.prepare(process);
    process.start();
    if (!process.waitForStarted()) {
        run.end = ProcessEnd::FailedToStart;
        return run;
    }
    group.adopt(process.processId());

    QElapsedTimer clock;
    clock.start();
    run.end = ProcessEnd::Finished;
    // Short waits, so a stop request or the timeout is noticed promptly; QProcess reads the
    // output while waiting, so the program never blocks on a full pipe.
    while (!process.waitForFinished(100)) {
        if (process.state() == QProcess::NotRunning)
            break;
        const bool stopRequested = request.stop && request.stop->load();
        if (stopRequested || clock.elapsed() >= request.timeoutMs) {
            run.end = stopRequested ? ProcessEnd::Stopped : ProcessEnd::TimedOut;
            group.stop(process);
            process.waitForFinished(5000);
            break;
        }
    }
    // Whatever it left behind goes too, so no stray pdflatex holds the build's files.
    if (run.end == ProcessEnd::Finished)
        group.stop(process);
    run.output = process.readAll();
    run.exitCode = run.end == ProcessEnd::Finished ? process.exitCode() : -1;
    return run;
}

} // namespace loom
