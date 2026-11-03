#include "HistoryChart.h"
#include "MapView.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

HistoryChart::HistoryChart(const MapView* view, QWidget* parent) : QWidget(parent), m_view(view)
{
    setMinimumHeight(112);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void HistoryChart::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QFont font = p.font(); font.setPixelSize(10); p.setFont(font);
    const QRectF area = QRectF(rect()).adjusted(38, 10, -14, -22);
    const auto& history = m_view->history();
    const double end = history.empty() ? 0 : history.back().stats.elapsedSeconds;
    const double begin = std::max(0.0, end - 60);
    const double span = std::max(5.0, end - begin);
    double peak = 5;
    for (const auto& s : history)
        if (s.stats.elapsedSeconds >= begin) peak = std::max(peak, double(s.colors));
    peak = std::ceil(peak / 5) * 5 + 5;
    for (int i = 0; i < 3; ++i) {
        const double y = area.bottom() - i * area.height() / 2;
        p.setPen(QPen(QColor("#e9eff2"), 1, Qt::DashLine));
        p.drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
        p.setPen(QColor("#91a0ab"));
        p.drawText(QRectF(0, y - 7, 28, 16), Qt::AlignRight, QString::number(int(peak * i / 2)));
    }
    p.setPen(QColor("#91a0ab"));
    p.drawText(QRectF(area.left(), area.bottom() + 5, 75, 15), Qt::AlignLeft, QString::number(begin, 'f', 0) + " s");
    p.drawText(QRectF(area.right() - 80, area.bottom() + 5, 80, 15), Qt::AlignRight, QString::number(begin + span, 'f', 0) + " s");
    QPainterPath curve;
    bool started = false;
    for (const auto& s : history) {
        if (s.stats.elapsedSeconds < begin) continue;
        const QPointF point(area.left() + (s.stats.elapsedSeconds - begin) / span * area.width(),
                            area.bottom() - s.colors / peak * area.height());
        if (!started) { curve.moveTo(point); started = true; } else curve.lineTo(point);
    }
    if (started && history.size() > 1) {
        QPainterPath fill = curve;
        fill.lineTo(curve.currentPosition().x(), area.bottom());
        fill.lineTo(area.left(), area.bottom()); fill.closeSubpath();
        QLinearGradient gradient(area.topLeft(), area.bottomLeft());
        gradient.setColorAt(0, QColor(13, 148, 126, 55)); gradient.setColorAt(1, QColor(13, 148, 126, 2));
        p.fillPath(fill, gradient);
        p.setPen(QPen(QColor("#0d947e"), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPath(curve);
        p.setBrush(QColor("#0d947e")); p.setPen(QPen(Qt::white, 2));
        p.drawEllipse(curve.currentPosition(), 3.5, 3.5);
    } else {
        p.setPen(QColor("#899ca7"));
        p.drawText(area, Qt::AlignCenter, "Démarrez la simulation pour tracer l’évolution");
    }
}
