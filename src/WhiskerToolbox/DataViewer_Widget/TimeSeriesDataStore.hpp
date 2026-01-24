#ifndef TIMESERIES_DATA_STORE_HPP
#define TIMESERIES_DATA_STORE_HPP

/**
 * @file TimeSeriesDataStore.hpp
 * @brief Centralized storage and management of time series data for DataViewer widget
 * 
 * This class extracts series data storage from OpenGLWidget to provide
 * a cleaner separation of concerns. It handles:
 * - Storage of analog, digital event, and digital interval series
 * - Display options management for each series type
 * - Layout request building for the LayoutEngine
 * - Layout response application to display options
 * - Default color assignment
 * - Series lookup by type
 * 
 * The store emits signals when series are added/removed, allowing the parent
 * widget and other observers to react appropriately.
 * 
 */

#include "AnalogVertexCache.hpp"
#include "CorePlotting/DataTypes/SeriesDataCache.hpp"
#include "CorePlotting/Layout/LayoutEngine.hpp"
#include "CorePlotting/Layout/LayoutTransform.hpp"
#include "SpikeSorterConfigLoader.hpp"

#include <QObject>

#include <array>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;
class SeriesOptionsRegistry;

namespace DataViewer {

/**
 * @brief Data structure holding analog series and its computed/cached state
 * 
 * NOTE: Display options (style, visibility, user_scale_factor, etc.) are stored in
 * DataViewerState via SeriesOptionsRegistry. This struct holds only:
 * - The data series itself
 * - Computed layout transform from LayoutEngine
 * - Cached statistics for efficient rendering
 * - Vertex cache for efficient scrolling
 */
struct AnalogSeriesEntry {
    std::shared_ptr<AnalogTimeSeries> series;
    
    /// Layout transform computed by LayoutEngine (offset, gain)
    CorePlotting::LayoutTransform layout_transform;
    
    /// Cached statistics (mean, std_dev, intrinsic_scale)
    CorePlotting::SeriesDataCache data_cache;

    /// Vertex cache for efficient scrolling (initialized lazily)
    mutable AnalogVertexCache vertex_cache;
};

/**
 * @brief Data structure holding digital event series and its computed state
 * 
 * NOTE: Display options are stored in DataViewerState via SeriesOptionsRegistry.
 */
struct DigitalEventSeriesEntry {
    std::shared_ptr<DigitalEventSeries> series;
    
    /// Layout transform computed by LayoutEngine
    CorePlotting::LayoutTransform layout_transform;
};

/**
 * @brief Data structure holding digital interval series and its computed state
 * 
 * NOTE: Display options are stored in DataViewerState via SeriesOptionsRegistry.
 */
struct DigitalIntervalSeriesEntry {
    std::shared_ptr<DigitalIntervalSeries> series;
    
    /// Layout transform computed by LayoutEngine
    CorePlotting::LayoutTransform layout_transform;
};

/**
 * @brief Series type identifiers for lookup
 */
enum class SeriesType {
    None,
    Analog,
    DigitalEvent,
    DigitalInterval
};

/**
 * @brief Default values and utilities for time series display configuration
 */
namespace DefaultColors {

inline constexpr std::array<char const *, 8> PALETTE = {
        "#0000ff",// Blue
        "#ff0000",// Red
        "#00ff00",// Green
        "#ff00ff",// Magenta
        "#ffff00",// Yellow
        "#00ffff",// Cyan
        "#ffa500",// Orange
        "#800080" // Purple
};

/**
 * @brief Get color from index, returns hash-based color if index exceeds palette size
 * @param index Index of the color to retrieve
 * @return Hex color string
 */
inline std::string getColorForIndex(size_t index) {
    if (index < PALETTE.size()) {
        return PALETTE[index];
    }
    // Generate a pseudo-random color based on index
    unsigned int const hash = static_cast<unsigned int>(index) * 2654435761u;
    int const r = static_cast<int>((hash >> 16) & 0xFF);
    int const g = static_cast<int>((hash >> 8) & 0xFF);
    int const b = static_cast<int>(hash & 0xFF);

    char hex_buffer[8];
    std::snprintf(hex_buffer, sizeof(hex_buffer), "#%02x%02x%02x", r, g, b);
    return std::string(hex_buffer);
}

}// namespace DefaultColors

/**
 * @brief Centralized storage for time series data in the DataViewer widget
 * 
 * Manages storage, addition, removal, and configuration of analog,
 * digital event, and digital interval time series. Provides signals
 * for change notification and integrates with the CorePlotting layout system.
 */
class TimeSeriesDataStore : public QObject {
    Q_OBJECT

public:
    explicit TimeSeriesDataStore(QObject * parent = nullptr);
    ~TimeSeriesDataStore() override = default;

