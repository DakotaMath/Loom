#pragma once

// `loom status`, `graph` and `refute` (Specs/m1-core.md, "The `loom` command" and "How status
// is computed").

#include "Cli.h"

#include <QStringList>

namespace cli {

/// Runs `loom status [<id>]`. Without an ID: one line per statement and stub (`ID  status
/// title`), then the counts by status; with `--json`, `{"statements": […], "counts": {…}}`.
/// With an ID: the sentence explaining its status; with `--json`, its element of `statements`.
/// @returns The exit code. Side effects: reads the project and prints; writes nothing.
int runStatus(const QStringList& args, const Context& context);

/// Runs `loom graph --json`: the nodes, proofs, `stated_using` and `paper_premises` edges and
/// cycles of premises, as the spec's JSON shape. Without `--json` it is a usage error, since the
/// graph is for programs (M5 draws it).
/// @returns The exit code. Side effects: reads the project and prints; writes nothing.
int runGraph(const QStringList& args, const Context& context);

/// Runs `loom refute <id> --by <proof-id>`, Dakota's click: at a terminal it asks for a typed
/// `yes`; without one it refuses.
/// @returns The exit code. Side effects: sets the statement's `refuted_by` and `refuted_on`;
///          prints its new status.
int runRefute(const QStringList& args, const Context& context);

} // namespace cli
