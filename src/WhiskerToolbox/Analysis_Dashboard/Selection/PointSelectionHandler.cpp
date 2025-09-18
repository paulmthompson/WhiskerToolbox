#include "PointSelectionHandler.hpp"
#include <QMouseEvent>

PointSelectionHandler::PointSelectionHandler(float world_tolerance)
    : _world_tolerance(world_tolerance) {}

void PointSelectionHandler::setNotificationCallback(NotificationCallback callback) {
    _notification_callback = callback;
}

void PointSelectionHandler::clearNotificationCallback() {
    _notification_callback = nullptr;
}

void PointSelectionHandler::mousePressEvent(QMouseEvent * event, QVector2D const & world_pos) {
    if (event->button() == Qt::LeftButton && event->modifiers().testFlag(Qt::ControlModifier)) {
        _world_pos = world_pos;
        _modifiers = event->modifiers();

        if (_notification_callback) {
            _notification_callback();
        }
    }
}