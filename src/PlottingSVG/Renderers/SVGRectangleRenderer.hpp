#ifndef PLOTTINGSVG_RENDERERS_SVGRECTANGLERENDERER_HPP
#define PLOTTINGSVG_RENDERERS_SVGRECTANGLERENDERER_HPP

/**
 * @file SVGRectangleRenderer.hpp
 * @brief RenderableRectangleBatch to SVG `<rect>` elements (stub; roadmap 1.3).
 */

#include "CorePlotting/SceneGraph/RenderablePrimitives.hpp"

#include <glm/mat4x4.hpp>

#include <string>
#include <vector>

namespace PlottingSVG {

class SVGRectangleRenderer {
public:
    [[nodiscard]] static std::vector<std::string>
    render(CorePlotting::RenderableRectangleBatch const & batch,
           glm::mat4 const & view,
           glm::mat4 const & projection,
           int canvas_width,
           int canvas_height);
};

}// namespace PlottingSVG

#endif// PLOTTINGSVG_RENDERERS_SVGRECTANGLERENDERER_HPP
