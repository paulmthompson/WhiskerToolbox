#ifndef INTERACTIVEOPENGLWIDGETBASE_HPP
#define INTERACTIVEOPENGLWIDGETBASE_HPP

// Prototype header (Lakos order)

// Project headers

// Third-party library headers
#include <QOpenGLWidget>
#include <QPoint>
#include <QRubberBand>
#include <QWheelEvent>

// Almost-standard / Boost headers

// Standard C++ headers
#include <optional>
#include <utility>
#include <algorithm>

/**
 * @brief Base class providing standardized interaction (zoom/pan/box-zoom and world cursor reporting) for OpenGL widgets.
 *
 * Derived classes are responsible for providing current projection bounds, updating their view state when zoom/pan
 * changes, and applying a box-zoom to a requested world rectangle.
 */
class InteractiveOpenGLWidgetBase : public QOpenGLWidget {

public:
    /**
     * @brief Construct a new InteractiveOpenGLWidgetBase
     * @param parent Qt parent widget
     */
    explicit InteractiveOpenGLWidgetBase(QWidget * parent = nullptr)
        : QOpenGLWidget(parent) {
    }

    ~InteractiveOpenGLWidgetBase() override = default;

protected:
    /**
     * @brief Hook for derived classes to react to mouse world position updates (e.g., emit a signal).
     */
    virtual void onMouseWorldMoved(float world_x, float world_y) = 0;
    // --------- Hooks required by derived classes ---------
    /**
     * @brief Get the current projection bounds (left, right, bottom, top) in world coordinates.
     */
    virtual void getCurrentProjectionBounds(float & left, float & right, float & bottom, float & top) const = 0;

    /**
     * @brief Update per-axis zoom levels; derived must update its internal view and matrices.
     */
    virtual void setPerAxisZoomLevels(float zoom_x, float zoom_y) = 0;

    /**
     * @brief Read per-axis zoom levels from derived state.
     */
    virtual void getPerAxisZoomLevels(float & zoom_x, float & zoom_y) const = 0;

    /**
     * @brief Set pan offset in world-normalized units; derived must update its internal view and matrices.
     */
    virtual void setPanOffsetWorld(float pan_x, float pan_y) = 0;

    /**
     * @brief Read pan offset from derived state.
     */
    virtual void getPanOffsetWorld(float & pan_x, float & pan_y) const = 0;

    /**
     * @brief Get the padding factor used around data bounds when computing projections (e.g., 1.1 for 10%).
     */
    virtual float getPaddingFactor() const = 0;

    /**
     * @brief Notify derived that view bounds changed (derived may emit its own signals).
     */
    virtual void onViewBoundsChanged(float left, float right, float bottom, float top) = 0;

    /**
     * @brief Apply a box-zoom to the given world rectangle. Derived should update its zoom/pan accordingly.
     */
    virtual void applyBoxZoomToWorldRect(float min_x, float max_x, float min_y, float max_y) = 0;

    // --------- Shared interaction implementation ---------
    /**
     * @brief Process wheel event with Ctrl/Shift modifiers for per-axis zoom. Returns true if consumed.
     */
    bool processWheelEvent(QWheelEvent * event) {
        float zoom_x, zoom_y;
        getPerAxisZoomLevels(zoom_x, zoom_y);
        float zoom_factor = 1.0f + (event->angleDelta().y() / 1200.0f);
        zoom_factor = std::clamp(zoom_factor, 0.1f, 10.0f);
        auto mods = event->modifiers();
        if (mods.testFlag(Qt::ControlModifier) && !mods.testFlag(Qt::ShiftModifier)) {
            zoom_x = std::clamp(zoom_x * zoom_factor, 0.1f, 10.0f);
        } else if (mods.testFlag(Qt::ShiftModifier) && !mods.testFlag(Qt::ControlModifier)) {
            zoom_y = std::clamp(zoom_y * zoom_factor, 0.1f, 10.0f);
        } else {
            zoom_x = std::clamp(zoom_x * zoom_factor, 0.1f, 10.0f);
            zoom_y = std::clamp(zoom_y * zoom_factor, 0.1f, 10.0f);
        }
        setPerAxisZoomLevels(zoom_x, zoom_y);
        float l, r, b, t;
        getCurrentProjectionBounds(l, r, b, t);
        onViewBoundsChanged(l, r, b, t);
        event->accept();
        return true;
    }

