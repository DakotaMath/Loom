#pragma once

// The statuses a change makes or would make (Specs/m2-statements-review.md, "What LoomCore gains in
// M2": "Status changes", and "Rules: Status changes"): every statement or stub whose computed
// status differs between two versions of a project, with raises and drops marked. Dakota's
// confirmation dialogs show them (#38), and Review shows them between `HEAD` and now (#40); the
// app never works them out itself.

#include "LoomCore/Status.h"

#include <QString>

#include <vector>

namespace loom {

struct ProjectContents;

/// Which way a status moved (Specs/m2-statements-review.md, "Rules: Status changes").
enum class StatusDirection {
    Raise,   ///< Up the order proved > conditional > unchecked > sketch > heuristic > open, or extracted → cited.
    Drop,    ///< Down one of those orders.
    Neither, ///< To or from false, between no status and a status, or between the two orders.
};

/// One statement or stub whose status differs.
struct StatusChange {
    QString id;    ///< Its tag.
    QString title; ///< Its title (for a stub without one, its gist), from the later version if it is there.
    Status before = Status::None; ///< Status::None if it isn't in the earlier version.
    Status after = Status::None;  ///< Status::None if it isn't in the later version.
    StatusDirection direction = StatusDirection::Neither;
};

/// Whether going from `before` to `after` is a raise, a drop or neither.
/// Rule (Specs/m2-statements-review.md, "Rules: Status changes"): "Raises go up the order proved >
/// conditional > unchecked > sketch > heuristic > open, and cited > extracted; a change to or from
/// false, or between no status and a status, is neither and gets no mark." A change from one of
/// the two orders to the other is neither too, since they don't compare. No side effects.
StatusDirection statusDirection(Status before, Status after);

/// The statements and stubs whose status differs between two graphs.
/// Rule: a statement in only one graph counts as having no status in the other. Sorted as the
/// spec says: "drops first, then raises, then the rest, each by tag".
/// @returns The changes; empty if no status differs. No side effects.
std::vector<StatusChange> statusChanges(const ProjectGraph& before, const ProjectGraph& after);

/// The same, for two versions of a project's contents: builds both graphs (buildGraph()) first.
/// No side effects.
std::vector<StatusChange> statusChanges(const ProjectContents& before, const ProjectContents& after);

} // namespace loom
