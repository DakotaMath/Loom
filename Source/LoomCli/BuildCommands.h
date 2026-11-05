#pragma once

// `loom build` (Specs/m2-statements-review.md, "`loom build`").

#include "Cli.h"

#include <QStringList>

namespace cli {

/// Runs `loom build <id> [--mode alone|premises|chain] [--verbose] [--rev <commit>]
/// [--timeout <seconds>]`: builds a statement or proof to PDF (loom::buildTarget()). Prints the
/// PDF's path (project-relative, with `/`) on stdout and each problem on stderr as `<file>:<line>:
/// <severity>: <message>`; with `--json`, only `{"target", "mode", "verbose", "rev", "pdf",
/// "built", "reused", "seconds", "problems"}` on stdout.
/// @returns 0 built (warnings allowed); 1 LaTeX errors, or a refusal (`build-stub`,
///          `ref-unknown`); 2 wrong usage; 3 can't work (`tex-missing`, `tex-timeout`, no project,
///          git failing for `--rev`, a file Loom can't write). Side effects: writes only under
///          `.loom/`; runs `latexmk` and, with `--rev`, git.
int runBuild(const QStringList& args, const Context& context);

} // namespace cli
