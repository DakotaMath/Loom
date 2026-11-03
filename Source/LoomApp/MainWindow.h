#pragma once

#include <QMainWindow>

namespace loom {

/// Loom's main window. Empty in M1: it exists so the Qt UI, its runtime and the UI tests are
/// wired up before any UI work starts.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /// Creates the window, titled "Loom". Doesn't show it.
    /// @param parent The Qt parent, or nullptr for a top-level window.
    explicit MainWindow(QWidget* parent = nullptr);
};

} // namespace loom
