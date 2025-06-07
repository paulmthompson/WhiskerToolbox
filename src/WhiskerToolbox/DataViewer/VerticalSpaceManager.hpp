#ifndef VERTICALSPACEMANAGER_HPP
#define VERTICALSPACEMANAGER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

/**
 * @brief Data type enumeration for vertical space management
 */
enum class DataSeriesType {
    Analog,          ///< Analog time series data
    DigitalEvent,    ///< Digital event series data  
    DigitalInterval  ///< Digital interval series data
};

/**
 * @brief Positioning parameters for a data series
 */
struct SeriesPosition {
    float y_offset;           ///< Vertical offset from canvas center (normalized coordinates)
    float scale_factor;       ///< Scaling factor for the series data
    float allocated_height;   ///< Height allocated to this series (normalized coordinates)
    int display_order;        ///< Order in which series should be displayed (0 = top)
};

/**
 * @brief Configuration parameters for data type groups
 */
struct DataTypeConfig {
    float min_height_per_series = 0.01f;   ///< Minimum height per series (normalized)
    float max_height_per_series = 0.5f;    ///< Maximum height per series (normalized) 
    float inter_series_spacing = 0.005f;   ///< Spacing between series of same type (normalized)
    float margin_factor = 0.1f;            ///< Margin factor (fraction of allocated space)
};

/**
 * @brief Manages vertical space allocation for multi-type data visualization
 * 
 * The VerticalSpaceManager coordinates vertical positioning and scaling across
 * different data types (analog, digital events, intervals) to prevent overlap
 * and optimize display space utilization. It maintains a common "vertical space
 * budget" that all data types draw from.
 * 
 * Key features:
 * - Order-preserving: New data is positioned below existing data
 * - Auto-redistribution: Adding new series triggers recalculation for optimal spacing
 * - Type-aware: Different data types have appropriate spacing characteristics
 * - Canvas-independent: Uses normalized coordinates for portability
 * 
 * @note This class is designed to be independent of Qt/OpenGL for easy unit testing
 */
class VerticalSpaceManager {
public:
    /**
     * @brief Constructor with default canvas configuration
     * 
     * @param canvas_height_pixels Physical canvas height in pixels (for pixel-based calculations)
     * @param total_normalized_height Total available normalized height (typically 2.0 for -1.0 to +1.0 range)
     */
    explicit VerticalSpaceManager(int canvas_height_pixels = 400, float total_normalized_height = 2.0f);

    /**
     * @brief Add a new data series to the space management system
     * 
     * The series will be positioned below all existing data and the entire layout
     * will be recalculated to accommodate the new addition.
     * 
     * @param series_key Unique identifier for the data series
     * @param data_type Type of data series (analog, digital event, digital interval)
     * @return SeriesPosition containing positioning parameters for rendering
     * 
     * @note If the series already exists, its position will be updated and returned
     */
    SeriesPosition addSeries(std::string const & series_key, DataSeriesType data_type);

    /**
     * @brief Remove a data series from the space management system
     * 
     * Removing a series triggers recalculation of remaining series positions
     * to optimize space utilization.
     * 
     * @param series_key Unique identifier for the data series to remove
     * @return true if the series was found and removed, false otherwise
     */
    bool removeSeries(std::string const & series_key);

    /**
     * @brief Get current position parameters for a specific series
     * 
     * @param series_key Unique identifier for the data series
     * @return Optional SeriesPosition if series exists, nullopt otherwise
     */
    std::optional<SeriesPosition> getSeriesPosition(std::string const & series_key) const;

    /**
     * @brief Recalculate all series positions with optimal spacing
     * 
     * This function redistributes vertical space among all registered series
     * to achieve optimal spacing. Useful for implementing "auto-arrange" functionality.
     * 
     * @note Maintains the order in which series were added
     */
    void recalculateAllPositions();

