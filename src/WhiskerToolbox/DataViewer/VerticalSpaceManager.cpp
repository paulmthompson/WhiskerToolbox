#include "VerticalSpaceManager.hpp"

#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <iomanip>

VerticalSpaceManager::VerticalSpaceManager(int canvas_height_pixels, float total_normalized_height)
    : _canvas_height_pixels(canvas_height_pixels),
      _total_normalized_height(total_normalized_height),
      _next_add_order(0) {
    
    // Initialize default configurations for each data type
    _type_configs[DataSeriesType::Analog] = _getDefaultConfig(DataSeriesType::Analog);
    _type_configs[DataSeriesType::DigitalEvent] = _getDefaultConfig(DataSeriesType::DigitalEvent);
    _type_configs[DataSeriesType::DigitalInterval] = _getDefaultConfig(DataSeriesType::DigitalInterval);
}

SeriesPosition VerticalSpaceManager::addSeries(std::string const & series_key, DataSeriesType data_type) {
    // Check if series already exists
    auto existing_it = _series_index_map.find(series_key);
    if (existing_it != _series_index_map.end()) {
        // Update existing series type if different and recalculate
        auto & existing_series = _series_list[existing_it->second];
        if (existing_series.type != data_type) {
            existing_series.type = data_type;
            _calculateOptimalLayout();
        }
        return existing_series.position;
    }
    
    // Add new series
    SeriesInfo new_series;
    new_series.key = series_key;
    new_series.type = data_type;
    new_series.add_order = _next_add_order++;
    new_series.position = {}; // Will be calculated
    
    // Add to data structures
    size_t const new_index = _series_list.size();
    _series_list.push_back(new_series);
    _series_index_map[series_key] = new_index;
    
    // Recalculate layout with new series
    _calculateOptimalLayout();
    
    std::cout << "VerticalSpaceManager: Added series '" << series_key 
              << "' of type " << static_cast<int>(data_type) 
              << " at position y_offset=" << _series_list[new_index].position.y_offset << std::endl;
    
    return _series_list[new_index].position;
}

bool VerticalSpaceManager::removeSeries(std::string const & series_key) {
    auto it = _series_index_map.find(series_key);
    if (it == _series_index_map.end()) {
        return false;
    }
    
    size_t const index_to_remove = it->second;
    
    // Remove from vector (this will invalidate indices)
    _series_list.erase(_series_list.begin() + static_cast<int64_t>(index_to_remove));
    
    // Rebuild the index map
    _series_index_map.clear();
    for (size_t i = 0; i < _series_list.size(); ++i) {
        _series_index_map[_series_list[i].key] = i;
    }
    
    // Recalculate layout
    _calculateOptimalLayout();
    
    std::cout << "VerticalSpaceManager: Removed series '" << series_key << "'" << std::endl;
    
    return true;
}

std::optional<SeriesPosition> VerticalSpaceManager::getSeriesPosition(std::string const & series_key) const {
    auto it = _series_index_map.find(series_key);
    if (it == _series_index_map.end()) {
        return std::nullopt;
    }
    
    return _series_list[it->second].position;
}

void VerticalSpaceManager::recalculateAllPositions() {
    _calculateOptimalLayout();
    std::cout << "VerticalSpaceManager: Recalculated all positions for " 
              << _series_list.size() << " series" << std::endl;
}

void VerticalSpaceManager::updateCanvasDimensions(int canvas_height_pixels, 
                                                 std::optional<float> total_normalized_height) {
    _canvas_height_pixels = canvas_height_pixels;
    if (total_normalized_height.has_value()) {
        _total_normalized_height = total_normalized_height.value();
    }
    
    // Recalculate positions with new dimensions
    _calculateOptimalLayout();
    
    std::cout << "VerticalSpaceManager: Updated canvas dimensions to " 
              << canvas_height_pixels << " pixels, " 
              << _total_normalized_height << " normalized" << std::endl;
}

void VerticalSpaceManager::setDataTypeConfig(DataSeriesType data_type, DataTypeConfig const & config) {
    _type_configs[data_type] = config;
    _calculateOptimalLayout(); // Recalculate with new config
}

DataTypeConfig VerticalSpaceManager::getDataTypeConfig(DataSeriesType data_type) const {
    auto it = _type_configs.find(data_type);
    if (it != _type_configs.end()) {
        return it->second;
    }
    return _getDefaultConfig(data_type);
}

