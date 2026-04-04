#ifndef PLOTTINGSVG_SVGEXPORT_HPP
#define PLOTTINGSVG_SVGEXPORT_HPP

/**
 * @file SVGExport.hpp
 * @brief Legacy flat SVG document assembly for DataViewer-style export (scene + scalebar).
 */

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <string>
#include <vector>

namespace PlottingSVG {

/// @brief Canvas and background for `buildSVGDocument` / scalebar layout.
struct SVGExportParams {
    int canvas_width{1920};
    int canvas_height{1080};
    std::string background_color{"#1E1E1E"};
};

/// @brief Visible time interval for scalebar length ↔ pixel mapping.
struct ScalebarTimeRange {
    float start{};
    float end{};
};

/// @brief Render all batches in scene order (rectangles, polylines, glyphs).
[[nodiscard]] std::vector<std::string> renderSceneToSVG(
        CorePlotting::RenderableScene const & scene,
        SVGExportParams const & params);

/// @brief Full SVG 1.1 document with XML header, background rect, and scene elements.
[[nodiscard]] std::string buildSVGDocument(
        CorePlotting::RenderableScene const & scene,
        SVGExportParams const & params);

/// @brief Bottom-right scalebar: bar, ticks, and numeric label (time units).
[[nodiscard]] std::vector<std::string> createScalebarSVG(
        int scalebar_length,
        ScalebarTimeRange time_range,
        SVGExportParams const & params);

}// namespace PlottingSVG

#endif// PLOTTINGSVG_SVGEXPORT_HPP
