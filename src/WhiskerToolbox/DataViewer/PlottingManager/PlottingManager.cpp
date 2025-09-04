#include "PlottingManager.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>

void PlottingManager::calculateAnalogSeriesAllocation(int series_index,
                                                      float & allocated_center,
                                                      float & allocated_height) const {
    if (total_analog_series <= 0) {
        allocated_center = 0.0f;
        allocated_height = 2.0f;// Full canvas height
        return;
    }

    // Calculate allocated height for this series
    allocated_height = (viewport_y_max - viewport_y_min) / static_cast<float>(total_analog_series);

    // Calculate center Y coordinate
    // Series are stacked from top to bottom starting at viewport_y_min
    allocated_center = viewport_y_min + allocated_height * (static_cast<float>(series_index) + 0.5f);
}

void PlottingManager::setVisibleDataRange(int start_index, int end_index) {
    visible_start_index = start_index;
    visible_end_index = end_index;
    total_data_points = end_index - start_index;
}

int PlottingManager::addAnalogSeries() {
    int series_index = total_analog_series;
    total_analog_series++;
    return series_index;
}

int PlottingManager::addDigitalIntervalSeries() {
    int series_index = total_digital_series;
    total_digital_series++;
    return series_index;
}

int PlottingManager::addDigitalEventSeries() {
    int series_index = total_event_series;
    total_event_series++;
    return series_index;
}

int PlottingManager::addAnalogSeries(std::string const & key,
                                     std::shared_ptr<AnalogTimeSeries> series,
                                     std::shared_ptr<TimeFrame> time_frame,
                                     std::string const & color) {
    int series_index = total_analog_series;
    
    AnalogSeriesInfo info;
    info.series = series;
    info.time_frame = time_frame;
    info.key = key;
    info.color = color.empty() ? generateDefaultColor(series_index) : color;
    info.visible = true;
    // Extract group and channel id from key pattern name_idx
    _extractGroupAndChannel(key, info.group_name, info.channel_id);
    
    analog_series_map[key] = std::move(info);
    total_analog_series++;
    
    return series_index;
}

int PlottingManager::addDigitalEventSeries(std::string const & key,
                                           std::shared_ptr<DigitalEventSeries> series,
                                           std::shared_ptr<TimeFrame> time_frame,
                                           std::string const & color) {
    int series_index = total_event_series;
    
    DigitalEventSeriesInfo info;
    info.series = series;
    info.time_frame = time_frame;
    info.key = key;
    info.color = color.empty() ? generateDefaultColor(total_analog_series + series_index) : color;
    info.visible = true;
    
    digital_event_series_map[key] = std::move(info);
    total_event_series++;
    
    return series_index;
}

int PlottingManager::addDigitalIntervalSeries(std::string const & key,
                                              std::shared_ptr<DigitalIntervalSeries> series,
                                              std::shared_ptr<TimeFrame> time_frame,
                                              std::string const & color) {
    int series_index = total_digital_series;
    
    DigitalIntervalSeriesInfo info;
    info.series = series;
    info.time_frame = time_frame;
    info.key = key;
    info.color = color.empty() ? generateDefaultColor(total_analog_series + total_event_series + series_index) : color;
    info.visible = true;
    
    digital_interval_series_map[key] = std::move(info);
    total_digital_series++;
    
    return series_index;
}

bool PlottingManager::removeAnalogSeries(std::string const & key) {
    auto it = analog_series_map.find(key);
    if (it != analog_series_map.end()) {
        analog_series_map.erase(it);
        updateSeriesCounts();
        return true;
    }
    return false;
}

bool PlottingManager::removeDigitalEventSeries(std::string const & key) {
    auto it = digital_event_series_map.find(key);
    if (it != digital_event_series_map.end()) {
        digital_event_series_map.erase(it);
        updateSeriesCounts();
        return true;
    }
    return false;
}

bool PlottingManager::removeDigitalIntervalSeries(std::string const & key) {
    auto it = digital_interval_series_map.find(key);
    if (it != digital_interval_series_map.end()) {
        digital_interval_series_map.erase(it);
        updateSeriesCounts();
        return true;
    }
    return false;
}

void PlottingManager::clearAllSeries() {
    analog_series_map.clear();
    digital_event_series_map.clear();
    digital_interval_series_map.clear();
    total_analog_series = 0;
    total_event_series = 0;
    total_digital_series = 0;
}

PlottingManager::AnalogSeriesInfo * PlottingManager::getAnalogSeriesInfo(std::string const & key) {
    auto it = analog_series_map.find(key);
    return (it != analog_series_map.end()) ? &it->second : nullptr;
}

PlottingManager::DigitalEventSeriesInfo * PlottingManager::getDigitalEventSeriesInfo(std::string const & key) {
    auto it = digital_event_series_map.find(key);
    return (it != digital_event_series_map.end()) ? &it->second : nullptr;
}

