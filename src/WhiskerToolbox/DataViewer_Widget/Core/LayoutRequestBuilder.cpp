/**
 * @file LayoutRequestBuilder.cpp
 * @brief Implementation of DataViewer layout request assembly helpers.
 */

#include "LayoutRequestBuilder.hpp"

#include "DataViewerState.hpp"
#include "DataViewerStateData.hpp"
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
    std::optional<int> explicit_lane_order;
    int insertion_index{0};
};

/**
 * @brief Return deterministic fallback precedence for series types.
 * @pre type must be a valid CorePlotting::SeriesType enum value (enforcement: none)
 */
[[nodiscard]] int typePrecedence(CorePlotting::SeriesType type) {
    switch (type) {
        case CorePlotting::SeriesType::Analog:
            return 0;
        case CorePlotting::SeriesType::DigitalEvent:
            return 1;
        case CorePlotting::SeriesType::DigitalInterval:
            return 2;
    }
    return 3;
}

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

        std::optional<int> explicit_order = std::nullopt;
        if (auto const * lane_override = context.state.getSeriesLaneOverride(key);
            lane_override != nullptr && lane_override->lane_order.has_value()) {
            explicit_order = lane_override->lane_order;
        }

        candidates.push_back(StackableSeriesCandidate{key, CorePlotting::SeriesType::Analog, explicit_order, insertion_index++});
        visible_analog_keys.push_back(key);
    }

    for (auto const & [key, data]: context.data_store.eventSeries()) {
        auto const * event_opts = context.state.seriesOptions().get<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
        if (!event_opts || !event_opts->get_is_visible() ||
            event_opts->plotting_mode != EventPlottingModeData::Stacked) {
            continue;
        }

        std::optional<int> explicit_order = std::nullopt;
        if (auto const * lane_override = context.state.getSeriesLaneOverride(key);
            lane_override != nullptr && lane_override->lane_order.has_value()) {
            explicit_order = lane_override->lane_order;
        }

        candidates.push_back(StackableSeriesCandidate{key, CorePlotting::SeriesType::DigitalEvent, explicit_order, insertion_index++});
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

    std::unordered_map<std::string, int> analog_order_rank;
    if (allow_spike_sorter_fallback) {
        auto sorted_analog_keys = orderKeysBySpikeSorterConfig(visible_analog_keys, context.spike_sorter_configs);
        for (int index = 0; index < static_cast<int>(sorted_analog_keys.size()); ++index) {
            analog_order_rank[sorted_analog_keys[static_cast<size_t>(index)]] = index;
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(),
                     [&](StackableSeriesCandidate const & lhs, StackableSeriesCandidate const & rhs) {
                         bool const lhs_explicit = lhs.explicit_lane_order.has_value();
                         bool const rhs_explicit = rhs.explicit_lane_order.has_value();
                         if (lhs_explicit != rhs_explicit) {
                             return lhs_explicit && !rhs_explicit;
                         }

                         if (lhs_explicit && rhs_explicit) {
                             int const lhs_order = lhs.explicit_lane_order.value();
                             int const rhs_order = rhs.explicit_lane_order.value();
                             if (lhs_order != rhs_order) {
                                 return lhs_order < rhs_order;
                             }
                         }

                         if (allow_spike_sorter_fallback &&
                             lhs.type == CorePlotting::SeriesType::Analog &&
                             rhs.type == CorePlotting::SeriesType::Analog) {
                             int const lhs_rank = analog_order_rank.contains(lhs.key)
                                                          ? analog_order_rank.at(lhs.key)
                                                          : std::numeric_limits<int>::max();
                             int const rhs_rank = analog_order_rank.contains(rhs.key)
                                                          ? analog_order_rank.at(rhs.key)
                                                          : std::numeric_limits<int>::max();
                             if (lhs_rank != rhs_rank) {
                                 return lhs_rank < rhs_rank;
                             }
                         }

                         int const lhs_type = typePrecedence(lhs.type);
                         int const rhs_type = typePrecedence(rhs.type);
                         if (lhs_type != rhs_type) {
                             return lhs_type < rhs_type;
                         }

                         if (lhs.key != rhs.key) {
                             return lhs.key < rhs.key;
                         }

                         return lhs.insertion_index < rhs.insertion_index;
                     });
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
        auto const * series_override = context.state.getSeriesLaneOverride(candidate.key);

        std::string lane_id;
        float lane_weight = 1.0F;
        int custom_stack_index = -1;

        if (series_override != nullptr && !series_override->lane_id.empty()) {
            lane_id = series_override->lane_id;
            lane_weight = series_override->lane_weight;
            if (series_override->lane_order.has_value()) {
                custom_stack_index = *series_override->lane_order;
            }

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
