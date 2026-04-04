/**
 * @file SVGScalebar.cpp
 * @brief Scalebar geometry in SVG pixel coordinates.
 */

#include "PlottingSVG/Decorations/SVGScalebar.hpp"

#include <sstream>

namespace PlottingSVG {

namespace {

/// @brief Bar origin (left end of horizontal bar), baseline y, and label anchor y.
struct ScalebarLayout {
    float bar_x{};
    float bar_y{};
    float label_x{};
    float label_y{};
};

struct CanvasPx {
    int width{};
    int height{};
};

struct ScalebarMetrics {
    int padding{};
    int tick_height{};
    int label_font_size{};
};

[[nodiscard]] ScalebarLayout computeLayout(
        ScalebarCorner corner,
        CanvasPx canvas,
        float bar_width_pixels,
        ScalebarMetrics metrics) {
    auto const W = static_cast<float>(canvas.width);
    auto const H = static_cast<float>(canvas.height);
    ScalebarLayout L{};

    switch (corner) {
        case ScalebarCorner::BottomRight:
            L.bar_x = W - bar_width_pixels - static_cast<float>(metrics.padding);
            L.bar_y = H - static_cast<float>(metrics.padding);
            L.label_x = L.bar_x + bar_width_pixels / 2.0f;
            L.label_y = L.bar_y - 10.0f;
            break;
        case ScalebarCorner::BottomLeft:
            L.bar_x = static_cast<float>(metrics.padding);
            L.bar_y = H - static_cast<float>(metrics.padding);
            L.label_x = L.bar_x + bar_width_pixels / 2.0f;
            L.label_y = L.bar_y - 10.0f;
            break;
        case ScalebarCorner::TopRight: {
            float const tick_half = 0.5F * static_cast<float>(metrics.tick_height);
            L.bar_x = W - bar_width_pixels - static_cast<float>(metrics.padding);
            L.bar_y = static_cast<float>(metrics.padding) + tick_half + 4.0f;
            L.label_x = L.bar_x + bar_width_pixels / 2.0f;
            L.label_y = L.bar_y + static_cast<float>(metrics.label_font_size) + 8.0f;
            break;
        }
        case ScalebarCorner::TopLeft: {
            float const tick_half = 0.5F * static_cast<float>(metrics.tick_height);
            L.bar_x = static_cast<float>(metrics.padding);
            L.bar_y = static_cast<float>(metrics.padding) + tick_half + 4.0f;
            L.label_x = L.bar_x + bar_width_pixels / 2.0f;
            L.label_y = L.bar_y + static_cast<float>(metrics.label_font_size) + 8.0f;
            break;
        }
    }
    return L;
}

}// namespace

SVGScalebar::SVGScalebar(
        int scalebar_length,// NOLINT(bugprone-easily-swappable-parameters)
        float time_range_start,
        float time_range_end)
    : _scalebar_length(scalebar_length),
      _time_start(time_range_start),
      _time_end(time_range_end) {
}

void SVGScalebar::setCorner(ScalebarCorner corner) {
    _corner = corner;
}

void SVGScalebar::setPadding(int pixels) {
    _padding = pixels;
}

void SVGScalebar::setBarThickness(int pixels) {
    _bar_thickness = pixels;
}

void SVGScalebar::setTickHeight(int pixels) {
    _tick_height = pixels;
}

void SVGScalebar::setLabelFontSize(int pixels) {
    _label_font_size = pixels;
}

void SVGScalebar::setStrokeColor(std::string hex) {
    _stroke_color = std::move(hex);
}

void SVGScalebar::setLabelFillColor(std::string hex) {
    _label_fill_color = std::move(hex);
}

std::vector<std::string>
SVGScalebar::render(int canvas_width,// NOLINT(bugprone-easily-swappable-parameters)
                    int canvas_height) const {
    std::vector<std::string> elements;

    float const time_span = _time_end - _time_start;
    if (!(time_span > 0.0f) || canvas_width <= 0 || canvas_height <= 0) {
        return elements;
    }

    float const time_to_pixel = static_cast<float>(canvas_width) / time_span;
    float const bar_width_pixels = static_cast<float>(_scalebar_length) * time_to_pixel;

    float const tick_half = 0.5F * static_cast<float>(_tick_height);
    ScalebarLayout const layout = computeLayout(
            _corner,
            CanvasPx{canvas_width, canvas_height},
            bar_width_pixels,
            ScalebarMetrics{_padding, _tick_height, _label_font_size});

    float const bar_x = layout.bar_x;
    float const bar_y = layout.bar_y;

    std::ostringstream bar;
    bar << R"(<line x1=")" << bar_x << R"(" y1=")" << bar_y << R"(" x2=")" << (bar_x + bar_width_pixels)
        << R"(" y2=")" << bar_y << R"(" stroke=")" << _stroke_color << R"(" stroke-width=")" << _bar_thickness
        << R"(" stroke-linecap="butt"/>)";
    elements.push_back(bar.str());

    std::ostringstream left_tick;
    left_tick << R"(<line x1=")" << bar_x << R"(" y1=")" << (bar_y - tick_half) << R"(" x2=")" << bar_x
              << R"(" y2=")" << (bar_y + tick_half) << R"(" stroke=")" << _stroke_color << R"(" stroke-width="2"/>)";
    elements.push_back(left_tick.str());

    std::ostringstream right_tick;
    right_tick << R"(<line x1=")" << (bar_x + bar_width_pixels) << R"(" y1=")" << (bar_y - tick_half)
               << R"(" x2=")" << (bar_x + bar_width_pixels) << R"(" y2=")" << (bar_y + tick_half)
               << R"(" stroke=")" << _stroke_color << R"(" stroke-width="2"/>)";
    elements.push_back(right_tick.str());

    std::ostringstream label;
    label << R"(<text x=")" << layout.label_x << R"(" y=")" << layout.label_y
          << R"(" font-family="Arial, sans-serif" font-size=")" << _label_font_size << R"(" fill=")"
          << _label_fill_color << R"(" text-anchor="middle">)" << _scalebar_length << R"(</text>)";
    elements.push_back(label.str());

    return elements;
}

}// namespace PlottingSVG
