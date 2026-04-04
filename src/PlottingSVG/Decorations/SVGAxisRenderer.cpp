/**
 * @file SVGAxisRenderer.cpp
 * @brief Axis spine, ticks, and labels in SVG coordinates.
 */

#include "PlottingSVG/Decorations/SVGAxisRenderer.hpp"

#include <cmath>
#include <limits>
#include <sstream>
#include <string_view>

namespace PlottingSVG {

namespace {

[[nodiscard]] std::string escapeForSvgText(std::string_view const text) {
    std::string out;
    out.reserve(text.size());
    for (char const c: text) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

[[nodiscard]] std::string formatTickLabel(float v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

}// namespace

SVGAxisRenderer::SVGAxisRenderer(
        float range_min,// NOLINT(bugprone-easily-swappable-parameters)
        float range_max,
        float tick_interval,
        std::string axis_title)
    : _range_min(range_min),
      _range_max(range_max),
      _tick_interval(tick_interval),
      _axis_title(std::move(axis_title)) {
}

void SVGAxisRenderer::setPosition(AxisPosition position) {
    _position = position;
}

void SVGAxisRenderer::setMargin(int pixels) {
    _margin = pixels;
}

void SVGAxisRenderer::setFontSize(int pixels) {
    _font_size = pixels;
}

void SVGAxisRenderer::setTickLength(int pixels) {
    _tick_length = pixels;
}

void SVGAxisRenderer::setAxisColor(std::string stroke_hex) {
    _axis_color = std::move(stroke_hex);
}

void SVGAxisRenderer::setLabelColor(std::string fill_hex) {
    _label_color = std::move(fill_hex);
}

std::vector<std::string>
SVGAxisRenderer::render(int canvas_width,// NOLINT(bugprone-easily-swappable-parameters)
                        int canvas_height) const {
    std::vector<std::string> elements;
    if (canvas_width <= 0 || canvas_height <= 0) {
        return elements;
    }

    float const span = _range_max - _range_min;
    if (!(span > 0.0f) || !std::isfinite(span)) {
        return elements;
    }

    int const M = _margin;
    if (M * 2 >= canvas_width || M * 2 >= canvas_height) {
        return elements;
    }

    auto const inner_w = static_cast<float>(canvas_width - 2 * M);
    auto const inner_h = static_cast<float>(canvas_height - 2 * M);

    if (_position == AxisPosition::Bottom) {
        auto const axis_y = static_cast<float>(canvas_height - M);
        auto const x0 = static_cast<float>(M);
        auto const x1 = static_cast<float>(canvas_width - M);

        std::ostringstream spine;
        spine << R"(<line x1=")" << x0 << R"(" y1=")" << axis_y << R"(" x2=")" << x1 << R"(" y2=")" << axis_y
              << R"(" stroke=")" << _axis_color << R"(" stroke-width="1"/>)";
        elements.push_back(spine.str());

        if (_tick_interval > 0.0f) {
            auto const step_d = static_cast<double>(_tick_interval);
            auto const first_d =
                    std::ceil(static_cast<double>(_range_min) / step_d) * step_d;
            auto v = static_cast<float>(first_d);
            int guard = 0;
            float const eps = std::max(1e-5f * span, std::numeric_limits<float>::epsilon() * 10.0f);
            while (v <= _range_max + eps && guard < 10000) {
                float const t = (v - _range_min) / span;
                float const x = x0 + t * inner_w;
                std::ostringstream tick;
                tick << R"(<line x1=")" << x << R"(" y1=")" << axis_y << R"(" x2=")" << x << R"(" y2=")"
                     << (axis_y + static_cast<float>(_tick_length)) << R"(" stroke=")" << _axis_color
                     << R"(" stroke-width="1"/>)";
                elements.push_back(tick.str());

                std::string const label = formatTickLabel(v);
                std::ostringstream tlab;
                tlab << R"(<text x=")" << x << R"(" y=")"
                     << (axis_y + static_cast<float>(_tick_length) + static_cast<float>(_font_size) * 0.85f)
                     << R"(" font-family="Arial, sans-serif" font-size=")" << _font_size << R"(" fill=")"
                     << _label_color << R"(" text-anchor="middle" dominant-baseline="alphabetic">)"
                     << escapeForSvgText(label) << R"(</text>)";
                elements.push_back(tlab.str());

