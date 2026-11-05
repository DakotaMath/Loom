#pragma once

// The window's tabs (Specs/m2-statements-review.md, "The window"; E4: "M2 shows only its own tabs
// (Statements, Review) and Settings; each later tab appears with its milestone").

#include <QKeySequence>
#include <QString>

#include <vector>

namespace loom {

/// A page of the window the side bar opens.
enum class Tab { Statements, Review, Settings };

/// What the side bar shows for a tab.
struct TabInfo {
    Tab tab;
    QString key;  ///< Its name in Loom.ini (`statements`): what *the tab last used* stores.
    QString name; ///< `Statements`.
    QString line; ///< The one line its tooltip gives.
};

/// The project's tabs, in side-bar order, which is also Ctrl+1, Ctrl+2, …'s order (positional,
/// as the spec says). Settings isn't among them: it sits at the bottom with its own shortcut.
const std::vector<TabInfo>& projectTabs();

/// The Settings page's entry.
const TabInfo& settingsTab();

/// The entry for a tab. No side effects.
const TabInfo& tabInfo(Tab tab);

/// A tab's shortcut: Ctrl+1, Ctrl+2, … by its place in projectTabs(), and Ctrl+, for Settings
/// (Specs/m2-statements-review.md, "Shortcuts"). No side effects.
QKeySequence tabShortcut(Tab tab);

/// The tab whose key is `key`, if there is one. @returns True and sets `tab` if found.
bool tabFromKey(const QString& key, Tab& tab);

} // namespace loom
