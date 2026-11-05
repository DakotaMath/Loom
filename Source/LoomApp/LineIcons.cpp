#include "LoomApp/LineIcons.h"

#include <QApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>

#include <cmath>
#include <numbers>

namespace loom {

namespace {

/// Draws one icon on a 24 × 24 grid, scaled to the rectangle asked for.
class LineIconEngine : public QIconEngine
{
public:
    explicit LineIconEngine(LineIcon icon) : m_icon(icon) {}

    QIconEngine* clone() const override { return new LineIconEngine(m_icon); }

    QString key() const override { return QStringLiteral("loom-line-icon"); }

    /// Draws the icon. Side effects: paints on `painter`.
    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State) override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const qreal side = qMin(rect.width(), rect.height());
        painter->translate(rect.x() + (rect.width() - side) / 2.0, rect.y() + (rect.height() - side) / 2.0);
        painter->scale(side / 24.0, side / 24.0);
        QPen pen(colour(mode), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        draw(*painter, pen);
        painter->restore();
    }

    /// A pixmap at the given scale, painted at that resolution rather than scaled up.
    QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        QPixmap pixmap(size * scale);
        pixmap.setDevicePixelRatio(scale);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        return pixmap;
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override { return scaledPixmap(size, mode, state, 1.0); }

private:
    /// The icon's colour: the status colours for Check my setup, else the palette's text colour.
    QColor colour(QIcon::Mode mode) const
    {
        const QPalette palette = QApplication::palette();
        switch (m_icon) {
        case LineIcon::Tick:
            return QColor(46, 160, 67);
        case LineIcon::Cross:
            return QColor(207, 34, 46);
        case LineIcon::Warning:
            return QColor(191, 135, 0);
        case LineIcon::Skipped:
            return palette.color(QPalette::PlaceholderText);
        case LineIcon::Statements:
        case LineIcon::Review:
        case LineIcon::Settings:
            break;
        }
        return palette.color(mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active, QPalette::WindowText);
    }

    /// Draws the icon's lines on the 24 × 24 grid.
    void draw(QPainter& painter, QPen& pen) const
    {
        switch (m_icon) {
        case LineIcon::Statements:
            painter.drawRoundedRect(QRectF(5, 3, 14, 18), 1.5, 1.5);
            painter.drawLine(QPointF(8.5, 8), QPointF(15.5, 8));
            painter.drawLine(QPointF(8.5, 11.5), QPointF(15.5, 11.5));
            painter.drawLine(QPointF(8.5, 15), QPointF(13, 15));
            break;
        case LineIcon::Review:
            painter.drawRoundedRect(QRectF(2.5, 5, 8.5, 14), 1.2, 1.2);
            painter.drawRoundedRect(QRectF(13, 5, 8.5, 14), 1.2, 1.2);
            painter.drawLine(QPointF(4.75, 12), QPointF(8.75, 12));
            painter.drawLine(QPointF(15.25, 12), QPointF(19.25, 12));
            painter.drawLine(QPointF(17.25, 10), QPointF(17.25, 14));
            break;
        case LineIcon::Settings: {
            const QPointF centre(12, 12);
            painter.drawEllipse(centre, 6.2, 6.2);
            painter.drawEllipse(centre, 2.6, 2.6);
            QPen teeth = pen;
            teeth.setWidthF(2.8);
            teeth.setCapStyle(Qt::FlatCap);
            painter.setPen(teeth);
            for (int i = 0; i < 8; ++i) {
                const double angle = i * std::numbers::pi / 4;
                const QPointF direction(std::cos(angle), std::sin(angle));
                painter.drawLine(centre + direction * 6.2, centre + direction * 9.4);
            }
            break;
        }
        case LineIcon::Tick:
            pen.setWidthF(2.4);
            painter.setPen(pen);
            painter.drawPolyline(QPolygonF({QPointF(5, 12.5), QPointF(10, 17.5), QPointF(19, 7)}));
            break;
        case LineIcon::Cross:
            pen.setWidthF(2.4);
            painter.setPen(pen);
            painter.drawLine(QPointF(6.5, 6.5), QPointF(17.5, 17.5));
            painter.drawLine(QPointF(17.5, 6.5), QPointF(6.5, 17.5));
            break;
        case LineIcon::Warning: {
            painter.drawPolygon(QPolygonF({QPointF(12, 3.5), QPointF(21, 19.5), QPointF(3, 19.5)}));
            pen.setWidthF(2);
            painter.setPen(pen);
            painter.drawLine(QPointF(12, 9.5), QPointF(12, 13.5));
            painter.drawPoint(QPointF(12, 16.8));
            break;
        }
        case LineIcon::Skipped:
            pen.setWidthF(2.2);
            painter.setPen(pen);
            painter.drawLine(QPointF(7, 12), QPointF(17, 12));
            break;
        }
    }

    LineIcon m_icon;
};

} // namespace

QIcon lineIcon(LineIcon icon)
{
    return QIcon(new LineIconEngine(icon));
}

} // namespace loom
