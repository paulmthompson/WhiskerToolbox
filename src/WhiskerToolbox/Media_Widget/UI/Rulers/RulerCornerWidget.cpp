#include "RulerCornerWidget.hpp"

#include <QPainter>

RulerCornerWidget::RulerCornerWidget(QWidget * parent)
    : QWidget(parent) {
    setFixedSize(kCornerSize, kCornerSize);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize RulerCornerWidget::sizeHint() const {
    return QSize(kCornerSize, kCornerSize);
}

void RulerCornerWidget::paintEvent(QPaintEvent * /* event */) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(30, 30, 30));

    painter.setPen(QColor(150, 150, 150));
    painter.drawLine(0, height() - 1, width(), height() - 1);
    painter.drawLine(width() - 1, 0, width() - 1, height());

    QFont font = painter.font();
    font.setPointSize(7);
    painter.setFont(font);
    painter.setPen(QColor(120, 120, 120));
    painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("px"));
}
