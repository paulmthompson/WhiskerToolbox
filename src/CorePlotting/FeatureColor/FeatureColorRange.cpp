/**
 * @file FeatureColorRange.cpp
 * @brief Implementation of effective color range computation
 */

#include "FeatureColorRange.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace CorePlotting::FeatureColor {

std::optional<std::pair<float, float>> computeEffectiveColorRange(
        std::span<std::optional<float> const> resolved_values,
        ColorRangeConfig const & config) {

    if (config.mode == ColorRangeMode::Manual) {
        return std::pair{config.manual_vmin, config.manual_vmax};
    }

    // Compute data min/max from non-nullopt values
    float data_min = std::numeric_limits<float>::max();
    float data_max = std::numeric_limits<float>::lowest();
    bool found_any = false;

    for (auto const & val: resolved_values) {
        if (!val.has_value()) {
            continue;
        }
        float const v = *val;
        if (std::isfinite(v)) {
            data_min = std::min(data_min, v);
            data_max = std::max(data_max, v);
            found_any = true;
        }
    }

    if (!found_any) {
        return std::nullopt;
    }

    if (config.mode == ColorRangeMode::Symmetric) {
        float const abs_max = std::max(std::abs(data_min), std::abs(data_max));
        return std::pair{-abs_max, abs_max};
    }

    // Auto mode
    return std::pair{data_min, data_max};
}

}// namespace CorePlotting::FeatureColor
