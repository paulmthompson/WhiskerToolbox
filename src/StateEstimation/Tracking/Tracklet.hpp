#ifndef STATEESTIMATION_TRACKLET_HPP
#define STATEESTIMATION_TRACKLET_HPP

#include "TimeFrame/TimeFrame.hpp"
#include "Entity/EntityTypes.hpp"
#include "Common.hpp"

#include <vector>

namespace StateEstimation {

struct NodeInfo {
        TimeFrameIndex frame = TimeFrameIndex(0);
        EntityId entity_id = 0;

        bool operator<(NodeInfo const & other) const {
            if (frame != other.frame) return frame < other.frame;
            return entity_id < other.entity_id;
        }
    };

    using Path = std::vector<NodeInfo>;

        /**
     * @brief Represents a greedy-linked sequence (meta-node) of cheap assignments across consecutive frames.
     * 
     * Each meta-node aggregates observations that are very likely to belong to the same chain, so that
     * min-cost flow can operate sparsely on these chains instead of per-observation nodes.
     */
    struct MetaNode {
        std::vector<NodeInfo> members;// consecutive observations included in this chain
        FilterState start_state;      // filter state after initializing on first observation
        FilterState end_state;        // filter state after updating on last observation
        TimeFrameIndex start_frame = TimeFrameIndex(0);
        TimeFrameIndex end_frame = TimeFrameIndex(0);
        EntityId start_entity = 0;
        EntityId end_entity = 0;
    };

    


}


#endif// STATEESTIMATION_TRACKLET_HPP