#ifndef ZONE_CONFIG_HPP
#define ZONE_CONFIG_HPP

/**
 * @file ZoneConfig.hpp
 * @brief Serializable zone configuration for runtime layout adjustment
 * 
 * This file defines data structures for persisting and loading zone layouts.
 * The configuration can be saved to JSON and loaded at runtime to adjust
 * the UI layout without restarting the application.
 * 
 * ## JSON Format Example
 * 
 * ```json
 * {
 *   "version": "1.0",
 *   "zone_ratios": {
 *     "left": 0.15,
 *     "center": 0.70,
 *     "right": 0.15,
 *     "bottom": 0.10
 *   },
 *   "zones": {
 *     "left": {
 *       "widgets": [
 *         {"type_id": "DataManager", "title": "Data Manager"},
 *         {"type_id": "GroupManagement", "title": "Groups"}
 *       ],
 *       "splits": []
 *     },
 *     "center": {
 *       "widgets": [
 *         {"type_id": "MediaWidget", "title": "Media Viewer"}
 *       ],
 *       "splits": [
 *         {
 *           "orientation": "vertical",
 *           "ratio": 0.7,
 *           "widgets": [
 *             {"type_id": "DataViewer", "title": "Data Viewer"}
 *           ]
 *         }
 *       ]
 *     }
 *   }
 * }
 * ```
 * 
 * @see ZoneManager for applying the configuration
 * @see ZoneManagerWidget for the configuration UI
 */

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ZoneConfig {

/**
 * @brief Ratios for the main zone areas
 * 
 * All ratios should sum to approximately 1.0 for horizontal zones
 * (left + center + right). Bottom ratio is separate (vertical split).
 */
struct ZoneRatios {
    float left = 0.15f;    ///< Left zone width ratio
    float center = 0.70f;  ///< Center zone width ratio
    float right = 0.15f;   ///< Right zone width ratio
    float bottom = 0.10f;  ///< Bottom zone height ratio
    
    /**
     * @brief Normalize horizontal ratios to sum to 1.0
     */
    void normalizeHorizontal() {
        float sum = left + center + right;
        if (sum > 0.0f) {
            left /= sum;
            center /= sum;
            right /= sum;
        }
    }
    
    /**
     * @brief Validate ratios are within reasonable bounds
     * @return true if all ratios are between 0.0 and 1.0
     */
    [[nodiscard]] bool isValid() const {
        return left >= 0.0f && left <= 1.0f &&
               center >= 0.0f && center <= 1.0f &&
               right >= 0.0f && right <= 1.0f &&
               bottom >= 0.0f && bottom <= 1.0f;
    }
};

/**
 * @brief Configuration for a single widget in a zone
 */
struct WidgetConfig {
    std::string type_id;                    ///< Editor type ID (e.g., "MediaWidget")
    std::optional<std::string> title;       ///< Custom title (uses default if not set)
    std::optional<std::string> instance_id; ///< Specific instance ID (for restoration)
    bool visible = true;                    ///< Whether the widget is visible
    bool closable = true;                   ///< Whether the widget can be closed
};

/**
 * @brief Orientation for splits within a zone
 */
enum class SplitOrientation {
    Horizontal,  ///< Split left-right
    Vertical     ///< Split top-bottom
};

/**
 * @brief Configuration for a split within a zone
 * 
 * Splits allow dividing a zone into sub-areas. Each split can contain
 * additional widgets.
 */
struct SplitConfig {
    SplitOrientation orientation = SplitOrientation::Vertical;
    float ratio = 0.5f;                     ///< Size ratio (0.0-1.0, first part gets this ratio)
    std::vector<WidgetConfig> widgets;      ///< Widgets in the split area
};

/**
 * @brief Configuration for a single zone
 */
struct ZoneContentConfig {
    std::vector<WidgetConfig> widgets;      ///< Widgets as tabs in this zone
    std::vector<SplitConfig> splits;        ///< Sub-splits within the zone
    std::optional<int> active_tab_index;    ///< Which tab is active (0-based)
};

/**
 * @brief Complete zone layout configuration
 */
struct ZoneLayoutConfig {
    std::string version = "1.0";                    ///< Config format version
    ZoneRatios zone_ratios;                         ///< Size ratios for zones
    std::map<std::string, ZoneContentConfig> zones; ///< Content per zone ("left", "center", etc.)
    
    /**
     * @brief Create a default configuration
     */
    [[nodiscard]] static ZoneLayoutConfig createDefault() {
        ZoneLayoutConfig config;
        config.version = "1.0";
        config.zone_ratios = ZoneRatios{};
        
        // Initialize empty zone content for each zone
        config.zones["left"] = ZoneContentConfig{};
        config.zones["center"] = ZoneContentConfig{};
        config.zones["right"] = ZoneContentConfig{};
        config.zones["bottom"] = ZoneContentConfig{};
        
        return config;
    }
    
    /**
     * @brief Validate the configuration
     * @return Error message if invalid, empty string if valid
     */
    [[nodiscard]] std::string validate() const {
        if (!zone_ratios.isValid()) {
            return "Zone ratios must be between 0.0 and 1.0";
        }
        
        // Check that zone names are valid
        for (auto const & [zone_name, content] : zones) {
            if (zone_name != "left" && zone_name != "center" && 
                zone_name != "right" && zone_name != "bottom") {
                return "Invalid zone name: " + zone_name;
            }
            
            // Validate split ratios
            for (auto const & split : content.splits) {
                if (split.ratio < 0.0f || split.ratio > 1.0f) {
                    return "Split ratio must be between 0.0 and 1.0";
                }
            }
        }
        
        return "";  // Valid
    }
};

// ============================================================================
// JSON Serialization Helpers
// ============================================================================

/**
 * @brief Load zone configuration from JSON string
 * @param json_str JSON string to parse
 * @return Result containing config or error
 */
inline rfl::Result<ZoneLayoutConfig> loadFromJson(std::string const & json_str) {
    return rfl::json::read<ZoneLayoutConfig>(json_str);
}

/**
 * @brief Load zone configuration from file
 * @param file_path Path to JSON file
 * @return Result containing config or error
 */
inline rfl::Result<ZoneLayoutConfig> loadFromFile(std::string const & file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return rfl::Error("Failed to open file: " + file_path);
    }
    
    std::string json_str((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
    
    return loadFromJson(json_str);
}

/**
 * @brief Save zone configuration to JSON string
 * @param config Configuration to save
 * @return JSON string
 */
inline std::string saveToJson(ZoneLayoutConfig const & config) {
    return rfl::json::write(config);
}

/**
 * @brief Save zone configuration to file
 * @param config Configuration to save
 * @param file_path Path to output file
 * @return true if saved successfully
 */
inline bool saveToFile(ZoneLayoutConfig const & config, std::string const & file_path) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        return false;
    }
    
    file << saveToJson(config);
    return true;
}

}  // namespace ZoneConfig

#endif  // ZONE_CONFIG_HPP