    // ========================================================================
    // Add Series Methods
    // ========================================================================

    /**
     * @brief Add an analog time series
     * 
     * Adds the series with display options. If no color is provided,
     * a default color is assigned based on the current series count.
     * Intrinsic properties (mean, std dev) are computed automatically.
     * 
     * @param key Unique identifier for the series
     * @param series Shared pointer to the analog time series
     * @param color Optional hex color (e.g., "#ff0000"), empty for auto
     */
    void addAnalogSeries(std::string const & key,
                         std::shared_ptr<AnalogTimeSeries> series,
                         std::string const & color = "");

    /**
     * @brief Add a digital event series
     * 
     * @param key Unique identifier for the series
     * @param series Shared pointer to the digital event series
     * @param color Optional hex color, empty for auto
     */
    void addEventSeries(std::string const & key,
                        std::shared_ptr<DigitalEventSeries> series,
                        std::string const & color = "");

    /**
     * @brief Add a digital interval series
     * 
     * @param key Unique identifier for the series
     * @param series Shared pointer to the digital interval series
     * @param color Optional hex color, empty for auto
     */
    void addIntervalSeries(std::string const & key,
                           std::shared_ptr<DigitalIntervalSeries> series,
                           std::string const & color = "");

    // ========================================================================
    // Remove Series Methods
    // ========================================================================

    /**
     * @brief Remove an analog time series
     * @param key Key of the series to remove
     * @return true if series was found and removed
     */
    bool removeAnalogSeries(std::string const & key);

    /**
     * @brief Remove a digital event series
     * @param key Key of the series to remove
     * @return true if series was found and removed
     */
    bool removeEventSeries(std::string const & key);

    /**
     * @brief Remove a digital interval series
     * @param key Key of the series to remove
     * @return true if series was found and removed
     */
    bool removeIntervalSeries(std::string const & key);

    /**
     * @brief Clear all series from the store
     * 
     * Removes all analog, digital event, and digital interval series.
     * Emits cleared() followed by individual seriesRemoved signals.
     */
    void clearAll();

    // ========================================================================
    // Series Accessors
    // ========================================================================

    /**
     * @brief Get the analog series map (const reference)
     */
    [[nodiscard]] std::unordered_map<std::string, AnalogSeriesEntry> const & analogSeries() const;

    /**
     * @brief Get the digital event series map (const reference)
     */
    [[nodiscard]] std::unordered_map<std::string, DigitalEventSeriesEntry> const & eventSeries() const;

    /**
     * @brief Get the digital interval series map (const reference)
     */
    [[nodiscard]] std::unordered_map<std::string, DigitalIntervalSeriesEntry> const & intervalSeries() const;

    /**
     * @brief Get mutable reference to analog series map (for modification)
     */
    [[nodiscard]] std::unordered_map<std::string, AnalogSeriesEntry> & analogSeriesMutable();

    /**
     * @brief Get mutable reference to digital event series map
     */
    [[nodiscard]] std::unordered_map<std::string, DigitalEventSeriesEntry> & eventSeriesMutable();

