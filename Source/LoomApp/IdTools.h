#pragma once

// What every place that shows or takes an ID shares (Specs/m2-statements-review.md, "IDs in the
// editor"): the latest snapshot's ID search, the previews, the picker, the hover card, and opening
// an ID. One per Statements tab.

#include "LoomApp/ProjectSnapshot.h"

#include "LoomCore/IdSearch.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

class QWidget;

namespace loom {

class AppSettings;
class IdHover;
class IdPicker;
class IdPreviews;

/// The name of the dynamic property that marks a widget as showing an ID: hovering such a widget
/// shows the ID's card (IdTools::eventFilter()).
inline constexpr char idProperty[] = "loomId";
/// On an item view, the item data role that holds each row's ID: hovering a row shows its card.
inline constexpr char idRoleProperty[] = "loomIdRole";

/// The ID search, previews, picker and hover card of one Statements tab.
class IdTools : public QObject
{
    Q_OBJECT

public:
    /// @param settings Whether titles are shown beside IDs, and how previews are built.
    /// @param previews The tab's previews.
    /// @param owner    The tab: hovers are shown for its widgets only, and the popups are its children.
    IdTools(AppSettings* settings, IdPreviews* previews, QWidget* owner);
    ~IdTools() override;

    /// Takes a new snapshot: every editor's IDs and titles are read again (snapshotChanged()).
    void setSnapshot(std::shared_ptr<const ProjectSnapshot> snapshot);
    const std::shared_ptr<const ProjectSnapshot>& snapshot() const { return m_snapshot; }
    /// The latest snapshot's ID search, or null.
    const IdIndex* index() const;
    IdPreviews* previews() const { return m_previews; }
    AppSettings* settings() const { return m_settings; }

    /// Where the most recently opened IDs come from (most recent first), for ranking ties.
    void setRecent(std::function<QStringList()> recent) { m_recent = std::move(recent); }
    QStringList recent() const { return m_recent ? m_recent() : QStringList(); }

    /// Opens the picker (closing one that's open).
    /// @param field  What it offers. @param self The statement being edited, or empty.
    /// @param query  The text it starts with. @param global Where it opens.
    /// @param chosen Called with the ID chosen.
    /// @param over   A widget of the window it opens over, when that isn't the tab's (a modal form,
    ///               which would otherwise block it); or null.
    /// @returns The picker.
    IdPicker* pick(IdField field, const QString& self, const QString& query, const QPoint& global,
                   std::function<void(const QString&)> chosen, QWidget* over = nullptr);
    /// The picker if it is open, else null.
    IdPicker* openPicker() const;

    /// Shows an ID's card at `global`, until the mouse leaves `anchor`, a key or button is pressed,
    /// or hideHover().
    void showHover(const QString& id, QWidget* anchor, const QPoint& global);
    void hideHover();
    /// The hover card, if it has been shown; for tests.
    IdHover* hover() const;

    /// Asks the tab to open an ID (Ctrl+click, a chip, the card).
    void requestOpen(const QString& id) { emit openRequested(id); }

signals:
    /// The tab should open this ID: a statement, a proof, or a stub read-only.
    void openRequested(const QString& id);
    /// A new snapshot, or the *Show titles beside IDs* setting changed.
    void snapshotChanged();

protected:
    /// Hovers over widgets marked with idProperty (chips) and rows of views marked with
    /// idRoleProperty (the list), in the owner's window; hides the card when the mouse leaves.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    AppSettings* m_settings;
    IdPreviews* m_previews;
    QWidget* m_owner;
    std::shared_ptr<const ProjectSnapshot> m_snapshot;
    std::function<QStringList()> m_recent;
    QPointer<IdPicker> m_picker;
    QPointer<IdHover> m_hover;
    QPointer<QWidget> m_hoverAnchor;
};

} // namespace loom
