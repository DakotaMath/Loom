#pragma once

// The side bar of tabs (Specs/m2-statements-review.md, "The window": "a narrow vertical bar on the
// far left, one line icon per tab … Settings (a gear) at the bottom … The open tab is
// highlighted"; "Badges").

#include "LoomApp/Tabs.h"

#include <QToolButton>
#include <QWidget>

#include <map>

namespace loom {

/// One icon of the side bar, with its badge: a number, or a dot.
class SideBarButton : public QToolButton
{
    Q_OBJECT

public:
    explicit SideBarButton(QWidget* parent = nullptr);

    /// The number on the badge; 0 hides it.
    int badgeCount() const { return m_count; }
    void setBadgeCount(int count);
    /// Whether the badge is a dot (when there's no number).
    bool badgeDot() const { return m_dot; }
    void setBadgeDot(bool dot);

protected:
    /// Draws the button, then its badge in the top-right corner.
    void paintEvent(QPaintEvent* event) override;

private:
    int m_count = 0;
    bool m_dot = false;
};

/// The bar: the project's tabs at the top, the gear at the bottom.
class SideBar : public QWidget
{
    Q_OBJECT

public:
    /// The bar's width in pixels, before the screen's scaling.
    static constexpr int barWidth = 48;

    explicit SideBar(QWidget* parent = nullptr);

    /// Shows or hides the project's tabs (there are none without a project; the gear stays).
    void setProjectTabsVisible(bool visible);
    /// Highlights a tab's icon, or none (the start screen).
    void setCurrent(Tab tab);
    void clearCurrent();
    /// A tab's button.
    SideBarButton* button(Tab tab) const;

    /// The tooltip for a tab: its name, its shortcut and its line, as rich text.
    static QString toolTipFor(const TabInfo& info, const QString& shortcut);

signals:
    /// A tab's icon was clicked.
    void tabClicked(loom::Tab tab);

private:
    std::map<Tab, SideBarButton*> m_buttons;
};

} // namespace loom
