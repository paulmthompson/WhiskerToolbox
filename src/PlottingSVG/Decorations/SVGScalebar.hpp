#ifndef PLOTTINGSVG_DECORATIONS_SVGSCALEBAR_HPP
#define PLOTTINGSVG_DECORATIONS_SVGSCALEBAR_HPP

/**
 * @file SVGScalebar.hpp
 * @brief Time scale bar as SVG elements (stub; filled in roadmap 1.6).
 */

#include "PlottingSVG/Decorations/SVGDecoration.hpp"

namespace PlottingSVG {

class SVGScalebar final : public SVGDecoration {
public:
    SVGScalebar() = default;

    [[nodiscard]] std::vector<std::string>
    render(int canvas_width, int canvas_height) const override;

private:
};

}// namespace PlottingSVG

#endif// PLOTTINGSVG_DECORATIONS_SVGSCALEBAR_HPP
