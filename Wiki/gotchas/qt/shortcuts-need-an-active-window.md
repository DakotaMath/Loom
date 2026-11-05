---
type: gotcha
tags: [qt, qtest, shortcuts, windows, ctest]
---

# Window shortcuts in Qt Tests need the window active, which parallel tests take away

> **Summary:** `QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier)` triggers a menu action's shortcut only while that window is the active window. Under `ctest -j8` on Windows another test program's window can take activation at any moment, so the key goes nowhere and a `QTRY_…` waiting for its effect runs into Qt Test's 300-second watchdog. Make the window active just before each shortcut, and check right after that it started what it should, with a short wait.

**Last verified:** 2026-11-05 · commit 81b7311

## Symptom
A Qt Test passes alone and in small groups, but in a full parallel `ctest` run it sometimes ends with exit code `0xc0000409` and only *"function time: 300015ms"* in the output: Qt Test's watchdog stopped it. Nothing hangs: the test thread sits in `QTest::qWait` inside a `QTRY_VERIFY_WITH_TIMEOUT` whose condition never comes true.

## Cause
Qt's shortcut map delivers a `QAction` shortcut (`Qt::WindowShortcut`) only to the active window. `QTest::keyClick` sends the key event straight to the widget, but whether it becomes a shortcut is decided against `QGuiApplication::focusWindow()`. Several GUI test programs running at once on the real Windows platform take turns being active, so a window made active at the start of a test may not be active a few seconds later. A long build wait after the key (two minutes here) then outlasts the watchdog.

## Fix
- Activate the window right before every shortcut, and fail if that doesn't work (`Tests/LoomAppTests/CompileTests.cpp`, `press()`):

```cpp
bool press(QWidget* window, Qt::Key key, Qt::KeyboardModifiers modifiers = {})
{
    window->activateWindow();
    if (!QTest::qWaitForWindowActive(window))
        return false;
    QTest::keyClick(window, key, modifiers);
    return true;
}
```

- After the key, check what it starts with the default five-second `QTRY_…` (a build's `buildStarted`), and only then wait long for the result, so a lost key fails in seconds instead of tripping the watchdog.
- Keep each long wait well under 300 seconds in total per test function.

## Dead ends
- It looked like a deadlock in Qt PDF (the stall came right after a PDF reload). Loading from a file instead of a `QBuffer`, and single-threaded page rendering, didn't change it. A stack dump of the test thread (dbghelp's `StackWalk64` from a watchdog thread, since no debugger was installed) showed it idle in `QTest::qWait`.

## See also
- Up: [Gotchas](../README.md)
- [QScintilla takes every shortcut its keymap binds](qscintilla-takes-bound-shortcuts.md)
