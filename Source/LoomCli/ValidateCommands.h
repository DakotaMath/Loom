#pragma once

// `loom validate` and `loom install-hooks` (Specs/m1-core.md, "Validation", "The project folder"
// and "The `loom` command").

#include "Cli.h"

#include <QStringList>

namespace cli {

/// Runs `loom validate [--staged]`: each problem on stderr as `<file>:<line>: <severity>:
/// <message>`, errors first, then a count line on stdout; with `--json`, only
/// `{"errors": n, "warnings": n, "problems": [...]}` on stdout.
/// @returns 0 without errors, 1 with errors, 2 wrong usage, 3 can't work (no project, a newer
///          format, an unreadable file, git missing). Side effects: runs git; writes nothing in
///          the project.
int runValidate(const QStringList& args, const Context& context);

/// Runs `loom install-hooks`: the pre-commit hook and the `loom-checks` merge driver.
/// @returns The exit code. Side effects: see loom::installHooks(); prints what it did.
int runInstallHooks(const QStringList& args, const Context& context);

} // namespace cli
