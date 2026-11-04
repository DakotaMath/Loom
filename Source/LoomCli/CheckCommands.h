#pragma once

// `loom fingerprint`, `add-check` and `merge-checks` (Specs/m1-core.md, "The `loom` command"
// and "Checks").

#include "Cli.h"

#include <QStringList>

namespace cli {

/// Runs `loom fingerprint <proof-id>`.
/// @param args The arguments after the subcommand, with `--json` and `--project` taken out.
/// @returns The exit code. Side effects: prints the fingerprint (`{"proof", "fingerprint"}`).
int runFingerprint(const QStringList& args, const Context& context);

/// Runs `loom add-check <proof-id> --by … --verdict … […]`. With `--by dakota` it is Dakota's:
/// at a terminal it asks for `changes` if a needs-changes has none, then for a typed `yes`;
/// without a terminal it refuses.
/// @returns The exit code. Side effects: appends to the proof's check history; prints the entry
///          and whether the proof is now checked.
int runAddCheck(const QStringList& args, const Context& context);

/// Runs `loom merge-checks <base> <ours> <theirs>`, the `loom-checks` git merge driver: merges
/// the three versions and writes the result into `<ours>`. Exit 0 merged, 1 left for a person
/// (git keeps its conflict), 3 a file couldn't be read or written.
/// @returns The exit code. Side effects: replaces `<ours>`; prints nothing on success, and says
///          so on stderr if it put back a deleted check.
int runMergeChecks(const QStringList& args, const Context& context);

} // namespace cli
