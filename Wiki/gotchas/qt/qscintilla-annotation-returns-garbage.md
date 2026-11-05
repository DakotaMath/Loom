---
type: gotcha
tags: [qt, qscintilla, editor, tests]
---

# QScintilla's `annotation(line)` returns text it never read

> **Summary:** In QScintilla 2.14.1, `QsciScintilla::annotation(int line)` allocates a buffer for the line's annotation but never asks Scintilla to fill it, so it returns uninitialised memory (it printed as a row of `?`). Read an annotation with `SCI_ANNOTATIONGETTEXT` yourself.

**Last verified:** 2026-11-05 · commit 7f08250

## Symptom
A test that set *"A2DF · Tame sequence"* under line 0 with `annotate()` read back a string of the same length made of `?` (or other junk), so `annotation(0).contains(…)` failed although the editor showed the title.

## Cause
`qsciscintilla.cpp`:

```cpp
QString QsciScintilla::annotation(int line) const
{
    int size = SendScintilla(SCI_ANNOTATIONGETTEXT, line, (const char *)0);
    char *buf = new char[size + 1];
    QString qs = bytesAsText(buf, size);   // buf was never filled
    ...
```

The second `SCI_ANNOTATIONGETTEXT` call, with the buffer, is missing. Setting annotations (`annotate()`) works.

## Fix
Ask Scintilla twice, as `IdTests`' `annotationOf()` does: once for the size, once to fill a buffer of that size. Use the `void*` overload of `SendScintilla`, since `nullptr` and `char*` arguments are ambiguous between its overloads on MSVC:

```cpp
const auto size = editor->SendScintilla(QsciScintillaBase::SCI_ANNOTATIONGETTEXT, static_cast<unsigned long>(line), static_cast<void*>(nullptr));
QByteArray bytes(static_cast<qsizetype>(size), char(0));
editor->SendScintilla(QsciScintillaBase::SCI_ANNOTATIONGETTEXT, static_cast<unsigned long>(line), static_cast<void*>(bytes.data()));
return QString::fromUtf8(bytes);
```

## See also
- Up: [Gotchas](../README.md)
- [QScintilla's dwell needs a real pointer](qscintilla-dwell-needs-a-real-pointer.md)
- [004: QScintilla](../../decisions/004-qscintilla.md)
