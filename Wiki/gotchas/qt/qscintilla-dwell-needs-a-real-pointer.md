---
type: gotcha
tags: [qt, qscintilla, editor, tests, hover]
---

# QScintilla's hover (dwell) doesn't fire from `QTest::mouseMove`

> **Summary:** The editor's hover card comes from Scintilla's `SCN_DWELLSTART`, which never arrived in a Qt Test after `QTest::mouseMove()` and a 5-second wait. Tests emit `SCN_DWELLSTART` and `SCN_DWELLEND` themselves, with a real position and point, and a real hover is left to the Windows check.

**Last verified:** 2026-11-05 · commit 7f08250

## Symptom
`IdTests::hoverShowsTheCard` moved the mouse onto an ID with `QTest::mouseMove(editor->viewport(), point)` and waited for the card: no `SCN_DWELLSTART` came at all (a trace in the handler never printed), so the card never showed. Hovering by hand in the app works.

## Cause
Not pinned down. Scintilla starts its dwell timer from its own mouse-move handling and fires after `SCI_SETMOUSEDWELLTIME` of stillness; the synthesized move from `QTest::mouseMove` apparently doesn't start it on Windows (the test machine's real pointer stays where it was). Chasing it further wasn't worth it: the timer is Scintilla's, and what Loom adds is the handler.

## Fix
Test the handler, not Scintilla's timer: emit the notification with the byte position and viewport point Scintilla would send.

```cpp
const QPoint at = pointIn(assist, editor, *span);
emit editor->SCN_DWELLSTART(span->position + 1, at.x(), at.y());
QVERIFY(tools->hover()->isVisible());
```

Hovers over other widgets (chips, list rows) are plain `QEvent::ToolTip` events, which a test can send with `QHelpEvent`.

## See also
- Up: [Gotchas](../README.md)
- [QScintilla's `annotation(line)` returns text it never read](qscintilla-annotation-returns-garbage.md)
