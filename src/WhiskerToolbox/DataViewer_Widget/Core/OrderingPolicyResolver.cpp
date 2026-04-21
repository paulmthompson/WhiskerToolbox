/**
 * @file OrderingPolicyResolver.cpp
 * @brief Policy resolver for stackable-series ordering precedence.
 */

#include "OrderingPolicyResolver.hpp"

#include <algorithm>
#include <limits>

namespace DataViewer {
namespace {

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

struct IndexedOrderingItem {
    int input_index{0};
    OrderingInputItem item;
};

}// namespace

OrderingResolution resolveOrdering(
        std::vector<OrderingInputItem> const & input_items,
        SortableRankMap const & fallback_ranks) {

    std::vector<IndexedOrderingItem> indexed;
    indexed.reserve(input_items.size());

    for (int i = 0; i < static_cast<int>(input_items.size()); ++i) {
        indexed.push_back(IndexedOrderingItem{i, input_items[static_cast<size_t>(i)]});
    }

    std::stable_sort(indexed.begin(), indexed.end(),
                     [&fallback_ranks](IndexedOrderingItem const & lhs, IndexedOrderingItem const & rhs) {
                         bool const lhs_explicit = lhs.item.explicit_lane_order.has_value();
                         bool const rhs_explicit = rhs.item.explicit_lane_order.has_value();
                         if (lhs_explicit != rhs_explicit) {
                             return lhs_explicit && !rhs_explicit;
                         }

                         if (lhs_explicit && rhs_explicit) {
                             int const lhs_order = lhs.item.explicit_lane_order.value();
                             int const rhs_order = rhs.item.explicit_lane_order.value();
                             if (lhs_order != rhs_order) {
                                 return lhs_order < rhs_order;
                             }
                         }

                         int const lhs_rank = fallback_ranks.contains(lhs.item.key)
                                                      ? fallback_ranks.at(lhs.item.key)
                                                      : std::numeric_limits<int>::max();
                         int const rhs_rank = fallback_ranks.contains(rhs.item.key)
                                                      ? fallback_ranks.at(rhs.item.key)
                                                      : std::numeric_limits<int>::max();
                         if (lhs_rank != rhs_rank) {
                             return lhs_rank < rhs_rank;
                         }

                         if (lhs.item.identity.group != rhs.item.identity.group) {
                             return lhs.item.identity.group < rhs.item.identity.group;
                         }

                         int const lhs_channel = lhs.item.identity.channel_id.value_or(std::numeric_limits<int>::max());
                         int const rhs_channel = rhs.item.identity.channel_id.value_or(std::numeric_limits<int>::max());
                         if (lhs_channel != rhs_channel) {
                             return lhs_channel < rhs_channel;
                         }

                         int const lhs_type = typePrecedence(lhs.item.type);
                         int const rhs_type = typePrecedence(rhs.item.type);
                         if (lhs_type != rhs_type) {
                             return lhs_type < rhs_type;
                         }

                         if (lhs.item.key != rhs.item.key) {
                             return lhs.item.key < rhs.item.key;
                         }

                         return lhs.item.insertion_index < rhs.item.insertion_index;
                     });

    OrderingResolution resolution;
    resolution.ordered_input_indices.reserve(indexed.size());
    resolution.diagnostics.reserve(indexed.size());

    for (int rank = 0; rank < static_cast<int>(indexed.size()); ++rank) {
        auto const & indexed_item = indexed[static_cast<size_t>(rank)];
        resolution.ordered_input_indices.push_back(indexed_item.input_index);

        OrderingDiagnosticReason reason = OrderingDiagnosticReason::DeterministicTieBreak;
        if (indexed_item.item.explicit_lane_order.has_value()) {
            reason = OrderingDiagnosticReason::ExplicitLaneOrder;
        } else if (fallback_ranks.contains(indexed_item.item.key)) {
            reason = OrderingDiagnosticReason::FallbackSortableRank;
        }

        OrderingDiagnostic diagnostic;
        diagnostic.key = indexed_item.item.key;
        diagnostic.resolved_rank = rank;
        diagnostic.reason = reason;
        diagnostic.explicit_lane_order = indexed_item.item.explicit_lane_order;
        if (fallback_ranks.contains(indexed_item.item.key)) {
            diagnostic.fallback_rank = fallback_ranks.at(indexed_item.item.key);
        }
        resolution.diagnostics.push_back(std::move(diagnostic));
    }

    return resolution;
}

}// namespace DataViewer
