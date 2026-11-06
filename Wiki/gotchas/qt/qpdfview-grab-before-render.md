---
type: gotcha
tags: [qt, pdf, tests, screenshots]
---

# QPdfView draws blank pages in a grab taken as soon as the PDF is ready

> **Summary:** `QPdfView` (Qt 6.11) renders its pages on a worker thread after the document's status becomes `Ready`, so `QWidget::grab()` right then shows white pages with no text. A test that saves a screenshot must give the view a moment first (`QTest::qWait(800)`); waiting for `Ready` isn't enough.

**Last verified:** 2026-11-05 · branch `m2/review`

## Symptom
The Review tab's screenshot (`review-before-after.png`) showed both PDF views as white pages, though the test had just checked that both documents were `QPdfDocument::Status::Ready`. In the app the pages appeared a moment later.

## Cause
`QPdfView` asks a page renderer for each visible page once the document is loaded, and the renderer works in the background. The status says the file has been read, not that any page has been drawn. `grab()` paints the widget as it is now, without its pages.

## Fix
Wait a little before grabbing, as `ReviewTests`' `screenshot()` does:

```cpp
QTest::qWait(800); // QPdfView renders its pages in the background
widget->grab().save(path);
```

Screenshots are never compared pixel by pixel (Specs/m2-statements-review.md, "Testing the UI"), so a fixed wait is fine. Assertions shouldn't depend on rendered pixels at all; check the document's status and page count instead.

## Dead ends
- `QTRY_COMPARE(document->status(), QPdfDocument::Status::Ready)` before the grab: the status is already `Ready` while the pages are still blank.

## See also
- Up: [Gotchas](../README.md)
- [QPdfView at Fit width can lay out for ever](qpdfview-fit-width-scrollbar-loop.md)
