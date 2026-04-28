#ifndef PLOTTINGSVG_DECORATIONS_SVGSCALEBAR_HPP
#define PLOTTINGSVG_DECORATIONS_SVGSCALEBAR_HPP

/**
 * @file SVGScalebar.hpp
 * @brief Horizontal time scale bar including bar, end ticks, and numeric label (canvas pixel space).
 */

#include "PlottingSVG/Decorations/SVGDecoration.hpp"

#include <string>

namespace PlottingSVG {

/**
 * @brief Corner used to anchor the scalebar (horizontal bar, end ticks, and label) on the canvas.
 */
enum class ScalebarCorner {
    BottomRight,
    BottomLeft,
    TopRight,
    TopLeft,
};

/**
 * @brief Decoration that draws a horizontal scale bar in pixel space.
 *
 * Bar length in pixels is `scalebar_length * (canvas_width / visible_time_span)`, matching
 * `PlottingSVG::createScalebarSVG` / legacy DataViewer export: full canvas width represents
 * `time_range_end - time_range_start`.
 */
class SVGScalebar final : public SVGDecoration {
public:
    /**
     * @brief Build a scalebar for a fixed length in time units over a visible time interval.
     *
     * @param scalebar_length   Extent of the bar in **time units** (label shows this integer as text).
     * @param time_range_start  Start of the visible time window (same units as the plot time axis).
     * @param time_range_end    End of the visible time window; must be strictly after `time_range_start`
     *                          for `render()` to emit elements.
     *
     * @pre `time_range_end > time_range_start` with a finite positive difference if callers expect
     *      non-empty output from `render()` (enforcement: none) [IMPORTANT]
     * @pre `time_range_start` and `time_range_end` are finite values so the stored span behaves as a
     *      normal numeric interval (enforcement: none) [LOW] — non-finite values make `render()` return
     *      an empty vector in typical cases but are not asserted
     * @pre `scalebar_length != 0` if the bar should have non-zero pixel length for typical ranges
     *      (enforcement: none) [LOW]
     *
     * @post Length and time endpoints match the arguments; style and corner use defaults until setters run.
     */
    explicit SVGScalebar(int scalebar_length, float time_range_start, float time_range_end);

    /**
     * @brief Choose which canvas corner anchors the scalebar group.
     *
     * @param corner Anchor corner for layout.
     *
     * @pre None (all enum values are handled).
     */
    void setCorner(ScalebarCorner corner);

    /**
     * @brief Inset from the relevant canvas edges when placing the bar (pixels).
     *
     * @param pixels Padding used by corner layout (distance from edges).
     *
     * @pre `pixels >= 0` so “inset from edge” matches the intended geometry (enforcement: none) [IMPORTANT]
     */
    void setPadding(int pixels);

    /**
     * @brief Stroke width of the main horizontal bar (`stroke-width` on the bar `<line>`).
     *
     * @param pixels Thickness in pixels.
     *
     * @pre `pixels > 0` for a visible bar stroke (enforcement: none) [LOW]
     */
    void setBarThickness(int pixels);

    /**
     * @brief Total extent of each end tick along the axis perpendicular to the bar (pixels).
     *
     * @param pixels Tick height (half above and half below the bar centerline).
     *
     * @pre `pixels >= 0` for non-inverted tick geometry (enforcement: none) [LOW]
     */
    void setTickHeight(int pixels);

    /**
     * @brief Font size for the numeric label (`font-size` on the `<text>` element).
     *
     * @param pixels Size in pixels.
     *
     * @pre `pixels > 0` for readable text (enforcement: none) [LOW]
     */
    void setLabelFontSize(int pixels);

    /**
     * @brief Stroke color for the bar and ticks (SVG `stroke` attribute value).
     *
     * @param hex Color string (e.g. `#000000`); invalid values yield invalid SVG (enforcement: none) [LOW]
     */
    void setStrokeColor(std::string hex);

    /**
     * @brief Fill color for the numeric label (SVG `fill` attribute value).
     *
     * @param hex Color string (e.g. `#000000`); invalid values yield invalid SVG (enforcement: none) [LOW]
     */
    void setLabelFillColor(std::string hex);

    /**
     * @brief Emit the bar, two end ticks, and centered numeric label as SVG element strings.
     *
     * @param canvas_width  SVG viewport width in pixels.
     * @param canvas_height SVG viewport height in pixels.
     *
     * @pre `canvas_width > 0` and `canvas_height > 0` for non-empty output (enforcement: runtime_check)
     * @pre `time_range_end - time_range_start` must be finite and strictly positive for non-empty output
     *      (enforcement: runtime_check)
     * @pre Layout is not clipped by the implementation: very large bar length in pixels or negative padding
     *      may draw outside the viewBox (enforcement: none) [LOW]
     *
     * @post Returns an empty vector if `canvas_width`, `canvas_height`, or the stored time span fail the
     *       runtime checks above.
     * @post Otherwise returns exactly four strings: main bar `<line>`, left tick `<line>`, right tick
     *       `<line>`, and label `<text>` (decimal text is `scalebar_length`).
     * @post Does not throw.
     */
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

} // namespace PlottingSVG

#endif // PLOTTINGSVG_DECORATIONS_SVGSCALEBAR_HPP
