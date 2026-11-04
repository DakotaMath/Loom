#pragma once

// Finding circles in a graph of IDs: `stated_using` and `superseded_by` here, premises in #7.

#include <QMap>
#include <QString>
#include <QStringList>

#include <vector>

namespace loom {

/// Finds one circle in each group of nodes that can all reach one another (each strongly
/// connected component with more than one node), so every circle in the graph is reported
/// through at least one of its groups, and no group twice. Edges from a node to itself are
/// ignored: the callers report those with their own message.
/// Each circle starts at its group's smallest ID (byte order) and is a shortest way back to it,
/// taking smaller IDs first; it ends by repeating the start, e.g. `["A", "B", "A"]`. Circles are
/// sorted by their first ID, so the result doesn't depend on the order of `edges`.
/// @param edges Each node's successors. Successors that aren't keys are nodes with no edges.
/// @returns The circles. No side effects.
std::vector<QStringList> findCycles(const QMap<QString, QStringList>& edges);

} // namespace loom