    /**
     * @brief Process mouse press: starts panning; Alt+Left starts box-zoom rubber band. Returns true if consumed.
     */
    bool processMousePressEvent(QMouseEvent * event) {
        if (event->button() == Qt::LeftButton) {
            _is_panning = true;
            _last_mouse_pos = event->pos();
            if (event->modifiers().testFlag(Qt::AltModifier)) {
                if (_rubber_band == nullptr) {
                    _rubber_band = new QRubberBand(QRubberBand::Rectangle, this);
                }
                _box_zoom_active = true;
                _rubber_origin = event->pos();
                _rubber_band->setGeometry(QRect(_rubber_origin, QSize()));
                _rubber_band->show();
            }
            event->accept();
            return true;
        }
        return false;
    }

    /**
     * @brief Process mouse move: panning if dragging; rubber band update if box-zooming; emits cursor world position.
     */
    bool processMouseMoveEvent(QMouseEvent * event) {
        // Emit mouse world coordinates for UI labels
        float l, r, b, t;
        getCurrentProjectionBounds(l, r, b, t);
        if (r != l && t != b && width() > 0 && height() > 0) {
            float world_x = l + (static_cast<float>(event->pos().x()) / width()) * (r - l);
            float world_y = t - (static_cast<float>(event->pos().y()) / height()) * (t - b);
            onMouseWorldMoved(world_x, world_y);
        }

        if (_box_zoom_active && _rubber_band) {
            QRect rect = QRect(_rubber_origin, event->pos()).normalized();
            _rubber_band->setGeometry(rect);
            event->accept();
            return true;
        }

        if (_is_panning && (event->buttons() & Qt::LeftButton)) {
            QPoint delta = event->pos() - _last_mouse_pos;
            // Convert pixel delta to world using current bounds
            float world_per_pixel_x = (r - l) / std::max(1, width());
            float world_per_pixel_y = (t - b) / std::max(1, height());
            float dx = delta.x() * world_per_pixel_x;
            float dy = -delta.y() * world_per_pixel_y;

            float pan_x, pan_y;
            getPanOffsetWorld(pan_x, pan_y);
            setPanOffsetWorld(pan_x + dx, pan_y + dy);
            _last_mouse_pos = event->pos();
            event->accept();
            return true;
        }
        return false;
    }

    /**
     * @brief Process mouse release: finalize box-zoom if active. Returns true if consumed.
     */
    bool processMouseReleaseEvent(QMouseEvent * event) {
        if (event->button() == Qt::LeftButton) {
            _is_panning = false;
            if (_box_zoom_active && _rubber_band) {
                _rubber_band->hide();
                QRect rect = _rubber_band->geometry();
                _box_zoom_active = false;
                if (rect.width() > 3 && rect.height() > 3) {
                    float l, r, b, t;
                    getCurrentProjectionBounds(l, r, b, t);
                    float min_x = l + (static_cast<float>(rect.left()) / width()) * (r - l);
                    float max_x = l + (static_cast<float>(rect.right()) / width()) * (r - l);
                    float min_y = t - (static_cast<float>(rect.bottom()) / height()) * (t - b);
                    float max_y = t - (static_cast<float>(rect.top()) / height()) * (t - b);
                    if (min_x > max_x) std::swap(min_x, max_x);
                    if (min_y > max_y) std::swap(min_y, max_y);
                    applyBoxZoomToWorldRect(min_x, max_x, min_y, max_y);
                }
                event->accept();
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Reset internal interaction state on leave.
     */
    void processLeaveEvent() {
        _is_panning = false;
        _box_zoom_active = false;
        if (_rubber_band) {
            _rubber_band->hide();
        }
    }

private:
    bool _is_panning = false;
    QPoint _last_mouse_pos;
    bool _box_zoom_active = false;
    QRubberBand * _rubber_band = nullptr;
    QPoint _rubber_origin;
};

#endif // INTERACTIVEOPENGLWIDGETBASE_HPP

