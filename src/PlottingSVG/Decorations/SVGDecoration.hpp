#ifndef PLOTTINGSVG_DECORATIONS_SVGDECORATION_HPP
#define PLOTTINGSVG_DECORATIONS_SVGDECORATION_HPP

/**
 * @file SVGDecoration.hpp
 * @brief Base interface for SVG-only scene overlays (axes, scalebar, etc.).
 */

#include <string>
#include <vector>

namespace PlottingSVG {

/// Interface for SVG-specific decorations rendered in canvas pixel space.
class SVGDecoration {
public:
    virtual ~SVGDecoration() = default;

    /// @brief Produce SVG element strings for this decoration.
    /// @param canvas_width  SVG width in pixels.
    /// @param canvas_height SVG height in pixels.
    /// @return Zero or more SVG fragments (e.g. `<line>`, `<text>`).
    [[nodiscard]] virtual std::vector<std::string>
    render(int canvas_width, int canvas_height) const = 0;
};

}// namespace PlottingSVG

#endif// PLOTTINGSVG_DECORATIONS_SVGDECORATION_HPP
