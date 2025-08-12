#include "GenericViewAdapter.hpp"
#include "BasePlotOpenGLWidget.hpp"

GenericViewAdapter::GenericViewAdapter(BasePlotOpenGLWidget* widget)
    : _widget(widget) {}

void GenericViewAdapter::getProjectionBounds(float& left, float& right, float& bottom, float& top) const {
    ViewUtils::calculateProjectionBounds(_widget->getViewState(), left, right, bottom, top);
}

void GenericViewAdapter::getPerAxisZoom(float& zoom_x, float& zoom_y) const {
    const auto& state = _widget->getViewState();
    zoom_x = state.zoom_level_x;
    zoom_y = state.zoom_level_y;
}

void GenericViewAdapter::setPerAxisZoom(float zoom_x, float zoom_y) {
    auto& state = _widget->getViewState();
    state.zoom_level_x = zoom_x;
    state.zoom_level_y = zoom_y;
    requestUpdate();
}

void GenericViewAdapter::getPan(float& pan_x, float& pan_y) const {
    const auto& state = _widget->getViewState();
    pan_x = state.pan_offset_x;
    pan_y = state.pan_offset_y;
}

void GenericViewAdapter::setPan(float pan_x, float pan_y) {
    auto& state = _widget->getViewState();
    state.pan_offset_x = pan_x;
    state.pan_offset_y = pan_y;
    requestUpdate();
}

float GenericViewAdapter::getPadding() const {
    return _widget->getViewState().padding_factor;
}

int GenericViewAdapter::viewportWidth() const {
    return _widget->width();
}

int GenericViewAdapter::viewportHeight() const {
    return _widget->height();
}

void GenericViewAdapter::requestUpdate() {
    // Update widget dimensions in view state
    auto& state = _widget->getViewState();
    state.widget_width = _widget->width();
    state.widget_height = _widget->height();
    
    // Trigger widget update
    _widget->updateViewMatrices();
    _widget->requestThrottledUpdate();
}

void GenericViewAdapter::applyBoxZoomToWorldRect(float min_x, float max_x, float min_y, float max_y) {
    auto& state = _widget->getViewState();
    
    // Update widget dimensions before box zoom calculation
    state.widget_width = _widget->width();
    state.widget_height = _widget->height();
    
    ViewUtils::applyBoxZoom(state, min_x, max_x, min_y, max_y);
    requestUpdate();
}
