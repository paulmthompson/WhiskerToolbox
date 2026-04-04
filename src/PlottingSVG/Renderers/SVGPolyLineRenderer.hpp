#ifndef PLOTTINGSVG_RENDERERS_SVGPOLYLINERENDERER_HPP
#define PLOTTINGSVG_RENDERERS_SVGPOLYLINERENDERER_HPP

/**
 * @file SVGPolyLineRenderer.hpp
 * @brief RenderablePolyLineBatch to SVG `<polyline>` elements.
 */

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <vector>

namespace PlottingSVG {

class SVGPolyLineRenderer {
public:
    /// @brief Convert one polyline batch to SVG fragments.
    [[nodiscard]] static std::vector<std::string>
    render(CorePlotting::RenderablePolyLineBatch const & batch,
           glm::mat4 const & view,
           glm::mat4 const & projection,
           int canvas_width,
           int canvas_height);
};

}// namespace PlottingSVG

#endif// PLOTTINGSVG_RENDERERS_SVGPOLYLINERENDERER_HPP
