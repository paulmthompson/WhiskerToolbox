#ifndef PLOTTINGSVG_DECORATIONS_SVGAXISRENDERER_HPP
#define PLOTTINGSVG_DECORATIONS_SVGAXISRENDERER_HPP

/**
 * @file SVGAxisRenderer.hpp
 * @brief Axis ticks, labels, and spine as SVG elements (stub; filled in roadmap 1.6).
 */

#include "PlottingSVG/Decorations/SVGDecoration.hpp"

namespace PlottingSVG {

class SVGAxisRenderer final : public SVGDecoration {
public:
    SVGAxisRenderer() = default;

    [[nodiscard]] std::vector<std::string>
    render(int canvas_width, int canvas_height) const override;
};

}// namespace PlottingSVG

#endif// PLOTTINGSVG_DECORATIONS_SVGAXISRENDERER_HPP
