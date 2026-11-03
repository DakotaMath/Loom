#include "LoomApp/MainWindow.h"

namespace loom {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Loom"));
    resize(1200, 800);
}

} // namespace loom
