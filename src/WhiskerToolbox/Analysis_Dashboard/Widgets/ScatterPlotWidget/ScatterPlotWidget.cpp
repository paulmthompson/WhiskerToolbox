#include "ScatterPlotWidget.hpp"

#include <QBrush>
#include <QPainter>
#include <QPen>
#include <QRandomGenerator>
#include <QRectF>
#include <QStyleOptionGraphicsItem>

ScatterPlotWidget::ScatterPlotWidget(QGraphicsItem * parent)
    : AbstractPlotWidget(parent) {
    setPlotTitle("Scatter Plot");
    generateSampleData();
}

QString ScatterPlotWidget::getPlotType() const {
    return "Scatter Plot";
}

void ScatterPlotWidget::paint(QPainter * painter, QStyleOptionGraphicsItem const * option, QWidget * widget) {
    Q_UNUSED(option)
    Q_UNUSED(widget)

    painter->setRenderHint(QPainter::Antialiasing);

    QRectF rect = boundingRect();

    // Draw background
    painter->fillRect(rect, QBrush(QColor(240, 240, 240)));

    // Draw border (highlight if selected)
    QPen border_pen;
    if (isSelected()) {
        border_pen.setColor(QColor(0, 120, 200));
        border_pen.setWidth(2);
    } else {
        border_pen.setColor(QColor(100, 100, 100));
        border_pen.setWidth(1);
    }
    painter->setPen(border_pen);
    painter->drawRect(rect);

    // Draw title
    painter->setPen(QColor(0, 0, 0));
    QFont title_font = painter->font();
    title_font.setBold(true);
    painter->setFont(title_font);

    QRectF title_rect = rect.adjusted(5, 5, -5, -rect.height() + 20);
    painter->drawText(title_rect, Qt::AlignCenter, _plot_title);

    // Draw plot area
    QRectF plot_rect = rect.adjusted(20, 25, -10, -10);
    painter->fillRect(plot_rect, QBrush(QColor(255, 255, 255)));
    painter->setPen(QColor(200, 200, 200));
    painter->drawRect(plot_rect);

    // Draw scatter points
    painter->setPen(QPen(QColor(50, 100, 200), 1));
    painter->setBrush(QBrush(QColor(50, 100, 200, 150)));

    for (QPointF const & point: _sample_points) {
        // Map point to plot area coordinates
        qreal x = plot_rect.left() + (point.x() * plot_rect.width());
        qreal y = plot_rect.bottom() - (point.y() * plot_rect.height());

        painter->drawEllipse(QPointF(x, y), 3, 3);
    }
}

void ScatterPlotWidget::generateSampleData() {
    _sample_points.clear();

    // Generate some random sample data
    QRandomGenerator * rng = QRandomGenerator::global();
    for (int i = 0; i < 50; ++i) {
        qreal x = rng->bounded(1000) / 1000.0;// 0.0 to 1.0
        qreal y = rng->bounded(1000) / 1000.0;// 0.0 to 1.0
        _sample_points.append(QPointF(x, y));
    }
}