                v += _tick_interval;
                ++guard;
            }
        }

        if (!_axis_title.empty()) {
            auto const title_x = static_cast<float>(canvas_width) / 2.0f;
            auto const title_y =
                    axis_y + static_cast<float>(_tick_length) + static_cast<float>(_font_size) * 2.2f;
            std::ostringstream title;
            title << R"(<text x=")" << title_x << R"(" y=")" << title_y
                  << R"(" font-family="Arial, sans-serif" font-size=")" << (_font_size + 2)
                  << R"(" fill=")" << _label_color << R"(" text-anchor="middle" dominant-baseline="alphabetic">)"
                  << escapeForSvgText(_axis_title) << R"(</text>)";
            elements.push_back(title.str());
        }
        return elements;
    }

    // Left axis
    auto const axis_x = static_cast<float>(M);
    auto const y0 = static_cast<float>(M);
    auto const y1 = static_cast<float>(canvas_height - M);

    std::ostringstream spine;
    spine << R"(<line x1=")" << axis_x << R"(" y1=")" << y0 << R"(" x2=")" << axis_x << R"(" y2=")" << y1
          << R"(" stroke=")" << _axis_color << R"(" stroke-width="1"/>)";
    elements.push_back(spine.str());

    if (_tick_interval > 0.0f) {
        auto const step_d = static_cast<double>(_tick_interval);
        auto const first_d = std::ceil(static_cast<double>(_range_min) / step_d) * step_d;
        auto v = static_cast<float>(first_d);
        int guard = 0;
        float const eps = std::max(1e-5f * span, std::numeric_limits<float>::epsilon() * 10.0f);
        while (v <= _range_max + eps && guard < 10000) {
            float const t = (v - _range_min) / span;
            float const y = y1 - t * inner_h;
            std::ostringstream tick;
            tick << R"(<line x1=")" << axis_x << R"(" y1=")" << y << R"(" x2=")"
                 << (axis_x - static_cast<float>(_tick_length)) << R"(" y2=")" << y << R"(" stroke=")"
                 << _axis_color << R"(" stroke-width="1"/>)";
            elements.push_back(tick.str());

            std::string const label = formatTickLabel(v);
            std::ostringstream tlab;
            tlab << R"(<text x=")" << (axis_x - static_cast<float>(_tick_length) - 4.0f) << R"(" y=")" << y
                 << R"(" font-family="Arial, sans-serif" font-size=")" << _font_size << R"(" fill=")"
                 << _label_color << R"(" text-anchor="end" dominant-baseline="middle">)"
                 << escapeForSvgText(label) << R"(</text>)";
            elements.push_back(tlab.str());

            v += _tick_interval;
            ++guard;
        }
    }

    if (!_axis_title.empty()) {
        auto const cx = static_cast<float>(_margin) * 0.35f;
        auto const cy = static_cast<float>(canvas_height) / 2.0f;
        std::ostringstream title;
        title << R"(<text x=")" << cx << R"(" y=")" << cy
              << R"(" font-family="Arial, sans-serif" font-size=")" << (_font_size + 2)
              << R"(" fill=")" << _label_color
              << R"(" text-anchor="middle" dominant-baseline="middle")"
              << " transform=\"rotate(-90," << cx << ',' << cy << ")\">"
              << escapeForSvgText(_axis_title) << R"(</text>)";
        elements.push_back(title.str());
    }

    return elements;
}

}// namespace PlottingSVG
