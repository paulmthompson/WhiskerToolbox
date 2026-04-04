#ifndef PLOTTINGSVG_RENDERERS_SVGGLYPHRENDERER_HPP
#define PLOTTINGSVG_RENDERERS_SVGGLYPHRENDERER_HPP

/**
 * @file SVGGlyphRenderer.hpp
 * @brief RenderableGlyphBatch to SVG primitives (tick, circle, square, cross).
 */

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <vector>

namespace PlottingSVG {

class SVGGlyphRenderer {
public:
    [[nodiscard]] static std::vector<std::string>
    render(CorePlotting::RenderableGlyphBatch const & batch,
           glm::mat4 const & view,
           glm::mat4 const & projection,
           int canvas_width,
           int canvas_height);
};

}// namespace PlottingSVG

#endif// PLOTTINGSVG_RENDERERS_SVGGLYPHRENDERER_HPP
