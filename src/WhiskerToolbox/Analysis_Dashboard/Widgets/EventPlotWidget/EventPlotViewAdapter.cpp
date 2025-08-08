#include "EventPlotViewAdapter.hpp"
#include "EventPlotOpenGLWidget.hpp"

EventPlotViewAdapter::EventPlotViewAdapter(EventPlotOpenGLWidget * widget)
    : _w(widget) {}

void EventPlotViewAdapter::getProjectionBounds(float & left, float & right, float & bottom, float & top) const {
    _w->calculateProjectionBounds(left, right, bottom, top);
}

void EventPlotViewAdapter::getPerAxisZoom(float & zoom_x, float & zoom_y) const {
    zoom_x = _w->_zoom_level_x;
    zoom_y = _w->_y_zoom_level;
}

void EventPlotViewAdapter::setPerAxisZoom(float zoom_x, float zoom_y) {
    _w->_zoom_level_x = zoom_x;
    _w->_y_zoom_level = zoom_y;
}

void EventPlotViewAdapter::getPan(float & pan_x, float & pan_y) const {
    pan_x = _w->_pan_offset_x;
    pan_y = _w->_pan_offset_y;
}

void EventPlotViewAdapter::setPan(float pan_x, float pan_y) {
    _w->setPanOffset(pan_x, pan_y);
}

float EventPlotViewAdapter::getPadding() const {
    return _w->_padding_factor;
}

int EventPlotViewAdapter::viewportWidth() const {
    return _w->_widget_width;
}

int EventPlotViewAdapter::viewportHeight() const {
    return _w->_widget_height;
}

void EventPlotViewAdapter::requestUpdate() {
    _w->updateMatrices();
    _w->update();
}

void EventPlotViewAdapter::applyBoxZoomToWorldRect(float min_x, float max_x, float min_y, float max_y) {
    float x_range_width = static_cast<float>(_w->_negative_range + _w->_positive_range);
    float y_range_height = 2.0f;
    float ar = static_cast<float>(_w->_widget_width) / std::max(1, _w->_widget_height);
    float pad = _w->_padding_factor;
    float zfx, zfy;
    if (ar > 1.0f) {
        zfx = (max_x - min_x) / (ar * x_range_width * pad);
        zfy = (max_y - min_y) / (y_range_height * pad);
    } else {
        zfx = (max_x - min_x) / (x_range_width * pad);
        zfy = ((max_y - min_y) * ar) / (y_range_height * pad);
    }
    _w->_zoom_level_x = std::clamp(1.0f / zfx, 0.1f, 10.0f);
    _w->_y_zoom_level = std::clamp(1.0f / zfy, 0.1f, 10.0f);
    float cx = 0.5f * (min_x + max_x);
    float cy = 0.5f * (min_y + max_y);
    _w->_pan_offset_x = cx / (x_range_width * (1.0f / _w->_zoom_level_x));
    _w->_pan_offset_y = cy / (y_range_height * (1.0f / _w->_y_zoom_level));
}

