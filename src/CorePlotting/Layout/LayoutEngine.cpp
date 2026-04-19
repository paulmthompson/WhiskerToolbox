#include "LayoutEngine.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace CorePlotting {

// ============================================================================
// LayoutRequest
// ============================================================================

int LayoutRequest::countSeriesOfType(SeriesType type) const {
    return static_cast<int>(std::count_if(series.begin(), series.end(),
                                          [type](SeriesInfo const & s) { return s.type == type; }));
}

int LayoutRequest::countStackableSeries() const {
    return static_cast<int>(std::count_if(series.begin(), series.end(),
                                          [](SeriesInfo const & s) { return s.is_stackable; }));
}

// ============================================================================
// LayoutResponse
// ============================================================================

SeriesLayout const * LayoutResponse::findLayout(std::string const & series_id) const {
    auto it = std::find_if(layouts.begin(), layouts.end(),
                           [&series_id](SeriesLayout const & layout) {
                               return layout.series_id == series_id;
                           });
    return (it != layouts.end()) ? &(*it) : nullptr;
}

bool LayoutResponse::isSeriesVisible(std::string const & series_id,
                                     float y_min, float y_max) const {
    auto const * layout = findLayout(series_id);
    if (!layout) {
        // Series not in layout (e.g., non-stackable) — conservatively visible
        return true;
    }
    float const half_height = std::abs(layout->y_transform.gain);
    float const lane_bottom = layout->y_transform.offset - half_height;
    float const lane_top = layout->y_transform.offset + half_height;
    // Interval overlap test: two ranges overlap iff neither is entirely before the other
    return lane_top >= y_min && lane_bottom <= y_max;
}

std::vector<std::string> LayoutResponse::visibleSeriesIds(float y_min, float y_max) const {
    std::vector<std::string> result;
    result.reserve(layouts.size());
    for (auto const & layout: layouts) {
        float const half_height = std::abs(layout.y_transform.gain);
        float const lane_bottom = layout.y_transform.offset - half_height;
        float const lane_top = layout.y_transform.offset + half_height;
        if (lane_top >= y_min && lane_bottom <= y_max) {
            result.push_back(layout.series_id);
        }
    }
    return result;
}

// ============================================================================
// LayoutEngine
// ============================================================================

LayoutEngine::LayoutEngine(std::unique_ptr<ILayoutStrategy> strategy)
    : _strategy(std::move(strategy)) {}

LayoutResponse LayoutEngine::compute(LayoutRequest const & request) const {
    if (!_strategy) {
        return LayoutResponse{};// Empty response if no strategy
    }
    return _strategy->compute(request);
}

void LayoutEngine::setStrategy(std::unique_ptr<ILayoutStrategy> strategy) {
    _strategy = std::move(strategy);
}

}// namespace CorePlotting
