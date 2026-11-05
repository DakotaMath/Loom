// loom-fake-tool: a stand-in for each program *Check my setup* runs (Specs/m2-statements-review.md,
// "Settings"; Verification, "Setup checks": "Found and missing tools (with a PATH of stand-ins)").
// The tests copy it into a folder as git, latexmk, pdflatex, synctex and loom; it acts as the one
// its file is named after. It uses no Qt, so it runs with a PATH that holds only stand-ins.
//
//   git       `--version` prints LOOM_FAKE_GIT_VERSION, else `git version 2.47.1.fake`;
//             `config --get user.name` / `user.email` print a made-up identity, unless
//             LOOM_FAKE_NO_IDENTITY is set (then exit 1, as for a key that isn't set);
//             `lfs version` prints a version, unless LOOM_FAKE_NO_LFS is set (then exit 1).
//   latexmk   `-v` prints a version; otherwise it builds: with LOOM_FAKE_LATEX=fail it writes a
//             `main.log` with an error and exits 12, with `slow` it waits 30 seconds, and
//             otherwise it writes `main.pdf` and `main.log` and exits 0.
//   pdflatex  `--version` prints a version.
//   synctex   `help` prints its banner and exits 1, as some versions do.
//   loom      `--version` prints `loom 0.1.0`.

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

/// Whether an environment variable is set. No side effects.
bool isSet(const char* name)
{
    return std::getenv(name) != nullptr;
}

/// git's commands.
int git(const std::vector<std::string>& args)
{
    if (args.size() == 1 && args[0] == "--version") {
        const char* version = std::getenv("LOOM_FAKE_GIT_VERSION");
        std::cout << (version ? version : "git version 2.47.1.fake") << '\n';
        return 0;
    }
    if (args.size() == 3 && args[0] == "config" && args[1] == "--get") {
        if (isSet("LOOM_FAKE_NO_IDENTITY"))
            return 1;
        std::cout << (args[2] == "user.name" ? "Ada Example" : "ada@example.com") << '\n';
        return 0;
    }
    if (args.size() == 2 && args[0] == "lfs" && args[1] == "version") {
        if (isSet("LOOM_FAKE_NO_LFS")) {
            std::cerr << "git: 'lfs' is not a git command. See 'git --help'.\n";
            return 1;
        }
        std::cout << "git-lfs/3.6.0 (fake)\n";
        return 0;
    }
    return 0;
}

/// latexmk's version, or a build of `main.tex` in the current folder.
int latexmk(const std::vector<std::string>& args)
{
    if (args.size() == 1 && args[0] == "-v") {
        std::cout << "\nLatexmk, John Collins, fake. Version 4.86\n";
        return 0;
    }
    const char* mode = std::getenv("LOOM_FAKE_LATEX");
    const std::string how = mode ? mode : "";
    if (how == "slow") {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        return 0;
    }
    std::ofstream log("main.log");
    log << "This is pdfTeX, Version 3.141592653-2.6-1.40.27 (fake)\n";
    if (how == "fail") {
        log << "! Undefined control sequence.\nl.2 \\begin{document}\\foo\n";
        return 12;
    }
    std::ofstream("main.pdf") << "%PDF-1.5 fake\n";
    return 0;
}

} // namespace

/// Acts as the program its file is named after; see the comment at the top.
/// @returns The exit code that program would give.
int main(int argc, char* argv[])
{
    const std::string name = std::filesystem::path(argv[0]).stem().string();
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (name == "git")
        return git(args);
    if (name == "latexmk")
        return latexmk(args);
    if (name == "pdflatex") {
        std::cout << "pdfTeX 3.141592653-2.6-1.40.27 (fake)\n";
        return 0;
    }
    if (name == "synctex") {
        std::cout << "This is SyncTeX command line utility, version 1.5 (fake)\n";
        return 1;
    }
    if (name == "loom") {
        std::cout << "loom 0.1.0\n";
        return 0;
    }
    return 0; // Run as itself: nothing to do (the lock tests use it as a process that ends).
}
