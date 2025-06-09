#include "PlottingManager.hpp"

void PlottingManager::calculateAnalogSeriesAllocation(int series_index,
                                                      float & allocated_center,
                                                      float & allocated_height) const {
    if (total_analog_series <= 0) {
        allocated_center = 0.0f;
        allocated_height = viewport_y_max - viewport_y_min;
        return;
    }

    // Calculate equal spacing for all analog series within the viewport
    float const total_viewport_height = viewport_y_max - viewport_y_min;
    float const series_height = total_viewport_height / static_cast<float>(total_analog_series);

    // Calculate center position for this series
    // Series are stacked from top to bottom
    float const series_bottom = viewport_y_min + static_cast<float>(series_index) * series_height;
    allocated_center = series_bottom + series_height * 0.5f;
    allocated_height = series_height;
}

void PlottingManager::setVisibleDataRange(int start_index, int end_index) {
    visible_start_index = start_index;
    visible_end_index = end_index;
}

int PlottingManager::addAnalogSeries() {
    int const new_series_index = total_analog_series;
    ++total_analog_series;
    return new_series_index;
}

int PlottingManager::addDigitalIntervalSeries() {
    int const new_series_index = total_digital_series;
    ++total_digital_series;
    return new_series_index;
}

void PlottingManager::calculateDigitalIntervalSeriesAllocation(int series_index,
                                                               float & allocated_center,
                                                               float & allocated_height) const {
    // Digital intervals typically extend across the full canvas for maximum visibility
    // This allows other data types (like analog time series) to be visible through
    // the semi-transparent interval rectangles

    allocated_center = (viewport_y_min + viewport_y_max) * 0.5f;// Center of viewport
    allocated_height = viewport_y_max - viewport_y_min;         // Full viewport height

    // Note: In the future, if we want to support stacked digital intervals,
    // we can implement spacing logic similar to analog series allocation
}

void PlottingManager::setPanOffset(float pan_offset) {
    vertical_pan_offset = pan_offset;
}

void PlottingManager::applyPanDelta(float pan_delta) {
    vertical_pan_offset += pan_delta;
}

float PlottingManager::getPanOffset() const {
    return vertical_pan_offset;
}

void PlottingManager::resetPan() {
    vertical_pan_offset = 0.0f;
}