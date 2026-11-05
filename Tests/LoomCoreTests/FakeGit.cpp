// loom-fake-git: a stand-in for git, for the failures of `loom new-project` and `loom validate` a
// real git can't be made to show on demand. LOOM_FAKE_GIT_MODE picks the failure:
//   no-lfs      `git lfs version` fails, as when Git LFS isn't installed.
//   init-fails  `git init` fails, after Loom has written the project files.
//   head-fails  `git rev-parse --git-dir` finds a repository, but reading `HEAD` fails with a
//               fatal error (exit 128), as for a damaged repository.
//   dubious     Every `git rev-parse` fails as git does in a repository it refuses to use
//               ("dubious ownership", exit 128).
//   slow        Every command but `--version` waits 30 seconds before doing anything, as a git
//               stuck on a lock or a slow network drive does; for timeouts.
//   slow-at     The command whose arguments start with LOOM_FAKE_GIT_SLOW (words separated by
//               spaces, e.g. `rev-parse --git-path`) waits 30 seconds; every other command runs
//               the real `git` on PATH, so a test can stop git at any step of a real run.
// Everything else succeeds and does nothing; `git config --get` prints a made-up value, except
// for `core.hooksPath`, which is unset (exit 1, as git gives for a key that isn't set).

#include <QProcess>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

/// The `slow-at` mode: waits 30 seconds if `args` start with the words of LOOM_FAKE_GIT_SLOW,
/// else runs the real git with the same arguments, input and output.
/// @returns The real git's exit code; 0 after waiting; 127 if the real git didn't run.
/// Side effects: whatever the real git command does.
int slowAt(const std::vector<std::string>& args)
{
    QStringList arguments;
    for (const std::string& arg : args)
        arguments << QString::fromLocal8Bit(arg); // main()'s arguments are in the local code page.
    const QStringList slow = qEnvironmentVariable("LOOM_FAKE_GIT_SLOW").split(u' ', Qt::SkipEmptyParts);
    if (!slow.isEmpty() && arguments.mid(0, slow.size()) == slow) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        return 0;
    }
    QProcess git;
    git.setProgram(QStringLiteral("git"));
    git.setArguments(arguments);
    git.setProcessChannelMode(QProcess::ForwardedChannels);
    git.setInputChannelMode(QProcess::ForwardedInputChannel);
    git.start();
    if (!git.waitForFinished(-1) || git.exitStatus() != QProcess::NormalExit)
        return 127;
    return git.exitCode();
}

/// Runs the stand-in.
/// @param argc, argv git's arguments.
/// @returns 0, or git's error codes for the failure LOOM_FAKE_GIT_MODE asks for.
/// Side effects: prints to stdout or stderr as git would.
int main(int argc, char* argv[])
{
    const QString mode = qEnvironmentVariable("LOOM_FAKE_GIT_MODE");
    const std::vector<std::string> args(argv + 1, argv + argc);
    const std::string command = args.empty() ? std::string() : args[0];
    if (mode == u"slow" && command != "--version")
        std::this_thread::sleep_for(std::chrono::seconds(30));
    if (mode == u"slow-at")
        return slowAt(args);

    if (command == "--version") {
        std::cout << "git version 0.0.0.fake\n";
    } else if (command == "lfs" && args.size() == 2 && args[1] == "version") {
        if (mode == u"no-lfs") {
            std::cerr << "git: 'lfs' is not a git command. See 'git --help'.\n";
            return 1;
        }
        std::cout << "git-lfs/0.0.0 (fake)\n";
    } else if (command == "config") {
        if (args.back() == "core.hooksPath")
            return 1;
        std::cout << "Fake Value\n";
    } else if (command == "init" && mode == u"init-fails") {
        std::cerr << "fatal: the fake git won't init.\n";
        return 128;
    } else if (command == "rev-parse" && mode == u"dubious") {
        std::cerr << "fatal: detected dubious ownership in repository at 'C:/Fake'\n";
        return 128;
    } else if (command == "rev-parse" && mode == u"head-fails") {
        if (args.size() == 2 && args[1] == "--git-dir") {
            std::cout << ".git\n";
            return 0;
        }
        std::cerr << "fatal: bad object HEAD\n";
        return 128;
    }
    return 0;
}
