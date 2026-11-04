// loom-fake-git: a stand-in for git, for the failures of `loom new-project` a real git can't be
// made to show on demand. LOOM_FAKE_GIT_MODE picks the failure:
//   no-lfs      `git lfs version` fails, as when Git LFS isn't installed.
//   init-fails  `git init` fails, after Loom has written the project files.
// Everything else succeeds and does nothing; `git config --get` prints a made-up value, except
// for `core.hooksPath`, which is unset (exit 1, as git gives for a key that isn't set).

#include <QString>
#include <QtGlobal>

#include <iostream>
#include <string>
#include <vector>

/// Runs the stand-in.
/// @param argc, argv git's arguments.
/// @returns 0, or git's error codes for the failure LOOM_FAKE_GIT_MODE asks for.
/// Side effects: prints to stdout or stderr as git would.
int main(int argc, char* argv[])
{
    const QString mode = qEnvironmentVariable("LOOM_FAKE_GIT_MODE");
    const std::vector<std::string> args(argv + 1, argv + argc);
    const std::string command = args.empty() ? std::string() : args[0];

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
    }
    return 0;
}
