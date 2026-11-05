#include "LoomApp/StatusMarks.h"

#include <QGuiApplication>
#include <QHash>
#include <QPainter>
#include <QPalette>
#include <QPixmap>

#include <algorithm>
#include <cmath>

namespace loom {

QColor statusColor(Status status, bool dark)
{
    // The spec's colours (Specs/m2-statements-review.md, "The list"), darkened for a light
    // background and lightened for a dark one until each has 3:1 contrast with it.
    switch (status) {
    case Status::Proved: return QColor(dark ? 0x66bb6a : 0x2e7d32);      // green
    case Status::Conditional: return QColor(dark ? 0x4db6ac : 0x00796b); // teal
    case Status::Unchecked: return QColor(dark ? 0xffca28 : 0xb26a00);   // amber
    case Status::Sketch: return QColor(dark ? 0xffa726 : 0xd84315);      // orange
    case Status::Heuristic: return QColor(dark ? 0xce93d8 : 0x8e5cc4);   // light purple
    case Status::Open: return QColor(dark ? 0x9e9e9e : 0x6e6e6e);        // grey, hollow
    case Status::Cited: return QColor(dark ? 0x64b5f6 : 0x1565c0);       // blue
    case Status::Extracted: return QColor(dark ? 0x81d4fa : 0x0277bd);   // light blue, hollow
    case Status::False: return QColor(dark ? 0xef5350 : 0xc62828);       // red, struck through
    case Status::None: break;
    }
    return {};
}

bool statusHollow(Status status)
{
    return status == Status::Open || status == Status::Extracted;
}

QIcon statusIcon(Status status, bool dark, int size)
{
    if (status == Status::None)
        return {};
    const qreal ratio = qGuiApp ? qGuiApp->devicePixelRatio() : 1.0;
    QPixmap pixmap(QSize(size, size) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor color = statusColor(status, dark);
    const qreal pen = std::max(1.5, size / 6.0);
    const QRectF disc = QRectF(0, 0, size, size).adjusted(pen / 2 + 0.5, pen / 2 + 0.5, -pen / 2 - 0.5, -pen / 2 - 0.5);
    if (statusHollow(status)) {
        painter.setPen(QPen(color, pen));
        painter.setBrush(Qt::NoBrush);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
    }
    painter.drawEllipse(disc);
    if (status == Status::False) {
        // Struck through, in the background's colour, so false never relies on red alone.
        painter.setPen(QPen(dark ? QColor(0x1e1e1e) : QColor(Qt::white), pen));
        painter.drawLine(QPointF(disc.left() + 1, disc.bottom() - 1), QPointF(disc.right() - 1, disc.top() + 1));
    }
    return QIcon(pixmap);
}

QString statusWord(Status status)
{
    const QString name = statusName(status);
    return name.isEmpty() ? QStringLiteral("no status") : name;
}

QString kindShort(const QString& kind)
{
    static const QHash<QString, QString> shorts = {
        {QStringLiteral("theorem"), QStringLiteral("thm")},     {QStringLiteral("lemma"), QStringLiteral("lem")},
        {QStringLiteral("proposition"), QStringLiteral("prop")}, {QStringLiteral("corollary"), QStringLiteral("cor")},
        {QStringLiteral("conjecture"), QStringLiteral("conj")},  {QStringLiteral("definition"), QStringLiteral("def")},
        {QStringLiteral("question"), QStringLiteral("q")},       {QStringLiteral("remark"), QStringLiteral("rem")},
        {QStringLiteral("example"), QStringLiteral("ex")},
    };
    return shorts.value(kind, kind);
}

QString kindTitle(const QString& kind)
{
    return kind.isEmpty() ? kind : kind.front().toUpper() + kind.mid(1);
}

bool darkTheme()
{
    return QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
}

double contrastRatio(const QColor& a, const QColor& b)
{
    const auto luminance = [](const QColor& c) {
        const auto channel = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
        return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF()) + 0.0722 * channel(c.blueF());
    };
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

} // namespace loom
