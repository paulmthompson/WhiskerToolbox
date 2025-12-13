#include "LayoutCalculator.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>

void LayoutCalculator::calculateAnalogSeriesAllocation(int series_index,
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

void LayoutCalculator::calculateDigitalIntervalSeriesAllocation(int series_index,
                                                               float & allocated_center,
                                                               float & allocated_height) const {

    static_cast<void>(series_index);

    // Digital intervals use the full canvas height by default
    allocated_center = (viewport_y_min + viewport_y_max) * 0.5f;// Center of viewport
    allocated_height = viewport_y_max - viewport_y_min;         // Full viewport height
}

void LayoutCalculator::setGlobalZoom(float zoom) {
    global_zoom = std::max(0.01f, zoom); // Prevent negative or zero zoom
}

float LayoutCalculator::getGlobalZoom() const {
    return global_zoom;
}

void LayoutCalculator::setGlobalVerticalScale(float scale) {
    global_vertical_scale = std::max(0.01f, scale); // Prevent negative or zero scale
}

float LayoutCalculator::getGlobalVerticalScale() const {
    return global_vertical_scale;
}

void LayoutCalculator::calculateDigitalEventSeriesAllocation(int series_index,
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

void LayoutCalculator::calculateGlobalStackedAllocation(int analog_series_index,
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

void LayoutCalculator::setPanOffset(float pan_offset) {
    vertical_pan_offset = pan_offset;
}

void LayoutCalculator::applyPanDelta(float pan_delta) {
    vertical_pan_offset += pan_delta;
}

float LayoutCalculator::getPanOffset() const {
    return vertical_pan_offset;
}

void LayoutCalculator::resetPan() {
    vertical_pan_offset = 0.0f;
}

void LayoutCalculator::loadAnalogSpikeSorterConfiguration(std::string const & group_name,
                                                        std::vector<AnalogGroupChannelPosition> const & positions) {
    _analog_group_configs[group_name] = positions;
}

void LayoutCalculator::clearAnalogGroupConfiguration(std::string const & group_name) {
    _analog_group_configs.erase(group_name);
}

bool LayoutCalculator::_extractGroupAndChannel(std::string const & key, std::string & group, int & channel_id) {
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

std::vector<std::string> LayoutCalculator::_orderedVisibleAnalogKeysByConfig(std::vector<std::string> const & visible_keys) const {
    // Group visible analog series by group_name
    struct Item { std::string key; std::string group; int channel; };
    std::vector<Item> items;
    items.reserve(visible_keys.size());
    for (auto const & key : visible_keys) {
        std::string group_name;
        int channel_id;
        _extractGroupAndChannel(key, group_name, channel_id);
        Item it{key, group_name, channel_id};
        items.push_back(std::move(it));
    }

    // Sort with configuration: by group; within group, if config present, by ascending y; else by channel id
    std::stable_sort(items.begin(), items.end(), [&](Item const & a, Item const & b) {
        if (a.group != b.group) return a.group < b.group;
        auto cfg_it = _analog_group_configs.find(a.group);
        if (cfg_it == _analog_group_configs.end()) {
            return a.channel < b.channel;
        }
        auto const & cfg = cfg_it->second;
        auto find_y = [&](int ch) {
            for (auto const & p : cfg) if (p.channel_id == ch) return p.y;
            return 0.0f;
        };
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

bool LayoutCalculator::getAnalogSeriesAllocationForKey(std::string const & key,
                                                     std::vector<std::string> const & visible_keys,
                                                     float & allocated_center,
                                                     float & allocated_height) const {
    // Build ordered visible list considering configuration
    auto ordered = _orderedVisibleAnalogKeysByConfig(visible_keys);
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
