#include "MinCostFlowSolver.hpp"

#include "ortools/graph/min_cost_flow.h"

#include <unordered_map>

namespace StateEstimation {

std::optional<std::vector<int>>
solveMinCostSingleUnitPath(int num_nodes,
                           int source_node,
                           int sink_node,
                           std::vector<ArcSpec> const & arcs) {
    operations_research::SimpleMinCostFlow min_cost_flow;

    for (ArcSpec const & a : arcs) {
        min_cost_flow.AddArcWithCapacityAndUnitCost(a.tail, a.head, a.capacity, a.unit_cost);
    }

    min_cost_flow.SetNodeSupply(source_node, 1);
    min_cost_flow.SetNodeSupply(sink_node, -1);

    auto const status = min_cost_flow.Solve();
    if (status != operations_research::SimpleMinCostFlow::OPTIMAL) {
        return std::nullopt;
    }

    // Build successor map from positive-flow arcs
    std::unordered_map<int, int> successor;
    successor.reserve(static_cast<size_t>(min_cost_flow.NumNodes()));
    for (int a = 0; a < min_cost_flow.NumArcs(); ++a) {
        if (min_cost_flow.Flow(a) > 0) {
            successor[min_cost_flow.Tail(a)] = min_cost_flow.Head(a);
        }
    }

    // Follow from source to sink to produce a node sequence
    std::vector<int> sequence;
    sequence.reserve(static_cast<size_t>(num_nodes));
    int current = source_node;
    sequence.push_back(current);
    // Safety cap to prevent infinite loops on malformed graphs
    for (int steps = 0; steps < num_nodes + 2; ++steps) {
        auto it = successor.find(current);
        if (it == successor.end()) break;
        current = it->second;
        sequence.push_back(current);
        if (current == sink_node) break;
    }

    if (sequence.empty() || sequence.front() != source_node) {
        return std::nullopt;
    }
    return sequence;
}

} // namespace StateEstimation