std::vector<std::string> VerticalSpaceManager::getAllSeriesKeys() const {
    std::vector<std::string> keys;
    keys.reserve(_series_list.size());
    
    // Sort by display order to maintain consistent ordering
    std::vector<SeriesInfo const *> sorted_series;
    sorted_series.reserve(_series_list.size());
    for (auto const & series : _series_list) {
        sorted_series.push_back(&series);
    }
    
    std::sort(sorted_series.begin(), sorted_series.end(),
             [](SeriesInfo const * a, SeriesInfo const * b) {
                 return a->position.display_order < b->position.display_order;
             });
    
    for (auto const * series : sorted_series) {
        keys.push_back(series->key);
    }
    
    return keys;
}

int VerticalSpaceManager::getSeriesCount(DataSeriesType data_type) const {
    return static_cast<int>(std::count_if(_series_list.begin(), _series_list.end(),
                                         [data_type](SeriesInfo const & series) {
                                             return series.type == data_type;
                                         }));
}

int VerticalSpaceManager::getTotalSeriesCount() const {
    return static_cast<int>(_series_list.size());
}

void VerticalSpaceManager::clear() {
    _series_list.clear();
    _series_index_map.clear();
    _next_add_order = 0;
    _total_content_height = 0.0f;
    std::cout << "VerticalSpaceManager: Cleared all series" << std::endl;
}

void VerticalSpaceManager::setUserSpacingMultiplier(float spacing_multiplier) {
    _user_spacing_multiplier = std::max(0.1f, spacing_multiplier); // Minimum 10% spacing
    _calculateOptimalLayout();
    std::cout << "VerticalSpaceManager: Set user spacing multiplier to " << _user_spacing_multiplier << std::endl;
}

void VerticalSpaceManager::setUserZoomFactor(float zoom_factor) {
    _user_zoom_factor = std::max(0.1f, zoom_factor); // Minimum 10% zoom
    _calculateOptimalLayout();
    std::cout << "VerticalSpaceManager: Set user zoom factor to " << _user_zoom_factor << std::endl;
}

float VerticalSpaceManager::getTotalContentHeight() const {
    return _total_content_height;
}

void VerticalSpaceManager::debugPrintPositions() const {
    std::cout << "=== VerticalSpaceManager Debug Info ===" << std::endl;
    std::cout << "Canvas dimensions: " << _canvas_height_pixels << " pixels, " 
              << _total_normalized_height << " normalized" << std::endl;
    std::cout << "Total series: " << _series_list.size() << std::endl;
    
    if (_series_list.empty()) {
        std::cout << "No series registered." << std::endl;
        return;
    }
    
    // Group by type for organized output
    std::unordered_map<DataSeriesType, std::vector<SeriesInfo const *>> type_groups;
    for (auto const & series : _series_list) {
        type_groups[series.type].push_back(&series);
    }
    
    auto type_name = [](DataSeriesType type) -> std::string {
        switch (type) {
            case DataSeriesType::Analog: return "Analog";
            case DataSeriesType::DigitalEvent: return "DigitalEvent"; 
            case DataSeriesType::DigitalInterval: return "DigitalInterval";
            default: return "Unknown";
        }
    };
    
    for (auto const & [type, group] : type_groups) {
        std::cout << "\n" << type_name(type) << " series (" << group.size() << "):" << std::endl;
        
        // Sort by display order
        std::vector<SeriesInfo const *> sorted_group = group;
        std::sort(sorted_group.begin(), sorted_group.end(),
                 [](SeriesInfo const * a, SeriesInfo const * b) {
                     return a->position.display_order < b->position.display_order;
                 });
        
        for (auto const * series : sorted_group) {
            float const top = series->position.y_offset + (series->position.allocated_height * 0.5f);
            float const bottom = series->position.y_offset - (series->position.allocated_height * 0.5f);
            
            std::cout << "  " << series->key 
                      << " | order=" << series->position.display_order
                      << " | y_offset=" << std::fixed << std::setprecision(3) << series->position.y_offset
                      << " | height=" << series->position.allocated_height
                      << " | range=[" << bottom << ", " << top << "]"
                      << " | scale=" << series->position.scale_factor << std::endl;
        }
    }
    
    std::cout << "\nView bounds: [-" << (_total_normalized_height * 0.5f) 
              << ", +" << (_total_normalized_height * 0.5f) << "]" << std::endl;
    std::cout << "=======================================" << std::endl;
}

