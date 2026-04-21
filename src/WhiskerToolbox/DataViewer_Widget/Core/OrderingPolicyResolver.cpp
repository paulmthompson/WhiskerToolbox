/**
 * @file OrderingPolicyResolver.cpp
 * @brief Policy resolver for stackable-series ordering precedence.
 */

#include "OrderingPolicyResolver.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <unordered_map>
#include <unordered_set>

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

[[nodiscard]] int fallbackRankFor(std::string const & key, SortableRankMap const & fallback_ranks) {
    return fallback_ranks.contains(key)
                   ? fallback_ranks.at(key)
                   : std::numeric_limits<int>::max();
}

[[nodiscard]] bool compareByBasePrecedence(IndexedOrderingItem const & lhs,
                                           IndexedOrderingItem const & rhs,
                                           SortableRankMap const & fallback_ranks) {
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

    int const lhs_rank = fallbackRankFor(lhs.item.key, fallback_ranks);
    int const rhs_rank = fallbackRankFor(rhs.item.key, fallback_ranks);
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
}

[[nodiscard]] bool conflictsWithExplicitPrecedence(IndexedOrderingItem const & above,
                                                   IndexedOrderingItem const & below) {
    bool const above_explicit = above.item.explicit_lane_order.has_value();
    bool const below_explicit = below.item.explicit_lane_order.has_value();

    if (above_explicit && below_explicit) {
        int const above_order = above.item.explicit_lane_order.value();
        int const below_order = below.item.explicit_lane_order.value();
        return above_order > below_order;
    }

    if (!above_explicit && below_explicit) {
        return true;
    }

    return false;
}

}// namespace

OrderingResolution resolveOrdering(
        std::vector<OrderingInputItem> const & input_items,
        SortableRankMap const & fallback_ranks,
        std::vector<OrderingConstraint> const & constraints) {

    std::vector<IndexedOrderingItem> indexed;
    indexed.reserve(input_items.size());

    for (int i = 0; i < static_cast<int>(input_items.size()); ++i) {
        indexed.push_back(IndexedOrderingItem{i, input_items[static_cast<size_t>(i)]});
    }

    std::stable_sort(indexed.begin(), indexed.end(),
                     [&fallback_ranks](IndexedOrderingItem const & lhs, IndexedOrderingItem const & rhs) {
                         return compareByBasePrecedence(lhs, rhs, fallback_ranks);
                     });

    std::unordered_map<std::string, int> key_to_sorted_index;
    key_to_sorted_index.reserve(indexed.size());
    for (int i = 0; i < static_cast<int>(indexed.size()); ++i) {
        key_to_sorted_index[indexed[static_cast<size_t>(i)].item.key] = i;
    }

    std::vector<std::set<int>> outgoing(indexed.size());
    std::vector<int> indegree(indexed.size(), 0);

    for (auto const & constraint: constraints) {
        if (!key_to_sorted_index.contains(constraint.above_key) ||
            !key_to_sorted_index.contains(constraint.below_key)) {
            continue;
        }

        int const above_index = key_to_sorted_index.at(constraint.above_key);
        int const below_index = key_to_sorted_index.at(constraint.below_key);
        if (above_index == below_index) {
            continue;
        }

        auto const & above_item = indexed[static_cast<size_t>(above_index)];
        auto const & below_item = indexed[static_cast<size_t>(below_index)];
        if (conflictsWithExplicitPrecedence(above_item, below_item)) {
            continue;
        }

        if (outgoing[static_cast<size_t>(above_index)].insert(below_index).second) {
            ++indegree[static_cast<size_t>(below_index)];
        }
    }

    std::unordered_set<int> constrained_nodes;
    constrained_nodes.reserve(indexed.size());
    for (int i = 0; i < static_cast<int>(indexed.size()); ++i) {
        if (!outgoing[static_cast<size_t>(i)].empty() || indegree[static_cast<size_t>(i)] > 0) {
            constrained_nodes.insert(i);
        }
    }

    std::vector<int> order_indices;
    order_indices.reserve(indexed.size());
    std::unordered_set<int> cycle_break_nodes;
    cycle_break_nodes.reserve(indexed.size());
    std::vector<bool> emitted(indexed.size(), false);

    while (order_indices.size() < indexed.size()) {
        int selected = -1;
        for (int i = 0; i < static_cast<int>(indexed.size()); ++i) {
            if (emitted[static_cast<size_t>(i)] || indegree[static_cast<size_t>(i)] != 0) {
                continue;
            }

            if (selected < 0 || compareByBasePrecedence(indexed[static_cast<size_t>(i)],
                                                        indexed[static_cast<size_t>(selected)],
                                                        fallback_ranks)) {
                selected = i;
            }
        }

        if (selected < 0) {
            for (int i = 0; i < static_cast<int>(indexed.size()); ++i) {
                if (emitted[static_cast<size_t>(i)]) {
                    continue;
                }
                if (selected < 0 || compareByBasePrecedence(indexed[static_cast<size_t>(i)],
                                                            indexed[static_cast<size_t>(selected)],
                                                            fallback_ranks)) {
                    selected = i;
                }
            }
            cycle_break_nodes.insert(selected);
        }

        emitted[static_cast<size_t>(selected)] = true;
        order_indices.push_back(selected);
        for (int const dst: outgoing[static_cast<size_t>(selected)]) {
            --indegree[static_cast<size_t>(dst)];
        }
    }

    OrderingResolution resolution;
    resolution.ordered_input_indices.reserve(order_indices.size());
    resolution.diagnostics.reserve(order_indices.size());

    for (int rank = 0; rank < static_cast<int>(order_indices.size()); ++rank) {
        int const ordered_index = order_indices[static_cast<size_t>(rank)];
        auto const & indexed_item = indexed[static_cast<size_t>(ordered_index)];
        resolution.ordered_input_indices.push_back(indexed_item.input_index);

        OrderingDiagnosticReason reason = OrderingDiagnosticReason::DeterministicTieBreak;
        if (indexed_item.item.explicit_lane_order.has_value()) {
            reason = OrderingDiagnosticReason::ExplicitLaneOrder;
        } else if (cycle_break_nodes.contains(ordered_index)) {
            reason = OrderingDiagnosticReason::ConstraintCycleBreak;
        } else if (constrained_nodes.contains(ordered_index)) {
            reason = OrderingDiagnosticReason::RelationalConstraint;
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
