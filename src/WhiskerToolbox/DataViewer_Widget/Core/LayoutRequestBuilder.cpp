/**
 * @file LayoutRequestBuilder.cpp
 * @brief Implementation of DataViewer layout request assembly helpers.
 */

#include "LayoutRequestBuilder.hpp"

#include "DataViewerState.hpp"
#include "DataViewerStateData.hpp"
#include "Ordering/OrderingPolicyResolver.hpp"
#include "TimeSeriesDataStore.hpp"

#include <QString>

#include <algorithm>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace DataViewer {
namespace {

/**
 * @brief Transient stackable-series metadata used during ordering.
 */
struct StackableSeriesCandidate {
    std::string key;
    CorePlotting::SeriesType type;
    bool is_stackable{true};
    std::string lane_id_hint;
    float lane_weight_hint{1.0F};
    NormalizedSeriesIdentity identity;
    std::optional<int> explicit_lane_order;
    int insertion_index{0};
};

/**
 * @brief Check whether every candidate is analog.
 * @pre candidates may be empty; empty input is treated as true (enforcement: runtime_check)
 */
[[nodiscard]] bool allCandidatesAnalog(std::vector<StackableSeriesCandidate> const & candidates) {
    return std::all_of(candidates.begin(), candidates.end(), [](StackableSeriesCandidate const & c) {
        return c.type == CorePlotting::SeriesType::Analog;
    });
}

/**
 * @brief Build a normalized ordering item for one visible stackable series.
 * @pre key must refer to a stable series identifier in current state/data-store view (enforcement: none)
 */
[[nodiscard]] StackableSeriesCandidate makeStackableCandidate(LayoutRequestBuildContext const & context,
                                                              std::string key,
                                                              CorePlotting::SeriesType type,
                                                              int insertion_index) {
    std::optional<int> explicit_order = std::nullopt;
    std::string lane_id_hint;
    float lane_weight_hint = 1.0F;

    if (auto const * lane_override = context.state.getSeriesLaneOverride(key); lane_override != nullptr) {
        lane_id_hint = lane_override->lane_id;
        lane_weight_hint = lane_override->lane_weight;
        if (lane_override->lane_order.has_value()) {
            explicit_order = lane_override->lane_order;
        }
    }

    auto const identity = parseSeriesIdentity(key);

    return StackableSeriesCandidate{
            std::move(key),
            type,
            true,
            std::move(lane_id_hint),
            lane_weight_hint,
            identity,
            explicit_order,
            insertion_index};
}

/**
 * @brief Append visible non-stackable series to the layout request.
 * @pre context.state must outlive this function call (enforcement: none)
 * @pre context.data_store must outlive this function call (enforcement: none)
 * @pre request is a valid mutable LayoutRequest instance (enforcement: none)
 */
void appendNonStackableSeries(LayoutRequestBuildContext const & context,
                              CorePlotting::LayoutRequest & request) {
    for (auto const & [key, data]: context.data_store.eventSeries()) {
        auto const * event_opts = context.state.seriesOptions().get<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
        if (!event_opts || !event_opts->get_is_visible()) {
            continue;
        }
        if (event_opts->plotting_mode == EventPlottingModeData::Stacked) {
            continue;
        }
        request.series.emplace_back(key, CorePlotting::SeriesType::DigitalEvent, false);
    }

    for (auto const & [key, data]: context.data_store.intervalSeries()) {
        auto const * interval_opts = context.state.seriesOptions().get<DigitalIntervalSeriesOptionsData>(QString::fromStdString(key));
        if (!interval_opts || !interval_opts->get_is_visible()) {
            continue;
        }
        request.series.emplace_back(key, CorePlotting::SeriesType::DigitalInterval, false);
    }
}

/**
 * @brief Collect visible stackable candidates and analog keys for optional fallback ordering.
 * @pre context.state must outlive this function call (enforcement: none)
 * @pre context.data_store must outlive this function call (enforcement: none)
 * @pre visible_analog_keys is writable by this function (enforcement: none)
 */
[[nodiscard]] std::vector<StackableSeriesCandidate> collectStackableCandidates(LayoutRequestBuildContext const & context,
                                                                               std::vector<std::string> & visible_analog_keys) {
    std::vector<StackableSeriesCandidate> candidates;
    candidates.reserve(context.data_store.analogSeries().size() + context.data_store.eventSeries().size());
    visible_analog_keys.clear();
    visible_analog_keys.reserve(context.data_store.analogSeries().size());

    int insertion_index = 0;

    for (auto const & [key, data]: context.data_store.analogSeries()) {
        auto const * analog_opts = context.state.seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(key));
        if (!analog_opts || !analog_opts->get_is_visible()) {
            continue;
        }

        candidates.push_back(makeStackableCandidate(
                context,
                key,
                CorePlotting::SeriesType::Analog,
                insertion_index++));
        visible_analog_keys.push_back(key);
    }

