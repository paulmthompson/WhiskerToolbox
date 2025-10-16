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

    auto status = min_cost_flow.Solve();
    if (status != operations_research::SimpleMinCostFlow::OPTIMAL) {
        std::cerr << "Min-cost flow solver failed with status: " << status << std::endl;

        //Try again with solvemaxflowwithmincost
        status = min_cost_flow.SolveMaxFlowWithMinCost();

        if (status != operations_research::SimpleMinCostFlow::OPTIMAL) {
            std::cerr << "Min-cost with max flow failed with status: " << status << std::endl;
            return std::nullopt;
        }
        else {
            std::cout << "Min-cost flow solver succeeded when switched to max flow with status: " << status << std::endl;
        }
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

    const int flow_limit = 10;

    for (int steps = 0; steps < (num_nodes + 2)*flow_limit; ++steps) {
        auto it = successor.find(current);
        if (it == successor.end()) break;
        current = it->second;
        sequence.push_back(current);
        if (current == sink_node) break;
    }

    if (sequence.empty() || sequence.front() != source_node) {
        std::cerr << "Min-cost flow solver failed to find a path from source to sink" << std::endl;
        return std::nullopt;
    }
    return sequence;
}

} // namespace StateEstimation


