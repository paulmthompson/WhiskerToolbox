#ifndef COREPLOTTING_DATATYPES_LINESTYLEDATA_HPP
#define COREPLOTTING_DATATYPES_LINESTYLEDATA_HPP

/**
 * @file LineStyleData.hpp
 * @brief Serializable line style configuration for line/curve rendering
 *
 * This header defines the canonical line style type used across all plot widgets
 * that render continuous lines or curves (time series, traces, contours, etc.).
 *
 * **Design Goals**:
 * - Plain struct with no Qt dependencies → includable in state data structs
 * - Directly serializable with reflectcpp (simple fields)
 * - Focused subset of SeriesStyle relevant to line appearance
 *
 * **Usage Pattern** (mirrors GlyphStyleData):
 * Include this struct as a member in your widget's StateData:
 * @code
 *   struct MyWidgetStateData {
 *       CorePlotting::LineStyleData line_style;              // single global style
 *       std::map<std::string, CorePlotting::LineStyleData> per_key_styles; // per-series
 *   };
 * @endcode
 *
 * @see LineStyleState for the QObject wrapper with signals
 * @see GlyphStyleData for the marker/point equivalent
 * @see SeriesStyle for the combined line+visibility style
 */

#include <string>

namespace CorePlotting {

/**
 * @brief Serializable line style configuration
 *
 * Contains all user-configurable visual properties for rendering
 * continuous lines and curves. This struct is designed to be embedded
 * directly in plot state data structs for reflectcpp serialization.
 *
 * All fields have sensible defaults so a default-constructed
 * LineStyleData produces a visible blue line.
 */
struct LineStyleData {
    std::string hex_color = "#007bff"; ///< Color as hex string (e.g., "#007bff")
    float thickness = 1.0f;            ///< Line thickness in pixels
    float alpha = 1.0f;                ///< Alpha transparency [0.0, 1.0]
};

} // namespace CorePlotting

#endif // COREPLOTTING_DATATYPES_LINESTYLEDATA_HPP