    for (auto const & [key, data]: context.data_store.eventSeries()) {
        auto const * event_opts = context.state.seriesOptions().get<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
        if (!event_opts || !event_opts->get_is_visible() ||
            event_opts->plotting_mode != EventPlottingModeData::Stacked) {
            continue;
        }

        candidates.push_back(makeStackableCandidate(
                context,
                key,
                CorePlotting::SeriesType::DigitalEvent,
                insertion_index++));
    }

    return candidates;
}

/**
 * @brief Apply ordering policy to stackable candidates in-place.
 * @pre candidates represents a complete stackable set for one request build (enforcement: none)
 * @pre visible_analog_keys must correspond to visible analog series in this same request build (enforcement: none)
 * @pre context.spike_sorter_configs keys, when provided, are compatible with orderKeysBySpikeSorterConfig (enforcement: none)
 */
void applyOrderingRules(LayoutRequestBuildContext const & context,
                        std::vector<StackableSeriesCandidate> & candidates,
                        std::vector<std::string> const & visible_analog_keys) {
    bool const has_explicit_order = std::any_of(candidates.begin(), candidates.end(),
                                                [](StackableSeriesCandidate const & candidate) {
                                                    return candidate.explicit_lane_order.has_value();
                                                });

    bool const allow_spike_sorter_fallback = !has_explicit_order &&
                                             allCandidatesAnalog(candidates) &&
                                             !context.spike_sorter_configs.empty();

    SortableRankMap analog_order_rank;
    if (allow_spike_sorter_fallback) {
        analog_order_rank = buildSwindaleSpikeSorterRanks(visible_analog_keys, context.spike_sorter_configs);
    }

    std::vector<OrderingInputItem> resolver_input;
    resolver_input.reserve(candidates.size());
    for (auto const & candidate: candidates) {
        resolver_input.push_back(OrderingInputItem{
                candidate.key,
                candidate.type,
                candidate.identity,
                candidate.explicit_lane_order,
                candidate.insertion_index});
    }

    std::vector<OrderingConstraint> resolver_constraints;
    resolver_constraints.reserve(context.state.orderingConstraints().size());
    for (auto const & constraint: context.state.orderingConstraints()) {
        resolver_constraints.push_back(OrderingConstraint{
                constraint.above_series_key,
                constraint.below_series_key});
    }

    OrderingResolution const resolution = resolveOrdering(resolver_input, analog_order_rank, resolver_constraints);

    std::vector<StackableSeriesCandidate> ordered_candidates;
    ordered_candidates.reserve(candidates.size());
    for (int const input_index: resolution.ordered_input_indices) {
        ordered_candidates.push_back(candidates[static_cast<size_t>(input_index)]);
    }
    candidates = std::move(ordered_candidates);
}

/**
 * @brief Append ordered stackable series to the layout request with lane metadata.
 * @pre candidates must already be in final order for this request (enforcement: none)
 * @pre request is a valid mutable LayoutRequest instance (enforcement: none)
 * @pre context.state lane overrides, if present, must be normalized (enforcement: runtime_check)
 */
void appendStackableSeries(LayoutRequestBuildContext const & context,
                           std::vector<StackableSeriesCandidate> const & candidates,
                           CorePlotting::LayoutRequest & request) {
    for (auto const & candidate: candidates) {
        std::string lane_id = candidate.lane_id_hint;
        float lane_weight = candidate.lane_weight_hint;
        int const custom_stack_index = candidate.explicit_lane_order.value_or(-1);

        if (!lane_id.empty()) {
            if (auto const * lane_override = context.state.getLaneOverride(lane_id);
                lane_override != nullptr && lane_override->lane_weight.has_value()) {
                lane_weight = *lane_override->lane_weight;
            }
        }

        request.series.emplace_back(candidate.key,
                                    candidate.type,
                                    true,
                                    std::move(lane_id),
                                    lane_weight,
                                    custom_stack_index);
    }
}

}// namespace

/**
 * @brief Build a complete layout request from DataViewer runtime/state inputs.
 * @pre context.state must outlive this function call (enforcement: none)
 * @pre context.data_store must outlive this function call (enforcement: none)
 * @pre context.viewport_y_min <= context.viewport_y_max (enforcement: none)
 */
CorePlotting::LayoutRequest buildLayoutRequest(LayoutRequestBuildContext const & context) {
    CorePlotting::LayoutRequest request;
    request.viewport_y_min = context.viewport_y_min;
    request.viewport_y_max = context.viewport_y_max;

    appendNonStackableSeries(context, request);

    std::vector<std::string> visible_analog_keys;
    auto stackable_candidates = collectStackableCandidates(context, visible_analog_keys);
    applyOrderingRules(context, stackable_candidates, visible_analog_keys);
    appendStackableSeries(context, stackable_candidates, request);

    return request;
}

}// namespace DataViewer
