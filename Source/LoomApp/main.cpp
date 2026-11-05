// LoomApp: Loom's desktop program.

#include "LoomApp/AppSettings.h"
#include "LoomApp/MainWindow.h"
#include "LoomCore/Version.h"

#include <QApplication>
#include <QString>

#include <string_view>

/// Starts Loom: applies the user's settings, opens a window (reopening the last project if the
/// settings say so) and runs Qt's event loop until the last window closes.
/// @param argc, argv The command line, passed to Qt.
/// @returns Qt's exit code.
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("Loom"));
    QApplication::setApplicationName(QStringLiteral("Loom"));
    const std::string_view version = loom::versionString();
    QApplication::setApplicationVersion(QString::fromUtf8(version.data(), static_cast<qsizetype>(version.size())));

    loom::AppSettings settings(loom::AppPaths::standard());
    settings.applyAppearance();

    auto* window = new loom::MainWindow(&settings);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
    window->restoreLastProject();
    return app.exec();
}