void VerticalSpaceManager::_calculateOptimalLayout() {
    if (_series_list.empty()) {
        return;
    }
    
    // Group series by type while maintaining add order
    std::unordered_map<DataSeriesType, std::vector<SeriesInfo *>> type_groups;
    for (auto & series : _series_list) {
        type_groups[series.type].push_back(&series);
    }
    
    // Sort each group by add order to maintain stable positioning
    for (auto & [type, group] : type_groups) {
        std::sort(group.begin(), group.end(),
                 [](SeriesInfo const * a, SeriesInfo const * b) {
                     return a->add_order < b->add_order;
                 });
    }
    
    // Calculate required height for each type group
    std::vector<std::pair<DataSeriesType, float>> type_heights;
    float total_required_height = 0.0f;
    
    for (auto const & [type, group] : type_groups) {
        auto const & config = _type_configs[type];
        float const group_height = _calculateGroupHeight(
            std::vector<SeriesInfo const *>(group.begin(), group.end()), config);
        type_heights.emplace_back(type, group_height);
        total_required_height += group_height;
    }
    
    // Apply user spacing multiplier to total height
    total_required_height *= _user_spacing_multiplier;
    
    // Available height for content (leave margins)
    float const available_height = _total_normalized_height * 0.8f; // 80% for content, 20% for margins
    
    // Scale factor if we need to compress everything to fit
    // But allow content to extend beyond viewport for panning
    float const scale_factor = 1.0f; // Don't compress - allow panning to access all content
    
    // Store total content height for panning bounds
    _total_content_height = total_required_height;
    
    // Position groups from top to bottom based on add order
    // Start from the top of the content area
    float const top_margin = _total_normalized_height * 0.1f;  // 10% margin at top
    float current_y_offset = (_total_normalized_height * 0.5f) - top_margin; // Start from top of usable area
    int current_display_order = 0;
    
    // Process type groups in the order they were first added
    std::vector<DataSeriesType> type_order;
    for (auto const & series : _series_list) {
        if (std::find(type_order.begin(), type_order.end(), series.type) == type_order.end()) {
            type_order.push_back(series.type);
        }
    }
    
    for (auto const type : type_order) {
        auto & group = type_groups[type];
        auto const & config = _type_configs[type];
        
        // Find the height for this type (after user spacing multiplier)
        auto const type_height_it = std::find_if(type_heights.begin(), type_heights.end(),
                                                [type](auto const & pair) { return pair.first == type; });
        if (type_height_it == type_heights.end()) continue;
        
        float const base_group_height = type_height_it->second;
        float const scaled_group_height = base_group_height * _user_spacing_multiplier * scale_factor;
        float const effective_height = scaled_group_height * (1.0f - config.margin_factor);
        float base_height_per_series = effective_height / static_cast<float>(group.size());
        
        // Apply user zoom factor to individual series heights
        float const height_per_series = base_height_per_series * _user_zoom_factor;
        
        // Position series within this group, starting from the top of the group area
        float const group_top_y = current_y_offset;
        float const group_bottom_y = current_y_offset - scaled_group_height;
        
        std::cout << "  Group " << static_cast<int>(type) << " positioning: top=" << group_top_y 
                  << ", bottom=" << group_bottom_y << ", height=" << scaled_group_height << std::endl;
        
        for (size_t i = 0; i < group.size(); ++i) {
            auto * series = group[i];
            
            // Calculate position within group (from top to bottom)
            float const series_offset_within_group = (static_cast<float>(i) + 0.5f) * (scaled_group_height / static_cast<float>(group.size()));
            float const series_center_y = group_top_y - series_offset_within_group;
            
            series->position.y_offset = series_center_y;
            series->position.allocated_height = height_per_series;
            series->position.display_order = current_display_order++;
            
            // Calculate scale factor based on series height and data type characteristics
            if (type == DataSeriesType::Analog) {
                // For analog data, scale factor affects amplitude scaling
                // Make it inversely proportional to allocated height for consistent visual amplitude
                series->position.scale_factor = 1.0f / std::max(height_per_series, config.min_height_per_series);
            } else {
                // For digital data (events/intervals), scale factor is less critical
                // but can be used for line thickness or visual emphasis
                series->position.scale_factor = std::min(1.0f, height_per_series / config.min_height_per_series);
            }
            
            std::cout << "  " << series->key << " (type=" << static_cast<int>(type) 
                      << ") positioned at y_offset=" << series->position.y_offset 
                      << ", height=" << series->position.allocated_height << std::endl;
        }
        
        // Move to next group position with proper inter-group spacing
        // Add a small gap between groups to prevent overlap
        float const inter_group_spacing = 0.01f; // 1% of total height as gap between groups
        current_y_offset = group_bottom_y - inter_group_spacing;
    }
    
    std::cout << "VerticalSpaceManager: Layout calculated for " << _series_list.size() 
              << " series across " << type_groups.size() << " data types" << std::endl;
    std::cout << "  Total required height: " << total_required_height 
              << ", scale factor: " << scale_factor << std::endl;
    std::cout << "  Final position range: top=" << ((_total_normalized_height * 0.5f) - top_margin) 
              << ", bottom=" << current_y_offset << std::endl;
              
    // Bounds checking to ensure all series are within viewing area
    float const view_top = _total_normalized_height * 0.5f;
    float const view_bottom = -_total_normalized_height * 0.5f;
    
    bool bounds_violation = false;
    for (auto const & series : _series_list) {
        float const series_top = series.position.y_offset + (series.position.allocated_height * 0.5f);
        float const series_bottom = series.position.y_offset - (series.position.allocated_height * 0.5f);
        
        if (series_top > view_top || series_bottom < view_bottom) {
            std::cout << "  WARNING: " << series.key << " extends outside viewing area. "
                      << "Series range: [" << series_bottom << ", " << series_top << "], "
                      << "View range: [" << view_bottom << ", " << view_top << "]" << std::endl;
            bounds_violation = true;
        }
    }
    
    if (!bounds_violation) {
        std::cout << "  ✓ All series positioned within viewing area [" << view_bottom << ", " << view_top << "]" << std::endl;
    }
}

