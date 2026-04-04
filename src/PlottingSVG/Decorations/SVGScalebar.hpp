#ifndef PLOTTINGSVG_DECORATIONS_SVGSCALEBAR_HPP
#define PLOTTINGSVG_DECORATIONS_SVGSCALEBAR_HPP

/**
 * @file SVGScalebar.hpp
 * @brief Horizontal time scale bar: bar, end ticks, and numeric label (canvas pixel space).
 */

#include "PlottingSVG/Decorations/SVGDecoration.hpp"

#include <string>

namespace PlottingSVG {

/// Corner placement for the scalebar group (horizontal bar + ticks + label).
enum class ScalebarCorner {
    BottomRight,
    BottomLeft,
    TopRight,
    TopLeft,
};

/// SVG decoration that draws a scalebar whose length in pixels matches `scalebar_length` time units
/// over the given visible time range (same mapping as legacy DataViewer export).
class SVGScalebar final : public SVGDecoration {
public:
    /// @param scalebar_length  Bar extent in **time units** (same as `createScalebarSVG`).
    /// @param time_range_start  Visible interval start (time).
    /// @param time_range_end    Visible interval end (time); must differ from start.
    explicit SVGScalebar(int scalebar_length, float time_range_start, float time_range_end);

    void setCorner(ScalebarCorner corner);

    /// @brief Inset from canvas edges used when anchoring the scalebar (pixels).
    void setPadding(int pixels);

    void setBarThickness(int pixels);

    void setTickHeight(int pixels);

    void setLabelFontSize(int pixels);

    /// @brief Stroke color for bar and ticks (e.g. `#000000`).
    void setStrokeColor(std::string hex);

    /// @brief Fill color for the numeric label (e.g. `#000000`).
    void setLabelFillColor(std::string hex);

    [[nodiscard]] std::vector<std::string>
    render(int canvas_width,// NOLINT(bugprone-easily-swappable-parameters)
           int canvas_height) const override;

private:
    int _scalebar_length{};
    float _time_start{};
    float _time_end{};
    ScalebarCorner _corner{ScalebarCorner::BottomRight};
    int _padding{50};
    int _bar_thickness{4};
    int _tick_height{8};
    int _label_font_size{14};
    std::string _stroke_color{"#000000"};
    std::string _label_fill_color{"#000000"};
};

}// namespace PlottingSVG

#endif// PLOTTINGSVG_DECORATIONS_SVGSCALEBAR_HPP
