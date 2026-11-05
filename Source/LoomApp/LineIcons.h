#pragma once

// The app's line icons, drawn with QPainter at whatever size and pixel ratio they're shown, in the
// palette's text colour at the moment they're painted, so they follow the theme. Drawing them
// needs no image files and no Qt SVG module.

#include <QIcon>

namespace loom {

/// Which icon.
enum class LineIcon {
    Statements, ///< A page with lines of text.
    Review,     ///< Two pages side by side: before and after.
    Settings,   ///< A gear.
    Tick,       ///< Check my setup: found (green).
    Cross,      ///< Check my setup: missing (red).
    Warning,    ///< Check my setup: a warning (amber).
    Skipped,    ///< Check my setup: not checked (a dash).
};

/// The icon. No side effects.
QIcon lineIcon(LineIcon icon);

} // namespace loom
