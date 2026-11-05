---
type: gotcha
tags: [qt, qscintilla, shortcuts, editor]
---

# QScintilla takes every shortcut its keymap binds

> **Summary:** While a QScintilla editor has focus, it accepts the `ShortcutOverride` event for any key its command set binds (Ctrl+-, Ctrl++, Ctrl+L, Ctrl+D, Ctrl+T, Ctrl+U, …), so a window action with the same shortcut never fires. Unbind the key from `standardCommands()` if the window's action should win.

**Last verified:** 2026-11-05 · commit 3be5679

## Symptom
A menu action's shortcut (Ctrl+- for *Smaller text*) works everywhere except while the cursor is in the editor, where QScintilla zooms its own view instead.

## Cause
`QsciScintilla::event()` accepts `QEvent::ShortcutOverride` for plain characters and for every key bound in `standardCommands()`, so Qt delivers the key to the editor instead of the shortcut. Its default bindings (`qscicommandset.cpp`) include Ctrl+- and Ctrl++ (zoom), Ctrl+L, Ctrl+Shift+L, Ctrl+T, Ctrl+D, Ctrl+U, Ctrl+Shift+U, Ctrl+/ and the undo keys. Alt+← and Alt+→ are bound only on macOS.

## Fix
Unbind the key so the window's action gets it, as `LatexEditor`'s constructor does:

```cpp
for (const int key : {Qt::Key_Minus | static_cast<int>(Qt::ControlModifier), Qt::Key_Plus | static_cast<int>(Qt::ControlModifier)}) {
    if (QsciCommand* command = standardCommands()->boundTo(key))
        command->setKey(0);
}
```

Before giving a new shortcut to the Statements tab, check it isn't bound in `qscicommandset.cpp`.

## See also
- Up: [Gotchas](../README.md)
- [004: QScintilla](../../decisions/004-qscintilla.md)
