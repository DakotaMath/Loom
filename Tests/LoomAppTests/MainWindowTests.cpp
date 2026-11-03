// The main window: for M1, an empty window (issue #3).

#include "LoomApp/MainWindow.h"

#include <QTest>

/// Qt Test cases for loom::MainWindow.
class MainWindowTests : public QObject
{
    Q_OBJECT

private slots:
    /// The window can be shown and becomes visible on screen.
    void opens();
};

void MainWindowTests::opens()
{
    loom::MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
}

QTEST_MAIN(MainWindowTests)
#include "MainWindowTests.moc"
