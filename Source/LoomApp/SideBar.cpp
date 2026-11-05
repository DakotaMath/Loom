#include "LoomApp/SideBar.h"

#include "LoomApp/LineIcons.h"

#include <QButtonGroup>
#include <QPainter>
#include <QStyleOptionToolButton>
#include <QVBoxLayout>

namespace loom {

namespace {

/// The icon for a tab.
LineIcon iconFor(Tab tab)
{
    switch (tab) {
    case Tab::Statements:
        return LineIcon::Statements;
    case Tab::Review:
        return LineIcon::Review;
    case Tab::Settings:
        break;
    }
    return LineIcon::Settings;
}

} // namespace

SideBarButton::SideBarButton(QWidget* parent)
    : QToolButton(parent)
{
    setCheckable(true);
    setAutoRaise(true);
    setIconSize(QSize(24, 24));
    setFixedSize(SideBar::barWidth, SideBar::barWidth);
    setFocusPolicy(Qt::TabFocus);
}

void SideBarButton::setBadgeCount(int count)
{
    if (count == m_count)
        return;
    m_count = qMax(0, count);
    update();
}

void SideBarButton::setBadgeDot(bool dot)
{
    if (dot == m_dot)
        return;
    m_dot = dot;
    update();
}

void SideBarButton::paintEvent(QPaintEvent* event)
{
    QToolButton::paintEvent(event);
    if (m_count == 0 && !m_dot)
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette().color(QPalette::Highlight));
    if (m_count > 0) {
        const QString text = m_count > 99 ? QStringLiteral("99+") : QString::number(m_count);
        QFont font = painter.font();
        font.setPixelSize(10);
        font.setBold(true);
        painter.setFont(font);
        const int textWidth = painter.fontMetrics().horizontalAdvance(text);
        const QRectF badge(width() - 8 - qMax(16, textWidth + 8), 6, qMax(16, textWidth + 8), 16);
        painter.drawRoundedRect(badge, 8, 8);
        painter.setPen(palette().color(QPalette::HighlightedText));
        painter.drawText(badge, Qt::AlignCenter, text);
    } else {
        painter.drawEllipse(QPointF(width() - 13, 13), 4, 4);
    }
}

SideBar::SideBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("sideBar"));
    setAttribute(Qt::WA_StyledBackground);
    setFixedWidth(barWidth);
    // The open tab: an accent line on its left edge and a shaded background.
    setStyleSheet(QStringLiteral(
        "#sideBar { background: palette(alternate-base); }"
        "loom--SideBarButton { border: none; border-left: 3px solid transparent; background: transparent; }"
        "loom--SideBarButton:hover { background: palette(midlight); }"
        "loom--SideBarButton:checked { border-left: 3px solid palette(highlight); background: palette(base); }"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);

    const auto addButton = [&](const TabInfo& info) {
        auto* button = new SideBarButton(this);
        button->setObjectName(info.key + QStringLiteral("Button"));
        button->setIcon(lineIcon(iconFor(info.tab)));
        button->setAccessibleName(info.name);
        button->setToolTip(toolTipFor(info, tabShortcut(info.tab).toString(QKeySequence::NativeText)));
        group->addButton(button);
        m_buttons[info.tab] = button;
        connect(button, &QToolButton::clicked, this, [this, tab = info.tab] { emit tabClicked(tab); });
        layout->addWidget(button);
    };
    for (const TabInfo& info : projectTabs())
        addButton(info);
    layout->addStretch(1);
    addButton(settingsTab());
}

QString SideBar::toolTipFor(const TabInfo& info, const QString& shortcut)
{
    return QStringLiteral("<b>%1</b>&nbsp;&nbsp;%2<br>%3").arg(info.name.toHtmlEscaped(), shortcut.toHtmlEscaped(), info.line.toHtmlEscaped());
}

void SideBar::setProjectTabsVisible(bool visible)
{
    for (const TabInfo& info : projectTabs())
        m_buttons.at(info.tab)->setVisible(visible);
}

void SideBar::setCurrent(Tab tab)
{
    m_buttons.at(tab)->setChecked(true);
}

void SideBar::clearCurrent()
{
    // An exclusive group never unchecks its last button, so drop exclusivity for a moment.
    QButtonGroup* group = m_buttons.begin()->second->group();
    group->setExclusive(false);
    for (auto& [tab, button] : m_buttons)
        button->setChecked(false);
    group->setExclusive(true);
}

SideBarButton* SideBar::button(Tab tab) const
{
    return m_buttons.at(tab);
}

} // namespace loom
