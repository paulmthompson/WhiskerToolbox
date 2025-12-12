#ifndef COREPLOTTING_SERIESSTYLE_HPP
#define COREPLOTTING_SERIESSTYLE_HPP

#include <string>

namespace CorePlotting {

/**
 * @brief Pure configuration for series rendering style
 * 
 * Contains only visual properties that affect how a series is drawn.
 * These are user-configurable settings that don't change based on
 * layout calculations or data statistics.
 * 
 * Separated from layout results and cached calculations per the
 * CorePlotting architecture (see DESIGN.md, ROADMAP.md Phase 0).
 */
struct SeriesStyle {
    std::string hex_color{"#007bff"}; ///< Color in hex format (e.g., "#007bff")
    float alpha{1.0f};                ///< Alpha transparency [0.0, 1.0]
    int line_thickness{1};            ///< Line thickness in pixels
    bool is_visible{true};            ///< Visibility flag

    /**
     * @brief Construct with default style
     */
    SeriesStyle() = default;

    /**
     * @brief Construct with custom color
     * @param color Hex color string
     * @param alpha_value Alpha transparency (default: 1.0)
     */
    SeriesStyle(std::string color, float alpha_value = 1.0f)
        : hex_color(std::move(color)), alpha(alpha_value) {}
};

} // namespace CorePlotting

#endif // COREPLOTTING_SERIESSTYLE_HPP
