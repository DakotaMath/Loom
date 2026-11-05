#include "LoomApp/IdTools.h"

#include "LoomApp/AppSettings.h"
#include "LoomApp/IdPicker.h"
#include "LoomApp/IdPreviews.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QWidget>

namespace loom {

namespace {

/// The ID a hover over `widget` at `pos` (in its coordinates) is about: the widget's own idProperty,
/// or, on an item view's viewport, the hovered row's ID in the view's idRoleProperty role.
QString idUnder(QWidget* widget, const QPoint& pos)
{
    if (!widget)
        return {};
    if (const QVariant id = widget->property(idProperty); id.isValid())
        return id.toString();
    if (auto* view = qobject_cast<QAbstractItemView*>(widget->parentWidget()); view && view->viewport() == widget) {
        if (const QVariant role = view->property(idRoleProperty); role.isValid()) {
            const QModelIndex index = view->indexAt(pos);
            if (index.isValid())
                return index.siblingAtColumn(0).data(role.toInt()).toString();
        }
    }
    return {};
}

} // namespace

IdTools::IdTools(AppSettings* settings, IdPreviews* previews, QWidget* owner)
    : QObject(owner)
    , m_settings(settings)
    , m_previews(previews)
    , m_owner(owner)
{
    qApp->installEventFilter(this);
    if (m_settings) {
        connect(m_settings, &AppSettings::changed, this, &IdTools::snapshotChanged);
    }
}

IdTools::~IdTools()
{
    qApp->removeEventFilter(this);
}

void IdTools::setSnapshot(std::shared_ptr<const ProjectSnapshot> snapshot)
{
    const bool otherProject = !m_snapshot || !snapshot || m_snapshot->root != snapshot->root;
    m_snapshot = std::move(snapshot);
    if (m_previews) {
        if (otherProject)
            m_previews->setProject(m_snapshot ? m_snapshot->root : QString());
        else
            m_previews->invalidate();
    }
    emit snapshotChanged();
}

const IdIndex* IdTools::index() const
{
    return m_snapshot ? m_snapshot->ids.get() : nullptr;
}

IdPicker* IdTools::pick(IdField field, const QString& self, const QString& query, const QPoint& global,
                        std::function<void(const QString&)> chosen, QWidget* over)
{
    hideHover();
    QWidget* parent = over && over->window() != m_owner->window() ? over->window() : m_owner;
    if (m_picker && m_picker->parentWidget() != parent) {
        m_picker->close();
        m_picker->deleteLater();
        m_picker = nullptr;
    }
    if (!m_picker)
        m_picker = new IdPicker(this, parent);
    m_picker->open(field, self, query, global, std::move(chosen));
    return m_picker;
}

IdPicker* IdTools::openPicker() const
{
    return m_picker && m_picker->isVisible() ? m_picker.data() : nullptr;
}

void IdTools::showHover(const QString& id, QWidget* anchor, const QPoint& global)
{
    if (id.isEmpty() || openPicker())
        return;
    if (!m_hover)
        m_hover = new IdHover(this, m_owner);
    m_hoverAnchor = anchor;
    m_hover->showAt(id, global);
}

IdHover* IdTools::hover() const
{
    return m_hover;
}

void IdTools::hideHover()
{
    if (m_hover)
        m_hover->hide();
    m_hoverAnchor = nullptr;
}

bool IdTools::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::ToolTip: {
        auto* widget = qobject_cast<QWidget*>(watched);
        // Only the tab's own widgets (and its dialogs, which are its children).
        bool ours = false;
        for (QWidget* w = widget; w && !ours; w = w->parentWidget())
            ours = w == m_owner;
        if (!ours)
            break;
        auto* help = static_cast<QHelpEvent*>(event);
        const QString id = idUnder(widget, help->pos());
        if (id.isEmpty())
            break;
        showHover(id, widget, help->globalPos() + QPoint(12, 16));
        return true; // Instead of the plain tool tip.
    }
    case QEvent::MouseMove:
        // A row's card goes once the mouse is over another row; the next rest shows that one's.
        // (An editor's card goes when Scintilla says the mouse moved on: SCN_DWELLEND.)
        if (m_hoverAnchor && watched == m_hoverAnchor && m_hover && m_hover->isVisible()
            && qobject_cast<QAbstractItemView*>(m_hoverAnchor->parentWidget())) {
            const QString id = idUnder(m_hoverAnchor, static_cast<QMouseEvent*>(event)->position().toPoint());
            if (id != m_hover->card()->id())
                hideHover();
        }
        break;
    case QEvent::Leave:
    case QEvent::Hide:
        if (m_hoverAnchor && watched == m_hoverAnchor)
            hideHover();
        break;
    case QEvent::MouseButtonPress:
    case QEvent::KeyPress:
    case QEvent::Wheel:
    case QEvent::WindowDeactivate:
        if (m_hover && m_hover->isVisible())
            hideHover();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

} // namespace loom