    /**
     * @brief Update canvas dimensions and recalculate positions
     * 
     * @param canvas_height_pixels New physical canvas height in pixels
     * @param total_normalized_height New total normalized height (optional)
     */
    void updateCanvasDimensions(int canvas_height_pixels, 
                               std::optional<float> total_normalized_height = std::nullopt);

    /**
     * @brief Configure spacing parameters for a specific data type
     * 
     * @param data_type Data type to configure
     * @param config Configuration parameters
     */
    void setDataTypeConfig(DataSeriesType data_type, DataTypeConfig const & config);

    /**
     * @brief Get current configuration for a data type
     * 
     * @param data_type Data type to query
     * @return DataTypeConfig for the specified type
     */
    DataTypeConfig getDataTypeConfig(DataSeriesType data_type) const;

    /**
     * @brief Get list of all registered series keys in display order
     * 
     * @return Vector of series keys ordered from top to bottom of display
     */
    std::vector<std::string> getAllSeriesKeys() const;

    /**
     * @brief Get count of series for a specific data type
     * 
     * @param data_type Type of data series to count
     * @return Number of series of the specified type
     */
    int getSeriesCount(DataSeriesType data_type) const;

    /**
     * @brief Get total number of series managed
     * 
     * @return Total number of series
     */
    int getTotalSeriesCount() const;

    /**
     * @brief Clear all series from the manager
     * 
     * Removes all series and resets the internal state.
     */
    void clear();
    
    /**
     * @brief Set global user spacing multiplier
     * 
     * Multiplies all calculated spacings by this factor to allow user control.
     * 
     * @param spacing_multiplier Global spacing scale factor (1.0 = normal, 2.0 = double space, etc.)
     */
    void setUserSpacingMultiplier(float spacing_multiplier);
    
    /**
     * @brief Set global user zoom factor
     * 
     * Applies additional scaling to series heights for zoom functionality.
     * 
     * @param zoom_factor Global zoom scale factor (1.0 = normal, 2.0 = double size, etc.)
     */
    void setUserZoomFactor(float zoom_factor);
    
    /**
     * @brief Get the total height of all positioned series
     * 
     * @return Total height in normalized coordinates needed for all series
     */
    float getTotalContentHeight() const;
    
    /**
     * @brief Print detailed debug information about all series positions
     * 
     * Useful for diagnosing positioning and overlap issues.
     */
    void debugPrintPositions() const;

private:
    struct SeriesInfo {
        std::string key;
        DataSeriesType type;
        SeriesPosition position;
        int add_order;  ///< Order in which series was added (for stable positioning)
    };

    // Canvas properties
    int _canvas_height_pixels;
    float _total_normalized_height;
    
    // Series management
    std::vector<SeriesInfo> _series_list;  ///< Ordered list of series (maintains add order)
    std::unordered_map<std::string, size_t> _series_index_map;  ///< Fast lookup by key
    int _next_add_order;
    
    // Configuration
    std::unordered_map<DataSeriesType, DataTypeConfig> _type_configs;
    
    // User controls
    float _user_spacing_multiplier{1.0f};  ///< Global spacing multiplier for user control
    float _user_zoom_factor{1.0f};         ///< Global zoom factor for user control
    float _total_content_height{0.0f};     ///< Cached total height of all positioned content
    
    /**
     * @brief Calculate optimal positions for all series
     * 
     * This is the core algorithm that distributes vertical space among series
     * based on their types and configuration parameters.
     */
    void _calculateOptimalLayout();
    
    /**
     * @brief Get default configuration for a data type
     * 
     * @param data_type Data type to get defaults for
     * @return Default DataTypeConfig
     */
    DataTypeConfig _getDefaultConfig(DataSeriesType data_type) const;
    
    /**
     * @brief Calculate space requirements for a group of series of the same type
     * 
     * @param series_of_type Vector of series info for a specific type
     * @param config Configuration for this data type
     * @return Required normalized height for this group
     */
    float _calculateGroupHeight(std::vector<SeriesInfo const *> const & series_of_type, 
                               DataTypeConfig const & config) const;
};

#endif // VERTICALSPACEMANAGER_HPP 