    /**
     * @brief Get mutable reference to digital interval series map
     */
    [[nodiscard]] std::unordered_map<std::string, DigitalIntervalSeriesEntry> & intervalSeriesMutable();

    // ========================================================================
    // Series Data Cache Accessors
    // ========================================================================

    /**
     * @brief Get analog data cache for a series
     * @param key Series key
     * @return Pointer to data cache, or nullptr if series not found
     */
    [[nodiscard]] CorePlotting::SeriesDataCache * getAnalogDataCache(std::string const & key);

    /**
     * @brief Get analog data cache for a series (const)
     * @param key Series key
     * @return Pointer to data cache, or nullptr if series not found
     */
    [[nodiscard]] CorePlotting::SeriesDataCache const * getAnalogDataCache(std::string const & key) const;

    // ========================================================================
    // Layout System Integration
    // ========================================================================

    /**
     * @brief Set the series options registry for visibility lookups
     * 
     * The data store needs access to the state's series options to determine
     * visibility when building layout requests.
     * 
     * @param registry Pointer to the SeriesOptionsRegistry (non-owning)
     */
    void setSeriesOptionsRegistry(SeriesOptionsRegistry * registry);

    /**
     * @brief Build a layout request from current series state
     * 
     * Constructs a LayoutRequest containing all visible series, ordered
     * according to spike sorter configuration if present.
     * 
     * @param viewport_y_min Minimum Y viewport value
     * @param viewport_y_max Maximum Y viewport value
     * @param spike_sorter_configs Configuration for channel ordering
     * @return LayoutRequest ready for LayoutEngine::compute()
     */
    [[nodiscard]] CorePlotting::LayoutRequest buildLayoutRequest(
            float viewport_y_min,
            float viewport_y_max,
            SpikeSorterConfigMap const & spike_sorter_configs) const;

    /**
     * @brief Apply layout response to display options
     * 
     * Updates each series' display options with the computed layout
     * (allocated_y_center, allocated_height) from the LayoutEngine.
     * 
     * @param response Layout response from LayoutEngine::compute()
     */
    void applyLayoutResponse(CorePlotting::LayoutResponse const & response);

    // ========================================================================
    // Series Lookup
    // ========================================================================

    /**
     * @brief Find the type of series by key
     * 
     * Searches all series maps to determine which type (if any) contains
     * a series with the given key.
     * 
     * @param key Series key to search for
     * @return SeriesType::Analog/DigitalEvent/DigitalInterval or SeriesType::None
     */
    [[nodiscard]] SeriesType findSeriesTypeByKey(std::string const & key) const;

    /**
     * @brief Check if any series are stored
     * @return true if at least one series exists
     */
    [[nodiscard]] bool isEmpty() const;

    /**
     * @brief Get total count of all series
     * @return Sum of analog, event, and interval series counts
     */
    [[nodiscard]] size_t totalSeriesCount() const;

signals:
    /**
     * @brief Emitted when a series is added
     * @param key The key of the added series
     * @param type The type of series ("analog", "event", "interval")
     */
    void seriesAdded(QString const & key, QString const & type);

    /**
     * @brief Emitted when a series is removed
     * @param key The key of the removed series
     */
    void seriesRemoved(QString const & key);

    /**
     * @brief Emitted when all series are cleared
     */
    void cleared();

    /**
     * @brief Emitted when layout needs to be recomputed
     * 
     * This signal is emitted when series are added/removed, indicating
     * that the parent widget should recompute the layout.
     */
    void layoutDirty();

private:
    std::unordered_map<std::string, AnalogSeriesEntry> _analog_series;
    std::unordered_map<std::string, DigitalEventSeriesEntry> _digital_event_series;
    std::unordered_map<std::string, DigitalIntervalSeriesEntry> _digital_interval_series;
    
    /// Non-owning pointer to SeriesOptionsRegistry for visibility lookups
    SeriesOptionsRegistry * _series_options_registry{nullptr};
};

}// namespace DataViewer

#endif// TIMESERIES_DATA_STORE_HPP