PlottingManager::DigitalIntervalSeriesInfo * PlottingManager::getDigitalIntervalSeriesInfo(std::string const & key) {
    auto it = digital_interval_series_map.find(key);
    return (it != digital_interval_series_map.end()) ? &it->second : nullptr;
}

void PlottingManager::setSeriesVisibility(std::string const & key, bool visible) {
    // Check all series types and update visibility
    auto analog_it = analog_series_map.find(key);
    if (analog_it != analog_series_map.end()) {
        analog_it->second.visible = visible;
        return;
    }
    
    auto event_it = digital_event_series_map.find(key);
    if (event_it != digital_event_series_map.end()) {
        event_it->second.visible = visible;
        return;
    }
    
    auto interval_it = digital_interval_series_map.find(key);
    if (interval_it != digital_interval_series_map.end()) {
        interval_it->second.visible = visible;
        return;
    }
}

std::vector<std::string> PlottingManager::getVisibleAnalogSeriesKeys() const {
    std::vector<std::string> visible_keys;
    for (auto const & [key, info] : analog_series_map) {
        if (info.visible) {
            visible_keys.push_back(key);
        }
    }
    return visible_keys;
}

std::vector<std::string> PlottingManager::getVisibleDigitalEventSeriesKeys() const {
    std::vector<std::string> visible_keys;
    for (auto const & [key, info] : digital_event_series_map) {
        if (info.visible) {
            visible_keys.push_back(key);
        }
    }
    return visible_keys;
}

std::vector<std::string> PlottingManager::getVisibleDigitalIntervalSeriesKeys() const {
    std::vector<std::string> visible_keys;
    for (auto const & [key, info] : digital_interval_series_map) {
        if (info.visible) {
            visible_keys.push_back(key);
        }
    }
    return visible_keys;
}

void PlottingManager::setGlobalZoom(float zoom) {
    global_zoom = std::max(0.01f, zoom); // Prevent negative or zero zoom
}

float PlottingManager::getGlobalZoom() const {
    return global_zoom;
}

void PlottingManager::setGlobalVerticalScale(float scale) {
    global_vertical_scale = std::max(0.01f, scale); // Prevent negative or zero scale
}

float PlottingManager::getGlobalVerticalScale() const {
    return global_vertical_scale;
}

void PlottingManager::calculateDigitalIntervalSeriesAllocation(int series_index,
                                                               float & allocated_center,
                                                               float & allocated_height) const {

    static_cast<void>(series_index);

    // Digital intervals use the full canvas height by default
    allocated_center = (viewport_y_min + viewport_y_max) * 0.5f;// Center of viewport
    allocated_height = viewport_y_max - viewport_y_min;         // Full viewport height
}

void PlottingManager::calculateDigitalEventSeriesAllocation(int series_index,
                                                            float & allocated_center,
                                                            float & allocated_height) const {
    // For now, assume stacked mode allocation (like analog series)
    // This can be extended to support different plotting modes
    if (total_event_series <= 0) {
        allocated_center = 0.0f;
        allocated_height = 2.0f;// Full canvas height
        return;
    }

    // Calculate allocated height for this series
    allocated_height = (viewport_y_max - viewport_y_min) / static_cast<float>(total_event_series);

    // Calculate center Y coordinate
    // Series are stacked from top to bottom starting at viewport_y_min
    allocated_center = viewport_y_min + allocated_height * (static_cast<float>(series_index) + 0.5f);
}

