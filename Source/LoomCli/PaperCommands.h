#pragma once

// `loom new-paper`, `new-stub`, `rekey` and `build-bib` (Specs/m1-core.md, "The `loom` command"
// and "Papers").

#include "Cli.h"

#include <QStringList>

namespace cli {

/// Runs `loom new-paper <draft.toml>`.
/// @param args The arguments after the subcommand, with `--json` and `--project` taken out.
/// @returns The exit code. Side effects: creates the paper's entry, and renames a paper that now
///          shares its key; prints the key (`{"key", "path", "renamed": [{"from", "to"}]}`), and
///          each rename on stderr.
int runNewPaper(const QStringList& args, const Context& context);

/// Runs `loom new-stub <key> --kind … --gist … --version … --at … --by … […]`.
/// @returns The exit code. Side effects: creates the stub; prints its tag (`{"id", "path"}`).
int runNewStub(const QStringList& args, const Context& context);

/// Runs `loom rekey <key>`.
/// @returns The exit code. Side effects: renames papers; prints `old → new`, or that the key is
///          already right (`{"from", "to", "renamed": [{"from", "to"}]}`).
int runRekey(const QStringList& args, const Context& context);

/// Runs `loom build-bib [--out <path>]`; `--out` is relative to the current folder and must be
/// inside the project.
/// @returns The exit code: 0 written (even with skipped entries), 3 can't work. Side effects:
///          writes the bibliography; prints its path, and its problems on stderr
///          (`{"path", "problems": [{"severity", "code", "file", "line", "message"}]}`).
int runBuildBib(const QStringList& args, const Context& context);

} // namespace cli
