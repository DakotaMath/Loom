#include "LoomCore/StatusChanges.h"

#include "LoomCore/ProjectContents.h"

#include <algorithm>
#include <map>

namespace loom {

namespace {

/// A status's place in its order, higher being better, and which order it is in: 1 for proved
/// down to open, 2 for cited and extracted; 0 for false and no status, which are in neither.
std::pair<int, int> rankOf(Status status)
{
    switch (status) {
    case Status::Proved: return {1, 6};
    case Status::Conditional: return {1, 5};
    case Status::Unchecked: return {1, 4};
    case Status::Sketch: return {1, 3};
    case Status::Heuristic: return {1, 2};
    case Status::Open: return {1, 1};
    case Status::Cited: return {2, 2};
    case Status::Extracted: return {2, 1};
    case Status::False:
    case Status::None: return {0, 0};
    }
    return {0, 0};
}

/// Where a direction sorts: drops first, then raises, then the rest.
int sortGroup(StatusDirection direction)
{
    switch (direction) {
    case StatusDirection::Drop: return 0;
    case StatusDirection::Raise: return 1;
    case StatusDirection::Neither: return 2;
    }
    return 2;
}

} // namespace

StatusDirection statusDirection(Status before, Status after)
{
    const auto [orderBefore, rankBefore] = rankOf(before);
    const auto [orderAfter, rankAfter] = rankOf(after);
    if (orderBefore == 0 || orderBefore != orderAfter || rankBefore == rankAfter)
        return StatusDirection::Neither;
    return rankAfter > rankBefore ? StatusDirection::Raise : StatusDirection::Drop;
}

std::vector<StatusChange> statusChanges(const ProjectGraph& before, const ProjectGraph& after)
{
    // Every tag in either graph, once, in tag order.
    std::map<QString, std::pair<const StatementNode*, const StatementNode*>> both;
    for (const StatementNode& node : before.statements)
        both[node.id].first = &node;
    for (const StatementNode& node : after.statements)
        both[node.id].second = &node;

    std::vector<StatusChange> changes;
    for (const auto& [id, nodes] : both) {
        const auto& [old, now] = nodes;
        StatusChange change;
        change.id = id;
        change.title = now ? now->title : old->title;
        change.before = old ? old->status : Status::None;
        change.after = now ? now->status : Status::None;
        if (change.before == change.after)
            continue;
        change.direction = statusDirection(change.before, change.after);
        changes.push_back(change);
    }
    // Stable, so each group stays in tag order.
    std::stable_sort(changes.begin(), changes.end(),
                     [](const StatusChange& a, const StatusChange& b) { return sortGroup(a.direction) < sortGroup(b.direction); });
    return changes;
}

std::vector<StatusChange> statusChanges(const ProjectContents& before, const ProjectContents& after)
{
    return statusChanges(buildGraph(before), buildGraph(after));
}

} // namespace loom