void PlottingManager::calculateGlobalStackedAllocation(int analog_series_index,
                                                       int event_series_index,
                                                       int total_stackable_series,
                                                       float & allocated_center,
                                                       float & allocated_height) const {
    if (total_stackable_series <= 0) {
        allocated_center = 0.0f;
        allocated_height = 2.0f;// Full canvas height
        return;
    }

    // Calculate allocated height for each series (equal division)
    allocated_height = (viewport_y_max - viewport_y_min) / static_cast<float>(total_stackable_series);

    // Determine the global index for this series
    int global_series_index;
    if (analog_series_index >= 0) {
        // This is an analog series
        global_series_index = analog_series_index;
    } else {
        // This is a digital event series, comes after all analog series
        global_series_index = total_analog_series + event_series_index;
    }

    // Calculate center Y coordinate based on global stacking order
    allocated_center = viewport_y_min + allocated_height * (static_cast<float>(global_series_index) + 0.5f);
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

void PlottingManager::updateSeriesCounts() {
    // Count only visible series
    total_analog_series = static_cast<int>(std::count_if(
        analog_series_map.begin(), analog_series_map.end(),
        [](auto const & pair) { return pair.second.visible; }));
    
    total_event_series = static_cast<int>(std::count_if(
        digital_event_series_map.begin(), digital_event_series_map.end(),
        [](auto const & pair) { return pair.second.visible; }));
    
    total_digital_series = static_cast<int>(std::count_if(
        digital_interval_series_map.begin(), digital_interval_series_map.end(),
        [](auto const & pair) { return pair.second.visible; }));
}

bool PlottingManager::_extractGroupAndChannel(std::string const & key, std::string & group, int & channel_id) {
    channel_id = -1;
    group.clear();
    auto const pos = key.rfind('_');
    if (pos == std::string::npos || pos + 1 >= key.size()) {
        return false;
    }
    group = key.substr(0, pos);
    try {
        int const parsed = std::stoi(key.substr(pos + 1));
        channel_id = parsed > 0 ? parsed - 1 : parsed;
    } catch (...) {
        channel_id = -1;
        return false;
    }
    return true;
}

void PlottingManager::loadAnalogSpikeSorterConfiguration(std::string const & group_name,
                                                        std::vector<AnalogGroupChannelPosition> const & positions) {
    _analog_group_configs[group_name] = positions;
}

void PlottingManager::clearAnalogGroupConfiguration(std::string const & group_name) {
    _analog_group_configs.erase(group_name);
}

std::vector<std::string> PlottingManager::_orderedVisibleAnalogKeysByConfig() const {
    // Group visible analog series by group_name
    struct Item { std::string key; std::string group; int channel; };
    std::vector<Item> items;
    items.reserve(analog_series_map.size());
    for (auto const & [key, info] : analog_series_map) {
        if (!info.visible) continue;
        Item it{key, info.group_name, info.channel_id};
        items.push_back(std::move(it));
    }

    // Sort with configuration: by group; within group, if config present, by descending y; else by channel id
    std::stable_sort(items.begin(), items.end(), [&](Item const & a, Item const & b) {
        if (a.group != b.group) return a.group < b.group;
        auto cfg_it = _analog_group_configs.find(a.group);
        if (cfg_it == _analog_group_configs.end()) {
            return a.channel < b.channel;
        }
        auto const & cfg = cfg_it->second;
        auto find_y = [&](int ch) {
            for (auto const & p : cfg) if (p.channel_id == ch) return p.y;
            return 0.0f; };
        float ya = find_y(a.channel);
        float yb = find_y(b.channel);
        if (ya == yb) return a.channel < b.channel;
        return ya < yb; // ascending by y so larger y get larger index (top)
    });

    std::vector<std::string> keys;
    keys.reserve(items.size());
    for (auto const & it : items) keys.push_back(it.key);
    return keys;
}

bool PlottingManager::getAnalogSeriesAllocationForKey(std::string const & key,
                                                     float & allocated_center,
                                                     float & allocated_height) const {
    // Build ordered visible list considering configuration
    auto ordered = _orderedVisibleAnalogKeysByConfig();
    if (ordered.empty()) {
        allocated_center = 0.0f;
        allocated_height = viewport_y_max - viewport_y_min;
        return false;
    }
    // Find index of key among visible
    int index = -1;
    for (size_t i = 0; i < ordered.size(); ++i) {
        if (ordered[i] == key) { index = static_cast<int>(i); break; }
    }
    if (index < 0) {
        return false;
    }
    // Use same formula as calculateAnalogSeriesAllocation but with index in ordered list and count = ordered.size()
    float const total = static_cast<float>(ordered.size());
    allocated_height = (viewport_y_max - viewport_y_min) / total;
    allocated_center = viewport_y_min + allocated_height * (static_cast<float>(index) + 0.5f);
    return true;
}

std::string PlottingManager::generateDefaultColor(int series_index) const {
    // Generate a nice color palette for series
    // Use HSV color space with fixed saturation and value, varying hue
    constexpr int num_colors = 12;
    float hue = (static_cast<float>(series_index % num_colors) / static_cast<float>(num_colors)) * 360.0f;
    
    // Convert HSV to RGB
    constexpr float saturation = 0.8f;
    constexpr float value = 0.9f;
    
    float c = value * saturation;
    float x = c * (1.0f - std::abs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
    float m = value - c;
    
    float r, g, b;
    if (hue >= 0 && hue < 60) {
        r = c; g = x; b = 0;
    } else if (hue >= 60 && hue < 120) {
        r = x; g = c; b = 0;
    } else if (hue >= 120 && hue < 180) {
        r = 0; g = c; b = x;
    } else if (hue >= 180 && hue < 240) {
        r = 0; g = x; b = c;
    } else if (hue >= 240 && hue < 300) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }
    
    // Convert to 0-255 range and create hex string
    int red = static_cast<int>((r + m) * 255);
    int green = static_cast<int>((g + m) * 255);
    int blue = static_cast<int>((b + m) * 255);
    
    std::ostringstream ss;
    ss << "#" << std::hex << std::setfill('0') 
       << std::setw(2) << red 
       << std::setw(2) << green 
       << std::setw(2) << blue;
    
    return ss.str();
}
