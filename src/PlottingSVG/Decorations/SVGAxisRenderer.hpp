#ifndef PLOTTINGSVG_DECORATIONS_SVGAXISRENDERER_HPP
#define PLOTTINGSVG_DECORATIONS_SVGAXISRENDERER_HPP

/**
 * @file SVGAxisRenderer.hpp
 * @brief Linear axis spine, ticks, numeric tick labels, and title in canvas pixel space.
 */

#include "PlottingSVG/Decorations/SVGDecoration.hpp"

#include <string>

namespace PlottingSVG {

/// Which edge of the plot region the axis occupies.
enum class AxisPosition {
    Left,
    Bottom,
};

/// Renders a simple linear axis (uniform tick spacing) for SVG overlays.
class SVGAxisRenderer final : public SVGDecoration {
public:
    /// @param range_min      Data coordinate at the start of the axis (left or bottom end).
    /// @param range_max      Data coordinate at the end of the axis.
    /// @param tick_interval  Spacing between ticks (same units as min/max); must be > 0 for ticks.
    /// @param axis_title     Text drawn as the axis title (minimal XML escaping applied).
    SVGAxisRenderer(float range_min, float range_max, float tick_interval, std::string axis_title);

    void setPosition(AxisPosition position);

    /// @brief Margin from canvas edges to the inner plot box (pixels).
    void setMargin(int pixels);

    void setFontSize(int pixels);

    void setTickLength(int pixels);

    void setAxisColor(std::string stroke_hex);

    void setLabelColor(std::string fill_hex);

    [[nodiscard]] std::vector<std::string>
    render(int canvas_width,// NOLINT(bugprone-easily-swappable-parameters)
           int canvas_height) const override;

private:
    float _range_min{};
    float _range_max{};
    float _tick_interval{};
    std::string _axis_title;
    AxisPosition _position{AxisPosition::Bottom};
    int _margin{48};
    int _font_size{12};
    int _tick_length{6};
    std::string _axis_color{"#000000"};
    std::string _label_color{"#000000"};
};

}// namespace PlottingSVG

#endif// PLOTTINGSVG_DECORATIONS_SVGAXISRENDERER_HPP
