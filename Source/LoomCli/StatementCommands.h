#pragma once

// `loom new-tag`, `new-statement`, `edit-statement`, `new-proof` and `edit-proof`
// (Specs/m1-core.md, "The `loom` command").

#include "Cli.h"

#include <QStringList>

namespace cli {

/// Runs `loom new-tag <tag> --kind … --title …`.
/// @param args The arguments after the subcommand, with `--json` and `--project` taken out.
/// @returns The exit code. Side effects: adds the tag to `loom.toml`; prints the tag.
int runNewTag(const QStringList& args, const Context& context);

/// Runs `loom new-statement`.
/// @returns The exit code. Side effects: creates the statement; prints its tag (`{"id", "path"}`).
int runNewStatement(const QStringList& args, const Context& context);

/// Runs `loom edit-statement <id>`.
/// @returns The exit code. Side effects: rewrites the statement; prints the changed fields.
int runEditStatement(const QStringList& args, const Context& context);

/// Runs `loom new-proof <statement-id>`.
/// @returns The exit code. Side effects: creates the proof; prints its ID (`{"id", "path"}`).
int runNewProof(const QStringList& args, const Context& context);

/// Runs `loom edit-proof <proof-id>`.
/// @returns The exit code. Side effects: rewrites the proof; prints the changed fields.
int runEditProof(const QStringList& args, const Context& context);

} // namespace cli
