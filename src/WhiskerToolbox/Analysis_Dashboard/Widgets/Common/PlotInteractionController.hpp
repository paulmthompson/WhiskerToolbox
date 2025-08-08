#ifndef PLOTINTERACTIONCONTROLLER_HPP
#define PLOTINTERACTIONCONTROLLER_HPP

#include "ViewAdapter.hpp"

#include <QObject>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QRubberBand>

class PlotInteractionController : public QObject {
    Q_OBJECT
public:
    PlotInteractionController(QWidget * host_widget, std::unique_ptr<ViewAdapter> adapter)
        : QObject(host_widget), _host(host_widget), _adapter(std::move(adapter)) {}

    bool handleWheel(QWheelEvent * event) {
        float zx, zy;
        _adapter->getPerAxisZoom(zx, zy);
        float zf = 1.0f + (event->angleDelta().y() / 1200.0f);
        zf = std::clamp(zf, 0.1f, 10.0f);
        auto mods = event->modifiers();
        if (mods.testFlag(Qt::ControlModifier) && !mods.testFlag(Qt::ShiftModifier)) {
            zx = std::clamp(zx * zf, 0.1f, 10.0f);
        } else if (mods.testFlag(Qt::ShiftModifier) && !mods.testFlag(Qt::ControlModifier)) {
            zy = std::clamp(zy * zf, 0.1f, 10.0f);
        } else {
            zx = std::clamp(zx * zf, 0.1f, 10.0f);
            zy = std::clamp(zy * zf, 0.1f, 10.0f);
        }
        _adapter->setPerAxisZoom(zx, zy);
        _adapter->requestUpdate();
        float l, r, b, t;
        _adapter->getProjectionBounds(l, r, b, t);
        emit viewBoundsChanged(l, r, b, t);
        event->accept();
        return true;
    }

    bool handleMousePress(QMouseEvent * event) {
        if (event->button() != Qt::LeftButton) return false;
        _is_panning = true;
        _last_mouse = event->pos();
        if (event->modifiers().testFlag(Qt::AltModifier)) {
            if (!_rubber) {
                _rubber = new QRubberBand(QRubberBand::Rectangle, _host);
            }
            _box_zoom = true;
            _rubber_origin = event->pos();
            _rubber->setGeometry(QRect(_rubber_origin, QSize()));
            _rubber->show();
        }
        event->accept();
        return true;
    }

    bool handleMouseMove(QMouseEvent * event) {
        float l, r, b, t;
        _adapter->getProjectionBounds(l, r, b, t);
        if (r != l && t != b && _adapter->viewportWidth() > 0 && _adapter->viewportHeight() > 0) {
            float wx = l + (static_cast<float>(event->pos().x()) / _adapter->viewportWidth()) * (r - l);
            float wy = t - (static_cast<float>(event->pos().y()) / _adapter->viewportHeight()) * (t - b);
            emit mouseWorldMoved(wx, wy);
        }

        if (_box_zoom && _rubber) {
            _rubber->setGeometry(QRect(_rubber_origin, event->pos()).normalized());
            event->accept();
            return true;
        }

        if (_is_panning && (event->buttons() & Qt::LeftButton)) {
            QPoint delta = event->pos() - _last_mouse;
            int vw = std::max(1, _adapter->viewportWidth());
            int vh = std::max(1, _adapter->viewportHeight());

            // World units per pixel in current view
            float world_per_pixel_x = (r - l) / vw;
            float world_per_pixel_y = (t - b) / vh;
            float dx_world = delta.x() * world_per_pixel_x;
            float dy_world = -delta.y() * world_per_pixel_y;

            // Convert world delta to normalized pan units expected by widgets
            float padding = _adapter->getPadding();
            float aspect = static_cast<float>(vw) / vh;
            // denom_x = data_width * zoom_x; denom_y = data_height * zoom_y
            float denom_x = (aspect > 1.0f) ? ((r - l) / (padding * aspect)) : ((r - l) / padding);
            float denom_y = (aspect > 1.0f) ? ((t - b) / padding) : (((t - b) * aspect) / padding);
            if (denom_x == 0.0f) denom_x = 1.0f;
            if (denom_y == 0.0f) denom_y = 1.0f;

            float dx_norm = dx_world / denom_x;
            float dy_norm = dy_world / denom_y;

            float px, py;
            _adapter->getPan(px, py);
            _adapter->setPan(px + dx_norm, py + dy_norm);
            _adapter->requestUpdate();
            _last_mouse = event->pos();
            event->accept();
            return true;
        }
        return false;
    }

    bool handleMouseRelease(QMouseEvent * event) {
        if (event->button() != Qt::LeftButton) return false;
        _is_panning = false;
        if (_box_zoom && _rubber) {
            _rubber->hide();
            QRect rect = _rubber->geometry();
            _box_zoom = false;
            if (rect.width() > 3 && rect.height() > 3) {
                float l, r, b, t;
                _adapter->getProjectionBounds(l, r, b, t);
                float min_x = l + (static_cast<float>(rect.left()) / _adapter->viewportWidth()) * (r - l);
                float max_x = l + (static_cast<float>(rect.right()) / _adapter->viewportWidth()) * (r - l);
                float min_y = t - (static_cast<float>(rect.bottom()) / _adapter->viewportHeight()) * (t - b);
                float max_y = t - (static_cast<float>(rect.top()) / _adapter->viewportHeight()) * (t - b);
                if (min_x > max_x) std::swap(min_x, max_x);
                if (min_y > max_y) std::swap(min_y, max_y);
                _adapter->applyBoxZoomToWorldRect(min_x, max_x, min_y, max_y);
                _adapter->requestUpdate();
                emit viewBoundsChanged(min_x, max_x, min_y, max_y);
            }
            event->accept();
            return true;
        }
        return false;
    }

    void handleLeave() {
        _is_panning = false;
        _box_zoom = false;
        if (_rubber) _rubber->hide();
    }

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

