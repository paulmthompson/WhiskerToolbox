#ifndef MEDIA_TOOL_ICONS_HPP
#define MEDIA_TOOL_ICONS_HPP

/**
 * @file MediaToolIcons.hpp
 * @brief Programmatic icons for Media Viewer toolbar buttons
 */

#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace MediaToolIcons {

/**
 * @brief Create a pointer/selection cursor icon
 * @param size Icon size in pixels
 * @return Icon suitable for a toolbar button
 */
[[nodiscard]] inline QIcon createSelectToolIcon(int size = 20) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    double const scale = static_cast<double>(size) / 20.0;
    painter.scale(scale, scale);

    QPainterPath cursor;
    cursor.moveTo(2.0, 1.0);
    cursor.lineTo(2.0, 16.0);
    cursor.lineTo(6.0, 12.0);
    cursor.lineTo(9.0, 18.0);
    cursor.lineTo(11.0, 17.0);
    cursor.lineTo(8.0, 11.0);
    cursor.lineTo(13.0, 11.0);
    cursor.closeSubpath();

    painter.setPen(QPen(QColor(20, 20, 20), 1.2));
    painter.setBrush(QColor(230, 230, 230));
    painter.drawPath(cursor);

    return QIcon(pixmap);
}

/**
 * @brief Create a pen nib icon for selected-line editing
 * @param size Icon size in pixels
 * @return Icon suitable for a toolbar button
 */
[[nodiscard]] inline QIcon createPenToolIcon(int size = 20) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    double const scale = static_cast<double>(size) / 20.0;
    painter.scale(scale, scale);

    QPainterPath nib;
    nib.moveTo(14.0, 2.0);
    nib.lineTo(17.0, 5.0);
    nib.lineTo(6.0, 16.0);
    nib.lineTo(3.0, 17.0);
    nib.lineTo(4.0, 14.0);
    nib.closeSubpath();

    painter.setPen(QPen(QColor(20, 20, 20), 1.0));
    painter.setBrush(QColor(220, 220, 220));
    painter.drawPath(nib);

    painter.setPen(QPen(QColor(180, 180, 180), 1.0));
    painter.drawLine(QPointF(6.0, 16.0), QPointF(14.0, 8.0));

    return QIcon(pixmap);
}

/**
 * @brief Create an eraser block icon
 * @param size Icon size in pixels
 * @return Icon suitable for a toolbar button
 */
[[nodiscard]] inline QIcon createEraserToolIcon(int size = 20) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    double const scale = static_cast<double>(size) / 20.0;
    painter.scale(scale, scale);

    QPainterPath body;
    body.moveTo(4.0, 14.0);
    body.lineTo(8.0, 17.0);
    body.lineTo(16.0, 9.0);
    body.lineTo(12.0, 6.0);
    body.closeSubpath();

    painter.setPen(QPen(QColor(20, 20, 20), 1.0));
    painter.setBrush(QColor(235, 180, 190));
    painter.drawPath(body);

    painter.setPen(QPen(QColor(170, 120, 130), 1.0));
    painter.drawLine(QPointF(6.0, 15.0), QPointF(14.0, 7.0));

    return QIcon(pixmap);
}

/**
 * @brief Create a smooth-curve icon for local line smoothing
 * @param size Icon size in pixels
 * @return Icon suitable for a toolbar button
 */
[[nodiscard]] inline QIcon createSmoothToolIcon(int size = 20) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    double const scale = static_cast<double>(size) / 20.0;
    painter.scale(scale, scale);

    QPainterPath curve;
    curve.moveTo(3.0, 14.0);
    curve.cubicTo(6.0, 4.0, 10.0, 16.0, 17.0, 6.0);

    painter.setPen(QPen(QColor(90, 170, 230), 2.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawPath(curve);

    painter.setPen(QPen(QColor(20, 20, 20), 1.0));
    painter.setBrush(QColor(200, 220, 240));
    painter.drawEllipse(QPointF(3.0, 14.0), 1.8, 1.8);
    painter.drawEllipse(QPointF(10.0, 10.0), 1.8, 1.8);
    painter.drawEllipse(QPointF(17.0, 6.0), 1.8, 1.8);

    return QIcon(pixmap);
}

}// namespace MediaToolIcons

#endif// MEDIA_TOOL_ICONS_HPP
