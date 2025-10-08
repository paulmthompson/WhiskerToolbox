#ifndef MIN_COST_FLOW_SOLVER_HPP
#define MIN_COST_FLOW_SOLVER_HPP

#include <cstdint>
#include <optional>
#include <vector>

namespace StateEstimation {

/**
 * @brief Arc specification for a directed graph edge used in min-cost flow.
 */
struct ArcSpec {
    int tail = 0;                 // from node index
    int head = 0;                 // to node index
    int64_t capacity = 0;         // capacity of the arc (typically 1 here)
    int64_t unit_cost = 0;        // non-negative integral cost per unit of flow
};

/**
 * @brief Solve a single-unit min-cost flow path problem.
 *
 * @pre Node indices in arcs, source_node, and sink_node are in [0, num_nodes).
 * @pre Exactly one unit of supply at source_node and -1 at sink_node is implied.
 * @post On success, returns the sequence of nodes (excluding none) encountered when
 *       following positive-flow arcs from source to sink. If no feasible solution
 *       exists, returns std::nullopt.
 */
[[nodiscard]] std::optional<std::vector<int>>
solveMinCostSingleUnitPath(int num_nodes,
                           int source_node,
                           int sink_node,
                           std::vector<ArcSpec> const & arcs);

}// namespace StateEstimation

#endif // MIN_COST_FLOW_SOLVER_HPP


