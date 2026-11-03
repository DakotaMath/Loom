// LoomApp: Loom's desktop program.

#include "LoomApp/MainWindow.h"
#include "LoomCore/Version.h"

#include <QApplication>
#include <QString>

#include <string_view>

/// Starts Loom: opens the main window and runs Qt's event loop until it closes.
/// @param argc, argv The command line, passed to Qt.
/// @returns Qt's exit code.
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Loom"));
    const std::string_view version = loom::versionString();
    QApplication::setApplicationVersion(QString::fromUtf8(version.data(), static_cast<qsizetype>(version.size())));

    loom::MainWindow window;
    window.show();
    return app.exec();
}