DataTypeConfig VerticalSpaceManager::_getDefaultConfig(DataSeriesType data_type) const {
    DataTypeConfig config;
    
    switch (data_type) {
        case DataSeriesType::Analog:
            config.min_height_per_series = 0.02f;   // Slightly larger for analog signals
            config.max_height_per_series = 0.3f;    // Can be fairly large
            config.inter_series_spacing = 0.01f;    // Reasonable spacing between channels
            config.margin_factor = 0.1f;            // 10% margin
            break;
            
        case DataSeriesType::DigitalEvent:
            config.min_height_per_series = 0.02f;   // Increased from 0.01f to match analog for visibility
            config.max_height_per_series = 0.2f;    // Increased from 0.1f to 0.2f for better visibility
            config.inter_series_spacing = 0.005f;   // Tight spacing for events
            config.margin_factor = 0.05f;           // Smaller margin
            break;
            
        case DataSeriesType::DigitalInterval:
            config.min_height_per_series = 0.015f;  // Intervals need to be visible
            config.max_height_per_series = 0.2f;    // Moderate height
            config.inter_series_spacing = 0.005f;   // Tight spacing
            config.margin_factor = 0.05f;           // Smaller margin
            break;
    }
    
    return config;
}

float VerticalSpaceManager::_calculateGroupHeight(std::vector<SeriesInfo const *> const & series_of_type, 
                                                 DataTypeConfig const & config) const {
    if (series_of_type.empty()) {
        return 0.0f;
    }
    
    size_t const num_series = series_of_type.size();
    
    // Calculate base height needed for all series
    float const base_height = static_cast<float>(num_series) * config.min_height_per_series;
    
    // Add spacing between series
    float const spacing_height = static_cast<float>(num_series - 1) * config.inter_series_spacing;
    
    // Add margins
    float const content_height = base_height + spacing_height;
    float const total_height = content_height / (1.0f - config.margin_factor);
    
    // Improve maximum group height calculation especially for digital events
    // Allow more generous space for large numbers of digital events
    float max_group_height;
    if (num_series >= 20) {
        // For large numbers of series (like 25-30 digital events), be more generous
        max_group_height = static_cast<float>(num_series) * config.max_height_per_series * 1.5f; // 50% overhead instead of 20%
    } else {
        // Standard calculation for smaller numbers
        max_group_height = static_cast<float>(num_series) * config.max_height_per_series * 1.2f; // 20% overhead
    }
    
    return std::min(total_height, max_group_height);
} 