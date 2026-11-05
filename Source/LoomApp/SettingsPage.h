#pragma once

// The Settings page (Specs/m2-statements-review.md, "Settings"): every setting of the spec's table,
// each saved as soon as it changes, per user, never in the project; and *Check my setup*.

#include <QWidget>

namespace loom {

class AppSettings;
class Lane;
class SetupCheckView;

/// The page the gear opens.
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    /// @param settings The settings it shows and changes.
    /// @param lane     The lane *Check my setup* runs on.
    /// @param parent   The Qt parent.
    SettingsPage(AppSettings* settings, Lane* lane, QWidget* parent = nullptr);

    /// The *Check my setup* part.
    SetupCheckView* setupCheck() const { return m_setupCheck; }

private:
    SetupCheckView* m_setupCheck;
};

} // namespace loom
