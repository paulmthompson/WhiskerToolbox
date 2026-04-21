#ifndef DATAVIEWER_ORDERING_POLICY_RESOLVER_HPP
#define DATAVIEWER_ORDERING_POLICY_RESOLVER_HPP

/**
 * @file OrderingPolicyResolver.hpp
 * @brief Resolves stackable-series ordering precedence with lightweight diagnostics.
 */

#include "Ordering/ChannelPositionMetadata.hpp"

#include "CorePlotting/Layout/LayoutEngine.hpp"

#include <optional>
#include <string>
#include <vector>

namespace DataViewer {

/**
 * @brief Normalized ordering input consumed by ordering policy resolution.
 */
struct OrderingInputItem {
    std::string key;
    CorePlotting::SeriesType type;
    NormalizedSeriesIdentity identity;
    std::optional<int> explicit_lane_order;
    int insertion_index{0};
};

/**
 * @brief High-level reason category for resolved placement diagnostics.
 */
enum class OrderingDiagnosticReason {
    ExplicitLaneOrder,
    RelationalConstraint,
    FallbackSortableRank,
    ConstraintCycleBreak,
    DeterministicTieBreak,
};

/**
 * @brief Relative ordering constraint between two series keys.
 *
 * `above_key` must appear above `below_key` in stackable ordering.
 */
struct OrderingConstraint {
    std::string above_key;
    std::string below_key;
};

/**
 * @brief Lightweight diagnostic for one ordered series.
 */
struct OrderingDiagnostic {
    std::string key;
    int resolved_rank{0};
    OrderingDiagnosticReason reason{OrderingDiagnosticReason::DeterministicTieBreak};
    std::optional<int> explicit_lane_order;
    std::optional<int> fallback_rank;
};

/**
 * @brief Ordering resolution output.
 */
struct OrderingResolution {
    std::vector<int> ordered_input_indices;
    std::vector<OrderingDiagnostic> diagnostics;
};

/**
 * @brief Resolve ordering with precedence:
 * 1) explicit lane order
 * 2) relational constraints (when present)
 * 3) fallback provider ranks
 * 4) deterministic tie-breaks
 */
[[nodiscard]] OrderingResolution resolveOrdering(
        std::vector<OrderingInputItem> const & input_items,
        SortableRankMap const & fallback_ranks,
        std::vector<OrderingConstraint> const & constraints = {});

}// namespace DataViewer

#endif// DATAVIEWER_ORDERING_POLICY_RESOLVER_HPP
