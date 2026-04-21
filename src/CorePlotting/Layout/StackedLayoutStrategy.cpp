#include "StackedLayoutStrategy.hpp"
#include "LayoutTransform.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace CorePlotting {

namespace {

struct LaneGroup {
    std::vector<int> member_indices;
    int first_series_index{std::numeric_limits<int>::max()};
    std::optional<int> explicit_order;
    float lane_weight{1.0f};
};

[[nodiscard]] float normalizeLaneWeight(float value) {
    return (std::isfinite(value) && value > 0.0F) ? value : 1.0F;
}

[[nodiscard]] bool isAutoLane(SeriesInfo const & info) {
    return info.lane_id.empty();
}

[[nodiscard]] std::string laneKeyForSeries(SeriesInfo const & info, int series_index) {
    if (!isAutoLane(info)) {
        return info.lane_id;
    }
    // Preserve legacy behavior: each auto lane series gets its own lane.
    return std::string("__auto_lane__") + std::to_string(series_index);
}

[[nodiscard]] SeriesLayout makeFullCanvasLayout(SeriesInfo const & series_info,
                                                int series_index,
                                                LayoutRequest const & request) {
    float const viewport_height = request.viewport_y_max - request.viewport_y_min;
    float const allocated_center = (request.viewport_y_min + request.viewport_y_max) * 0.5f;
    float const gain = viewport_height * 0.5f;
    float const offset = allocated_center;
    LayoutTransform const y_transform(offset, gain);
    return {series_info.id, y_transform, series_index};
}

}// namespace

LayoutResponse StackedLayoutStrategy::compute(LayoutRequest const & request) const {
    LayoutResponse response;
    response.layouts.resize(request.series.size());

    if (request.series.empty()) {
        return response;
    }

    // Build lane groups from stackable series only.
    // Non-stackable series retain full-canvas behavior.
    std::unordered_map<std::string, LaneGroup> groups_by_key;
    std::vector<std::string> lane_keys_in_insert_order;
    lane_keys_in_insert_order.reserve(request.series.size());

    for (size_t i = 0; i < request.series.size(); ++i) {
        auto const & info = request.series[i];
        int const series_index = static_cast<int>(i);

        if (!info.is_stackable) {
            response.layouts[i] = makeFullCanvasLayout(info, series_index, request);
            continue;
        }

        auto const lane_key = laneKeyForSeries(info, series_index);
        auto [it, inserted] = groups_by_key.try_emplace(lane_key);
        if (inserted) {
            lane_keys_in_insert_order.push_back(lane_key);
        }

        auto & group = it->second;
        group.member_indices.push_back(series_index);
        group.first_series_index = std::min(group.first_series_index, series_index);
        group.lane_weight = std::max(group.lane_weight, normalizeLaneWeight(info.lane_weight));
        if (info.custom_stack_index >= 0) {
            if (!group.explicit_order.has_value() || info.custom_stack_index < group.explicit_order.value_or(info.custom_stack_index)) {
                group.explicit_order = info.custom_stack_index;
            }
        }
    }

    if (lane_keys_in_insert_order.empty()) {
        return response;
    }

    // Sort lanes deterministically by explicit order (if present), then first appearance.
    std::stable_sort(lane_keys_in_insert_order.begin(), lane_keys_in_insert_order.end(),
                     [&groups_by_key](std::string const & lhs_key, std::string const & rhs_key) {
                         auto const & lhs = groups_by_key.at(lhs_key);
                         auto const & rhs = groups_by_key.at(rhs_key);
                         bool const lhs_explicit = lhs.explicit_order.has_value();
                         bool const rhs_explicit = rhs.explicit_order.has_value();

                         if (lhs_explicit != rhs_explicit) {
                             return lhs_explicit && !rhs_explicit;
                         }
                         if (lhs_explicit && rhs_explicit && lhs.explicit_order.value_or(0) != rhs.explicit_order.value_or(0)) {
                             return lhs.explicit_order.value_or(0) < rhs.explicit_order.value_or(0);
                         }
                         return lhs.first_series_index < rhs.first_series_index;
                     });

    float const viewport_height = request.viewport_y_max - request.viewport_y_min;
    float total_weight = 0.0f;
    for (auto const & key: lane_keys_in_insert_order) {
        total_weight += normalizeLaneWeight(groups_by_key.at(key).lane_weight);
    }
    if (!(std::isfinite(total_weight) && total_weight > 0.0f)) {
        total_weight = static_cast<float>(lane_keys_in_insert_order.size());
    }

    float lane_start = request.viewport_y_min;
    for (auto const & key: lane_keys_in_insert_order) {
        auto const & group = groups_by_key.at(key);
        float const normalized_weight = normalizeLaneWeight(group.lane_weight) / total_weight;
        float const lane_height = viewport_height * normalized_weight;
        float const lane_center = lane_start + lane_height * 0.5f;
        LayoutTransform const y_transform(lane_center, lane_height * 0.5f);

        for (int const member_index: group.member_indices) {
            auto const & series_info = request.series[static_cast<size_t>(member_index)];
            response.layouts[static_cast<size_t>(member_index)] = {series_info.id, y_transform, member_index};
        }

        lane_start += lane_height;
    }

    return response;
}

SeriesLayout StackedLayoutStrategy::computeFullCanvasLayout(
        SeriesInfo const & series_info,
        int series_index,
        LayoutRequest const & request) {
    return makeFullCanvasLayout(series_info, series_index, request);
}

}// namespace CorePlotting
