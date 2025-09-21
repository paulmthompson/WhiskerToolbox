#include "PlotInteractionController.hpp"

#include "ViewAdapter.hpp"

#include <QMouseEvent>
#include <QObject>
#include <QRubberBand>
#include <QWheelEvent>

PlotInteractionController::PlotInteractionController(QWidget * host_widget, std::unique_ptr<ViewAdapter> adapter)
    : QObject(host_widget),
      _host(host_widget),
      _adapter(std::move(adapter)) {}

PlotInteractionController::~PlotInteractionController() {
    if (_rubber) {
        delete _rubber;
        _rubber = nullptr;
    }
}

bool PlotInteractionController::handleWheel(QWheelEvent * event) {
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
    BoundingBox bounds = _adapter->getProjectionBounds();
    emit viewBoundsChanged(bounds);
    event->accept();
    return true;
}

bool PlotInteractionController::handleMousePress(QMouseEvent * event) {
    if (event->button() != Qt::LeftButton) return false;

    // Don't handle panning if Ctrl is held (for selection modes)
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        return false;
    }

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

bool PlotInteractionController::handleMouseMove(QMouseEvent * event) {
    BoundingBox bounds = _adapter->getProjectionBounds();
    if (bounds.width() > 0 && bounds.height() > 0 && _adapter->viewportWidth() > 0 && _adapter->viewportHeight() > 0) {
        float wx = bounds.min_x + (static_cast<float>(event->pos().x()) / static_cast<float>(_adapter->viewportWidth())) * (bounds.width());
        float wy = bounds.max_y + (static_cast<float>(event->pos().y()) / static_cast<float>(_adapter->viewportHeight())) * (bounds.height());
        emit mouseWorldMoved(wx, wy);
    }

    if (_box_zoom && _rubber) {
        _rubber->setGeometry(QRect(_rubber_origin, event->pos()).normalized());
        event->accept();
        return true;
    }

    if (_is_panning && (event->buttons() & Qt::LeftButton) && !event->modifiers().testFlag(Qt::ControlModifier)) {
        QPoint delta = event->pos() - _last_mouse;
        int vw = std::max(1, _adapter->viewportWidth());
        int vh = std::max(1, _adapter->viewportHeight());

        // World units per pixel in current view
        float world_per_pixel_x = (bounds.width()) / static_cast<float>(vw);
        float world_per_pixel_y = (bounds.height()) / static_cast<float>(vh);
        float dx_world = static_cast<float>(delta.x()) * world_per_pixel_x;
        float dy_world = static_cast<float>(-delta.y()) * world_per_pixel_y;

        // Convert world delta to normalized pan units expected by widgets
        float padding = _adapter->getPadding();
        float aspect = static_cast<float>(vw) / static_cast<float>(vh);
        // denom_x = data_width * zoom_x; denom_y = data_height * zoom_y
        float denom_x = (aspect > 1.0f) ? ((bounds.width()) / (padding * aspect)) : ((bounds.width()) / padding);
        float denom_y = (aspect > 1.0f) ? ((bounds.height()) / padding) : (((bounds.height()) * aspect) / padding);
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

bool PlotInteractionController::handleMouseRelease(QMouseEvent * event) {
    if (event->button() != Qt::LeftButton) return false;
    _is_panning = false;
    if (_box_zoom && _rubber) {
        _rubber->hide();
        QRect rect = _rubber->geometry();
        _box_zoom = false;
        if (rect.width() > 3 && rect.height() > 3) {
            auto bounds = _adapter->getProjectionBounds();
            float min_x = bounds.min_x + (static_cast<float>(rect.left()) / static_cast<float>(_adapter->viewportWidth())) * (bounds.width());
            float max_x = bounds.min_x + (static_cast<float>(rect.right()) / static_cast<float>(_adapter->viewportWidth())) * (bounds.width());
            float min_y = bounds.max_y - (static_cast<float>(rect.bottom()) / static_cast<float>(_adapter->viewportHeight())) * (bounds.height());
            float max_y = bounds.max_y - (static_cast<float>(rect.top()) / static_cast<float>(_adapter->viewportHeight())) * (bounds.height());
            if (min_x > max_x) std::swap(min_x, max_x);
            if (min_y > max_y) std::swap(min_y, max_y);
            BoundingBox box(min_x, min_y, max_x, max_y);
            _adapter->applyBoxZoomToWorldRect(box);
            _adapter->requestUpdate();
            emit viewBoundsChanged(box);
        }
        event->accept();
        return true;
    }
    return false;
}

void PlotInteractionController::handleLeave() {
    _is_panning = false;
    _box_zoom = false;
    if (_rubber) _rubber->hide();
}
