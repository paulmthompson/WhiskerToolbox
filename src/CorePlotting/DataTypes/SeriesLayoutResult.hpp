#ifndef COREPLOTTING_SERIESLAYOUTRESULT_HPP
#define COREPLOTTING_SERIESLAYOUTRESULT_HPP

namespace CorePlotting {

/**
 * @brief Layout output computed by LayoutEngine
 * 
 * Contains the positioning information calculated by the layout system.
 * These values are READ-ONLY outputs from layout calculations, not
 * user-configurable settings.
 * 
 * This struct represents the "where" a series goes, computed by analyzing
 * the viewport and the set of series to display. It feeds into the Model
 * matrix construction during rendering.
 * 
 * Separated from style configuration and cached data per the
 * CorePlotting architecture (see DESIGN.md, ROADMAP.md Phase 0).
 */
struct SeriesLayoutResult {
    float allocated_y_center{0.0f}; ///< Y-coordinate center allocated by layout engine
    float allocated_height{1.0f};   ///< Height allocated by layout engine in world space

    /**
     * @brief Construct with default layout (single full-height series)
     */
    SeriesLayoutResult() = default;

    /**
     * @brief Construct with specific layout values
     * @param y_center Y-coordinate center in world space
     * @param height Allocated height in world space
     */
    SeriesLayoutResult(float y_center, float height)
        : allocated_y_center(y_center), allocated_height(height) {}
};

} // namespace CorePlotting

#endif // COREPLOTTING_SERIESLAYOUTRESULT_HPP
