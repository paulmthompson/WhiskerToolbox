#ifndef PLOTTINGSVG_DECORATIONS_SVGAXISRENDERER_HPP
#define PLOTTINGSVG_DECORATIONS_SVGAXISRENDERER_HPP

/**
 * @file SVGAxisRenderer.hpp
 * @brief Linear axis spine, ticks, numeric tick labels, and title in canvas pixel space.
 */

#include "PlottingSVG/Decorations/SVGDecoration.hpp"

#include <string>

namespace PlottingSVG {

/**
 * @brief Which edge of the inner plot box the axis spine follows.
 */
enum class AxisPosition {
    Left,
    Bottom,
};

/**
 * @brief Renders a simple linear axis (uniform tick spacing) as SVG fragments in pixel space.
 *
 * Data coordinates from `range_min` through `range_max` map linearly to the inner rectangle
 * inset by `margin` from the canvas border. Tick positions are multiples of `tick_interval`
 * starting at the first tick at or above `range_min`.
 */
class SVGAxisRenderer final : public SVGDecoration {
public:
    /**
     * @brief Construct an axis decoration with fixed domain, tick step, and title text.
     *
     * @param range_min      Data value at the left (vertical axis) or bottom (horizontal axis) end.
     * @param range_max      Data value at the opposite end; must exceed `range_min` for `render()` to
     *                       emit geometry when the canvas and margins are valid.
     * @param tick_interval  Distance between tick marks in data units; if `<= 0`, only the spine
     *                       (and optional title) are drawn.
     * @param axis_title     Shown as the axis title when non-empty; `&`, `<`, `>` are escaped for SVG text.
     *
     * @pre `range_max > range_min` and both endpoints are finite if callers expect non-empty output from
     *      `render()` under valid canvas and margin settings (enforcement: none) [IMPORTANT]
     * @pre `tick_interval` is finite whenever tick marks are desired (`tick_interval > 0`) (enforcement: none)
     *      [LOW]
     *
     */
    SVGAxisRenderer(float range_min, float range_max, float tick_interval, std::string axis_title);

    /**
     * @brief Select bottom (horizontal) or left (vertical) axis layout.
     *
     * @param position Edge used for the spine and tick direction.
     *
     */
    void setPosition(AxisPosition position);

    /**
     * @brief Set the inset from each canvas edge to the inner plot box (pixels).
     *
     * @param pixels Margin on all sides; used for spine placement and tick mapping.
     *
     * @pre `pixels >= 0` so inset arithmetic matches the intended “inner rectangle” model (enforcement: none)
     *      [IMPORTANT]
     *
     */
    void setMargin(int pixels);

    /**
     * @brief Set the font size for tick labels (SVG `font-size`).
     *
     * @param pixels Font size in pixels.
     *
     * @pre `pixels > 0` for readable output (enforcement: none) [LOW]
     *
     */
    void setFontSize(int pixels);

    /**
     * @brief Set the length of tick marks extending from the spine (pixels).
     *
     * @param pixels Tick length; non-positive values invert or collapse ticks visually (enforcement: none)
     *        [LOW]
     *
     */
    void setTickLength(int pixels);

    /**
     * @brief Set the stroke color for the spine and tick segments (SVG `stroke` attribute).
     *
     * @param stroke_hex Color string (e.g. `#000000`); invalid SVG paint values produce invalid SVG
     *        fragments (enforcement: none) [LOW]
     *
     */
    void setAxisColor(std::string stroke_hex);

    /**
     * @brief Set the fill color for tick labels and title text (SVG `fill` attribute).
     *
     * @param fill_hex Color string (e.g. `#000000`); invalid SVG paint values produce invalid SVG
     *        fragments (enforcement: none) [LOW]
     */
    void setLabelColor(std::string fill_hex);

    /**
     * @brief Emit `<line>` and `<text>` elements for the configured axis.
     *
     * @param canvas_width  SVG viewport width in pixels.
     * @param canvas_height SVG viewport height in pixels.
     *
     * @pre `canvas_width > 0` and `canvas_height > 0` are required for any non-empty output (enforcement:
     *      runtime_check)
     * @pre Let `span = range_max - range_min`. `span` must be finite and `span > 0` for spine/ticks to be
     *      emitted (enforcement: runtime_check)
     * @pre `2 * margin < canvas_width` and `2 * margin < canvas_height` so the inner box has positive size
     *      (enforcement: runtime_check)
     * @pre If `tick_interval > 0`, it must be finite so tick positions are well-defined (enforcement: none)
     *      [LOW] — non-finite interval is not guarded and may yield undefined tick placement
     *
     * @post Return value is empty if any runtime-checked precondition above fails.
     * @post Otherwise returns a sequence of SVG element strings (spine; optional ticks and labels; optional
     *       title) with `&`, `<`, `>` escaped in text nodes.
     * @post Does not throw.
     */
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

} // namespace PlottingSVG

#endif // PLOTTINGSVG_DECORATIONS_SVGAXISRENDERER_HPP
