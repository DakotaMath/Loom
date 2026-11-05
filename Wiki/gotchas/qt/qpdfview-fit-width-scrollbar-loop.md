---
type: gotcha
tags: [qt, pdf, layout, hang]
---

# QPdfView at Fit width can lay out for ever

> **Summary:** With `ZoomMode::FitToWidth` and the default vertical scroll bar (as needed), some window sizes make `QPdfView` (Qt 6.11) add and remove its scroll bar endlessly: the window stops responding at 100% of one core. Keep the vertical scroll bar always on (`Qt::ScrollBarAlwaysOn`), as `LoomPdfView` does.

**Last verified:** 2026-11-05 · commit 7f08250 (the bug), fixed on `m2/id-picker`

## Symptom
Loom hung on start for Dakota ("not responding"; it had to be ended in Task Manager), every time, on one project, with his saved window size. A test window of another size never hung. The main thread was busy inside Qt (no Loom function on its stack), and `QPdfView` showed in the samples.

## Cause
At Fit width the page's height depends on the viewport's width. When the page is a little taller than the viewport, the scroll bar appears; the viewport narrows by the bar's width, the page shrinks, now fits, and the bar goes; the viewport widens, the page grows, and the bar comes back. Each step posts another layout, so the event loop never idles. It only happens in the band of sizes where the page fits with the bar and not without it, which is why one saved window size reproduced it and others didn't.

## Fix
```cpp
setZoomMode(ZoomMode::FitToWidth);
setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
```

The viewport's width then never depends on the page. `CompileTests::fitWidthSettlesAtEverySize` sweeps window sizes and checks the width settles; without the fix it fails (the width flips between 548 and 536 px).

## Dead ends
- Debugging the hang with a debugger: none is installed here, and Qt from aqt has no symbols. A small sampler (`OpenProcess`, `SuspendThread`, `StackWalk64` from dbghelp) on the hung `LoomApp.exe` was enough to see the loop was in Qt, not Loom; then removing the saved window geometry, and separately the Fit width zoom, each stopped the hang.

## See also
- Up: [Gotchas](../README.md)
- [005: Qt PDF](../../decisions/005-qt-pdf.md)
