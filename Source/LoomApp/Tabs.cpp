#include "LoomApp/Tabs.h"

namespace loom {

const std::vector<TabInfo>& projectTabs()
{
    // The lines are the spec's, without the name it starts them with.
    static const std::vector<TabInfo> tabs = {
        {Tab::Statements, QStringLiteral("statements"), QStringLiteral("Statements"),
         QStringLiteral("Edit statements and proofs, with the compiled PDF beside them")},
        {Tab::Review, QStringLiteral("review"), QStringLiteral("Review"), QStringLiteral("Your uncommitted changes, as compiled math")},
    };
    return tabs;
}

const TabInfo& settingsTab()
{
    static const TabInfo settings = {Tab::Settings, QStringLiteral("settings"), QStringLiteral("Settings"),
                                     QStringLiteral("Theme, fonts, compiling, and Check my setup")};
    return settings;
}

const TabInfo& tabInfo(Tab tab)
{
    for (const TabInfo& info : projectTabs()) {
        if (info.tab == tab)
            return info;
    }
    return settingsTab();
}

QKeySequence tabShortcut(Tab tab)
{
    const std::vector<TabInfo>& tabs = projectTabs();
    for (std::size_t i = 0; i < tabs.size() && i < 9; ++i) {
        if (tabs[i].tab == tab)
            return QKeySequence(QKeyCombination(Qt::ControlModifier, static_cast<Qt::Key>(Qt::Key_1 + static_cast<int>(i))));
    }
    return QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_Comma));
}

bool tabFromKey(const QString& key, Tab& tab)
{
    for (const TabInfo& info : projectTabs()) {
        if (info.key == key) {
            tab = info.tab;
            return true;
        }
    }
    if (key == settingsTab().key) {
        tab = Tab::Settings;
        return true;
    }
    return false;
}

} // namespace loom
