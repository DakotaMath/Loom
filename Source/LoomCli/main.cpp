// The `loom` command (Specs/m1-core.md, "The `loom` command"). For now it knows only
// `--version`; the other subcommands arrive with their build issues (#4 onwards).

#include "LoomCore/Version.h"

#include <iostream>
#include <string_view>
#include <vector>

namespace {

// Exit codes, from the spec's table.
constexpr int exitDone = 0;
constexpr int exitUsage = 2;

/// Prints how to use `loom` to stderr.
/// @returns exitUsage, so callers can `return printUsage();`.
int printUsage()
{
    std::cerr << "Usage: loom --version [--json]\n";
    return exitUsage;
}

} // namespace

/// Runs `loom`.
/// @param argc, argv The command line.
/// @returns An exit code from the spec's table: 0 done, 2 wrong usage.
/// Side effects: writes the result to stdout and usage messages to stderr.
int main(int argc, char* argv[])
{
    std::vector<std::string_view> args(argv + 1, argv + argc);

    bool json = false;
    std::erase_if(args, [&json](std::string_view arg) {
        if (arg == "--json") {
            json = true;
            return true;
        }
        return false;
    });

    if (args.size() == 1 && args[0] == "--version") {
        if (json)
            std::cout << "{\"version\": \"" << loom::versionString() << "\"}\n";
        else
            std::cout << "loom " << loom::versionString() << '\n';
        return exitDone;
    }

    return printUsage();
}
