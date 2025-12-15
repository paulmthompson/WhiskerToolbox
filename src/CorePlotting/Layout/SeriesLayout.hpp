#ifndef COREPLOTTING_LAYOUT_SERIESLAYOUT_HPP
#define COREPLOTTING_LAYOUT_SERIESLAYOUT_HPP

#include "../DataTypes/SeriesLayoutResult.hpp"
#include <string>

namespace CorePlotting {

/**
 * @brief Extended layout information with metadata
 * 
 * Wraps SeriesLayoutResult with additional context needed for layout coordination.
 * Used internally by LayoutEngine to track series identity and type during
 * layout calculations.
 */
struct SeriesLayout {
    /// Core layout result (y_center, height)
    SeriesLayoutResult result;
    
    /// Series identifier (e.g., key in DataManager)
    std::string series_id;
    
    /// Index in the layout sequence
    int series_index{0};
    
    /**
     * @brief Default constructor
     */
    SeriesLayout() = default;
    
    /**
     * @brief Construct from layout result and metadata
     * @param layout_result Computed layout positioning
     * @param id Series identifier
     * @param index Index in layout sequence
     */
    SeriesLayout(SeriesLayoutResult const& layout_result, 
                 std::string id, 
                 int index)
        : result(layout_result)
        , series_id(std::move(id))
        , series_index(index) {}
};

} // namespace CorePlotting

#endif // COREPLOTTING_LAYOUT_SERIESLAYOUT_HPP
