#pragma once

// Status marks: a coloured disc per status, with the word on hover (Specs/m2-statements-review.md,
// "The list": "colour is never the only signal"), and the kinds' short names in the list.

#include "LoomCore/Status.h"

#include <QColor>
#include <QIcon>
#include <QString>

namespace loom {

/// The disc's colour for a status (the spec's table), for a light or a dark background. Each has
/// at least 3:1 contrast against the list's background in its theme (StatusMarksTests checks).
/// @returns The colour; invalid for Status::None, which has no disc. No side effects.
QColor statusColor(Status status, bool dark);

/// Whether the status's disc is hollow (a ring): open and extracted.
bool statusHollow(Status status);

/// The disc as an icon of `size` pixels: filled or hollow, and for false struck through. A null
/// icon for Status::None. No side effects.
QIcon statusIcon(Status status, bool dark, int size = 12);

/// The status word for the hover (`proved`, …), or `no status`. No side effects.
QString statusWord(Status status);

/// The kind in small letters as the list shows it: `thm`, `lem`, `prop`, `cor`, `conj`, `def`,
/// `q`, `rem`, `ex`; the kind itself if it isn't one of them. No side effects.
QString kindShort(const QString& kind);

/// The kind with a capital, for headers: `Theorem`. No side effects.
QString kindTitle(const QString& kind);

/// Whether the application is drawn dark (its palette's window colour is dark).
bool darkTheme();

/// The contrast ratio of two colours (WCAG 2), 1 to 21. No side effects.
double contrastRatio(const QColor& a, const QColor& b);

} // namespace loom
