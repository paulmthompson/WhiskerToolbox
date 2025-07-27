#ifndef POINTSELECTIONHANDLER_HPP
#define POINTSELECTIONHANDLER_HPP

#include "SelectionModes.hpp"
#include "CoreGeometry/points.hpp"

#include <QMatrix4x4>
#include <QVector2D>
#include <QMouseEvent>

#include <functional>
#include <memory>

class QKeyEvent;


class PointSelectionHandler {
public:
    using NotificationCallback = std::function<void()>;

    explicit PointSelectionHandler(float world_tolerance);
    ~PointSelectionHandler() = default;

    void setNotificationCallback(NotificationCallback callback);
    void clearNotificationCallback();

    void render(QMatrix4x4 const & mvp_matrix) {}
    void deactivate() {}

    std::unique_ptr<SelectionRegion> const & getActiveSelectionRegion() const {
        static std::unique_ptr<SelectionRegion> null_region = nullptr;
        return null_region;
    }

    void mousePressEvent(QMouseEvent * event, QVector2D const & world_pos);
    void mouseMoveEvent(QMouseEvent * event, QVector2D const & world_pos) {}
    void mouseReleaseEvent(QMouseEvent * event, QVector2D const & world_pos) {}
    void keyPressEvent(QKeyEvent * event) {}

    QVector2D getWorldPos() const { return _world_pos; }
    Qt::KeyboardModifiers getModifiers() const { return _modifiers; }
    
    // TODO: This tolerance should be updated when the zoom level changes
    float getWorldTolerance() const { return _world_tolerance; }


private:
    NotificationCallback _notification_callback;
    QVector2D _world_pos;
    Qt::KeyboardModifiers _modifiers;
    float _world_tolerance;
};

#endif // POINTSELECTIONHANDLER_HPP 