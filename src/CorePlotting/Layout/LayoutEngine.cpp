#include "LayoutEngine.hpp"
#include <algorithm>

namespace CorePlotting {

// ============================================================================
// LayoutRequest
// ============================================================================

int LayoutRequest::countSeriesOfType(SeriesType type) const {
    return static_cast<int>(std::count_if(series.begin(), series.end(),
        [type](SeriesInfo const& s) { return s.type == type; }));
}

int LayoutRequest::countStackableSeries() const {
    return static_cast<int>(std::count_if(series.begin(), series.end(),
        [](SeriesInfo const& s) { return s.is_stackable; }));
}

// ============================================================================
// LayoutResponse
// ============================================================================

SeriesLayout const* LayoutResponse::findLayout(std::string const& series_id) const {
    auto it = std::find_if(layouts.begin(), layouts.end(),
        [&series_id](SeriesLayout const& layout) {
            return layout.series_id == series_id;
        });
    return (it != layouts.end()) ? &(*it) : nullptr;
}

// ============================================================================
// LayoutEngine
// ============================================================================

LayoutEngine::LayoutEngine(std::unique_ptr<ILayoutStrategy> strategy)
    : _strategy(std::move(strategy)) {}

LayoutResponse LayoutEngine::compute(LayoutRequest const& request) const {
    if (!_strategy) {
        return LayoutResponse{}; // Empty response if no strategy
    }
    return _strategy->compute(request);
}

void LayoutEngine::setStrategy(std::unique_ptr<ILayoutStrategy> strategy) {
    _strategy = std::move(strategy);
}

} // namespace CorePlotting
