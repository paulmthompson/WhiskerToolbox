#ifndef PLOTINTERACTIONCONTROLLER_HPP
#define PLOTINTERACTIONCONTROLLER_HPP

#include "ViewAdapter.hpp"

#include <QObject>
#include <QPoint>

class QRubberBand;
class QWheelEvent;
class QMouseEvent;

class PlotInteractionController : public QObject {
    Q_OBJECT
public:
    PlotInteractionController(QWidget * host_widget, std::unique_ptr<ViewAdapter> adapter);

    ~PlotInteractionController();

    bool handleWheel(QWheelEvent * event);

    bool handleMousePress(QMouseEvent * event);

    bool handleMouseMove(QMouseEvent * event);
            
    bool handleMouseRelease(QMouseEvent * event);

    void handleLeave();

signals:
    void viewBoundsChanged(float left, float right, float bottom, float top);
    void mouseWorldMoved(float world_x, float world_y);

private:
    QWidget * _host;
    std::unique_ptr<ViewAdapter> _adapter;
    bool _is_panning = false;
    QPoint _last_mouse;
    bool _box_zoom = false;
    QRubberBand * _rubber = nullptr;
    QPoint _rubber_origin;
};

#endif // PLOTINTERACTIONCONTROLLER_HPP

