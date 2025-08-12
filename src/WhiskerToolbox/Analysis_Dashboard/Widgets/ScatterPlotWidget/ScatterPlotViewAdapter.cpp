#include "ScatterPlotViewAdapter.hpp"
#include "ScatterPlotOpenGLWidget_Refactored.hpp"
#include "CoreGeometry/boundingbox.hpp"

ScatterPlotViewAdapter::ScatterPlotViewAdapter(ScatterPlotOpenGLWidget * widget)
    : _w(widget) {}

void ScatterPlotViewAdapter::getProjectionBounds(float & left, float & right, float & bottom, float & top) const {
    // Use the base class method to compute camera world view
    auto data_bounds = _w->getDataBounds();
    float padding = _w->_padding_factor;
    
    // Calculate world bounds based on current view state
    float data_width = data_bounds.width();
    float data_height = data_bounds.height();
    float center_x = data_bounds.center_x();
    float center_y = data_bounds.center_y();
    
    if (data_width <= 0 || data_height <= 0) {
        left = right = bottom = top = 0.0f;
        return;
    }
    
    // Apply zoom and pan transformations
    float aspect = static_cast<float>(_w->width()) / std::max(1, _w->height());
    float half_w, half_h;
    
    if (aspect > 1.0f) {
        half_w = (data_width * padding * aspect) / (2.0f * _w->_zoom_level_x);
        half_h = (data_height * padding) / (2.0f * _w->_zoom_level_y);
    } else {
        half_w = (data_width * padding) / (2.0f * _w->_zoom_level_x);
        half_h = (data_height * padding / aspect) / (2.0f * _w->_zoom_level_y);
    }
    
    float pan_world_x = (_w->_pan_offset_x * data_width) / _w->_zoom_level_x;
    float pan_world_y = (_w->_pan_offset_y * data_height) / _w->_zoom_level_y;
    
    left = center_x - half_w + pan_world_x;
    right = center_x + half_w + pan_world_x;
    bottom = center_y - half_h + pan_world_y;
    top = center_y + half_h + pan_world_y;
}

void ScatterPlotViewAdapter::getPerAxisZoom(float & zoom_x, float & zoom_y) const {
    zoom_x = _w->_zoom_level_x;
    zoom_y = _w->_zoom_level_y;
}

void ScatterPlotViewAdapter::setPerAxisZoom(float zoom_x, float zoom_y) {
    _w->_zoom_level_x = zoom_x;
    _w->_zoom_level_y = zoom_y;
    requestUpdate();
}

void ScatterPlotViewAdapter::getPan(float & pan_x, float & pan_y) const {
    pan_x = _w->_pan_offset_x;
    pan_y = _w->_pan_offset_y;
}

void ScatterPlotViewAdapter::setPan(float pan_x, float pan_y) {
    _w->_pan_offset_x = pan_x;
    _w->_pan_offset_y = pan_y;
    requestUpdate();
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
    _w->updateViewMatrices();
    _w->requestThrottledUpdate();
}

void ScatterPlotViewAdapter::applyBoxZoomToWorldRect(float min_x, float max_x, float min_y, float max_y) {
    auto data_bounds = _w->getDataBounds();
    float data_width = data_bounds.width();
    float data_height = data_bounds.height();
    float center_x = data_bounds.center_x();
    float center_y = data_bounds.center_y();
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
    //_w->_zoom_level = std::clamp((_w->_zoom_level_x + _w->_zoom_level_y) * 0.5f, 0.1f, 10.0f);
    float target_cx = 0.5f * (min_x + max_x);
    float target_cy = 0.5f * (min_y + max_y);
    float pan_norm_x = (target_cx - center_x) / (data_width * (1.0f / _w->_zoom_level_x));
    float pan_norm_y = (target_cy - center_y) / (data_height * (1.0f / _w->_zoom_level_y));
    _w->_pan_offset_x = pan_norm_x;
    _w->_pan_offset_y = pan_norm_y;
    requestUpdate();
}

