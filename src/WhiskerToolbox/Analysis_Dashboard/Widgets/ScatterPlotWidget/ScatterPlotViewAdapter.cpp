#include "ScatterPlotViewAdapter.hpp"
#include "ScatterPlotOpenGLWidget.hpp"

ScatterPlotViewAdapter::ScatterPlotViewAdapter(ScatterPlotOpenGLWidget * widget)
    : _w(widget) {}

void ScatterPlotViewAdapter::getProjectionBounds(float & left, float & right, float & bottom, float & top) const {
    _w->calculateProjectionBounds(left, right, bottom, top);
}

void ScatterPlotViewAdapter::getPerAxisZoom(float & zoom_x, float & zoom_y) const {
    zoom_x = _w->_zoom_level_x;
    zoom_y = _w->_zoom_level_y;
}

void ScatterPlotViewAdapter::setPerAxisZoom(float zoom_x, float zoom_y) {
    _w->_zoom_level_x = zoom_x;
    _w->_zoom_level_y = zoom_y;
}

void ScatterPlotViewAdapter::getPan(float & pan_x, float & pan_y) const {
    pan_x = _w->_pan_offset_x;
    pan_y = _w->_pan_offset_y;
}

void ScatterPlotViewAdapter::setPan(float pan_x, float pan_y) {
    _w->setPanOffset(pan_x, pan_y);
}

float ScatterPlotViewAdapter::getPadding() const {
    return _w->_padding_factor;
}

int ScatterPlotViewAdapter::viewportWidth() const {
    return _w->width();
}

int ScatterPlotViewAdapter::viewportHeight() const {
    return _w->height();
}

void ScatterPlotViewAdapter::requestUpdate() {
    _w->updateProjectionMatrix();
    _w->requestThrottledUpdate();
}

void ScatterPlotViewAdapter::applyBoxZoomToWorldRect(float min_x, float max_x, float min_y, float max_y) {
    float data_width = _w->_data_max_x - _w->_data_min_x;
    float data_height = _w->_data_max_y - _w->_data_min_y;
    float center_x = (_w->_data_min_x + _w->_data_max_x) * 0.5f;
    float center_y = (_w->_data_min_y + _w->_data_max_y) * 0.5f;
    float target_width = std::max(1e-6f, max_x - min_x);
    float target_height = std::max(1e-6f, max_y - min_y);
    float aspect_ratio = static_cast<float>(_w->width()) / std::max(1, _w->height());
    float padding = _w->_padding_factor;
    float zfx, zfy;
    if (aspect_ratio > 1.0f) {
        zfx = target_width / (aspect_ratio * data_width * padding);
        zfy = target_height / (data_height * padding);
    } else {
        zfx = target_width / (data_width * padding);
        zfy = (target_height * aspect_ratio) / (data_height * padding);
    }
    _w->_zoom_level_x = std::clamp(1.0f / zfx, 0.1f, 10.0f);
    _w->_zoom_level_y = std::clamp(1.0f / zfy, 0.1f, 10.0f);
    _w->_zoom_level = std::clamp((_w->_zoom_level_x + _w->_zoom_level_y) * 0.5f, 0.1f, 10.0f);
    float target_cx = 0.5f * (min_x + max_x);
    float target_cy = 0.5f * (min_y + max_y);
    float pan_norm_x = (target_cx - center_x) / (data_width * (1.0f / _w->_zoom_level_x));
    float pan_norm_y = (target_cy - center_y) / (data_height * (1.0f / _w->_zoom_level_y));
    _w->_pan_offset_x = pan_norm_x;
    _w->_pan_offset_y = pan_norm_y;
}

