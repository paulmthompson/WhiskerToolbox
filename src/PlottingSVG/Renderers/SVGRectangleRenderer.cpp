/**
 * @file SVGRectangleRenderer.cpp
 * @brief Stub batch renderer; logic migrates from CorePlotting in roadmap 1.3.
 */

#include "PlottingSVG/Renderers/SVGRectangleRenderer.hpp"

namespace PlottingSVG {

std::vector<std::string>
SVGRectangleRenderer::render(CorePlotting::RenderableRectangleBatch const & batch,
                             glm::mat4 const & view,
                             glm::mat4 const & projection,
                             int canvas_width,
                             int canvas_height) {
    (void) batch;
    (void) view;
    (void) projection;
    (void) canvas_width;
    (void) canvas_height;
    return {};
}

}// namespace PlottingSVG
