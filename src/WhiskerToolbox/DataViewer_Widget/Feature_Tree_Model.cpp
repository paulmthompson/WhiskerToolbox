#include "Feature_Tree_Model.hpp"
#include "DataManager/DataManager.hpp"

#include <algorithm>

Feature_Tree_Model::Feature_Tree_Model(QObject * parent)
    : QObject(parent) {
    _initializeDefaultColors();
}

void Feature_Tree_Model::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);
}

std::string Feature_Tree_Model::getFeatureColor(std::string const & feature_key) const {
    // Check if feature has a custom color
    auto it = _feature_colors.find(feature_key);
    if (it != _feature_colors.end()) {
        return it->second;
    }
    
    // Get default color based on data type
    if (_data_manager) {
        auto data_type = _data_manager->getType(feature_key);
        return getDefaultColorForType(data_type);
    }
    
    // Fallback to blue
    return "#0000FF";
}

void Feature_Tree_Model::setFeatureColor(std::string const & feature_key, std::string const & hex_color) {
    _feature_colors[feature_key] = hex_color;
    emit featureColorChanged(feature_key, hex_color);
}

std::string Feature_Tree_Model::getDefaultColorForType(DM_DataType data_type) const {
    auto it = _default_colors.find(data_type);
    if (it != _default_colors.end()) {
        return it->second;
    }
    return "#0000FF"; // Default blue
}

void Feature_Tree_Model::setEnabledFeatures(std::vector<std::string> const & feature_keys) {
    _enabled_features = feature_keys;
}

std::vector<std::string> Feature_Tree_Model::getEnabledFeatures() const {
    return _enabled_features;
}

bool Feature_Tree_Model::isFeatureEnabled(std::string const & feature_key) const {
    return std::find(_enabled_features.begin(), _enabled_features.end(), feature_key) != _enabled_features.end();
}

void Feature_Tree_Model::_initializeDefaultColors() {
    // Set up default colors for each data type
    _default_colors[DM_DataType::Analog] = "#00FF00";           // Green for analog
    _default_colors[DM_DataType::DigitalEvent] = "#FF0000";     // Red for digital events
    _default_colors[DM_DataType::DigitalInterval] = "#FFA500";  // Orange for digital intervals
    _default_colors[DM_DataType::Points] = "#0000FF";          // Blue for points
    _default_colors[DM_DataType::Line] = "#FF00FF";            // Magenta for lines
    _default_colors[DM_DataType::Mask] = "#FFFF00";            // Yellow for masks
    _default_colors[DM_DataType::Video] = "#800080";           // Purple for video
    _default_colors[DM_DataType::Images] = "#008080";          // Teal for images
    _default_colors[DM_DataType::Tensor] = "#808080";          // Gray for tensors
    _default_colors[DM_DataType::Time] = "#000000";            // Black for time
    _default_colors[DM_DataType::Unknown] = "#0000FF";         // Blue for unknown
}
