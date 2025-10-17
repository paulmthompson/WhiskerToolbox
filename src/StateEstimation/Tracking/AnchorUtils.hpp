#ifndef STATEESTIMATION_TRACKING_ANCHOR_UTILS_HPP
#define STATEESTIMATION_TRACKING_ANCHOR_UTILS_HPP

#include "Tracking/Tracklet.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace StateEstimation {

        /**
     * @brief Locate anchors with both meta-node and member indices.
     *
     * @return Optional positions if both anchors are found.
     */
struct AnchorPositions {
    int start_meta_index = -1;
    size_t start_member_index = 0;
    int end_meta_index = -1;
    size_t end_member_index = 0;
};

/**
 * @brief Locate positions of start and end anchors within a collection of meta-nodes.
 *
 * Searches for the first occurrences of (start_frame,start_entity) and
 * (end_frame,end_entity) inside `meta_nodes` and returns both the meta-node index
 * and the member index for each anchor when found.
 *
 * @return std::optional of tuple (start_meta_index, start_member_index, end_meta_index, end_member_index)
 */
inline std::optional<std::tuple<int, std::size_t, int, std::size_t>>
findAnchorPositionsInMetaNodes(std::vector<MetaNode> const & meta_nodes,
                               TimeFrameIndex start_frame,
                               EntityId start_entity,
                               TimeFrameIndex end_frame,
                               EntityId end_entity) {
    int start_meta_index = -1;
    std::size_t start_member_index = 0;
    int end_meta_index = -1;
    std::size_t end_member_index = 0;

    for (std::size_t i = 0; i < meta_nodes.size(); ++i) {
        auto const & mn = meta_nodes[i];
        for (std::size_t k = 0; k < mn.members.size(); ++k) {
            auto const & m = mn.members[k];
            if (start_meta_index == -1 && m.frame == start_frame && m.entity_id == start_entity) {
                start_meta_index = static_cast<int>(i);
                start_member_index = k;
            }
            if (end_meta_index == -1 && m.frame == end_frame && m.entity_id == end_entity) {
                end_meta_index = static_cast<int>(i);
                end_member_index = k;
            }
        }
        if (start_meta_index != -1 && end_meta_index != -1) break;
    }

    if (start_meta_index == -1 || end_meta_index == -1) {
        return std::nullopt;
    }
    return std::make_optional(std::make_tuple(start_meta_index, start_member_index,
                                              end_meta_index, end_member_index));
}

/**
 * @brief Returns AnchorPositions (meta-node and member indices) for the two anchors.
 */
inline std::optional<AnchorPositions>
findAnchorPositions(std::vector<MetaNode> const & meta_nodes,
                    TimeFrameIndex start_frame,
                    EntityId start_entity,
                    TimeFrameIndex end_frame,
                    EntityId end_entity) {
    auto pos_opt = findAnchorPositionsInMetaNodes(meta_nodes, start_frame, start_entity, end_frame, end_entity);
    if (!pos_opt) return std::nullopt;
    auto const & [smi, sidx, emi, eidx] = *pos_opt;
    AnchorPositions positions;
    positions.start_meta_index = smi;
    positions.start_member_index = sidx;
    positions.end_meta_index = emi;
    positions.end_member_index = eidx;
    return positions;
}

/**
 * @brief Convenience that returns only the meta-node indices that contain the two anchors.
 * Returns {-1,-1} if either anchor is not found.
 */
inline std::pair<int, int>
findAnchorMetaNodeIndices(std::vector<MetaNode> const & meta_nodes,
                          TimeFrameIndex start_frame,
                          EntityId start_entity,
                          TimeFrameIndex end_frame,
                          EntityId end_entity) {
    auto pos_opt = findAnchorPositionsInMetaNodes(meta_nodes, start_frame, start_entity, end_frame, end_entity);
    if (!pos_opt) return {-1, -1};
    auto const & [smi, /*sidx*/_, emi, /*eidx*/__] = *pos_opt;
    return {smi, emi};
}

} // namespace StateEstimation

#endif // STATEESTIMATION_TRACKING_ANCHOR_UTILS_HPP


