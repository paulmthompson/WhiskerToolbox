#ifndef COREPLOTTING_LAYOUT_LAYOUTENGINE_HPP
#define COREPLOTTING_LAYOUT_LAYOUTENGINE_HPP

#include "SeriesLayout.hpp"
#include "../DataTypes/SeriesLayoutResult.hpp"
#include <vector>
#include <string>
#include <memory>

namespace CorePlotting {

/**
 * @brief Type of data series for layout purposes
 */
enum class SeriesType {
    Analog,           ///< Analog time series (continuous signals)
    DigitalEvent,     ///< Digital event series (discrete time points)
    DigitalInterval   ///< Digital interval series (time ranges)
};

/**
 * @brief Metadata for a series to be laid out
 */
struct SeriesInfo {
    std::string id;        ///< Series identifier
    SeriesType type;       ///< Type of series
    bool is_stackable;     ///< Whether series participates in stacking (vs full-canvas)
    
    SeriesInfo(std::string series_id, SeriesType series_type, bool stackable = true)
        : id(std::move(series_id))
        , type(series_type)
        , is_stackable(stackable) {}
};

/**
 * @brief Request for layout computation
 * 
 * Contains all information needed by LayoutEngine to compute positions.
 * Immutable input to layout algorithms.
 */
struct LayoutRequest {
    /// Series to be laid out
    std::vector<SeriesInfo> series;
    
    /// Viewport bounds in NDC (typically -1 to +1)
    float viewport_y_min{-1.0f};
    float viewport_y_max{1.0f};
    
    /// Global scaling factors (from user zoom/pan)
    float global_zoom{1.0f};
    float global_vertical_scale{1.0f};
    float vertical_pan_offset{0.0f};
    
    /**
     * @brief Count series of a specific type
     */
    [[nodiscard]] int countSeriesOfType(SeriesType type) const;
    
    /**
     * @brief Count stackable series (analog + stacked events)
     */
    [[nodiscard]] int countStackableSeries() const;
};

/**
 * @brief Response from layout computation
 * 
 * Contains computed layout for all requested series.
 */
struct LayoutResponse {
    /// Computed layouts (parallel to request.series)
    std::vector<SeriesLayout> layouts;
    
    /**
     * @brief Find layout by series ID
     * @return Pointer to layout, or nullptr if not found
     */
    [[nodiscard]] SeriesLayout const* findLayout(std::string const& series_id) const;
};

/**
 * @brief Strategy interface for layout algorithms
 * 
 * Implements the Strategy pattern to allow different layout algorithms
 * (stacked, row-based, grid, etc.) without changing LayoutEngine API.
 */
class ILayoutStrategy {
public:
    virtual ~ILayoutStrategy() = default;
    
    /**
     * @brief Compute layout for the given request
     * @param request Layout parameters and series list
     * @return Computed layout positions
     */
    virtual LayoutResponse compute(LayoutRequest const& request) const = 0;
};

/**
 * @brief Main layout engine coordinator
 * 
 * Pure function-based layout computation. No data storage, no global state.
 * Delegates to strategy implementations for actual computation.
 * 
 * This is the evolution of LayoutCalculator, following CorePlotting architecture:
 * - Takes LayoutRequest → returns LayoutResponse
 * - No series data storage (that lives in widgets)
 * - No mutable state (pure calculation)
 * - Extensible via Strategy pattern
 */
class LayoutEngine {
public:
    /**
     * @brief Construct with a specific layout strategy
     * @param strategy Layout algorithm to use (engine takes ownership)
     */
    explicit LayoutEngine(std::unique_ptr<ILayoutStrategy> strategy);
    
    /**
     * @brief Compute layout using configured strategy
     * @param request Layout parameters and series list
     * @return Computed layout positions
     */
    [[nodiscard]] LayoutResponse compute(LayoutRequest const& request) const;
    
    /**
     * @brief Change layout strategy
     * @param strategy New layout algorithm (engine takes ownership)
     */
    void setStrategy(std::unique_ptr<ILayoutStrategy> strategy);

private:
    std::unique_ptr<ILayoutStrategy> _strategy;
};

} // namespace CorePlotting

#endif // COREPLOTTING_LAYOUT_LAYOUTENGINE_HPP
