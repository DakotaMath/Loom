// The main window: for M1, an empty window titled "Loom" (issue #3).

#include "LoomApp/MainWindow.h"

#include <QTest>

/// Qt Test cases for loom::MainWindow.
class MainWindowTests : public QObject
{
    Q_OBJECT

private slots:
    /// The window's title is "Loom".
    void isTitledLoom();
    /// The window can be shown and becomes visible on screen.
    void opens();
};

void MainWindowTests::isTitledLoom()
{
    loom::MainWindow window;
    QCOMPARE(window.windowTitle(), QStringLiteral("Loom"));
}

void MainWindowTests::opens()
{
    loom::MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
}

QTEST_MAIN(MainWindowTests)
#include "MainWindowTests.moc"
